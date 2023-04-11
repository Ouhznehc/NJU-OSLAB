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

int a[20] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8 KB, 32 KB, 512 KB, 1 MB, 2 MB, 8 MB, 16 MB};
static void os_run() {
  for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
    putch(*s == '*' ? '0' + cpu_current() : *s);
  }
  while(1){

    void *test = pmm->alloc(a[rand() % 20]);
    if(test != NULL) pmm->free(test);
    //Log("--------------");
  }
  while (1) ;
}                     
        
MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
};
