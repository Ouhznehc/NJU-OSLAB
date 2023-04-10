#include <common.h>

bool holding(spinlock_t *lk){
  printf("holding %s: %d %d cpu=%d\n", lk->name, lk->cpu, lk->locked, cpu_current());
  return lk->locked && lk->cpu == cpu_current();
}

void init_lock(spinlock_t *lk, char *name){
  lk->name = name;
  lk->locked = 0;
  lk->cpu = -1;
}

void spin_lock(spinlock_t *lk) {
  if(holding(lk)) panic("spin_lock: %s", lk->name);
  while(atomic_xchg(&lk->locked, 1) != 0);
  __sync_synchronize();
  lk->cpu = cpu_current();
  printf("%s: %d\n", lk->name, lk->cpu);
  Log("fuck");
}

void spin_unlock(spinlock_t *lk) {
  if(!holding(lk)) panic("spin_unlock: %s", lk->name);
  lk->cpu = -1;
  __sync_synchronize();
  atomic_xchg(&lk->locked, 0);
}



