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
  FILE *fp;
  #define debug(...) printf(__VA_ARGS__)
#else
  #define debug(...)  
#endif

#define DP(x, y) (((x) >= 0 && (y) >= 0) ? dp[x][y] : 0)
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX3(x, y, z) MAX(MAX(x, y), z)

// thread define
cond_t cv  = COND_INIT();
mutex_t lk = MUTEX_INIT();

// worker define [l, r)
typedef struct info{
  short row, l, r;
}Work;
Work workload[MAXN];
bool kill_signal;
bool busy[MAXN];
bool inqueue[MAXN];
short debug_dp[MAXN][MAXN][MAXN];
// queue API
short head, tail;
Work queue[MAXN << 1];

bool queue_size(){
  return head != tail;
}
bool queue_push(Work work){
  if(inqueue[work.row] || busy[work.row]) return false;
  queue[tail] = work;
  if (++tail == MAXN << 1) tail = 0;
  return true;
}
Work queue_pop(){
  Work front = queue[head];
  inqueue[front.row] = false;
  if (++head == MAXN << 1) head = 0;
  return front;
}



#define WORKER_COND (queue_size() || kill_signal)

void allocate_work(Work work){
  short len = MIN(1024, M - work.r);
  if(work.row == N - 1 && work.r == M) return;
  else if(work.row == N - 1) queue_push((Work){work.row, work.r, work.r + len});
  else if(work.r == M) queue_push((Work){work.row + 1, work.l, work.r});
  else {
    queue_push((Work){work.row, work.r, work.r + len});
    queue_push((Work){work.row + 1, work.l, work.r});
  }
  return;
}


void Tworker(int id) {
  Work thread_work;
  while(1){
    mutex_lock(&lk);
    while(!WORKER_COND){
      debug("worker %d sleep \n", id);
      cond_wait(&cv, &lk);
      debug("worker %d awake \n", id);
    }
    debug("worker %d locked\n", id);
    if(kill_signal){
      debug("worker %d killed\n", id);
      debug("worker %d unlock\n", id);
      cond_broadcast(&cv);
      mutex_unlock(&lk);
      break;
    }
    thread_work = queue_pop();
    busy[thread_work.row] = true;
    if(thread_work.row == N - 1 && thread_work.r == M) {
      kill_signal = 1;
      debug("kill signal is send by worker %d\n", id);
    }
    debug("worker %d unlock: row = %d, l = %d, r = %d\n", id, thread_work.row, thread_work.l, thread_work.r);
    mutex_unlock(&lk);

    short i = thread_work.row;
    for(short j = thread_work.l; j < thread_work.r; j++){
        short skip_a = DP(i - 1, j);
        short skip_b = DP(i, j - 1);
        short take_both = DP(i - 1, j - 1) + (A[i] == B[j]);
        dp[i][j] = MAX3(skip_a, skip_b, take_both);
    }

    mutex_lock(&lk);
    busy[thread_work.row] = false;
    allocate_work(thread_work);
    cond_broadcast(&cv);
    debug("worker %d finished\n", id);
    mutex_unlock(&lk);
  }
}

int main(int argc, char *argv[]) {
  #ifdef __DEBUG__MODE_
    fp = fopen("log.txt", "w");
    setbuf(stdout, NULL);
  #endif

  // No need to change
  assert(scanf("%s%s", A, B) == 2);
  N = strlen(A);
  M = strlen(B);
  T = !argv[1] ? 1 : atoi(argv[1]);
  // Add preprocessing code here


  queue_push((Work){0, 0, MIN(1024, M)});
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
