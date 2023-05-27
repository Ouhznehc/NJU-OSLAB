#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
  pid_t pid = fork();

  if (pid == -1) {
    // fork 失败
    printf("Failed to fork.\n");
    return 1;
  }

  if (pid == 0) {
    // 子进程执行编译命令
    execlp("gcc", "gcc", "-shared", "-fPIC", "source.c", "-o", "output", NULL);
    // 如果 execlp 函数执行失败，则表示无法执行编译命令
    exit(1);
  }
  else {
    // 父进程等待子进程完成
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
      // 子进程正常退出
      int exit_status = WEXITSTATUS(status);
      if (exit_status == 0) {
        // 编译成功
        printf("Compilation succeeded.\n");
      }
      else {
        // 编译失败
        printf("Compilation failed. Exit status: %d\n", exit_status);
      }
    }
    else {
      // 子进程异常退出
      printf("Compilation failed. Abnormal termination.\n");
    }
  }

  return 0;
}
