#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>

#define MAX_PATHS 100
#define MAX_ARGVS 100
#define MAX_FILENAME 64
#define MAX_SYSCALL 100
#define MAX_BUFFER 512
#define MAX_ENVP 1024

//! syscall_t
typedef struct syscall_t {
  char name[64];
  double time;
}syscall_t;
int syscall_compare(const void* a, const void* b) {
  const syscall_t* syscallA = (const syscall_t*)a;
  const syscall_t* syscallB = (const syscall_t*)b;
  return (syscallA->time > syscallB->time) - (syscallA->time < syscallB->time);
}

syscall_t syscalls[MAX_SYSCALL];
int syscall_count;
double total_time;
double output_time;

//! path_env
char* env_path[MAX_PATHS];
// char envp[10][MAX_ENVP];
char* args[MAX_ARGVS];
char file_path[2][MAX_FILENAME];

void fetch_path_env() {
  char* path_env = getenv("PATH");
  // snprintf(envp[0], 1024, "PATH=%s", path_env);
  char* path = strtok(path_env, ":");
  int path_count = 0;
  while (path != NULL && path_count < MAX_PATHS) {
    env_path[path_count] = path;
    path = strtok(NULL, ":");
    path_count++;
  }
}


void display_sperf() {
  qsort(syscalls, syscall_count, sizeof(syscall_t), syscall_compare);
  printf("%d\n", syscall_count);
  for (int i = 0; i < syscall_count; i++) {
    printf("%s : %lf\n", syscalls[i].name, syscalls[i].time);
  }
  fflush(stdout);
}

void fetch_strace_info(int fd) {
  char buffer[MAX_BUFFER];
  FILE* pipe_stream = fdopen(fd, "r");

  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLIN;

  int result = poll(&pfd, 1, 1000);  // 使用1000毫秒（1秒）超时
  if (result == -1) {
    perror("poll");
    exit(EXIT_FAILURE);
  }

  if (result)
    while (fgets(buffer, MAX_BUFFER, pipe_stream) != NULL) {
      char syscall_name[64];
      double time;
      if (sscanf(buffer, "%63[^'(](%*[^<]<%lf>)", syscall_name, &time) == 2) {
        int exist = 0;
        total_time += time;
        for (int i = 0; i < syscall_count; i++) {
          if (strcmp(syscalls[i].name, syscall_name) == 0) {
            syscalls[i].time += time;
            exist = 1;
          }
        }
        if (!exist) {
          strcpy(syscalls[syscall_count].name, syscall_name);
          syscalls[syscall_count].time = time;
          syscall_count++;
        }
      }
    }
  else display_sperf();
  fclose(pipe_stream);
}

char* fetch_command(char* name) {
  static int counter = -1;
  counter++;
  if (name[0] == '/') return name;
  for (int i = 0; env_path[i]; i++) {
    snprintf(file_path[counter], sizeof(file_path[counter]), "%s/%s", env_path[i], name);
    if (access(file_path[counter], F_OK) == 0)
      return file_path[counter];
  }
  perror(name);
  exit(EXIT_FAILURE);
}

void fetch_strace_argv(int argc, char* argv[]) {
  fetch_path_env();
  args[0] = fetch_command("strace");
  args[1] = "-T";
  args[2] = fetch_command(argv[1]);
  for (int i = 2; i < argc; i++) args[i + 1] = argv[i];
}



int main(int argc, char* argv[]) {
  char* exec_argv[] = { "strace", "-T", "ls", NULL, };
  char* exec_envp[] = { "PATH=/bin", NULL, };
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }
  int flags = fcntl(pipefd[0], F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL");
    exit(EXIT_FAILURE);
  }

  flags |= O_NONBLOCK;
  if (fcntl(pipefd[0], F_SETFL, flags) == -1) {
    perror("fcntl F_SETFL");
    exit(EXIT_FAILURE);
  }
  pid_t pid = fork();
  if (pid == 0) {
    int fd = open("/dev/null", O_WRONLY);
    close(pipefd[0]);
    if (dup2(pipefd[1], STDERR_FILENO) == -1) {
      perror("dup2");
      exit(EXIT_FAILURE);
    }
    close(pipefd[1]);
    fetch_strace_argv(argc, argv);
    fflush(stdout);
    dup2(fd, STDOUT_FILENO);
    close(fd);
    execve(args[0], args, NULL);
    perror("execve");
    exit(EXIT_FAILURE);
  }
  else {
    close(pipefd[1]);
    fetch_strace_info(pipefd[0]);
    close(pipefd[0]);
  }
}
