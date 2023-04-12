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
size_t num[100005];
static void os_run()
{
  for (int i = 0; i < 100005; i++)
  {
    init_lock(&lk[i], "lk");
  }
  int now = cpu_current();
  while (1)
  {
    int pos = (rand() * rand()) % 10000;
    Log("pos1=%d", pos);
    spin_lock(&lk[pos]);
    Log("pos2=%d", pos);
    Log("alloc[pos] = %p", alloc[pos]);
    if (alloc[pos] == 0)
    {
      int size = rand() % 20 + 4;
      if (size < 16)
        size = 1 << (size % 10);
      else if (size != 19)
        size = 4096;
      alloc[pos] = (uintptr_t)pmm->alloc(size);
      if (alloc[pos] == 0)
      {
        Log("no more space\n");
        spin_unlock(&lk[pos]);
        Log("pos=%d", pos);
        continue;
      }
      else
      {
        for (int i = 0; i < size / 4; i++)
        {
          int *check = (int *)(alloc[pos] + 4 * i);
          if (*check == -1)
            panic("double alloc");
        }
        num[pos] = size;
        memset((void *)alloc[pos], 0x1, size);
      }

      Log("cpu %d alloc at %p with %dB\n", now, alloc[pos], size);
    }
    else
    {
      pmm->free((void *)alloc[pos]);
      for (int i = 0; i < num[pos] / 4; i++)
      {
        int *check = (int *)(alloc[pos] + 4 * i);
        if (*check == 0)
          panic("double free");
      }
      memset((void *)alloc[pos], 0x1, num[pos]);
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
