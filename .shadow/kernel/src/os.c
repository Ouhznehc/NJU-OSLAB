#include <common.h>

#ifdef __DEBUG_MODE__
kspinlock_t debug_lk;
#endif

static void os_init() {
#ifdef __DEBUG_MODE__
  init_klock(&debug_lk, "debug_lk");
#endif
  pmm->init();
}
static void os_run() {

  while (1);
  halt(0);
}

MODULE_DEF(os) = {
    .init = os_init,
    .run = os_run,
};
