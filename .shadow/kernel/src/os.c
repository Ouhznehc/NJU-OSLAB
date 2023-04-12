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
#ifdef __DEBUG_MODE__
spinlock_t lk[100005];
uintptr_t alloc[100005];
size_t num[100005];
#endif
static void os_run()
{
#ifdef __DEBUG_MODE__
  for (int i = 0; i < 100005; i++)
  {
    init_lock(&lk[i], "lk");
  }
  int now = cpu_current();
  while (1)
  {
    int pos = (rand() * rand()) % 10000;
    Log("");
    Log("pos %d", pos);
    spin_lock(&lk[pos]);
    if (alloc[pos] == 0)
    {
      int size = rand() % 20;
      if (size < 16)
        size = 1 << (size % 10 + 2);
      else if (size != 19)
        size = 4096;
      alloc[pos] = (uintptr_t)pmm->alloc(size);
      if (alloc[pos] == 0)
      {
        Log("no more space\n");
        spin_unlock(&lk[pos]);
        continue;
      }
      else
      {
        assert(size >= 4);
        for (int i = 0; i < size / 4; i++)
        {
          int *check = (int *)(alloc[pos] + 4 * i);
          Assert(*check == 0, "check alloc=%07p", *check);
          if (*check == -1)
            panic("double alloc");
        }
        num[pos] = size;
        memset((void *)alloc[pos], -1, size);
      }
    }
    else
    {
      assert(num[pos] >= 4);
      pmm->free((void *)alloc[pos]);
      for (int i = 0; i < num[pos] / 4; i++)
      {
        int *check = (int *)(alloc[pos] + 4 * i);
        Assert(*check == -1, "check free=%07p, %07p", *check, MAGIC + 1);
        if (*check == 0)
          panic("double free");
      }
      memset((void *)alloc[pos], 0, num[pos]);
      Log("cpu %d free time %d at %p", now, pos, alloc[pos]);
      alloc[pos] = 0;
      num[pos] = 0;
    }
    spin_unlock(&lk[pos]);
  }

  uintptr_t al[4005];
  for (int i = 1; i <= 4000; i++)
  {
    al[i] = (uintptr_t)pmm->alloc(2);
    Log("%d times: %p\n", i, al[i]);
  }
  for (int i = 1; i <= 1000; i++)
  {
    pmm->free((void *)al[i]);
    void *ret = pmm->alloc(4096);
    Log("%d times: %p\n", i, ret);
  }
#endif

  while (1)
    ;
  halt(0);
}

MODULE_DEF(os) = {
    .init = os_init,
    .run = os_run,
};
