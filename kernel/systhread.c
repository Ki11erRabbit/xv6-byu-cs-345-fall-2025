#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "thread.h"


uint64 sys_spawn(void) {
  void *thread_arg = 0;
  void (*fnptr)(void *) = 0;
  char *thread_name = 0;
  uint64 fnptraddr = 0;
  uint64 thread_argaddr = 0;
  uint64 thread_nameaddr = 0;
  argaddr(0, &fnptraddr);
  argaddr(1, &thread_argaddr);
  argaddr(2, &thread_nameaddr);

  fnptr = (void (*)(void *))fnptraddr;
  thread_arg = (void *)thread_argaddr;
  thread_name = (char *)thread_nameaddr;

  return thread_spawn(fnptr, thread_arg, thread_name);
}

uint64 sys_threadexit(void) {
  threadexit();
  return 0;
}  
