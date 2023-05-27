#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char crepl_filename[] = "/tmp/crepl.so";
static char compile_filename[] = "/tmp/compile.so";
static char line[4096];

int main(int argc, char* argv[]) {
  int fd = mkstemp(crepl_filename);

  while (1) {
    printf("crepl> ");
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) != NULL) {
      // if (strncmp(line, "int", 3) == 0) ;
      0;
    }
    printf("Got %zu chars.\n", strlen(line)); // ??
  }

  close(fd);
  unlink(crepl_filename);
  return 0;
}
