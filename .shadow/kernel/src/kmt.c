#include <os.h>
#include <common.h>

#define INT_MAX 2147483647
#define INT_MIN -2147483648
#define STACK_SIZE 8192

static void kmt_init();
static int kmt_create(task_t* task, const char* name, void (*entry)(void* arg), void* arg);
static void kmt_teardown(task_t* task);
static void kmt_spin_init(spinlock_t* lk, const char* name);
static void kmt_spin_lock(spinlock_t* lk);
static void kmt_spin_unlock(spinlock_t* lk);
static void kmt_sem_init(sem_t* sem, const char* name, int value);
static void kmt_sem_wait(sem_t* sem);
static void kmt_sem_signal(sem_t* sem);


static int is_lock[MAX_CPU];
static int lock_cnt[MAX_CPU];



static spinlock_t os_trap_lk;
static task_t* current_task[MAX_CPU], * buffer_task[MAX_CPU];
task_t* runnable_task[MAX_TASK];
int runnable_head = 0, runnable_tail = 0;


/*====================== lock ====================== */

static void pushcli() {
  int istatus = ienabled();
  iset(false);
  int cpu = cpu_current();
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
  int spin_time = 0;
  if (holding(lk)) panic("double lock: %s in CPU #%d", lk->name, lk->cpu);
  while (atomic_xchg(&lk->locked, 1) != 0) {
    spin_time++;
    if (spin_time >= 1000000) panic("%s: spin time exceed", lk->name);
  }
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
  Assert(sem->count >= 0, "kmt_sem_wait: sem->count < 0");
  int sem_time = 0;
  while (sem->count == 0) {
    sem_time++;
    if (sem_time >= 100000) panic("%s: sem time exceeded", sem->name);
    kmt_spin_unlock(&sem->lk);
    yield();
    kmt_spin_lock(&sem->lk);
  }
  sem->count--;
  kmt_spin_unlock(&sem->lk);
}

static void kmt_sem_signal(sem_t* sem) {
  kmt_spin_lock(&sem->lk);
  Assert(sem->count >= 0, "kmt_sem_signal: sem->count < 0");
  sem->count++;
  kmt_spin_unlock(&sem->lk);
}


/*====================== kmt ====================== */

static void runnable_task_push(task_t* task) {
  runnable_task[runnable_tail] = task;
  runnable_tail = (runnable_tail + 1) % MAX_TASK;
}

static task_t* runnable_task_pop() {
  task_t* ret = NULL;
  while (runnable_task[runnable_head]->status != RUNNABLE && runnable_head != runnable_tail) runnable_head = (runnable_head + 1) % MAX_TASK;
  if (runnable_head != runnable_tail) {
    ret = runnable_task[runnable_head];
    runnable_head = (runnable_head + 1) % MAX_TASK;
  }
  return ret;
}


static Context* kmt_context_save(Event ev, Context* context) {
  kmt_spin_lock(&os_trap_lk);
  int cpu = cpu_current();
  if (current_task[cpu] != NULL) current_task[cpu]->context = context;
  kmt_spin_unlock(&os_trap_lk);
  return NULL;
}

static Context* kmt_schedule(Event ev, Context* context) {
  int cpu = cpu_current();
  task_t* ret = NULL;
  kmt_spin_lock(&os_trap_lk);
  if (buffer_task[cpu] != NULL) {
    Assert(buffer_task[cpu]->status == RUNNING, "buffer_task not RUNNING");
    buffer_task[cpu]->status = RUNNABLE;
    runnable_task_push(buffer_task[cpu]);
    buffer_task[cpu] = NULL;
  }
  task_t* next_task = runnable_task_pop();
  if (next_task == NULL) ret = current_task[cpu];
  else {
    ret = next_task;
    buffer_task[cpu] = current_task[cpu];
    current_task[cpu] = next_task;
    next_task->status = RUNNING;
  }
  Log("CPU#%d is running", cpu);
  // Log("%p ret\n", ret->context);
  kmt_spin_unlock(&os_trap_lk);
  return ret->context;
}

static int kmt_create(task_t* task, const char* name, void (*entry)(void* arg), void* arg) {
  task->name = name;
  task->stack = pmm->alloc(STACK_SIZE);
  Area stack = (Area){ task->stack, task->stack + STACK_SIZE };
  task->context = kcontext(stack, entry, arg);
  task->status = RUNNABLE;
  runnable_task_push(task);
  return 0;
}

static void kmt_teardown(task_t* task) {
  // kmt_spin_lock(&os_trap_lk);
  // pmm->free(task->stack);
  // task_list_delete(task);
  // kmt_spin_unlock(&os_trap_lk);
}


static void kmt_init() {
  kmt_spin_init(&os_trap_lk, "os_trap_lk");
  for (int i = 0; i < MAX_CPU; i++) {
    lock_cnt[i] = is_lock[i] = 0;
    buffer_task[i] = NULL;
    current_task[i] = NULL;
  }
  memset(runnable_task, 0, sizeof(runnable_task));
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
