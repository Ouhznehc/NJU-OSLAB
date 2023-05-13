#include <os.h>
#include <common.h>

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

void pushcli() {
  int cpu = cpu_current();
  int istatus = ienabled();
  iset(false);
  if (lock_cnt[cpu] == 0) is_lock[cpu] = istatus;
  lock_cnt[cpu]++;
}

void popcli() {
  int cpu = cpu_current();
  int istatus = ienabled();
  if (istatus) panic("popcli interrupt is enabled");
  if (--lock_cnt[cpu] < 0) panic("popcli");
  if (lock_cnt[cpu] == 0 && is_lock[cpu]) iset(true);
}

int holding(spinlock_t* lk) {
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
