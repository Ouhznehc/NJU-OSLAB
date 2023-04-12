#include <common.h>

static spinlock_t heap_lock;
static spinlock_t slab_lock;
static memory_t heap_pool;
static memory_t slab_pool;
static kmem_cache kmem[MAX_CPU];
int slab_type[SLAB_TYPE] = {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};

static inline int match_slab_type(size_t size)
{
  for (int i = 0; i < SLAB_TYPE; i++)
    if (size <= slab_type[i])
      return i;
  panic("match_slab_type: should not reach here");
  return 0;
}

static inline size_t align_size(size_t size)
{
  size_t msb = 31 - __builtin_clz(size);
  if (size == (1 << msb))
    return size;
  else
    return (1 << (msb + 1));
}

static inline uintptr_t align_address(void *address, size_t size)
{
  return ROUNDUP((uintptr_t)address, size);
}

static void *object_from_slab(slab_t *page)
{
  void *ret = NULL;
  for (int i = 0; i < 32; i++)
  {
    if (page->bitset[i] == (int)(-1))
      continue;
    for (int j = 0; j < 32; j++)
    {
      if (getbit(page->bitset[i], j) == 0)
      {
        if (page->object_counter == 0)
        {
          int slab_index = match_slab_type(page->object_size);
          kmem[page->cpu].free_slab[slab_index]--;
        }
        setbit(page->bitset[i], j);
        page->object_counter++;
        ret = page->object_start + (32 * i + j) * page->object_size;
        return ret;
      }
    }
  }
  panic("object_from_slab: should not reach here");
  return NULL;
}

// get memory from heap with size
static memory_t *memory_from_heap(size_t size)
{
  memory_t *ret = NULL;
  spin_lock(&heap_lock);
  if (heap_pool.next == NULL)
    ret = NULL;
  else
  {
    memory_t *cur = heap_pool.next, *prev = NULL;
    while (cur != NULL && ((uintptr_t)cur->memory_start + cur->memory_size < align_address(cur->memory_start, size) + size))
    {
      prev = cur;
      cur = cur->next;
    }
    if (cur == NULL)
      ret = NULL;
    else
    {
      uintptr_t memory_start = align_address(cur->memory_start, size);
      uintptr_t remain_space = (uintptr_t)cur->memory_start + cur->memory_size - size - memory_start;
      if (remain_space >= 8 KB)
      {
        memory_t *new_memory = (memory_t *)(memory_start + size);
        new_memory->memory_start = (void *)((uintptr_t)new_memory + MEMORY_CONFIG);
        new_memory->memory_size = remain_space - MEMORY_CONFIG;
        if (cur == heap_pool.next)
        {
          heap_pool.next = new_memory;
        }
        else
        {
          new_memory->next = cur->next;
          prev->next = new_memory;
        }
      }
      else
      {
        prev->next = cur->next;
      }
      cur->memory_start = (void *)memory_start;
      cur->memory_size = size;
      uintptr_t *magic = (uintptr_t *)(memory_start - sizeof(intptr_t));
      uintptr_t *header = (uintptr_t *)(memory_start - 2 * sizeof(intptr_t));
      *magic = MAGIC, *header = (uintptr_t)cur;
      ret = cur;
    }
  }
  spin_unlock(&heap_lock);
  assert(ret != NULL);
  return ret;
}

// free memoru to heap
static void memory_to_heap(memory_t *memory)
{
  spin_lock(&heap_lock);
  memory->memory_size = (uintptr_t)memory->memory_start + memory->memory_size - (uintptr_t)memory - MEMORY_CONFIG;
  memory->memory_start = (void *)(uintptr_t)memory + MEMORY_CONFIG;
  memory->next = heap_pool.next;
  heap_pool.next = memory;
  spin_unlock(&heap_lock);
}

// get one page from heap_pool to slab_pool
static memory_t *page_from_heap_pool()
{
  return memory_from_heap(4 KB);
}

static void page_to_slab_pool(memory_t *page)
{
  spin_lock(&slab_lock);
  page->next = slab_pool.next;
  slab_pool.next = page;
  spin_unlock(&slab_lock);
}

// get one page from slab_pool
static memory_t *page_from_slab_pool()
{
  memory_t *ret = NULL;
  spin_lock(&slab_lock);
  if (slab_pool.next != NULL)
  {
    ret = slab_pool.next;
    slab_pool.next = ret->next;
    spin_unlock(&slab_lock);
  }
  else
  {
    spin_unlock(&slab_lock);
    ret = page_from_heap_pool();
  }
  return ret;
}

// attach one page to slab_list
static slab_t *fetch_page_to_slab(int slab_index, int cpu)
{
  slab_t *page = (slab_t *)page_from_slab_pool();
  if (page == NULL)
    return NULL;
  Log("lock");
  spin_lock(&kmem[cpu].lk);
  memset(page, 0, sizeof(slab_t));
  page->next = kmem[cpu].slab_list[slab_index].next;
  kmem[cpu].slab_list[slab_index].next = page;
  kmem[cpu].available_page[slab_index] = page;
  Log("unlock");
  spin_unlock(&kmem[cpu].lk);
  page->object_size = slab_type[slab_index];
  page->cpu = cpu;
  page->object_capacity = (SLAB_SIZE - SLAB_CONFIG) / page->object_size;
  page->object_start = (page->object_size <= SLAB_CONFIG) ? (void *)page + SLAB_CONFIG : (void *)page + page->object_size;
  assert(page->object_counter == 0);
  for (int i = 0; i < 32; i++)
    assert(page->bitset[i] == 0);
  return page;
}

static void *kmalloc_large(size_t size)
{
  return memory_from_heap(size)->memory_start;
}

static void *kmalloc_page()
{
  return page_from_slab_pool()->memory_start;
}

static void *kmalloc_slab(size_t size)
{
  void *ret = NULL;
  int cpu = cpu_current(), slab_index = match_slab_type(size);
  Log("lock");
  spin_lock(&kmem[cpu].lk);
  slab_t *page = kmem[cpu].available_page[slab_index];
  if (page->object_counter < page->object_capacity)
    ret = object_from_slab(page);
  else
  {
    page = kmem[cpu].slab_list[slab_index].next;
    assert(page->object_counter <= page->object_capacity);
    while (page->object_counter == page->object_capacity && page->next != NULL)
    {
      page = page->next;
      assert(page->object_counter <= page->object_counter);
      assert(page->next != NULL);
    }
    if (page->next == NULL)
    {
      slab_t *new_page = fetch_page_to_slab(slab_index, cpu);
      if (new_page == NULL)
        ret = NULL;
      else
      {
        assert(new_page->object_size == slab_type[slab_index]);
        assert(new_page->object_counter == 0);
        ret = object_from_slab(new_page);
      }
    }
    else
    {
      assert(page->object_counter < page->object_capacity);
      kmem[cpu].available_page[slab_index] = page;
      ret = object_from_slab(page);
    }
  }
  Log("unlock");
  spin_unlock(&kmem[cpu].lk);
  return ret;
}

static void kfree_large(memory_t *memory)
{
  if (memory->memory_size == 4 KB)
    return page_to_slab_pool(memory);
  else
    return memory_to_heap(memory);
}

static void kfree_slab(slab_t *page, void *ptr)
{
  Log("lock");
  spin_lock(&kmem[page->cpu].lk);
  int offset = (ptr - page->object_start) / page->object_size;
  int i = offset / 32, j = offset % 32;
  assert(getbit(page->bitset[i], j) == 1);
  clrbit(page->bitset[i], j);
  if (page->object_counter == 0)
  {
    int slab_index = match_slab_type(page->object_size);
    kmem[page->cpu].free_slab[slab_index]++;
  }
  // Log("success free %07p", ptr);
  Log("unlock");
  spin_unlock(&kmem[page->cpu].lk);
}

static void *kalloc(size_t size)
{
  if (size > 16 MB)
    return NULL;
  void *ret = NULL;
  size = align_size(size);
  if (size > 4 KB)
  {
    ret = kmalloc_large(size);
  }
  else if (size == 4 KB)
  {
    ret = kmalloc_page();
  }
  else
  {
    ret = kmalloc_slab(size);
  }
  return ret;
}

static inline int fetch_magic(void *ptr)
{
  return *(int *)((uintptr_t)ptr - sizeof(intptr_t));
}

static inline void *fetch_header(void *ptr, int magic)
{
  if (magic == MAGIC)
    return (void *)(*(size_t *)((uintptr_t)ptr - 2 * sizeof(intptr_t)));
  else
    return (void *)((uintptr_t)ptr & SLAB_MASK);
}

static void kfree(void *ptr)
{
  int magic = fetch_magic(ptr);
  if (magic == MAGIC)
  {
    memory_t *memory = fetch_header(ptr, magic);
    return kfree_large(memory);
  }
  else
  {
    slab_t *page = fetch_header(ptr, magic);
    return kfree_slab(page, ptr);
  }
}

static void slab_init()
{
  for (int cpu = 0; cpu < cpu_count(); cpu++)
  {
    init_lock(&kmem[cpu].lk, "cpu");
    for (int slab = 0; slab < SLAB_TYPE; slab++)
    {
      fetch_page_to_slab(slab, cpu);
      kmem[cpu].free_slab[slab] = SLAB_MIN;
    }
  }
}

static void pmm_init()
{
  init_lock(&heap_lock, "heap_lock");
  init_lock(&slab_lock, "slab_lock");

  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

  Log("begin initial");

  memory_t *heap_start = (memory_t *)(heap.start);
  heap_start->next = NULL;
  heap_start->memory_start = (void *)((uintptr_t)heap_start + MEMORY_CONFIG);
  heap_start->memory_size = pmsize - MEMORY_CONFIG;

  heap_pool.next = heap_start;
  slab_pool.next = NULL;

  slab_init();
}

MODULE_DEF(pmm) = {
    .init = pmm_init,
    .alloc = kalloc,
    .free = kfree,
};
