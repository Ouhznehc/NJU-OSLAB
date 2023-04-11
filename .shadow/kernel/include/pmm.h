#ifndef __PMM_H__
#define __PMM_H__

#define KB * 1024
#define MB KB * 1024

typedef struct list_t
{
  struct list_t *next;
}list_t;

typedef struct kmem_cache
{
  int cpu;
  spinlock_t lk;

}kmem_cache;


#endif