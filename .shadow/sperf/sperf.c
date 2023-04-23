#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define MAX_PATHS 100
int main(int argc, char* argv[]) {
  // char* path_env = getenv("PATH");
  // char* path = strtok(path_env, ":");
  // char* paths[MAX_PATHS]; // 存储路径的数组
  // int path_count = 0; // 计数器

  // // 将每个路径存储到数组中
  // while (path != NULL && path_count < MAX_PATHS) {
  //   paths[path_count] = path;
  //   path = strtok(NULL, ":");
  //   path_count++;
  // }

  // // 输出每个路径
  // for (int i = 0; i < path_count; i++) {
  //   printf("%s\n", paths[i]);
  // }
  char* exec_argv[] = { "strace", "ls", NULL, };
  char* exec_envp[] = { "PATH=/bin", NULL, };
  execve("strace", exec_argv, exec_envp);
  // execve("/bin/strace", exec_argv, exec_envp);
  // execve("/usr/bin/strace", exec_argv, exec_envp);
  perror(argv[0]);
  exit(EXIT_FAILURE);
}
