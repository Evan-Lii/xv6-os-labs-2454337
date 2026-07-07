#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "usage: xargs command [args...]\n");
    exit(1);
  }

  char *xargv[MAXARG];
  char line[512];
  char c;
  int n = 0;

  for(int i = 1; i < argc; i++){
    xargv[i - 1] = argv[i];
  }

  int base = argc - 1;

  while(read(0, &c, 1) == 1){
    if(c == '\n'){
      line[n] = 0;

      if(n > 0){
        int arg_index = base;
        char *p = line;

        while(*p && arg_index < MAXARG - 1){
          while(*p == ' ' || *p == '\t'){
            p++;
          }

          if(*p == 0){
            break;
          }

          xargv[arg_index++] = p;

          while(*p && *p != ' ' && *p != '\t'){
            p++;
          }

          if(*p){
            *p = 0;
            p++;
          }
        }

        xargv[arg_index] = 0;

        int pid = fork();
        if(pid < 0){
          fprintf(2, "fork error\n");
          exit(1);
        }

        if(pid == 0){
          exec(xargv[0], xargv);
          fprintf(2, "exec %s failed\n", xargv[0]);
          exit(1);
        } else {
          wait(0);
        }
      }

      n = 0;
    } else {
      if(n < sizeof(line) - 1){
        line[n++] = c;
      }
    }
  }

  if(n > 0){
    line[n] = 0;

    int arg_index = base;
    char *p = line;

    while(*p && arg_index < MAXARG - 1){
      while(*p == ' ' || *p == '\t'){
        p++;
      }

      if(*p == 0){
        break;
      }

      xargv[arg_index++] = p;

      while(*p && *p != ' ' && *p != '\t'){
        p++;
      }

      if(*p){
        *p = 0;
        p++;
      }
    }

    xargv[arg_index] = 0;

    int pid = fork();
    if(pid < 0){
      fprintf(2, "fork error\n");
      exit(1);
    }

    if(pid == 0){
      exec(xargv[0], xargv);
      fprintf(2, "exec %s failed\n", xargv[0]);
      exit(1);
    } else {
      wait(0);
    }
  }

  exit(0);
}
