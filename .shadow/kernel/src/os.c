#include <common.h>

#ifdef __DEBUG_MODE__
spinlock_t debug_lk;
#endif


static void os_init() {
  #ifdef __DEBUG_MODE__
  init_lock(&debug_lk, "debug_lk");
  #endif
  pmm->init();
}    

spinlock_t lk[100005], plock;
uintptr_t alloc[100005];
static void os_run() {
  int now = cpu_current();
  // int s = 0, maxx = 0;
  while (1)
  {
    int pos = (rand() * rand()) % 10000;
    // int pos = s++;
    // if (pos == 100000)
    //   break;
    spin_lock(&lk[pos]);
    if (alloc[pos] == 0)
    {
      int size = rand() % 20;
      if (size < 16)
        size = 1 << (size % 10);
      else if (size != 19)
        size = 4096;
      // else
      // size = 4096;
      // spin_lock(&plock);
      // printf("cpu %d want to alloc time %d with %dB\n", now, pos, size);
      // spin_unlock(&plock);

      alloc[pos] = (uintptr_t)pmm->alloc(size);
      if (alloc[pos] == 0)
      {
        spin_lock(&plock);
        printf("no more space\n");
        spin_unlock(&plock);
        spin_unlock(&lk[pos]);
        // s = 0;
        // maxx = pos;
        continue;
      }
      spin_lock(&plock);
      printf("cpu %d alloc at %p with %dB\n", now, alloc[pos], size);
      spin_unlock(&plock);
    }
    else
    {
      pmm->free((void *)alloc[pos]);
      spin_lock(&plock);
      printf("cpu %d free time %d at %p\n", now, pos, alloc[pos]);
      spin_unlock(&plock);
      alloc[pos] = 0;
      // if (pos == maxx - 1)
      // {
      //   s = 0;
      //   spin_unlock(&lk[pos]);
      //   continue;
      // }
    }
    spin_unlock(&lk[pos]);
  }

  uintptr_t al[4005];
  for (int i = 1; i <= 4000; i++)
  {
    al[i] = (uintptr_t)pmm->alloc(2);
    printf("%p\n", al[i]);
  }
  printf("\n");
  for (int i = 1; i <= 9; i++)
  {
    pmm->free((void *)al[i]);
    printf("%p\n", (uintptr_t)pmm->alloc(1024));
  // }
  while(1);
  }
}                    
        
MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
};
