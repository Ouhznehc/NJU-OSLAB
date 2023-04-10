#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

typedef struct spinlock{
  bool locked;
  const char *name;
  int cpu;
}spinlock_t;

void init_lock(spinlock_t *lk, char *name);
void spin_lock(spinlock_t *lk);
void spin_unlock(spinlock_t *lk);

#endif