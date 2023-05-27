#include <common.h>

#define MAX_TASK 128

enum task_status {
  BLOCKED = 0,
  RUNNABLE = 1,
  RUNNING = 2,
  DELETED = 3
};

struct task {
  int status;
  const char* name;
  struct task* next;
  Context* context;
  void* stack;
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

