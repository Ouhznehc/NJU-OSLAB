#include <common.h>

#define MAX_TASK 64

enum task_status {
  BLOCKED = 0,
  RUNNABLE = 1,
  RUNNING = 2
};

struct task {
  int status;
  const char* name;
  struct task* prev, * next;
  Context* context;
  uintptr_t* stack;
};

struct spinlock {
  int locked;
  const char* name;
  int cpu;
};

struct semaphore {
  const char* name;
  spinlock_t lk;
  int count;
};

