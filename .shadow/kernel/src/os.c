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

static void os_run() {
  for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
    putch(*s == '*' ? '0' + cpu_current() : *s);
  }
  while(1){
    void *test = pmm->alloc(16 MB);
    if(test != NULL) pmm->free(test);
    Log("--------------");
  }
  while (1) ;
}                     
        
MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
};
