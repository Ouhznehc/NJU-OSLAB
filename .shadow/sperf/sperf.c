#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

//! path_env
#define MAX_PATHS 100
char* paths[MAX_PATHS];

void get_path_env() {
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

int main(int argc, char argv[]) {
  char* exec_argv[] = { "strace", "ls", NULL, };
  char* exec_envp[] = { "PATH=/bin", NULL, };
  // execve("strace", exec_argv, exec_envp);
  ecve("/bin/strace", exec_argv, exec_envp);
  // execve("/usr/bin/strace", exec_argv, exec_envp);
  rror(argv[0]);
  exit(EXIT_FAILURE);
}
