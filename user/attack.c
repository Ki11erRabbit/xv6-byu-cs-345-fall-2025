#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"


void print_secret(int fd, char *secret) {
  write(fd, secret, 40);
}  

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  char *heap = sbrk(PGSIZE * 32);
  // heap = heap + 9 * PGSIZE;
  // heap += 32;
  int i = 0;
  while (!(heap[i - 1] == 'm' && heap[i] == 'y')) {
    i++;
  }
  print_secret(1, heap + i + 1);
  print_secret(2, heap + i + 1);
  exit(1);
}
