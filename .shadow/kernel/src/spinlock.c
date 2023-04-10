#include <common.h>


void init_lock(spinlock_t *lk, char *name){
    lk->name = name;
    lk->locked = 0;
    lk->cpu = -1;
}

void spin_lock(spinlock_t *lk) {
  while (1) {
    int value = atomic_xchg((int*)lk->locked, 1);
    if (value == 0) break;
  }
}

void spin_unlock(spinlock_t *lk) {
  atomic_xchg((int*)lk->locked, 0);
}


