#include "kernel/types.h"
#include "user/user.h"

void
sieve(int left[2])
{
  int prime;
  int num;

  close(left[1]);

  if(read(left[0], &prime, sizeof(prime)) != sizeof(prime)){
    close(left[0]);
    exit(0);
  }

  printf("prime %d\n", prime);

  int right[2];
  if(pipe(right) < 0){
    fprintf(2, "pipe error\n");
    close(left[0]);
    exit(1);
  }

  int pid = fork();

  if(pid < 0){
    fprintf(2, "fork error\n");
    close(left[0]);
    close(right[0]);
    close(right[1]);
    exit(1);
  }

  if(pid == 0){
    close(left[0]);
    sieve(right);
    exit(0);
  } else {
    close(right[0]);

    while(read(left[0], &num, sizeof(num)) == sizeof(num)){
      if(num % prime != 0){
        write(right[1], &num, sizeof(num));
      }
    }

    close(left[0]);
    close(right[1]);

    wait(0);
    exit(0);
  }
}

int
main(int argc, char *argv[])
{
  int p[2];

  if(pipe(p) < 0){
    fprintf(2, "pipe error\n");
    exit(1);
  }

  int pid = fork();

  if(pid < 0){
    fprintf(2, "fork error\n");
    close(p[0]);
    close(p[1]);
    exit(1);
  }

  if(pid == 0){
    sieve(p);
    exit(0);
  } else {
    close(p[0]);

    for(int i = 2; i <= 35; i++){
      write(p[1], &i, sizeof(i));
    }

    close(p[1]);
    wait(0);
    exit(0);
  }

  exit(0);
}
