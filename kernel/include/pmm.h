#ifndef __PMM_H__
#define __PMM_H__

#define KB * 1024
#define MB KB * 1024
#define PAGE_SIZE (8 KB)
#define PAGE_CONFIG (256)
#define PAGE_MASK (0xffffe000)
#define MAX_CPU (8)
#define SLAB_TYPE (11)
#define SLAB_MIN (8)
#define SLAB_MAX (16)

// #define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
// get struct entry
#define  container_of(ptr, type, member) ({    \
     const typeof( ((type *)0)->member ) *__mptr = (ptr); \
     (type *)( (char *)__mptr - offsetof(type,member) );})
#define setbit(x,pos)  ((x) |=  (1<<(pos)))     
#define clrbit(x,pos)  ((x) &= ~(1<<(pos)))   
#define getbit(x,pos)  ((x) >> (pos) & 1) 

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
    spinlock_t lk;
    int cpu;
    int object_size; // "0" means free page
    int object_counter;
    int object_capacity;
    void *object_start;
    __int128_t bitset[8];
  };
  uint8_t data[PAGE_SIZE];
}page_t;




#endif