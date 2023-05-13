#include <common.h>

bool holding(kspinlock_t* lk) {
  return lk->locked && lk->cpu == cpu_current();
  // return lk->locked;
}

void init_klock(kspinlock_t* lk, char* name) {
  lk->name = name;
  lk->locked = 0;
  lk->cpu = -1;
}

void kspin_lock(kspinlock_t* lk) {
  if (holding(lk))
    kernal_panic("spin_lock: %s at CPU #&d", lk->name, lk->cpu);
  while (atomic_xchg(&lk->locked, 1) != 0)
    ;
  __sync_synchronize();
  lk->cpu = cpu_current();
}

void kspin_unlock(kspinlock_t* lk) {
  if (!holding(lk))
    kernal_panic("spin_unlock: %s at CPU #%d", lk->name, lk->cpu);
  lk->cpu = -1;
  __sync_synchronize();
  atomic_xchg(&lk->locked, 0);
}