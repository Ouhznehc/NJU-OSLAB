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
//   if (size == (1 << msb))
//     return size;
//   else
//     return (1 << (msb + 1));
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
//   if (heap_pool.next == NULL)
//     ret = NULL;
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
//   if (page == NULL)
//     return NULL;
//   memset(page, 0, sizeof(slab_t));
//   assert(page != NULL);
//   page->next = kmem[cpu].slab_list[slab_index].next;
//   kmem[cpu].slab_list[slab_index].next = page;
//   page->object_size = slab_type[slab_index];
//   page->cpu = cpu;
//   page->object_capacity = 4 KB / page->object_size;
//   page->object_start = (void*)page + 4 KB;
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
//     if (new_page == NULL) {

//       ret = NULL;
//     }
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

#define CPUS 8
#define TYPE 11

#define KiB *1024
#define MiB *1024*1024
#define PageSize (8 KiB)

#define BIT(n) (1 << get_bit(n))
#define MASK(n) (BIT(n) - 1)
#define ALIGN(n, m) ((n) & ~MASK(m))

//spin_lock
#define SPIN_INIT() 0

void spin_lock(int* lk);
void spin_unlock(int* lk);

void spin_lock(int* lk) {
  while (1) {
    intptr_t value = atomic_xchg(lk, 1);
    if (value == 0) {
      break;
    }
  }
}
void spin_unlock(int* lk) {
  atomic_xchg(lk, 0);
}

int heap_lock = SPIN_INIT();
int page_lock = SPIN_INIT();
int cpu_lock[CPUS] = { SPIN_INIT() };

typedef union Page {
  struct {
    int cpu_id;
    void* ptr;
    union Page* next;
    int slab_type;
    int capacity;
    int num;
    int map[128];
  };
  uint8_t SIZE[PageSize];
}Page;

Page slab_cpu[CPUS][TYPE];

typedef struct heap_free {
  uintptr_t size;
  void* ptr;
  struct heap_free* next;
}heap_free;

heap_free initial_heap, all_head, all_tail;
heap_free* head_heap = &all_head, * tail_heap = &all_tail;

int get_bit(uintptr_t n) {
  if (n == 1) return 1;
  int cnt = 0; n -= 1;
  while (n) {
    n >>= 1;
    cnt++;
  }
  return cnt;
}

void push_head_heap(heap_free* node) {
  node->next = head_heap;
  head_heap = node;
}

void push_tail_heap(heap_free* node) {
  node->next = NULL;
  tail_heap->next = node;
  tail_heap = node;
}

void push_heap(heap_free* node, size_t size) {
  if (size > 8 KiB) return push_head_heap(node);
  else return push_tail_heap(node);
}


int check_heap(heap_free* node, size_t size, uintptr_t* start, uintptr_t* end) {
  uintptr_t node_end = (uintptr_t)node->ptr + node->size;
  uintptr_t final_align = ALIGN(node_end, size);
  if (final_align + size > node_end) final_align -= BIT(size);
  if (final_align < (intptr_t)node->ptr || final_align + size > node_end) return 0;
  *start = final_align;
  *end = final_align + size;
  *end = ROUNDUP(*end, 4 KiB);
  if (final_align == (intptr_t)node->ptr) return 1;
  else return 2;
}

void* kalloc_large(size_t size) {
  spin_lock(&heap_lock);
  void* ret = NULL;
  heap_free* last = head_heap;
  // for (heap_free* cur = head_heap; cur <= tail_heap; cur++) {
  for (heap_free* cur = head_heap->next; cur; cur = cur->next) {
    uintptr_t new_start, cur_start = (uintptr_t)cur->ptr;
    uintptr_t new_end, cur_end = (uintptr_t)cur->ptr + cur->size;
    int type = check_heap(cur, size, &new_start, &new_end);
    //printf("\ncur st  %p    ed  %p\nnew st  %p    ed  %p\nhead st  %p    ed  %p\n\n", cur_start,cur_end,new_start,new_end,(uintptr_t)head_heap->ptr,(uintptr_t)head_heap->ptr+(uintptr_t)head_heap->size);
    if (type == 0) { continue; last = cur; }
    if (type == 1) {
      if (cur_end - new_end >= 8 KiB) {
        heap_free* new_heap = (heap_free*)new_end;
        new_heap->next = cur->next;
        new_heap->ptr = (void*)((uintptr_t)new_heap + 4 KiB);
        new_heap->size = cur_end - new_end - 4 KiB;
        if (last != NULL) last->next = new_heap;
        else head_heap = new_heap;
        // if(cur == tail_heap) tail_heap = new_heap;
      }
      else {
        // if (cur == head_heap && cur == tail_heap) {
        // if (cur == head_heap && cur->next == NULL) {
        //   ret = NULL;
        //   break;
        // }
        last->next = cur->next;
        // if (last != NULL) last->next = cur->next;
        // else head_heap = cur->next;
        // if(tail_heap == cur && cur->next != NULL) tail_heap = cur->next; 
      }
      ret = cur->ptr;
      // cur->next = NULL;
      cur->size = new_end - cur_start;
      if (cur_end - new_end < 8 KiB) cur->size += cur_end - new_end;
      break;
    }
    if (type == 2) {
      if (new_start - cur_start == 4 KiB) {
        heap_free* new_heap = (heap_free*)(new_start - 4 KiB);
        new_heap->next = NULL;
        new_heap->ptr = (void*)new_start;
        new_heap->size = new_end - new_start;
        // if (cur_end - new_end < 8 KiB) {
          //new_heap->size += cur_end - new_end;
          // if (cur == head_heap && cur == tail_heap) {
          // if (cur == head_heap && cur->next == NULL) {
          //   ret = NULL;
          //   break;
          // }
        last->next = cur->next;
        // if (last != NULL) last->next = cur->next;
        // else head_heap = cur->next;

        // if(cur == tail_heap && cur->next != NULL) tail_heap = cur->next;
      // }
      // else {
      //   heap_free* new_heap_t = (heap_free*)new_end;
      //   new_heap_t->next = cur->next;
      //   new_heap_t->ptr = (void*)(new_heap + 4 KiB);
      //   new_heap_t->size = cur_end - new_end - 4 KiB;
      //   if (last != NULL) last->next = new_heap_t;
      //   else head_heap = new_heap_t;
      // }
      }
      else {
        cur->size = new_start - cur_start - 4 KiB;
        // if (cur_end - new_end >= 8 KiB) {
        //   heap_free* new_heap = (heap_free*)new_end;
        //   new_heap->next = cur->next;
        //   new_heap->ptr = (void*)(new_heap + 4 KiB);
        //   new_heap->size = cur_end - new_end - 4 KiB;
        //   cur->next = new_heap;
        //   if(cur == tail_heap) tail_heap = new_heap;
        // }
        // else {
         // new_end = cur_end;
        // }
      }
      ret = (void*)new_start;
      heap_free* message = (heap_free*)(new_start - 4 KiB);
      message->next = NULL;
      message->ptr = ret;
      message->size = new_end - new_start;
      break;
    }
  }
  //printf("------       %p %p\n", (uintptr_t)ret, size + (uintptr_t)ret);
  if (ret != NULL) {
    uint8_t* type = (uint8_t*)(ret - 1); *type = 12;
  }
  spin_unlock(&heap_lock);
  return ret;
}

void* kalloc_page(size_t size) {
  spin_lock(&page_lock);
  if (tail_heap->next == NULL) {
    spin_unlock(&page_lock);
    return kalloc_large(size);
  }
  heap_free* newpage = tail_heap->next;
  tail_heap->next = newpage->next;
  newpage->next = NULL;
  spin_unlock(&page_lock);
  return newpage->ptr;
}

int bit_map(int size, int map[128]) {
  int x = size / 32, y = size % 32;
  for (int i = 0; i < x; i++) {
    for (int j = 0; j < 32; j++) {
      if ((map[i] & (1 << j)) == 0) return 32 * i + j;
    }
  }
  for (int j = 0; j < y; j++) {
    if ((map[x] & (1 << j)) == 0) return 32 * x + j;
  }
  return 0;
}

void set_bit(int cnt, int map[128]) {
  int x = cnt / 32, y = cnt % 32;
  map[x] ^= (1 << y);
}

void* kalloc_little(size_t size) {
  int cpu = cpu_current(), type;
  void* ret = NULL;
  Page* cur = NULL;
  //printf("cpu  %d    size  %p\n", cpu, size);
  for (int i = 0; i < TYPE; i++) {
    if (get_bit(size) == i + 1) {
      cur = &slab_cpu[cpu][i];
      type = (1 << (i + 1));
      //printf("size=%d, type=%d\n", size, type);
      break;
    }
  }
  spin_lock(&cpu_lock[cpu]);
  //assert(cur != NULL);
  if (cur->next != NULL) {
    Page* last = cur;
    for (Page* tmp = cur->next; tmp; tmp = tmp->next) {
      if (tmp->num < tmp->capacity) {
        int cnt = bit_map(tmp->capacity, tmp->map);
        set_bit(cnt, tmp->map);
        tmp->num++;
        if (tmp->num == tmp->capacity) {
          last->next = tmp->next;
          tmp->next = NULL;
        }
        ret = tmp->ptr + cnt * type;
        break;
      }
    }
    if (ret == NULL) {
      void* slab = kalloc_page(4 KiB);
      if (slab != NULL) {
        memset(slab - 4 KiB, 0, 8 KiB);
        uint8_t* magic = (uint8_t*)(slab - 1); *magic = 21;
        Page* new_page = (Page*)(slab - 4 KiB);
        new_page->capacity = 4 KiB / type;
        new_page->cpu_id = cpu;
        new_page->num = 1;
        new_page->ptr = slab;
        new_page->slab_type = type;
        new_page->next = cur->next;
        int cnt = bit_map(new_page->capacity, new_page->map);
        set_bit(cnt, new_page->map);
        ret = slab + cnt * type;
        cur->next = new_page;
      }
      else {
        ret = NULL;
      }
    }
  }
  else {
    void* slab = kalloc_page(4 KiB);
    if (slab != NULL) {
      memset(slab - 4 KiB, 0, 8 KiB);
      uint8_t* magic = (uint8_t*)(slab - 1); *magic = 21;
      Page* new_page = (Page*)(slab - 4 KiB);
      cur->next = new_page;
      new_page->capacity = 4 KiB / type;
      new_page->cpu_id = cpu;
      new_page->num = 1;
      new_page->ptr = slab;
      new_page->slab_type = type;
      new_page->next = NULL;
      int cnt = bit_map(new_page->capacity, new_page->map);
      set_bit(cnt, new_page->map);
      ret = slab + cnt * type;
    }
    else ret = NULL;

  }
  spin_unlock(&cpu_lock[cpu]);
  return ret;
}

static void* kalloc(size_t size) {
  if (size > 16 MiB || size <= 0 MiB) return NULL;
  if (size > 2 KiB && size <= 4 KiB) return kalloc_page(size);
  if (size > 4 KiB) return kalloc_large(size);
  else return kalloc_little(size);
}

static void kfree(void* ptr) {
  //return;
  uintptr_t align = ALIGN((uintptr_t)ptr, 4 KiB);
  uint8_t* type = (uint8_t*)(align - 1);
  //printf("typ=%d\n", *type);
  if (*type == 12) {
    heap_free* to_free = (heap_free*)(align - 4 KiB);
    if (to_free->size == 4 KiB) {
      spin_lock(&page_lock);
      to_free->next = tail_heap->next;
      tail_heap->next = to_free;
      spin_unlock(&page_lock);
    }
    else {
      spin_lock(&heap_lock);
      to_free->next = head_heap->next;
      head_heap->next = to_free;
      spin_unlock(&heap_lock);
    }
    // tail_heap->next = to_free;
    // to_free->next = NULL;
    // tail_heap = to_free;
  }
  if (*type == 21) {
    Page* to_free = (Page*)(align - 4 KiB);
    int cpu = to_free->cpu_id;
    spin_lock(&cpu_lock[cpu]);
    int typ = to_free->slab_type;
    int offset = (uintptr_t)ptr - align;
    //assert(offset % typ == 0);
    int cnt = offset / typ;
    to_free->num--;
    if (to_free->num + 1 == to_free->capacity) {
      to_free->next = slab_cpu[to_free->cpu_id][get_bit(to_free->slab_type) - 1].next;
      slab_cpu[to_free->cpu_id][get_bit(to_free->slab_type) - 1].next = to_free;
    }
    set_bit(cnt, to_free->map);
    spin_unlock(&cpu_lock[cpu]);
  }
}

#ifndef TEST
static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  initial_heap.ptr = (void*)heap.start;
  initial_heap.next = NULL;
  initial_heap.size = pmsize - 4 KiB;
  // head_heap = &initial_heap; tail_heap = &initial_heap;
  head_heap->next = &initial_heap;
  tail_heap->next = NULL;
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
  for (int i = 0; i < CPUS; i++) {
    for (int j = 0; j < TYPE; j++) {
      slab_cpu[i][j].next = NULL;
      slab_cpu[i][j].cpu_id = i;
    }
  }
}
#else
static void pmm_init() {
  char* ptr = malloc(HEAP_SIZE);
  heap.start = ptr;
  heap.end = ptr + HEAP_SIZE;
  printf("Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);
}
#endif

MODULE_DEF(pmm) = {
  .init = pmm_init,
  .alloc = kalloc,
  .free = kfree,
};