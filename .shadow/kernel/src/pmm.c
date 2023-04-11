#include <common.h>

static spinlock_t heap_lock;
static page_t *heap_start = NULL;
static kmem_cache kmem[MAX_CPU];
int slab_type[SLAB_TYPE] = {2, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

static inline int match_slab_type(size_t size)
{
  for (int i = 0; i < SLAB_TYPE; i++)
    if (size <= slab_type[i])
      return i;
  panic("match_slab_type: should not reach here");
  return 0;
}

static inline size_t size2page(size_t size)
{
  return size / PAGE_SIZE + 1;
}

static inline size_t align(size_t size)
{
  size_t msb = 31 - __builtin_clz(size);
  if (size == (1 << msb))
    return size;
  else
    return (1 << (msb + 1));
}

static bool address_align(size_t address, size_t size){
  size_t lsb_address = __builtin_ctz(address);
  size_t lsb_size = __builtin_ctz(size);
  return lsb_address == lsb_size;
}

static int heap_valid(page_t *page, size_t size)
{
  size_t pages = size2page(size);
  if ((void *)(page + pages) >= heap.end)
    return 0;
  //Log("page=%07p, add=%07p", page, (void *)page + 4 KB);
  assert(((void *)page + 4 KB) <= heap.end);
  if(size >= PAGE_SIZE && !address_align((size_t)((void *)page + 4 KB), size)) return 1;
  for (int i = 0; i < pages; i++)
  {
    if ((page + i)->object_size)
      return 1;
  }
  return 2;
}

static inline page_t *increase_by_page(page_t *page)
{
  return page + size2page(page->object_size);
}

static page_t *find_heap_space(size_t size)
{
  page_t *page = heap_start;
  while (1)
  {
    switch (heap_valid(page, size))
    {
    case 0:
      return NULL;
    case 1:
      page = increase_by_page(page);
      break;
    case 2:
      return page;
    default:
      panic("find_heap_space: should not reach here");
    }
  };
}

static void *object_from_slab(page_t *page)
{
  void *ret = NULL;
  for (int i = 0; i < 8; i++)
  {
    if (page->bitset[i] == (__int128_t)(-1))
      continue;
    for (int j = 0; j < 128; j++)
    {
      if (getbit(page->bitset[i], j) == 0)
      {
        if (page->object_counter == 0)
        {
          int slab_index = match_slab_type(page->object_size);
          kmem[page->cpu].free_page[slab_index]--;
        }
        setbit(page->bitset[i], j);
        page->object_counter++;
        ret = page->object_start + (128 * i + j) * page->object_size;
        return ret;
      }
    }
  }
  panic("object_from_slab: should not reach here");
  return NULL;
}

static page_t *pages_from_heap(int cpu, int slab_type, int pages)
{
  spin_lock(&heap_lock);
  page_t *ret = NULL;
  while (pages--)
  {
    page_t *page = find_heap_space(slab_type);
    if (page == NULL)
    {
      spin_unlock(&heap_lock);
      return NULL;
    }
    init_lock(&page->lk, "page");
    page->cpu = cpu;
    page->object_size = slab_type;
    page->object_counter = 0;
    page->object_capacity = (PAGE_SIZE - PAGE_CONFIG) / slab_type;
    page->object_start = (slab_type <= PAGE_CONFIG) ? (void *)page + PAGE_CONFIG : (void*)page + slab_type;
    page->node.next = NULL;
    if (ret == NULL)
      ret = page;
    else
    {
      page->node.next = &ret->node;
      ret = page;
    }
  }
  spin_unlock(&heap_lock);
  return ret;
}

static page_t *attach_page2slab(int slab_index, int cpu)
{
  page_t *page = pages_from_heap(cpu, slab_type[slab_index], 1);
  if (page == NULL)
    return NULL;
  assert(kmem[cpu].free_list[slab_index].next->next == NULL);
  kmem[cpu].free_list[slab_index].next->next = &page->node;
  kmem[cpu].free_list[slab_index].next = &page->node;
  kmem[cpu].free_page[slab_index]++;
  return page;
}

static void *kmalloc_large(size_t size)
{
  void *ret = NULL;
  spin_lock(&heap_lock);
  page_t *page = find_heap_space(size);
  if (page == NULL)
    return NULL;
  panic_on(page->object_size, "find_heap_size: page=%07p, size=%07p", page, page->object_size);
  init_lock(&page->lk, "page");
  spin_lock(&page->lk);
  page->object_size = size;
  ret = page->object_start = (void *)page + 4096;
  Log("success alloc %07p, size = %07p", ret, page->object_size);
  spin_unlock(&page->lk);
  spin_unlock(&heap_lock);
  return ret;
}

static void kfree_large(page_t *page)
{
  spin_lock(&heap_lock);
  page->object_size = 0;
  Log("success free %07p", page->object_start);
  spin_unlock(&heap_lock);
}

static void *kalloc(size_t size)
{
  if (size > 16 MB)
    return NULL;
  void *ret = NULL;
  size = align(size);
  if (size >= PAGE_SIZE)
    return kmalloc_large(size);
  int cpu = cpu_current(), slab_index = match_slab_type(size);
  spin_lock(&kmem[cpu].lk);
  page_t *freepage = container_of(kmem[cpu].free_list[slab_index].next, page_t, node);
  spin_lock(&freepage->lk);
  if (freepage->object_counter < freepage->object_capacity)
  {
    ret = object_from_slab(freepage);
    spin_unlock(&freepage->lk);
  }
  else
  {
    spin_unlock(&freepage->lk);
    list_t *node = kmem[cpu].slab_list[slab_index].next;
    freepage = container_of(node, page_t, node);
    assert(freepage->object_counter <= freepage->object_capacity);
    assert(freepage->cpu == cpu);
    while (freepage->object_counter == freepage->object_capacity && node->next != NULL)
    {
      node = node->next;
      freepage = container_of(node, page_t, node);
      assert(freepage->object_counter <= freepage->object_capacity);
      assert(freepage->cpu == cpu);
    }
    if (node->next != NULL)
    {
      spin_lock(&freepage->lk);
      ret = object_from_slab(freepage);
      spin_unlock(&freepage->lk);
    }
    else
    {
      page_t *page = attach_page2slab(slab_index, cpu);
      if (page == NULL)
        return NULL;
      ret = object_from_slab(page);
    }
  }
  spin_unlock(&kmem[cpu].lk);
  return ret;
}

static void kfree(void *ptr)
{
  page_t *page = (page_t *)((size_t)ptr & PAGE_MASK);
  spin_lock(&page->lk);
  if (page->object_size >= PAGE_SIZE)
  {
    spin_unlock(&page->lk);
    return kfree_large(page);
  }
  int offset = (ptr - page->object_start) / page->object_size;
  int i = offset / 128, j = offset % 128;
  assert(getbit(page->bitset[i], j) == 1);
  clrbit(page->bitset[i], j);
  if (page->object_counter == 0)
  {
    int cpu = page->cpu, slab_index = match_slab_type(page->object_size);
    spin_lock(&kmem[cpu].lk);
    kmem[cpu].free_page[slab_index]++;
    spin_unlock(&kmem[cpu].lk);
  }
  spin_unlock(&page->lk);
}

static void pmm_init()
{
  init_lock(&heap_lock, "heap_lock");
  heap_start = heap.start;
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
  for (int i = 0; i < cpu_count(); i++)
  {
    kmem[i].cpu = i;
    init_lock(&kmem[i].lk, "kmem");
    for (int j = 0; j < SLAB_TYPE; j++)
    {
      page_t *pages = pages_from_heap(i, slab_type[j], SLAB_MIN);
      kmem[i].slab_list[j].next = &pages->node;
      kmem[i].free_list[j].next = &pages->node;
      kmem[i].free_page[j] = SLAB_MIN;
    }
  }
}

MODULE_DEF(pmm) = {
    .init = pmm_init,
    .alloc = kalloc,
    .free = kfree,
};
