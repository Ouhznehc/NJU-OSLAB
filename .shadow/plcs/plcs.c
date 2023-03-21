#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "thread.h"
#include "thread-sync.h"

#define MAXN 10000
int T, N, M;
char A[MAXN + 1], B[MAXN + 1];
int dp[MAXN][MAXN];
int result;

mutex_t lk = MUTEX_INIT();
cond_t thread = COND_INIT();
cond_t global = COND_INIT();
int consent[MAXN][MAXN];
int global_x, global_y;
int round_cnt;

#define DP(x, y) (((x) >= 0 && (y) >= 0) ? dp[x][y] : 0)
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX3(x, y, z) MAX(MAX(x, y), z)
#define THREAD_COND consent[global_x][global_y]
#define GLOBAL_COND round_cnt


void Tworker(int id) {
  int thread_x, thread_y;
  while(1){
    mutex_lock(&lk);
    while(!THREAD_COND)
      cond_wait(&thread, &lk);
    thread_x = global_x;
    thread_y = global_y;
    consent[global_x][global_y] = 0;
    consent[global_x - 1][global_y + 1] = 1;
    global_x--; global_y++;
    cond_broadcast(&thread);
    mutex_unlock(&lk);
    int skip_a = DP(thread_x - 1, thread_y);
    int skip_b = DP(thread_x, thread_y - 1);
    int take_both = DP(thread_x - 1, thread_y - 1) + (A[thread_x] == B[thread_y]);
    dp[thread_x][thread_y] = MAX3(skip_a, skip_b, take_both);
    round_cnt--;
    if(!round_cnt) cond_broadcast(&global);
  }

}

int main(int argc, char *argv[]) {
  // No need to change
  assert(scanf("%s%s", A, B) == 2);
  N = strlen(A);
  M = strlen(B);
  T = !argv[1] ? 1 : atoi(argv[1]);

  // Add preprocessing code here

  for (int i = 0; i < T; i++) {
    create(Tworker); 
  }
  for(int round = 0; round < N + M - 1; round++){
    mutex_lock(&lk);
    while(!GLOBAL_COND)
      cond_wait(&global, &lk);
    if(round < N){
      global_x = round; 
      global_y = 0;
    }
    else{
      global_x = N - 1;
      global_y = round - N + 1;
    }
    round_cnt = MIN(global_x, M - global_y);
    consent[global_x][global_y] = 1;
    cond_broadcast(&thread);
    mutex_unlock(&lk);
  }

  join();  // Wait for all workers

  printf("%d\n", result);
}
