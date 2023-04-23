#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_PATHS 100
#define MAX_ARGVS 100
#define MAX_FILENAME 64
#define MAX_SYSCALL 100
#define MAX_BUFFER 512
#define MAX_ENVP 10

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

//! path_env
char* env_path[MAX_PATHS];
char* envp[MAX_ENVP];
char* args[MAX_ARGVS];
char file_path[MAX_FILENAME];

void fetch_path_env() {
  char* path_env = getenv("PATH");
  snprintf(envp[0], 1024, "PATH=%s", path_env);

  char* path = strtok(path_env, ":");
  int path_count = 0;
  while (path != NULL && path_count < MAX_PATHS) {
    env_path[path_count] = path;
    path = strtok(NULL, ":");
    path_count++;
  }
}

void fetch_strace_info(int fd) {
  char buffer[MAX_BUFFER];
  FILE* pipe_stream = fdopen(fd, "r");
  if (pipe_stream == NULL) printf("error.\n");
  while (fgets(buffer, MAX_BUFFER, pipe_stream) != NULL) {
    char syscall_name[64];
    double time;
    if (sscanf(buffer, "%63[^'(]%*[^(](%*[^<]<%lf>)", syscall_name, &time) == 2) {
      int exist = 0;
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
  fclose(pipe_stream);
}

char* fetch_command(char* name) {
  if (name[0] == '/') return name;
  for (int i = 0; env_path[i]; i++) {
    snprintf(file_path, sizeof(file_path), "%s/%s", env_path[i], name);
    if (access(file_path, F_OK) == 0)
      return file_path;
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

void display_sperf() {
  qsort(syscalls, syscall_count, sizeof(syscall_t), syscall_compare);
  for (int i = 0; i < syscall_count; i++) {
    printf("%s : %lf\n", syscalls[i].name, syscalls[i].time);
  }
}

int main(int argc, char* argv[]) {
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }
  pid_t pid = fork();
  if (pid == 0) {
    // close(pipefd[0]);
    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
      perror("dup2");
      exit(EXIT_FAILURE);
    }
    // close(pipefd[1]);
    fetch_strace_argv(argc, argv);
    execve("strace", args, envp);
    perror("execve");
    exit(EXIT_FAILURE);
  }
  else {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
      printf("strace exit with %d\n", WIFEXITED(status));
    }
    else {
      printf("strace exit with some error.\n");
    }
    // close(pipefd[1]);
    fetch_strace_info(pipefd[0]);
    // close(pipefd[0]);
    display_sperf();
  }
}
