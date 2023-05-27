#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <assert.h>



static char line[4096];
static int is_expression;
static int rc;

static char crepl_filename[] = "/tmp/creplXXXXXX";
static char compile_filename[] = "/tmp/compileXXXXXX";
static int compile_fd;
static int crepl_fd;
static FILE* compile_file;
static FILE* crepl_file;

static void copy_shared_lib(FILE* src_file, FILE* dst_file) {
  assert(src_file != NULL && dst_file != NULL);
  char string[4096];
  while (fgets(string, sizeof(string), src_file) != NULL) fputs(string, dst_file);
}

static int compile_new_lib(FILE* lib_file, char* code) {
  assert(lib_file != NULL);
  fprintf(lib_file, "%s", code);

  pid_t pid = fork();
  if (pid == 0) {
    execlp("gcc", "gcc", "-shared", "-fPIC", compile_filename, "-o", "/tmp/compile.so", NULL);
    exit(1);
  }
  else {
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
}

static void update_shared_lib(char* code) {
  FILE* lib_file = fdopen(crepl_fd, "w");
  fprintf(lib_file, "%s", code);

  pid_t pid = fork();
  if (pid == 0) execlp("gcc", "gcc", "-shared", "-fPIC", compile_filename, "-o", "/tmp/crepl.so", NULL);
  else {
    int status;
    waitpid(pid, &status, 0);
    return;
  }
}

static int compile_shared_function(char* code) {
  copy_shared_lib(crepl_file, compile_file);
  int ret = compile_new_lib(crepl_file, code);
  if (ret) update_shared_lib(code);
  return ret;
}

static int compile_shared_lib(char* code) {
  is_expression = 0;
  int ret = compile_shared_function(code);
  return ret;
}

static int fetch_expression_value(char* expression) {
  is_expression = 1;
  return 0;
}

int main(int argc, char* argv[]) {
  crepl_fd = mkstemp(crepl_filename);
  crepl_file = fdopen(crepl_fd, "w");

  compile_fd = mkstemp(compile_filename);
  compile_file = fdopen(compile_fd, "w");

  while (1) {
    printf("crepl> ");
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) != NULL) {
      if (strncmp(line, "int", 3) == 0) rc = compile_shared_lib(line);
      else rc = fetch_expression_value(line);
    }
    if (is_expression) printf("= %d\n", rc);
    else printf("Compile %s.\n", rc ? "ok" : "error");
  }
  fclose(crepl_file);
  fclose(compile_file);
  unlink(crepl_filename);
  unlink(compile_filename);
  return 0;
}
