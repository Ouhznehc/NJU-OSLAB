#include <common.h>

static page_t *heap_start = NULL;
//static kmem_cache kmem[MAX_CPU];
static spinlock_t heap_lock;
int slab_type[SLAB_TYPE] = {2, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

static inline size_t size2page(size_t size){
  return size / PAGE_SIZE + 1;
}

static inline size_t align(size_t size){
  size_t msb = 31 - __builtin_clz(size);
  if(size == (1 << msb)) return size;
  else return (1 << (msb + 1));
}

static bool heap_valid(page_t *page, size_t size){
  size_t pages = size2page(size);
  for(int i = 0; i < pages; i++)
    if((page + i)->object_size) return false;
  return true;
}

static page_t* increase_by_page(page_t *page){
  Assert(page->object_size, "increase_by_page");
  return page + size2page(page->object_size);
}

static page_t* find_heap_space(size_t size){
  page_t *page = heap_start;
  while(!heap_valid(page, size)){
    page = increase_by_page(page);
  };
  return page;
}


static void *kmalloc_large(size_t size){
  void *ret = NULL;
  spin_lock(&heap_lock);
  page_t *page = find_heap_space(size);
  page->object_size = size;
  ret = page->object_start = (void *)page + PAGE_SIZE;
  Log("success alloc %07p", ret);
  spin_unlock(&heap_lock);
  return ret;
}

static void kfree_large(page_t *page){
  spin_lock(&heap_lock);
  page->object_size = 0;
  Log("success free %07p", page->object_start);
  spin_unlock(&heap_lock);
}

static void *kalloc(size_t size) {
  size = align(size);
  if(size > PAGE_SIZE) return kmalloc_large(size);

  return NULL;
}

static void kfree(void *ptr) {
  page_t *page = (page_t *)((size_t)ptr & PAGE_MASK);
  Log("%d", page->object_size);
  if(page->object_size > PAGE_SIZE) return kfree_large(page);
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
