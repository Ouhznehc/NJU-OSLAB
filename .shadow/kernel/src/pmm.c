#include <common.h>

static page_t *heap_start = NULL;
// static kmem_cache kmem[MAX_CPU];
// static spinlock_t heap_lock;
int slab_type[SLAB_TYPE] = {2, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};


static inline size_t align(size_t size){
  size_t msb = 32 - __builtin_clz(size);
  return (1 << msb);
}






static void *kalloc(size_t size) {
  size = align(size);
  Log("size = %d", size);
  return NULL;
}

static void kfree(void *ptr) {
}

static void pmm_init() {
  heap_start = heap.start;
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
