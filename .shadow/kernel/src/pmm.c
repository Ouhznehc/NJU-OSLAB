#include <common.h>

// #define DEAD_LOCK
static kspinlock_t heap_lock;
static kspinlock_t slab_lock;
static memory_t heap_pool;
static memory_t slab_pool;
static kmem_cache kmem[MAX_CPU];
int slab_type[SLAB_TYPE] = { 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048 };

static inline int match_slab_type(size_t size) {
  for (int i = 0; i < SLAB_TYPE; i++)
    if (size <= slab_type[i])
      return i;
  panic("match_slab_type: should not reach here");
  return 0;
}

static inline size_t align_size(size_t size) {
  size_t msb = 31 - __builtin_clz(size);
  if (size == (1 << msb)) return size;
  else return (1 << (msb + 1));
}

static inline uintptr_t align_address(void* address, size_t size) {
  return ROUNDUP((uintptr_t)address, size);
}

static void* object_from_slab(slab_t* page) {
  void* ret = NULL;
  assert(page != NULL);
  assert(page->object_counter < page->object_capacity);
  for (int i = 0; i < 64; i++) {
    if (page->bitset[i] == (int)(-1))
      continue;
    for (int j = 0; j < 32; j++) {
      if (getbit(page->bitset[i], j) == 0) {
        assert(32 * i + j <= page->object_capacity);
        setbit(page->bitset[i], j);
        page->object_counter++;
        if (page->object_counter == page->object_capacity) {
          int cpu = page->cpu, slab_index = match_slab_type(page->object_size);
          assert(kmem[cpu].slab_list[slab_index].next = page);
          kmem[cpu].slab_list[slab_index].next = page->next;
          page->next = NULL;
        }
        ret = page->object_start + (32 * i + j) * page->object_size;
        return ret;
      }
    }
  }
  panic("object_from_slab: should not reach here");
  return NULL;
}

// get memory from heap with size
static memory_t* memory_from_heap(size_t size) {
  memory_t* ret = NULL;
  kspin_lock(&heap_lock);
  if (heap_pool.next == NULL) ret = NULL;
  else {
    memory_t* cur = heap_pool.next, * prev = NULL;
    assert(cur != NULL);
    while (cur != NULL && ((uintptr_t)cur->memory_start + cur->memory_size < align_address(cur->memory_start, size) + size)) {
      assert(cur != NULL);
      prev = cur;
      cur = cur->next;
    }
    if (cur == NULL) {
      ret = NULL;
    }
    else {
      assert(cur != NULL);
      uintptr_t memory_start = align_address(cur->memory_start, size);
      uintptr_t remain_space = (uintptr_t)cur->memory_start + cur->memory_size - size - memory_start;
      if (remain_space >= 8 KB) {
        assert(remain_space % 8 KB == 0);
        memory_t* new_memory = (memory_t*)(memory_start + size);
        assert(new_memory != NULL);
        new_memory->memory_start = (void*)((uintptr_t)new_memory + MEMORY_CONFIG);
        new_memory->memory_size = remain_space - MEMORY_CONFIG;
        if (cur == heap_pool.next) {
          heap_pool.next = new_memory;
          new_memory->next = cur->next;
          cur->next = NULL;
        }
        else {
          assert(prev != NULL);
          assert(cur != NULL);
          new_memory->next = cur->next;
          prev->next = new_memory;
          cur->next = NULL;
        }
      }
      else {
        if (cur == heap_pool.next) {
          heap_pool.next = cur->next;
          cur->next = NULL;
        }
        else {
          assert(prev != NULL);
          prev->next = cur->next;
          cur->next = NULL;
        }
      }
      assert(cur != NULL);
      cur->memory_start = (void*)memory_start;
      cur->memory_size = size;
      uintptr_t* magic = (uintptr_t*)(memory_start - sizeof(intptr_t));
      uintptr_t* header = (uintptr_t*)(memory_start - 2 * sizeof(intptr_t));
      assert((uintptr_t)header > (uintptr_t)cur + MEMORY_CONFIG);
      *magic = MAGIC, * header = (uintptr_t)cur;
      ret = cur;
    }
  }
  assert((uintptr_t)ret % 4 KB == 0);
  kspin_unlock(&heap_lock);
  return ret;
}

// free memoru to heap
static void memory_to_heap(memory_t* memory) {
  kspin_lock(&heap_lock);
  assert(memory != NULL);
  assert(*(uintptr_t*)(memory->memory_start - sizeof(uintptr_t)) == MAGIC);
  assert(*(uintptr_t*)(memory->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)memory);
  memory->memory_size = (uintptr_t)memory->memory_start + memory->memory_size - (uintptr_t)memory - MEMORY_CONFIG;
  memory->memory_start = (void*)memory + MEMORY_CONFIG;
  memory->next = heap_pool.next;
  heap_pool.next = memory;
  kspin_unlock(&heap_lock);
}

// get one page from heap_pool to slab_pool
static memory_t* page_from_heap_pool() {
  void* ret = memory_from_heap(4 KB);
  if (ret == NULL)
    return NULL;
  assert(*(uintptr_t*)(((memory_t*)ret)->memory_start - sizeof(uintptr_t)) == MAGIC);
  assert(*(uintptr_t*)(((memory_t*)ret)->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)ret);
  assert(ret != NULL);
  return ret;
}

static void page_to_slab_pool(memory_t* page) {
  kspin_lock(&slab_lock);
  assert(page != NULL);
  assert((uintptr_t)page->memory_start + page->memory_size - (uintptr_t)page == 8 KB);
  assert(page->memory_size == 4 KB);
  assert(*(uintptr_t*)(page->memory_start - sizeof(uintptr_t)) == MAGIC);
  assert(*(uintptr_t*)(page->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)page);
  assert(page->next == NULL);
  page->next = slab_pool.next;
  slab_pool.next = page;
  kspin_unlock(&slab_lock);
}

// get one page from slab_pool
static memory_t* page_from_slab_pool() {
  memory_t* ret = NULL;
  kspin_lock(&slab_lock);
  if (slab_pool.next != NULL) {
    ret = slab_pool.next;
    assert(ret != NULL);
    slab_pool.next = ret->next;
    ret->next = NULL;
    assert(ret->memory_size == 4 KB);
    assert(*(uintptr_t*)(ret->memory_start - sizeof(uintptr_t)) == MAGIC);
    assert(*(uintptr_t*)(ret->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)ret);
    kspin_unlock(&slab_lock);
  }
  else {
    kspin_unlock(&slab_lock);
    ret = page_from_heap_pool();
  }
  return ret;
}

// attach one page to slab_list
static slab_t* fetch_page_to_slab(int slab_index, int cpu) {
  assert(cpu < MAX_CPU);
  slab_t* page = (slab_t*)page_from_slab_pool();
  if (page == NULL) return NULL;
  memset(page, 0, sizeof(slab_t));
  assert(page != NULL);
  page->next = kmem[cpu].slab_list[slab_index].next;
  kmem[cpu].slab_list[slab_index].next = page;
  page->object_size = slab_type[slab_index];
  page->cpu = cpu;
  page->object_capacity = 4 KB / page->object_size;
  page->object_start = (void*)page + 4 KB;
  assert(*(uintptr_t*)(page->object_start - sizeof(uintptr_t)) == 0);
  assert(*(uintptr_t*)(page->object_start - 2 * sizeof(uintptr_t)) == 0);
  assert(page->object_counter == 0);
  return page;
}

static void* kalloc_large(size_t size) {
  memory_t* ret = memory_from_heap(size);
  if (ret == NULL)
    return NULL;
  assert(ret != NULL);
  assert(*(uintptr_t*)(ret->memory_start - sizeof(uintptr_t)) == MAGIC);
  assert(*(uintptr_t*)(ret->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)ret);
  return ret->memory_start;
}

static void* kalloc_page() {
  memory_t* ret = page_from_slab_pool();
  if (ret == NULL)
    return NULL;
  assert(ret != NULL);
  assert(*(uintptr_t*)(ret->memory_start - sizeof(uintptr_t)) == MAGIC);
  assert(*(uintptr_t*)(ret->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)ret);
  assert((uintptr_t)ret->memory_start + ret->memory_size - (uintptr_t)ret == 8 KB);
  assert(ret->memory_size == 4 KB);
  return ret->memory_start;
}

static void* kalloc_slab(size_t size) {
  void* ret = NULL;
  int cpu = cpu_current(), slab_index = match_slab_type(size);
  assert(cpu < MAX_CPU);
#ifdef DEAD_LOCK
  Log("spin_lock CPU#%d", cpu);
#endif
  kspin_lock(&kmem[cpu].lk);
  slab_t* page = kmem[cpu].slab_list[slab_index].next;
  if (page != NULL) {
    ret = object_from_slab(page);
  }
  else {
    // Log("alloc slab");
    slab_t* new_page = fetch_page_to_slab(slab_index, cpu);
    if (new_page == NULL) ret = NULL;
    else {
      assert(new_page->object_size == slab_type[slab_index]);
      assert(new_page->object_counter == 0);
      ret = object_from_slab(new_page);
    }
  }
#ifdef DEAD_LOCK
  Log("spin_unlock CPU#%d", cpu);
#endif
  kspin_unlock(&kmem[cpu].lk);
  return ret;
}

static void kfree_large(memory_t* memory) {
  assert(memory != NULL);
  assert(*(uintptr_t*)(memory->memory_start - sizeof(uintptr_t)) == MAGIC);
  assert(*(uintptr_t*)(memory->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)memory);
  if (memory->memory_size == 4 KB) {
    return page_to_slab_pool(memory);
  }
  else {
    return memory_to_heap(memory);
  }
}

static void kfree_slab(slab_t* page, void* ptr) {
#ifdef DEAD_LOCK
  Log("spin_lock CPU#%d", page->cpu);
#endif
  assert(page != NULL);
  kspin_lock(&kmem[page->cpu].lk);
  if (page->object_counter == page->object_capacity) {
    int cpu = page->cpu, slab_index = match_slab_type(page->object_size);
    page->next = kmem[cpu].slab_list[slab_index].next;
    kmem[cpu].slab_list[slab_index].next = page;
    page->object_counter--;
  }
  int offset = (ptr - page->object_start) / page->object_size;
  int i = offset / 32, j = offset % 32;
  assert(getbit(page->bitset[i], j) == 1);
  clrbit(page->bitset[i], j);
#ifdef DEAD_LOCK
  Log("spin_unlock CPU#%d", page->cpu);
#endif
  kspin_unlock(&kmem[page->cpu].lk);
}

static void* kalloc(size_t size) {
  if (size > 16 MB)
    return NULL;
  void* ret = NULL;
  size = align_size(size);
  if (size > 4 KB) {
    ret = kalloc_large(size);
  }
  else if (size == 4 KB) {
    ret = kalloc_page();
  }
  else {
    ret = kalloc_slab(size);
  }
  // Log("success alloc with size=%dB at %07p", size, ret);
  return ret;
}

static inline int fetch_magic(void* ptr) {
  return *(int*)((uintptr_t)ptr - sizeof(intptr_t));
}

static inline void* fetch_header(void* ptr, int magic) {
  if (magic == MAGIC)
    return (void*)(*(uintptr_t*)((uintptr_t)ptr - 2 * sizeof(intptr_t)));
  else
    return (void*)((uintptr_t)ptr & SLAB_MASK);
}

static void kfree(void* ptr) {
  int magic = fetch_magic(ptr);
  if (magic == MAGIC) {
    memory_t* memory = fetch_header(ptr, magic);
    return kfree_large(memory);
  }
  else {
    slab_t* page = fetch_header(ptr, magic);
    return kfree_slab(page, ptr);
  }
}

static void slab_init() {
  for (int cpu = 0; cpu < cpu_count(); cpu++) {
    init_klock(&kmem[cpu].lk, "cpu");
    kspin_lock(&kmem[cpu].lk);
    for (int slab = 0; slab < SLAB_TYPE; slab++) {
      fetch_page_to_slab(slab, cpu);
    }
    kspin_unlock(&kmem[cpu].lk);
  }
}

static void pmm_init() {
  init_klock(&heap_lock, "heap_lock");
  init_klock(&slab_lock, "slab_lock");

  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

  memory_t* heap_start = (memory_t*)(heap.start);
  assert(heap_start != NULL);
  heap_start->next = NULL;
  heap_start->memory_start = (void*)((uintptr_t)heap_start + MEMORY_CONFIG);
  heap_start->memory_size = pmsize - MEMORY_CONFIG;
  heap_pool.next = heap_start;

  slab_pool.next = NULL;
  slab_init();
}

static void* kalloc_safe(size_t size) {
  bool i = ienabled();
  iset(false);
  void* ret = kalloc(size);
  if (i) iset(true);
  return ret;
}

static void kfree_safe(void* ptr) {
  int i = ienabled();
  iset(false);
  kfree(ptr);
  if (i) iset(true);
}


MODULE_DEF(pmm) = {
    .init = pmm_init,
    .alloc = kalloc_safe,
    .free = kfree_safe,
};

