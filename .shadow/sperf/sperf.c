#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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
int try() {
  int a[100];
  for (int i = 0; i < 100; i++) {
    a[i] = 1;
  }
  return a[99];
}

int main(int argc, char* argv[]) {

  fetch_path_env();
  try();
  int i = 0;
  while (paths[i] != NULL) {
    printf("%s\n", paths[i]);
    i++;
  }
  char* exec_argv[] = { "strace", "ls", NULL, };
  char* exec_envp[] = { "PATH=/bin", NULL, };
  // execve("strace", exec_argv, exec_envp);
  // execve("/bin/strace", exec_argv, exec_envp);
  execve("/usr/bin/strace", exec_argv, exec_envp);
  perror(argv[0]);
  exit(EXIT_FAILURE);
}
