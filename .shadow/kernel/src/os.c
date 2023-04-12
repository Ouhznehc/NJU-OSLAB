#include <common.h>

#ifdef __DEBUG_MODE__
spinlock_t debug_lk;
#endif

static void os_init()
{
#ifdef __DEBUG_MODE__
  init_lock(&debug_lk, "debug_lk");
#endif
  pmm->init();
}

spinlock_t lk[100005];
uintptr_t alloc[100005];
static void os_run()
{
  // for (int i = 0; i < 100005; i++)
  // {
  //   init_lock(&lk[i], "lk");
  // }
  int now = cpu_current();
  while (1)
  {
    int pos = (rand() * rand()) % 10000;
    spin_lock(&lk[pos]);
    if (alloc[pos] == 0)
    {
      int size = rand() % 20;
      if (size < 16)
        size = 1 << (size % 10);
      else if (size != 19)
        size = 4096;
      alloc[pos] = (uintptr_t)pmm->alloc(size);
      if (alloc[pos] == 0)
      {
        Log("no more space\n");
        spin_unlock(&lk[pos]);
        continue;
      }
      Log("cpu %d alloc at %p with %dB\n", now, alloc[pos], size);
    }
    else
    {
      pmm->free((void *)alloc[pos]);
      Log("cpu %d free time %d at %p\n", now, pos, alloc[pos]);
      alloc[pos] = 0;
    }
    spin_unlock(&lk[pos]);
  }

  uintptr_t al[4005];
  for (int i = 1; i <= 4000; i++)
  {
    al[i] = (uintptr_t)pmm->alloc(2);
    Log("%p\n", al[i]);
  }
  Log("\n");
  for (int i = 1; i <= 9; i++)
  {
    pmm->free((void *)al[i]);
    Log("%p\n", (uintptr_t)pmm->alloc(1024));
    // }
    while (1)
      ;
  }
}

MODULE_DEF(os) = {
    .init = os_init,
    .run = os_run,
};
