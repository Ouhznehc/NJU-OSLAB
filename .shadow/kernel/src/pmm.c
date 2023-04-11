#include <common.h>

static page_t *heap_start = NULL;
static kmem_cache kmem[MAX_CPU];
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

static int heap_valid(page_t *page, size_t size){
  size_t pages = size2page(size);
  if((void*)(page + pages) >= heap.end) return 0;
  for(int i = 0; i < pages; i++){
    if((page + i)->object_size) return 1;
  }
  return 2;
}

static inline page_t* increase_by_page(page_t *page){
  //Log("page=%07p, size=%07p", page, page->object_size);
  return page + size2page(page->object_size);
}

static page_t* find_heap_space(size_t size){
  page_t *page = heap_start;
  while(1){
    switch (heap_valid(page, size))
    {
      case 0: return NULL;
      case 1: page = increase_by_page(page); break;
      case 2: return page;
      default: panic("find_heap_space");
    }
  };
}


static page_t* pages_from_heap(int cpu, int slab_type, int pages){
  spin_lock(&heap_lock);
  page_t *ret = NULL;
  while(pages--){
    page_t *page = find_heap_space(slab_type);
    if(page == NULL){
      spin_unlock(&heap_lock);
      return NULL;
    }
    init_lock(&page->lk, "page");
    page->cpu = cpu;
    page->object_size = slab_type;
    page->object_counter = 0;
    page->object_capacity = (PAGE_SIZE - PAGE_CONFIG) / slab_type;
    page->object_start = (void*)page + PAGE_CONFIG;
    page->node.next = NULL;
    if(ret == NULL) ret = page;
    else{
      page->node.next = &ret->node;
      ret = page;
    }
  }
  spin_unlock(&heap_lock);
  return ret;
}


static void *kmalloc_large(size_t size){
  void *ret = NULL;
  spin_lock(&heap_lock);
  page_t *page = find_heap_space(size);
  if(page == NULL) return NULL;
  panic_on(page->object_size, "find_heap_size: page=%07p, size=%07p", page, page->object_size);
  init_lock(&page->lk, "page");
  spin_lock(&page->lk);
  page->object_size = size;
  ret = page->object_start = (void *)page + PAGE_CONFIG;
  Log("success alloc %07p, size = %07p", ret, page->object_size);
  spin_unlock(&page->lk);
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
  // Log("try free %07p at page %07p", ptr, page);
  spin_lock(&page->lk);
  if(page->object_size > PAGE_SIZE){
    spin_unlock(&page->lk);
    return kfree_large(page);
  }
}

static void pmm_init() {
  init_lock(&heap_lock, "heap_lock");
  heap_start = heap.start;
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
  for(int i = 0; i < cpu_count(); i++){
    kmem[i].cpu = i;
    init_lock(&kmem[i].lk, "kmem");
    for(int j = 0; j < SLAB_TYPE; j++){
      page_t *pages = pages_from_heap(i, slab_type[j], SLAB_MIN);
      kmem[i].slab_list[j].next = &pages->node;
      kmem[i].free_list[j].next = &pages->node;
      kmem[i].free_page[j] = SLAB_MIN;
      Assert(((size_t)&pages == (size_t)&(pages->node)), "fuck union: page=%07p, node=%07p", (size_t)&pages, (size_t)&pages->node);
    }

  }
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
