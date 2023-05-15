#include <os.h>
#include <common.h>

#define INT_MAX 2147483647
#define INT_MIN -2147483648
#define STACK_SIZE 8196

static void kmt_init();
static int kmt_create(task_t* task, const char* name, void (*entry)(void* arg), void* arg);
static void kmt_teardown(task_t* task);
static void kmt_spin_init(spinlock_t* lk, const char* name);
static void kmt_spin_lock(spinlock_t* lk);
static void kmt_spin_unlock(spinlock_t* lk);
static void kmt_sem_init(sem_t* sem, const char* name, int value);
static void kmt_sem_wait(sem_t* sem);
static void kmt_sem_signal(sem_t* sem);



/*====================== lock ====================== */

static int is_lock[MAX_CPU];
static int lock_cnt[MAX_CPU];

static void pushcli() {
  int cpu = cpu_current();
  int istatus = ienabled();
  iset(false);
  if (lock_cnt[cpu] == 0) is_lock[cpu] = istatus;
  lock_cnt[cpu]++;
}

static void popcli() {
  int cpu = cpu_current();
  int istatus = ienabled();
  if (istatus) panic("popcli interrupt is enabled");
  if (--lock_cnt[cpu] < 0) panic("popcli");
  if (lock_cnt[cpu] == 0 && is_lock[cpu]) iset(true);
}

static int holding(spinlock_t* lk) {
  pushcli();
  int rc = lk->locked && lk->cpu == cpu_current();
  popcli();
  return rc;
}

static void kmt_spin_init(spinlock_t* lk, const char* name) {
  lk->name = name;
  lk->locked = 0;
  lk->cpu = -1;
}

static void kmt_spin_lock(spinlock_t* lk) {
  pushcli();
  if (holding(lk)) panic("double lock: %s in CPU #%d", lk->name, lk->cpu);
  while (atomic_xchg(&lk->locked, 1) != 0);
  __sync_synchronize();
  lk->cpu = cpu_current();
}

static void kmt_spin_unlock(spinlock_t* lk) {
  if (!holding(lk)) panic("double unlock: %s in CPU#%d", lk->name, lk->cpu);
  lk->cpu = -1;
  __sync_synchronize();
  atomic_xchg(&lk->locked, 0);
  popcli();
}




/*====================== semaphore ====================== */

static void kmt_sem_init(sem_t* sem, const char* name, int value) {
  kmt_spin_init(&sem->lk, name);
  sem->count = value;
  sem->name = name;
}

static void kmt_sem_wait(sem_t* sem) {
  kmt_spin_lock(&sem->lk);
  // Log("%s try P with count = %d", sem->name, sem->count);
  Assert(sem->count >= 0, "kmt_sem_wait: sem->count < 0");
  int is_wait = 0;
  while (sem->count == 0) {
    if (is_wait == 0) kmt_spin_unlock(&sem->lk);
    is_wait = 1;
    // Log("%s P failed: yield()", sem->name);
    yield();
  }
  if (is_wait == 1) kmt_spin_lock(&sem->lk);
  sem->count--;
  kmt_spin_unlock(&sem->lk);
}

static void kmt_sem_signal(sem_t* sem) {
  kmt_spin_lock(&sem->lk);
  // Log("%s try V with count = %d", sem->name, sem->count);
  Assert(sem->count >= 0, "kmt_sem_signal: sem->count < 0");
  sem->count++;
  kmt_spin_unlock(&sem->lk);
}


/*====================== kmt ====================== */

static spinlock_t os_trap_lk;
static task_t* current_task[MAX_CPU], * buffer_task[MAX_CPU];
static task_t* task_list;

static void task_list_insert(task_t* insert_task) {
  if (task_list == NULL) {
    task_list = insert_task;
    insert_task->next = task_list;
  }
  else {
    insert_task->next = task_list->next;
    task_list->next = insert_task;
  }
}

static void task_list_delete(task_t* delete_task) {
  delete_task->status = DELETED;
}

static task_t* task_list_query(int cpu) {
  Log("begin");
  for (task_t* cur = current_task[cpu]->next; cur != current_task[cpu]; cur = cur->next) {
    assert(cur != NULL);
    if (cur->status == RUNNABLE) {
      Log("return");
      return cur;
    }
  }
  return NULL;
}



static Context* kmt_context_save(Event ev, Context* context) {
  int cpu = cpu_current();
  current_task[cpu]->context = context;
  return NULL;
}

static Context* kmt_schedule(Event ev, Context* context) {
  int cpu = cpu_current();
  Log("CPU#%d", cpu);
  Context* ret = NULL;
  kmt_spin_lock(&os_trap_lk);
  Log("begin");
  if (buffer_task[cpu] != NULL) {
    Assert(buffer_task[cpu]->status == RUNNING, "buffer_task not RUNNING");
    buffer_task[cpu]->status = RUNNABLE;
    buffer_task[cpu] = NULL;
    Log("1");
  }
  Log("2");
  task_t* next_task = task_list_query(cpu);
  Log("3");
  if (next_task == NULL) ret = current_task[cpu]->context;
  else {
    Log("find runnable");
    ret = next_task->context;
    buffer_task[cpu] = current_task[cpu];
    current_task[cpu] = next_task;
    next_task->status = RUNNING;
  }
  Log("end");
  kmt_spin_unlock(&os_trap_lk);
  return ret;
}

static int kmt_create(task_t* task, const char* name, void (*entry)(void* arg), void* arg) {
  task->name = name;
  task->stack = pmm->alloc(STACK_SIZE);
  Area stack = (Area){ task->stack, task->stack + STACK_SIZE };
  task->context = kcontext(stack, entry, arg);
  task->status = RUNNABLE;
  task_list_insert(task);
  return 0;
}

static void kmt_teardown(task_t* task) {
  // kmt_spin_lock(&os_trap_lk);
  pmm->free(task->stack);
  task_list_delete(task);
  // kmt_spin_unlock(&os_trap_lk);
}


static void kmt_init() {
  kmt_spin_init(&os_trap_lk, "os_trap_lk");
  for (int i = 0; i < MAX_CPU; i++) {
    lock_cnt[i] = is_lock[i] = 0;
    buffer_task[i] = NULL;
  }
  os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
  os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
}

MODULE_DEF(kmt) = {
  .init = kmt_init,
  .create = kmt_create,
  .teardown = kmt_teardown,
  .spin_init = kmt_spin_init,
  .spin_lock = kmt_spin_lock,
  .spin_unlock = kmt_spin_unlock,
  .sem_init = kmt_sem_init,
  .sem_wait = kmt_sem_wait,
  .sem_signal = kmt_sem_signal,
};
