#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define MAX_SYS_CALLS 1024
#define TOP_COUNT 5

typedef struct SysCall {
  char name[64];
  double totalTime;
} SysCall;

int sys_call_compare(const void* a, const void* b) {
  const SysCall* syscallA = (const SysCall*)a;
  const SysCall* syscallB = (const SysCall*)b;

  return (syscallA->totalTime < syscallB->totalTime) - (syscallA->totalTime > syscallB->totalTime);
}

int main(int argc, char* argv[], char* envp[]) {
  if (argc < 2) {
    printf("Usage: %s program [args]\n", argv[0]);
    exit(1);
  }

  int pipefd[2];
  if (pipe(pipefd) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  }
  else if (pid == 0) {
    close(pipefd[0]);

    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
      perror("dup2");
      exit(EXIT_FAILURE);
    }

    close(pipefd[1]);

    char* straceArgs[argc + 3];
    straceArgs[0] = "strace";
    straceArgs[1] = "-T";
    for (int i = 1; i < argc; i++) {
      straceArgs[i + 1] = argv[i];
    }
    straceArgs[argc + 1] = NULL;

    execve("/usr/bin/strace", straceArgs, envp);
    perror("execve");
    exit(EXIT_FAILURE);
  }
  else {
    close(pipefd[1]);

    SysCall sysCalls[MAX_SYS_CALLS] = { 0 };
    int sysCallCount = 0;

    char buffer[BUFFER_SIZE];
    FILE* pipeStream = fdopen(pipefd[0], "r");

    while (fgets(buffer, BUFFER_SIZE, pipeStream) != NULL) {
      char name[64];
      double time;

      if (sscanf(buffer, "%63[^'(]%*[^(](%*[^<]<%lf>)", name, &time) == 2) {
        int found = 0;
        for (int i = 0; i < sysCallCount; i++) {
          if (strcmp(sysCalls[i].name, name) == 0) {
            sysCalls[i].totalTime += time;
            found = 1;
            break;
          }
        }

        if (!found && sysCallCount < MAX_SYS_CALLS) {
          strncpy(sysCalls[sysCallCount].name, name, sizeof(name) - 1);
          sysCalls[sysCallCount].totalTime = time;
          sysCallCount++;
        }
      }
    }

    fclose(pipeStream);
    close(pipefd[0]);

    qsort(sysCalls, sysCallCount, sizeof(SysCall), sys_call_compare);
    fflush(stdout);
    printf("Top %d system calls by time:\n", TOP_COUNT);
  }
  return 0;
}
