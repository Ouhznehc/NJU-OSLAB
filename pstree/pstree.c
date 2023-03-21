#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>


#define MAX_STRING 512
#define MAX_PROCESS 32768
#define MAX_CHILD 512
static int root = 0;
enum{OPT_NONE = 0x0, OPT_V = 0x7, OPT_N = 0x2, OPT_P = 0x1, OPT_NP = 0x3, OPT_INVALID = 0xf};
struct process_info
{
  char name[MAX_STRING];
  int pid;
  int ppid;
  int child_process_cnt;
  struct process_info *child_process[MAX_CHILD];
}Process[MAX_PROCESS];

int error_msg(){
  printf("pstree: invalid option\n");
  printf("Usage: Display a tree of processes.\n");
  printf("  -p, --show-pids       show PIDS, don't compact identical subtrees\n");
  printf("  -n, --numeric-sort    sort output by PID\n");
  printf("  -V, --version         display version information\n");
  return 1;
}

int version_msg(){
  freopen("/dev/tty", "w", stdout);
  printf("pstree (PSmisc) 23.4\n");
  printf("Copyright (C) 1993-2020 Werner Almesberger and Craig Small\n");
  printf("\n");
  printf("PSmisc comes with ABSOLUTELY NO WARRANTY.\n");
  printf("This is free software, and you are welcome to redistribute it under\n");
  printf("the terms of the GNU General Public License.\n");
  printf("For more information about these matters, see the files named COPYING.\n");
  return 0;
}  

int compare_pid(const void *processA, const void *processB)
{
  return (Process[*(int *)processA].pid - Process[*(int *)processB].pid);
}

void print_pstree(int args, struct process_info *process, int depth)
{
  for(int i = 1; i <= depth; i++) printf("  ");
  printf("%s", process->name);
  if(args == OPT_P || args == OPT_NP) printf("(%d)", process->pid);
  printf("\n");
  if(process->child_process != NULL){
    int child_pid[MAX_CHILD];
    for(int i = 0; i < process->child_process_cnt; i++) child_pid[i] = process->child_process[i]->pid;
    if(args == OPT_N || args == OPT_NP) qsort(child_pid, process->child_process_cnt, sizeof(int), compare_pid);
    for(int i = 0; i < process->child_process_cnt; i++) print_pstree(args, &Process[child_pid[i]], depth + 1);
  }
}

void construct_process_tree(struct process_info* process){
  struct process_info *parent_process = &Process[process->ppid];
  parent_process->child_process[parent_process->child_process_cnt++] = &Process[process->pid];
}

struct process_info* parse_file(char* path){
  struct process_info *process = malloc(sizeof(struct process_info));
  FILE* fp = fopen (path, "r");
  char title[MAX_STRING], content[MAX_STRING];
  while(fscanf(fp, "%s %s", title, content) != -1)
  {
    if(strcmp(title, "State:") == 0) fscanf(fp, "%s", content);
    if(strcmp(title, "Name:")  == 0) strcpy(process->name, content);
    if(strcmp(title, "Pid:")   == 0) process->pid = atoi(content);
    if(strcmp(title, "PPid:")  == 0) 
    {
      process->ppid = atoi(content);
      break;
    }
  }
  fclose(fp);
  if(process->ppid == 0) root = process->pid; 
  strcpy(Process[process->pid].name, process->name);
  Process[process->pid].pid = process->pid;
  Process[process->pid].ppid = process->ppid;
  Process[process->pid].child_process_cnt = 0;
  return process;
}

void preprocess_dir(char* path, void (*call_back)(char*)){
  char filename[MAX_STRING];
  struct dirent *dir_buf;
  DIR* dir;
  if((dir = opendir(path)) == NULL)
  {
    printf("open dir error\n");
    return;
  }
  while((dir_buf = readdir(dir)) != NULL)
  {
    if(strcmp(dir_buf->d_name, ".") == 0 || strcmp(dir_buf->d_name, "..") == 0) continue;
    if(strspn(dir_buf->d_name, "0123456789") != strlen(dir_buf->d_name) && strcmp(dir_buf->d_name, "status") != 0) continue;
    sprintf(filename, "%s/%s", path, dir_buf->d_name);
    call_back(filename);
  }
}

void preprocess_file(char* path){
  struct stat stat_buf;
  if(stat(path, &stat_buf) == -1){
    printf("stat error %s\n", path);
    return;
  }
  if(S_ISDIR(stat_buf.st_mode)) //directory
  {
    preprocess_dir(path, preprocess_file);
  }
  if(S_ISREG(stat_buf.st_mode))
  {
    struct process_info* process = parse_file(path);
    construct_process_tree(process);
  } 
}

int pstree(int args){
  preprocess_file("/proc");
  print_pstree(args, &Process[root], 0);
  return 0;
}

int handle_cmd(int argc, char *argv[])
{
  int handle_flag = OPT_NONE;
  for(int i = 1; i < argc; i++){
    if(strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--show-pids")         == 0) handle_flag |= OPT_P;
    else if(strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--numeric-sort") == 0) handle_flag |= OPT_N;
    else if(strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version")      == 0) handle_flag |= OPT_V;
    else handle_flag |= OPT_INVALID;
  }
  return handle_flag;
}

int run_cmd(int args){
  switch(args){
    case OPT_INVALID: return error_msg();
    case OPT_V:       return version_msg();
    case OPT_NONE:
    case OPT_P:
    case OPT_N:
    case OPT_NP:      return pstree(args);
    default: return 1;
  }
}

int main(int argc, char *argv[])
{
  int handle_flag = handle_cmd(argc, argv);
  int exit_flag = run_cmd(handle_flag);
  exit(exit_flag);
}


