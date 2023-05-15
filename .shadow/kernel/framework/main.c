#include <klib.h>
#include <os.h>
#include <common.h>

#define P kmt->sem_wait
#define V kmt->sem_signal
sem_t empty, fill;
void Tproduce(void* arg) {
  while (1) {
    P(&empty);
    putch('(');
    V(&fill);
  }
}
void Tconsume(void* arg) {
  while (1) {
    P(&fill);
    putch(')');
    V(&empty);
  }
}
static void create_threads() {
  kmt->sem_init(&empty, "empty", 1);
  kmt->sem_init(&fill, "fill", 0);
  for (int i = 0; i < 1; i++) {
    kmt->create(pmm->alloc(sizeof(task_t)), "producer", Tproduce, NULL);
  }
  for (int i = 0; i < 1; i++) {
    kmt->create(pmm->alloc(sizeof(task_t)), "consumer", Tconsume, NULL);
  }
}
int main() {
  ioe_init();
  cte_init(os->trap);
  assert(0);
  os->init();
  assert(0);
  create_threads();
  assert(0);
  mpe_init(os->run);
  return 1;
}
