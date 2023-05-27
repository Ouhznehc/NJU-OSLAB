#include <klib.h>
#include <os.h>
#include <common.h>

extern struct task* runnable_task[MAX_TASK];
extern int runnable_head, runnable_tail;

#define P kmt->sem_wait
#define V kmt->sem_signal
sem_t empty, fill;
void Tproduce(void* arg) {
  assert(0);
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
  Log("head = %d, tail = %d", runnable_head, runnable_tail);
  for (int i = 0; i < 5; i++) {
    void* ret = pmm->alloc(sizeof(task_t));
    for (int i = 0; i < sizeof(task_t) - sizeof(int); i++) {
      assert(*(int*)(ret + i) == 0);
    }
    kmt->create(ret, "producer", Tproduce, NULL);
  }
  for (int i = 0; i < 1; i++) {
    kmt->create(pmm->alloc(sizeof(task_t)), "consumer", Tconsume, NULL);
  }
  Log("head = %d, tail = %d", runnable_head, runnable_tail);
  for (int j = runnable_head; j < runnable_tail; j++) {
    Log("TASK#%p : rip = %p", runnable_task[j]->stack, runnable_task[j]->context->rip);
  }
}
int main() {
  ioe_init();
  cte_init(os->trap);
  os->init();
  create_threads();
  mpe_init(os->run);
  return 1;
}
