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
static int is_valid;
static int rc;

static char crepl_filename[] = "/tmp/crepl_XXXXXXX.c";
static char compile_filename[] = "/tmp/compile_XXXXXXX.c";

static int compile_fd;
static int crepl_fd;

static FILE* compile_file;
static FILE* crepl_file;


static void copy_shared_lib(char* src_filename, char* dst_filename) {
  FILE* src_file = fopen(src_filename, "r");
  FILE* dst_file = fopen(dst_filename, "w");

  assert(src_file != NULL && dst_file != NULL);

  char string[4096];
  while (fgets(string, sizeof(string), src_file) != NULL) fputs(string, dst_file);

  fclose(src_file);
  fclose(dst_file);
}

static int compile_new_lib(char* lib_filename, char* code) {
  FILE* lib_file = fopen(lib_filename, "a");
  assert(lib_file != NULL);
  int ret = fprintf(lib_file, "%s", code);
  assert(ret >= 0);
  fclose(lib_file);

  pid_t pid = fork();
  if (pid == 0) {
    fclose(stderr);
    fclose(stdout);
    if (sizeof(void*) == 4)
      execlp("gcc", "gcc", "-m32", "-shared", "-Werror", "-fPIC", lib_filename, "-o", "/tmp/compile.so", NULL);
    else
      execlp("gcc", "gcc", "-m64", "-shared", "-Werror", "-fPIC", lib_filename, "-o", "/tmp/compile.so", NULL);
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
  FILE* lib_file = fopen(crepl_filename, "a");
  assert(lib_file != NULL);
  int ret = fprintf(lib_file, "%s", code);
  assert(ret >= 0);
  fclose(lib_file);
}

static int compile_shared_function(char* code) {
  copy_shared_lib(crepl_filename, compile_filename);
  int ret = compile_new_lib(compile_filename, code);
  if (ret) update_shared_lib(code);
  return ret;
}

static int compile_shared_lib(char* code) {
  is_expression = 0;
  int ret = compile_shared_function(code);
  return ret;
}

static int compile_expression_with_lib(char* lib_filename, char* expression) {
  FILE* lib_file = fopen(lib_filename, "a");
  assert(lib_file != NULL);
  fprintf(lib_file, "int __expr_wrapper__(){\n");
  fprintf(lib_file, "return %s;\n", expression);
  fprintf(lib_file, "}\n");
  fclose(lib_file);

  pid_t pid = fork();
  if (pid == 0) {
    fclose(stderr);
    fclose(stdout);
    if (sizeof(void*) == 4)
      execlp("gcc", "gcc", "-m32", "-shared", "-Werror", "-fPIC", lib_filename, "-o", "/tmp/compile.so", NULL);
    else
      execlp("gcc", "gcc", "-m64", "-shared", "-Werror", "-fPIC", lib_filename, "-o", "/tmp/compile.so", NULL);
    exit(1);
  }
  else {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 1;
    else return 0;
  }
}

static int compile_expression(char* expression) {
  copy_shared_lib(crepl_filename, compile_filename);
  int ret = compile_expression_with_lib(compile_filename, expression);
  return ret;
}

static int fetch_expression_value_with_lib() {
  void* handle = dlopen("/tmp/compile.so", RTLD_NOW);
  assert(handle != NULL);
  typedef int (*expression_func)();
  char function_name[128];
  sprintf(function_name, "__expr_wrapper__");
  expression_func function = (expression_func)dlsym(handle, function_name);
  int ret = function();
  dlclose(handle);
  return ret;
}

static int fetch_expression_value(char* expression) {
  is_expression = 1;
  is_valid = compile_expression(expression);

  if (!is_valid) return 0;
  else return fetch_expression_value_with_lib();
}

int main(int argc, char* argv[]) {
  crepl_fd = mkstemps(crepl_filename, 2);
  compile_fd = mkstemps(compile_filename, 2);
  while (1) {
    printf("crepl> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) break;

    if (strncmp(line, "int", 3) == 0) rc = compile_shared_lib(line);
    else rc = fetch_expression_value(line);

    if (is_expression) {
      if (is_valid) printf("%d\n", rc);
      else printf("Invalid expression.\n");
    }
    else printf("Compile %s.\n", rc ? "ok" : "error");
  }
  return 0;
}
