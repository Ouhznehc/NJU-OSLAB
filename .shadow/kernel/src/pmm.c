// #include <common.h>

// // #define DEAD_LOCK
// static kspinlock_t heap_lock;
// static kspinlock_t slab_lock;
// static memory_t heap_pool;
// static memory_t slab_pool;
// static kmem_cache kmem[MAX_CPU];
// int slab_type[SLAB_TYPE] = { 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048 };

// static inline int match_slab_type(size_t size) {
//   for (int i = 0; i < SLAB_TYPE; i++)
//     if (size <= slab_type[i])
//       return i;
//   panic("match_slab_type: should not reach here");
//   return 0;
// }

// static inline size_t align_size(size_t size) {
//   size_t msb = 31 - __builtin_clz(size);
//   if (size == (1 << msb)) return size;
//   else return (1 << (msb + 1));
// }

// static inline uintptr_t align_address(void* address, size_t size) {
//   return ROUNDUP((uintptr_t)address, size);
// }

// static void* object_from_slab(slab_t* page) {
//   void* ret = NULL;
//   assert(page != NULL);
//   assert(page->object_counter < page->object_capacity);
//   for (int i = 0; i < 64; i++) {
//     if (page->bitset[i] == (int)(-1))
//       continue;
//     for (int j = 0; j < 32; j++) {
//       if (getbit(page->bitset[i], j) == 0) {
//         assert(32 * i + j <= page->object_capacity);
//         setbit(page->bitset[i], j);
//         page->object_counter++;
//         if (page->object_counter == page->object_capacity) {
//           int cpu = page->cpu, slab_index = match_slab_type(page->object_size);
//           assert(kmem[cpu].slab_list[slab_index].next = page);
//           kmem[cpu].slab_list[slab_index].next = page->next;
//           page->next = NULL;
//         }
//         ret = page->object_start + (32 * i + j) * page->object_size;
//         return ret;
//       }
//     }
//   }
//   panic("object_from_slab: should not reach here");
//   return NULL;
// }

// // get memory from heap with size
// static memory_t* memory_from_heap(size_t size) {
//   memory_t* ret = NULL;
//   kspin_lock(&heap_lock);
//   if (heap_pool.next == NULL) ret = NULL;
//   else {
//     memory_t* cur = heap_pool.next, * prev = NULL;
//     assert(cur != NULL);
//     while (cur != NULL && ((uintptr_t)cur->memory_start + cur->memory_size < align_address(cur->memory_start, size) + size)) {
//       assert(cur != NULL);
//       prev = cur;
//       cur = cur->next;
//     }
//     if (cur == NULL) {
//       ret = NULL;
//     }
//     else {
//       assert(cur != NULL);
//       uintptr_t memory_start = align_address(cur->memory_start, size);
//       uintptr_t remain_space = (uintptr_t)cur->memory_start + cur->memory_size - size - memory_start;
//       if (remain_space >= 8 KB) {
//         assert(remain_space % 8 KB == 0);
//         memory_t* new_memory = (memory_t*)(memory_start + size);
//         assert(new_memory != NULL);
//         new_memory->memory_start = (void*)((uintptr_t)new_memory + MEMORY_CONFIG);
//         new_memory->memory_size = remain_space - MEMORY_CONFIG;
//         if (cur == heap_pool.next) {
//           heap_pool.next = new_memory;
//           new_memory->next = cur->next;
//           cur->next = NULL;
//         }
//         else {
//           assert(prev != NULL);
//           assert(cur != NULL);
//           new_memory->next = cur->next;
//           prev->next = new_memory;
//           cur->next = NULL;
//         }
//       }
//       else {
//         if (cur == heap_pool.next) {
//           heap_pool.next = cur->next;
//           cur->next = NULL;
//         }
//         else {
//           assert(prev != NULL);
//           prev->next = cur->next;
//           cur->next = NULL;
//         }
//       }
//       assert(cur != NULL);
//       cur->memory_start = (void*)memory_start;
//       cur->memory_size = size;
//       uintptr_t* magic = (uintptr_t*)(memory_start - sizeof(intptr_t));
//       uintptr_t* header = (uintptr_t*)(memory_start - 2 * sizeof(intptr_t));
//       assert((uintptr_t)header > (uintptr_t)cur + MEMORY_CONFIG);
//       *magic = MAGIC, * header = (uintptr_t)cur;
//       ret = cur;
//     }
//   }
//   assert((uintptr_t)ret % 4 KB == 0);
//   kspin_unlock(&heap_lock);
//   return ret;
// }

// // free memoru to heap
// static void memory_to_heap(memory_t* memory) {
//   kspin_lock(&heap_lock);
//   assert(memory != NULL);
//   assert(*(uintptr_t*)(memory->memory_start - sizeof(uintptr_t)) == MAGIC);
//   assert(*(uintptr_t*)(memory->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)memory);
//   memory->memory_size = (uintptr_t)memory->memory_start + memory->memory_size - (uintptr_t)memory - MEMORY_CONFIG;
//   memory->memory_start = (void*)memory + MEMORY_CONFIG;
//   memory->next = heap_pool.next;
//   heap_pool.next = memory;
//   kspin_unlock(&heap_lock);
// }

// // get one page from heap_pool to slab_pool
// static memory_t* page_from_heap_pool() {
//   void* ret = memory_from_heap(4 KB);
//   if (ret == NULL)
//     return NULL;
//   assert(*(uintptr_t*)(((memory_t*)ret)->memory_start - sizeof(uintptr_t)) == MAGIC);
//   assert(*(uintptr_t*)(((memory_t*)ret)->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)ret);
//   assert(ret != NULL);
//   return ret;
// }

// static void page_to_slab_pool(memory_t* page) {
//   kspin_lock(&slab_lock);
//   assert(page != NULL);
//   assert((uintptr_t)page->memory_start + page->memory_size - (uintptr_t)page == 8 KB);
//   assert(page->memory_size == 4 KB);
//   assert(*(uintptr_t*)(page->memory_start - sizeof(uintptr_t)) == MAGIC);
//   assert(*(uintptr_t*)(page->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)page);
//   assert(page->next == NULL);
//   page->next = slab_pool.next;
//   slab_pool.next = page;
//   kspin_unlock(&slab_lock);
// }

// // get one page from slab_pool
// static memory_t* page_from_slab_pool() {
//   memory_t* ret = NULL;
//   kspin_lock(&slab_lock);
//   if (slab_pool.next != NULL) {
//     ret = slab_pool.next;
//     assert(ret != NULL);
//     slab_pool.next = ret->next;
//     ret->next = NULL;
//     assert(ret->memory_size == 4 KB);
//     assert(*(uintptr_t*)(ret->memory_start - sizeof(uintptr_t)) == MAGIC);
//     assert(*(uintptr_t*)(ret->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)ret);
//     kspin_unlock(&slab_lock);
//   }
//   else {
//     kspin_unlock(&slab_lock);
//     ret = page_from_heap_pool();
//   }
//   return ret;
// }

// // attach one page to slab_list
// static slab_t* fetch_page_to_slab(int slab_index, int cpu) {
//   assert(cpu < MAX_CPU);
//   slab_t* page = (slab_t*)page_from_slab_pool();
//   if (page == NULL) return NULL;
//   memset(page, 0, sizeof(slab_t));
//   assert(page != NULL);
//   page->next = kmem[cpu].slab_list[slab_index].next;
//   kmem[cpu].slab_list[slab_index].next = page;
//   page->object_size = slab_type[slab_index];
//   page->cpu = cpu;
//   page->object_capacity = 4 KB / page->object_size;
//   page->object_start = (void*)page + 4 KB;
//   assert(*(uintptr_t*)(page->object_start - sizeof(uintptr_t)) == 0);
//   assert(*(uintptr_t*)(page->object_start - 2 * sizeof(uintptr_t)) == 0);
//   assert(page->object_counter == 0);
//   return page;
// }

// static void* kalloc_large(size_t size) {
//   memory_t* ret = memory_from_heap(size);
//   if (ret == NULL)
//     return NULL;
//   assert(ret != NULL);
//   assert(*(uintptr_t*)(ret->memory_start - sizeof(uintptr_t)) == MAGIC);
//   assert(*(uintptr_t*)(ret->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)ret);
//   return ret->memory_start;
// }

// static void* kalloc_page() {
//   memory_t* ret = page_from_slab_pool();
//   if (ret == NULL)
//     return NULL;
//   assert(ret != NULL);
//   assert(*(uintptr_t*)(ret->memory_start - sizeof(uintptr_t)) == MAGIC);
//   assert(*(uintptr_t*)(ret->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)ret);
//   assert((uintptr_t)ret->memory_start + ret->memory_size - (uintptr_t)ret == 8 KB);
//   assert(ret->memory_size == 4 KB);
//   return ret->memory_start;
// }

// static void* kalloc_slab(size_t size) {
//   void* ret = NULL;
//   int cpu = cpu_current(), slab_index = match_slab_type(size);
//   assert(cpu < MAX_CPU);
// #ifdef DEAD_LOCK
//   Log("spin_lock CPU#%d", cpu);
// #endif
//   kspin_lock(&kmem[cpu].lk);
//   slab_t* page = kmem[cpu].slab_list[slab_index].next;
//   if (page != NULL) {
//     ret = object_from_slab(page);
//   }
//   else {
//     // Log("alloc slab");
//     slab_t* new_page = fetch_page_to_slab(slab_index, cpu);
//     if (new_page == NULL) ret = NULL;
//     else {
//       assert(new_page->object_size == slab_type[slab_index]);
//       assert(new_page->object_counter == 0);
//       ret = object_from_slab(new_page);
//     }
//   }
// #ifdef DEAD_LOCK
//   Log("spin_unlock CPU#%d", cpu);
// #endif
//   kspin_unlock(&kmem[cpu].lk);
//   return ret;
// }

// static void kfree_large(memory_t* memory) {
//   assert(memory != NULL);
//   assert(*(uintptr_t*)(memory->memory_start - sizeof(uintptr_t)) == MAGIC);
//   assert(*(uintptr_t*)(memory->memory_start - 2 * sizeof(uintptr_t)) == (uintptr_t)memory);
//   if (memory->memory_size == 4 KB) {
//     return page_to_slab_pool(memory);
//   }
//   else {
//     return memory_to_heap(memory);
//   }
// }

// static void kfree_slab(slab_t* page, void* ptr) {
// #ifdef DEAD_LOCK
//   Log("spin_lock CPU#%d", page->cpu);
// #endif
//   assert(page != NULL);
//   kspin_lock(&kmem[page->cpu].lk);
//   if (page->object_counter == page->object_capacity) {
//     int cpu = page->cpu, slab_index = match_slab_type(page->object_size);
//     page->next = kmem[cpu].slab_list[slab_index].next;
//     kmem[cpu].slab_list[slab_index].next = page;
//     page->object_counter--;
//   }
//   int offset = (ptr - page->object_start) / page->object_size;
//   int i = offset / 32, j = offset % 32;
//   assert(getbit(page->bitset[i], j) == 1);
//   clrbit(page->bitset[i], j);
// #ifdef DEAD_LOCK
//   Log("spin_unlock CPU#%d", page->cpu);
// #endif
//   kspin_unlock(&kmem[page->cpu].lk);
// }

// static void* kalloc(size_t size) {
//   if (size > 16 MB)
//     return NULL;
//   void* ret = NULL;
//   size = align_size(size);
//   Log("alloc size = %d", size);
//   if (size > 4 KB) {
//     ret = kalloc_large(size);
//   }
//   else if (size == 4 KB) {
//     ret = kalloc_page();
//   }
//   else {
//     ret = kalloc_slab(size);
//   }
//   // Log("success alloc with size=%dB at %07p", size, ret);
//   return ret;
// }

// static inline int fetch_magic(void* ptr) {
//   return *(int*)((uintptr_t)ptr - sizeof(intptr_t));
// }

// static inline void* fetch_header(void* ptr, int magic) {
//   if (magic == MAGIC)
//     return (void*)(*(uintptr_t*)((uintptr_t)ptr - 2 * sizeof(intptr_t)));
//   else
//     return (void*)((uintptr_t)ptr & SLAB_MASK);
// }

// static void kfree(void* ptr) {
//   int magic = fetch_magic(ptr);
//   if (magic == MAGIC) {
//     memory_t* memory = fetch_header(ptr, magic);
//     return kfree_large(memory);
//   }
//   else {
//     slab_t* page = fetch_header(ptr, magic);
//     return kfree_slab(page, ptr);
//   }
// }

// static void slab_init() {
//   for (int cpu = 0; cpu < cpu_count(); cpu++) {
//     init_klock(&kmem[cpu].lk, "cpu");
//     kspin_lock(&kmem[cpu].lk);
//     for (int slab = 0; slab < SLAB_TYPE; slab++) {
//       fetch_page_to_slab(slab, cpu);
//     }
//     kspin_unlock(&kmem[cpu].lk);
//   }
// }

// static void pmm_init() {
//   init_klock(&heap_lock, "heap_lock");
//   init_klock(&slab_lock, "slab_lock");

//   uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
//   printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

//   memory_t* heap_start = (memory_t*)(heap.start);
//   assert(heap_start != NULL);
//   heap_start->next = NULL;
//   heap_start->memory_start = (void*)((uintptr_t)heap_start + MEMORY_CONFIG);
//   heap_start->memory_size = pmsize - MEMORY_CONFIG;
//   heap_pool.next = heap_start;

//   slab_pool.next = NULL;
//   slab_init();
// }

// static void* kalloc_safe(size_t size) {
//   bool i = ienabled();
//   iset(false);
//   void* ret = kalloc(size);
//   if (i) iset(true);
//   return ret;
// }

// static void kfree_safe(void* ptr) {
//   int i = ienabled();
//   iset(false);
//   kfree(ptr);
//   if (i) iset(true);
// }


// MODULE_DEF(pmm) = {
//     .init = pmm_init,
//     .alloc = kalloc_safe,
//     .free = kfree_safe,
// };


#include <common.h>

typedef int pmm_spinlock_t;
#define SPIN_INIT() 0
static void pmm_spin_lock(pmm_spinlock_t* lk) {
  while (1) {
    int value = atomic_xchg(lk, 1);
    if (value == 0) {
      break;
    }
  }
}
static void pmm_spin_unlock(pmm_spinlock_t* lk) {
  atomic_xchg(lk, 0);
}

// #define DEBUG  1

struct SLAB {
  int magic;
  int order;
  int used[64];
  int num, num_max;
  int master;
  uintptr_t addr;
  struct SLAB* next;
};

struct per_CPU {
  struct SLAB* fast_next[12];
};

struct per_CPU per_CPU_list[8];
pmm_spinlock_t cpu_lk[8];

struct NODE {
  uintptr_t l;
  size_t len;
  struct NODE* next, * pre;
} unused_list, page_list;

pmm_spinlock_t unused_list_lk, page_list_lk;

static void node_init(struct NODE* now) {
  now->l = 0;
  now->len = 0;
  now->next = NULL;
  now->pre = NULL;
}

static void slab_init(struct SLAB* now, int order) {
  now->magic = 114514;
  now->order = order;
  now->num = 0;
  now->num_max = 4096 / (1 << order);
  for (int i = 0; i < 64; i++)
    now->used[i] = 0;
  now->master = 0;
  now->addr = (uintptr_t)now + sizeof(struct SLAB);
  now->next = 0;
}

static int size2num(size_t size) {
  int ret = 1, tmp = 1;
  for (int i = 1; i <= 11; i++) {
    tmp = tmp * 2;
    if (tmp == size)
      ret = i;
  }
  return ret;
}

static void* kalloc(size_t size) {
  if (size > (16 << 20)) {
    return NULL;
  }
  size_t power = 2;
  while (power < size)
    power *= 2;
  size = power;
  if (size < 4096) {
    int num = size2num(size);
    uintptr_t ret;
    int master = cpu_current();
    pmm_spin_lock(&cpu_lk[master]);
    struct SLAB* now = per_CPU_list[master].fast_next[num];
    if (now != NULL) {
      assert(now->magic == 114514);
      int pos = 0;
      while ((now->used[pos] == -1) && pos < 64) {
        pos++;
      }
      if (pos != 64) {
        int rank = 0;
        for (int i = 0; i < 32; i++)
          if (((1 << i) & now->used[pos]) == 0) {
            now->used[pos] |= (1 << i);
            rank = i + pos * 32;
            break;
          }
        now->num++;
        if (now->num == now->num_max)
          per_CPU_list[master].fast_next[num] = now->next;
        ret = now->addr + (1 << num) * rank;
        pmm_spin_unlock(&cpu_lk[master]);
        return (void*)ret;
      }
    }
    pmm_spin_unlock(&cpu_lk[master]);

    uintptr_t new_slab = (uintptr_t)(pmm->alloc(1 << 12));

    if (new_slab == 0) {
      return NULL;
    }
    struct SLAB* tmp = (struct SLAB*)(new_slab - sizeof(struct SLAB));
    slab_init(tmp, num);
    tmp->master = master;
    tmp->used[0] |= 1;
    tmp->num = 1;
    pmm_spin_lock(&cpu_lk[master]);
    per_CPU_list[master].fast_next[num] = tmp;
    pmm_spin_unlock(&cpu_lk[master]);
    ret = tmp->addr;
    return (void*)ret;
  }
  if (size == (1 << 12)) {
    uintptr_t ans;
    pmm_spin_lock(&page_list_lk);
    if (page_list.next != NULL) {
      struct NODE* now = page_list.next;
      ans = now->l;
      page_list.next = now->next;
      pmm_spin_unlock(&page_list_lk);
      return (void*)ans;
    }
    pmm_spin_unlock(&page_list_lk);
  }
  pmm_spin_lock(&unused_list_lk);

  struct NODE* spare = &unused_list, * pre = NULL;
  while (spare && (spare->l + spare->len < ROUNDUP(spare->l, size) + size)) {
    pre = spare;
    spare = spare->next;
  }

  if (!spare) {
    pmm_spin_unlock(&unused_list_lk);
    return NULL;
  }
  uintptr_t ans = ROUNDUP(spare->l, size);

  if (spare->l + spare->len - ans - size >= (1 << 13)) {
    struct NODE* tmp = (struct NODE*)(ans + size);
    node_init(tmp);
    tmp->len = spare->l + spare->len - (uintptr_t)tmp - sizeof(struct NODE);
    tmp->l = (uintptr_t)tmp + sizeof(struct NODE);
    tmp->next = spare->next;
    pre->next = tmp;
  }
  else {
    pre->next = spare->next;
  }

  spare->l = ans;
  spare->len = size;
  uintptr_t* pos = (uintptr_t*)(spare->l - sizeof(uintptr_t*) - sizeof(struct SLAB));
  *pos = (uintptr_t)spare;

  pmm_spin_unlock(&unused_list_lk);

  return (void*)ans;
}

static void kfree(void* ptr) {
  void* extra = ptr;
  if (((uintptr_t)ptr & (~0xfff)) - sizeof(struct SLAB) >= (uintptr_t)heap.start) {
    struct SLAB* tmp = (struct SLAB*)(((uintptr_t)ptr & (~0xfff)) - sizeof(struct SLAB));
    if (tmp->magic == 114514) {
      extra = NULL;
      int master = tmp->master;
      pmm_spin_lock(&cpu_lk[master]);
      int num = tmp->order;
      int rank = ((uintptr_t)ptr - (uintptr_t)tmp->addr) / (1 << num);
      int pos = rank / 32;
      rank %= 32;
      if ((tmp->used[pos] & (1 << rank)) == 0)
        assert(0);
      tmp->used[pos] -= (1 << rank);
      if (tmp->num == tmp->num_max) {
        tmp->next = per_CPU_list[master].fast_next[num];
        per_CPU_list[master].fast_next[num] = tmp;
      }
      tmp->num--;
      pmm_spin_unlock(&cpu_lk[master]);
    }
  }
  if (extra == NULL)
    return;
  ptr = extra;

  uintptr_t* pos = (uintptr_t*)(ptr - sizeof(uintptr_t*) - sizeof(struct SLAB));
  struct NODE* using = (struct NODE*)(*pos);

  if (!using) {
    assert(0);
    return;
  }
  if (using->len == (1 << 12)) {
    pmm_spin_lock(&page_list_lk);
    using->next = page_list.next;
    page_list.next = using;
    pmm_spin_unlock(&page_list_lk);
  }
  else {
    pmm_spin_lock(&unused_list_lk);
    using->len = using->l + using->len - (uintptr_t) using - sizeof(struct NODE);
    using->l = (uintptr_t) using + sizeof(struct NODE);
    using->next = unused_list.next;
    unused_list.next = using;
    pmm_spin_unlock(&unused_list_lk);
  }
}

static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

  struct NODE* tmp = (struct NODE*)ROUNDUP(heap.start, 1 << 12);
  node_init(tmp);
  tmp->l = (uintptr_t)tmp + sizeof(struct NODE);
  tmp->len = pmsize - sizeof(struct NODE);

  node_init(&unused_list);
  unused_list.next = tmp;

  node_init(&page_list);
  unused_list_lk = SPIN_INIT();
  page_list_lk = SPIN_INIT();
  int cpu_num = cpu_count();
  for (int i = 0; i < cpu_num; i++) {
    cpu_lk[i] = SPIN_INIT();
    for (int j = 1; j <= 11; j++) {
      struct SLAB* tmp = (struct SLAB*)(pmm->alloc(1 << 12) - sizeof(struct SLAB));
      slab_init(tmp, j);
      per_CPU_list[i].fast_next[j] = tmp;
      tmp->master = i;
    }
  }
}

static void* kalloc_safe(size_t size) {
  bool i = ienabled();
  iset(false);
  void* ret = kalloc(size);
  if (i)
    iset(true);
  return ret;
}

static void kfree_safe(void* ptr) {
  int i = ienabled();
  iset(false);
  kfree(ptr);
  if (i)
    iset(true);
}

MODULE_DEF(pmm) = {
    .init = pmm_init,
    .alloc = kalloc_safe,
    .free = kfree_safe,
};
