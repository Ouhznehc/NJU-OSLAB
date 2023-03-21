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
int global_x, gobal_y;

#define DP(x, y) (((x) >= 0 && (y) >= 0) ? dp[x][y] : 0)
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MAX3(x, y, z) MAX(MAX(x, y), z)
#define COND consent[global_x][gobal_y]


void Tworker(int id) {
  int thread_x, thread_y;
  while(1){
    mutex_lock(&lk);
    while(!COND)
      cond_wait(&thread, &lk);
    
    cond_broadcast(&thread);
    mutex_unlock(&lk);

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

  }

  join();  // Wait for all workers

  printf("%d\n", result);
}
