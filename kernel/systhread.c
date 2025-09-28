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
  argaddr(0, &fnptr);
  argaddr(1, &thread_arg);
  argaddr(2, &thread_name);

  return thread_spawn(fnptr, thread_arg, thread_name);
}  
