#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <assert.h>
#include <regex.h>

#define MAX_PATHS 1000
#define MAX_ARGVS 1000
#define MAX_FILENAME 64
#define MAX_SYSCALL 100
#define MAX_BUFFER 512
#define MAX_ENVP 1024
#define MIN(a, b) ((a) < (b) ? (a) : (b))

//! syscall_t
typedef struct syscall_t {
  char name[64];
  double time;
}syscall_t;
int syscall_compare(const void* a, const void* b) {
  const syscall_t* syscallA = (const syscall_t*)a;
  const syscall_t* syscallB = (const syscall_t*)b;
  return (syscallA->time < syscallB->time) - (syscallA->time > syscallB->time);
}

syscall_t syscalls[MAX_SYSCALL];
int syscall_count;
double total_time;
int display_time;

//! path_env
char* env_path[MAX_PATHS];
char* args[MAX_ARGVS];
char file_path[2][MAX_FILENAME];
extern char** environ;
char path_env[2048];

void fetch_path_env() {
  char* path_environ = getenv("PATH");
  strcpy(path_env, path_environ);
  char* path = strtok(path_env, ":");
  int path_count = 0;
  while (path != NULL && path_count < MAX_PATHS) {
    env_path[path_count] = path;
    path = strtok(NULL, ":");
    path_count++;
  }
  // for (int i = 0; i < path_count; i++) {
  //   printf("%s\n", env_path[i]);
  // }
}


void display_sperf() {
  qsort(syscalls, syscall_count, sizeof(syscall_t), syscall_compare);
  printf("Time: %ds\n", display_time);
  for (int i = 0; i < MIN(syscall_count, 5); i++) {
    int ratio = (int)((syscalls[i].time / total_time) * 100);
    printf("%s (%d%%)\n", syscalls[i].name, ratio);
  }
  printf("================\n");
  for (int i = 0; i < 80; i++) putchar('\0');
  fflush(stdout);
}

void fetch_strace_info(int fd, int pid) {
  char buffer[MAX_BUFFER];
  FILE* pipe_stream = fdopen(fd, "r");
  time_t start_time = time(NULL);
  while (waitpid(pid, NULL, WNOHANG) == 0) {
    while (fgets(buffer, MAX_BUFFER, pipe_stream) != NULL) {
      char syscall_name[64];
      double syscall_time;
      if (sscanf(buffer, "%[^(](%*[^<]<%lf>)", syscall_name, &syscall_time) == 2) {
        // printf("%s : %lf\n", syscall_name, time);

        int exist = 0;
        total_time += syscall_time;
        for (int i = 0; i < syscall_count; i++) {
          if (strcmp(syscalls[i].name, syscall_name) == 0) {
            syscalls[i].time += syscall_time;
            exist = 1;
          }
        }
        if (!exist) {
          strcpy(syscalls[syscall_count].name, syscall_name);
          syscalls[syscall_count].time = syscall_time;
          syscall_count++;
        }
      }
      if (difftime(time(NULL), start_time) >= 1.0) {
        start_time = time(NULL);
        display_time++;
        display_sperf();
      }
    }
    if (display_time == 0) display_sperf();

  }
  fclose(pipe_stream);
}

char* fetch_command(char* name) {
  static int counter = -1;
  counter++;
  if (name[0] == '/') return name;
  for (int i = 0; env_path[i]; i++) {
    snprintf(file_path[counter], sizeof(file_path[counter]), "%s/%s", env_path[i], name);
    if (access(file_path[counter], X_OK) == 0)
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

  int pipefd[2];
  pipe(pipefd);

  pid_t pid = fork();
  if (pid == 0) {
    int fd = open("/dev/null", O_WRONLY);
    close(pipefd[0]);
    dup2(pipefd[1], STDERR_FILENO);
    dup2(fd, STDOUT_FILENO);
    close(pipefd[1]);
    close(fd);
    fetch_strace_argv(argc, argv);

    // fflush(stdout);
    execve(args[0], args, environ);
    perror("execve");
    exit(EXIT_FAILURE);
  }
  else {

    close(pipefd[1]);
    fetch_strace_info(pipefd[0], pid);
    close(pipefd[0]);
  }
}
