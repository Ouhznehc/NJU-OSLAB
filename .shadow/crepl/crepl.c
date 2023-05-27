#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>

static char crepl_filename[] = "/tmp/crepl.so";
static char compile_filename[] = "/tmp/compile.so";
static char line[4096];
static int is_expression;
static int rc;

static int compile_shared_lib(char* line) {
  return 1;
}

static int fetch_expression_value(char* line) {
  return 0;
}

int main(int argc, char* argv[]) {
  int fd = mkstemp(crepl_filename);

  while (1) {
    printf("crepl> ");
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) != NULL) {
      if (strncmp(line, "int", 3) == 0) rc = compile_shared_lib(line);
      else rc = fetch_expression_value(line);
    }
    if (is_expression) printf("= %d\n", rc);
    else printf("Compile %s.\n", rc ? "OK" : "error");
  }

  close(fd);
  unlink(crepl_filename);
  return 0;
}
