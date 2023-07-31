#include <common.h>


void init_klock(kspinlock_t* lk, char* name) {
  lk->locked = 0;
}

void kspin_lock(kspinlock_t* lk) {
  while (atomic_xchg(&lk->locked, 1) != 0);
  __sync_synchronize();
}

void kspin_unlock(kspinlock_t* lk) {
  __sync_synchronize();
  atomic_xchg(&lk->locked, 0);
}
