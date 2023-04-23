#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

//! syscall_t
#define MAX_SYSCALL 100
typedef struct syscall_t {
  char name[64];
  double time;
}syscall_t;
int syscall_compare(const void* a, const void* b) {
  const syscall_t* syscallA = (const syscall_t*)a;
  const syscall_t* syscallB = (const syscall_t*)b;
  return syscallA->time < syscallB->time;
}

syscall_t syscalls[MAX_SYSCALL];


//! path_env
#define MAX_PATHS 100
char* paths[MAX_PATHS];

void fetch_path_env() {
  char* path_env = getenv("PATH");
  char* path = strtok(path_env, ":");
  int path_count = 0;
  while (path != NULL && path_count < MAX_PATHS) {
    paths[path_count] = path;
    path = strtok(NULL, ":");
    path_count++;
  }
  paths[path_count] = NULL;
}





int main(int argc, char* argv[]) {

  fetch_path_env();

}
