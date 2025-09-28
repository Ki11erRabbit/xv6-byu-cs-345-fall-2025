#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void thread(char* input) {
  printf("%s from thread!", input);
}

int main(int argc, char **argv) {
  printf("spawning thread"); 
  spawn((void(*)(void*))thread, (void*)"Hello, World!","hello");
  printf("spawned thread");
  exit(0);
}
