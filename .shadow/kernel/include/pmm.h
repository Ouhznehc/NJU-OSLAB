#ifndef __PMM_H__
#define __PMM_H__

#define KB * 1024
#define MB KB * 1024
#define SLAB_TYPE (11)
#define PAGE_SIZE (8 KB)
#define INFO_SIZE (256)
#define PAGE_MASK (0xffff2000)
#define MAX_CPU (8)


typedef struct list_t
{
  struct list_t *next;
}list_t;

typedef struct kmem_cache
{
  int cpu;
  spinlock_t lk;
  list_t free_list[SLAB_TYPE];
  list_t slab_list[SLAB_TYPE];
  int free_page[SLAB_TYPE];
}kmem_cache;

typedef union page_t{
  struct
  {
    list_t node;
    int cpu;
    int object_size; // "0" means free page
    int object_counter;
    int object_capacity;
    void *object_start;
    __int128_t bitset[8];
  };
  uint8_t data[PAGE_SIZE];
}__attribute__((packed)) page_t;




#endif