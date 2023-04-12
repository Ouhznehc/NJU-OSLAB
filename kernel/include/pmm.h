#ifndef __PMM_H__
#define __PMM_H__

#define KB *1024
#define MB KB * 1024
#define SLAB_SIZE (8 KB)
#define SLAB_CONFIG (256)
#define MAX_CPU (8)
#define SLAB_TYPE (11)
#define SLAB_INIT (1)

// #define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

// get struct entry
#define  container_of(ptr, type, member) ({    \
     const typeof( ((type *)0)->member ) *__mptr = (ptr); \
     (type *)( (char *)__mptr - offsetof(type,member) );})

#define setbit(x, pos) ((x) |= (1 << (pos)))
#define clrbit(x, pos) ((x) &= ~(1 << (pos)))
#define getbit(x, pos) (((x) >> (pos)) & 1)

typedef union slab_t
{
  struct
  {
    int cpu;
    union slab_t *next;
    size_t object_size; // "0" means free page
    int object_counter;
    int object_capacity;
    void *object_start;
    int bitset[32]; // each bit stand for the existence of object
  };
  uint8_t data[SLAB_SIZE];
} slab_t;

typedef struct kmem_cache
{
  int cpu;
  spinlock_t lk;
  slab_t *available_page[SLAB_TYPE];
  slab_t *slab_list[SLAB_TYPE]; // the head of slab_list
  size_t free_slab[SLAB_TYPE]; // the number of freepage in each slab
} kmem_cache;

typedef struct page_t
{
  struct page_t *next;
}page_t;

typedef struct memory_t
{
  struct memory_t *next;
  void *memory_start;
  size_t memory_size;

}memory_t;
#endif