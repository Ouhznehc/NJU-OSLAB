#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <assert.h>


static char crepl_filename[] = "/tmp/crepl.c";
static char compile_filename[] = "/tmp/compile.c";
static char line[4096];
static int is_expression;
static int rc;

static int compile_fd;
static int crepl_fd;

static void copy_shared_lib(int src, int dst) {
  FILE* src_file = fdopen(src, "r");
  FILE* dst_file = fdopen(dst, "w");
  assert(src_file != NULL && dst_file != NULL);
  char string[4096];
  while (fgets(string, sizeof(string), src_file) != NULL) fputs(string, dst_file);
  fclose(src_file);
  fclose(dst_file);
}

static int compile_new_lib(char* code) {
  FILE* lib_file = fdopen(compile_fd, "w");
  fprintf(lib_file, "%s", code);
  fclose(lib_file);

  pid_t pid = fork();
  if (pid == 0) {
    execlp("gcc", "gcc", "-shared", "-fPIC", compile_filename, NULL);
    exit(1);
  }
  else {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 1;
    else return 0;
  }
}

static void update_shared_lib(char* code) {
  FILE* lib_file = fdopen(crepl_fd, "w");
  fprintf(lib_file, "%s", code);
  fclose(lib_file);

  pid_t pid = fork();
  if (pid == 0) execlp("gcc", "gcc", "-shared", "-fPIC", compile_filename, "-o", "/tmp/crepl.so", NULL);
  else {
    int status;
    waitpid(pid, &status, 0);
    return;
  }
}

static int compile_shared_function(char* code) {
  compile_fd = mkstemp(compile_filename);
  copy_shared_lib(crepl_fd, compile_fd);
  int ret = compile_new_lib(code);
  if (ret) update_shared_lib(code);
  unlink(compile_filename);
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

  unlink(crepl_filename);
  return 0;
}
