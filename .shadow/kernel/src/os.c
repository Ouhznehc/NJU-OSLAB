#include <common.h>

typedef struct interrupt_request {
  int seq;
  int event;
  handler_t handler;
  struct interrupt_request* next;
}irq_t;

static irq_t* irq_list_head;


#ifdef __DEBUG_MODE__
kspinlock_t debug_lk;
#endif

static void os_init() {
#ifdef __DEBUG_MODE__
  init_klock(&debug_lk, "debug_lk");
#endif
  pmm->init();
  irq_list_head = pmm->alloc(sizeof(irq_t));
  irq_list_head->next = NULL;
  irq_list_head->seq = 0;
  irq_list_head->handler = NULL;
  irq_list_head->event = EVENT_NULL;
  kmt->init();
}
static void os_run() {
  iset(true);
  while (1);
}


static Context* os_trap(Event ev, Context* ctx) {
  Context* next = NULL;
  for (irq_t* h = irq_list_head->next; h != NULL; h = h->next) {
    if (h->event == EVENT_NULL || h->event == ev.event) {
      Context* r = h->handler(ev, ctx);
      panic_on(r && next, "returning multiple contexts");
      if (r) next = r;
    }
  }
  assert(0);
  panic_on(!next, "returning NULL context");
  return next;
}

static void os_on_irq(int seq, int event, handler_t handler) {

  irq_t* new_irq = pmm->alloc(sizeof(irq_t));
  new_irq->seq = seq;
  new_irq->event = event;
  new_irq->handler = handler;

  irq_t* cur = irq_list_head;
  while (cur->next != NULL && cur->next->seq < seq) cur = cur->next;
  new_irq->next = cur->next;
  cur->next = new_irq;
}



MODULE_DEF(os) = {
    .init = os_init,
    .run = os_run,
    .trap = os_trap,
    .on_irq = os_on_irq,
};
