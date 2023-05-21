#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "thread.h"
#include "thread-sync.h"

#define MAXN 10000
int T, N, M;
char A[MAXN + 1], B[MAXN + 1];
short dp[MAXN][MAXN];

#if __DEBUG__MODE_
FILE* fp;
#define debug(...) fprintf(fp, __VA_ARGS__)
#else
#define debug(...)  
#endif

#define DP(x, y) (((x) >= 0 && (y) >= 0) ? dp[x][y] : 0)
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX3(x, y, z) MAX(MAX(x, y), z)

// thread define
cond_t cv = COND_INIT();
mutex_t lk = MUTEX_INIT();

// worker define [l, r) default step is 1024
#define STEP 1024
typedef struct info {
  short row, l, r;
}Work;
Work workload[MAXN];
bool kill_signal;
bool busy[MAXN];
bool inqueue[MAXN];
short progress[MAXN];

// queue API
short head, tail;
Work queue[MAXN << 1];

bool queue_size() {
  return head != tail;
}
void print_queue() {
  short my_head = head;
  short my_tail = tail;
  while (my_head != my_tail) {
    debug("{%d %d %d} ", queue[my_head].row, queue[my_head].l, queue[my_head].r);
    if (++my_head == MAXN << 1) my_head = 0;
  }
  debug("\n");
}
bool queue_push(Work work) {
  if (inqueue[work.row] || busy[work.row]) {
    debug("{%d %d %d} push failed\n", work.row, work.l, work.r);
    return false;
  }
  inqueue[work.row] = true;
  queue[tail] = work;
  if (++tail == MAXN << 1) tail = 0;
  return true;
}
Work queue_pop() {
  Work front = queue[head];
  inqueue[front.row] = false;
  if (++head == MAXN << 1) head = 0;
  return front;
}


#define WORKER_COND (queue_size() || kill_signal)

void allocate_work(Work work) {
  short bound = work.row ? progress[work.row - 1] : M;
  Work right = (Work){ .row = work.row, .l = work.r, .r = MIN(bound, work.r + STEP) };
  Work down = (Work){ .row = work.row + 1, .l = progress[work.row + 1], .r = MIN(M, progress[work.row + 1] + STEP) };
  if (work.row == N - 1 && work.r == M) return;
  else if (work.row == N - 1) {
    debug("work {%d %d %d} allocate {%d %d %d}\n", work.row, work.l, work.r, right.row, right.l, right.r);
    if (right.l < right.r)
      queue_push(right);
  }
  else if (work.r == M) {
    debug("work {%d %d %d} allocate {%d %d %d}\n", work.row, work.l, work.r, down.row, down.l, down.r);
    if (down.l < down.r)
      queue_push(down);
  }
  else {
    debug("work {%d %d %d} allocate {%d %d %d}\n", work.row, work.l, work.r, right.row, right.l, right.r);
    debug("work {%d %d %d} allocate {%d %d %d}\n", work.row, work.l, work.r, down.row, down.l, down.r);
    if (right.l < right.r)
      queue_push(right);
    if (down.l < down.r)
      queue_push(down);
  }
  return;
}
void check() {
  for (int i = 0; i < N - 1; i++) {
    if (progress[i] < progress[i + 1]) {
      debug("%d: %d fuck %d: %d\n", i, progress[i], i + 1, progress[i + 1]);
      assert(0);
    }
  }
}

void Tworker(int id) {
  Work thread_work;
  while (1) {
    mutex_lock(&lk);
    while (!WORKER_COND) {
      debug("worker %d sleep \n", id);
      cond_wait(&cv, &lk);
      debug("worker %d awake \n", id);
    }
    debug("worker %d locked\n", id);
    if (kill_signal) {
      debug("worker %d killed\n", id);
      debug("worker %d unlock\n", id);
      cond_broadcast(&cv);
      mutex_unlock(&lk);
      break;
    }
    thread_work = queue_pop();
    //print_queue();
    busy[thread_work.row] = true;
    if (thread_work.row == N - 1 && thread_work.r == M) {
      kill_signal = 1;
      debug("kill signal is send by worker %d\n", id);
    }
    debug("worker %d unlock: {%d %d %d}\n", id, thread_work.row, thread_work.l, thread_work.r);
    mutex_unlock(&lk);

    short i = thread_work.row;
    for (short j = thread_work.l; j < thread_work.r; j++) {
      short skip_a = DP(i - 1, j);
      short skip_b = DP(i, j - 1);
      short take_both = DP(i - 1, j - 1) + (A[i] == B[j]);
      dp[i][j] = MAX3(skip_a, skip_b, take_both);
    }

    mutex_lock(&lk);
    busy[thread_work.row] = false;
    progress[thread_work.row] = thread_work.r;
    allocate_work(thread_work);
    cond_broadcast(&cv);
    debug("worker %d finished\n", id);
    //print_queue();
    //check();
    mutex_unlock(&lk);
  }
}

int main(int argc, char* argv[]) {
#ifdef __DEBUG__MODE_
  fp = freopen("log.txt", "w", stderr);
  setbuf(stdout, NULL);
#endif

  // No need to change
  assert(scanf("%s%s", A, B) == 2);
  N = strlen(A);
  M = strlen(B);
  T = !argv[1] ? 1 : atoi(argv[1]);
  // Add preprocessing code here
  if (T == 1) usleep(300000);

  queue_push((Work) { 0, 0, MIN(STEP, M) });
  for (int i = 0; i < T; i++) {
    create(Tworker);
  }
  join();  // Wait for all workers
  printf("%d\n", dp[N - 1][M - 1]);
#ifdef __DEBUG__MODE_
  fclose(fp);
#endif
  return 0;
}
