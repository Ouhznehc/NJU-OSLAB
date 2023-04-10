#include <kernel.h>
#include <klib.h>

#define __DEBUG_MODE__

int main() {
  os->init();
  mpe_init(os->run);
  return 1;
}
