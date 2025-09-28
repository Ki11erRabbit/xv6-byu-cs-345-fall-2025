#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "thread.h"
#include "defs.h"

struct cpu cpus[NCPU];
struct thread thread[NTHREAD];

int nexttid = 1;
struct spinlock tid_lock;

extern void forkret(void);

extern char trampoline[]; // trampoline.S

// Allocate a page for each threads's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
thread_mapstacks(pagetable_t kpgtbl)
{
  struct thread *t;
  
  for(t = thread; t < &thread[NTHREAD]; t++) {
    char *ta = kalloc();
    if(ta == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (t - thread));
    kvmmap(kpgtbl, va, (uint64)ta, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the thread table.
void
threadinit(void)
{
  struct thread *t;
  
  initlock(&tid_lock, "nexttid");
  for(t = thread; t < &thread[NTHREAD]; t++) {
      initlock(&t->lock, "thread");
      t->state = T_UNUSED;
      t->kstack = KSTACK((int) (t - thread));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct thread *, or zero if none.
struct thread*
mythread(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct thread *t = c->thread;
  pop_off();
  return t;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  if (c->thread == 0) {
    pop_off();
    return 0;
  }
  struct proc *p = c->thread->proc;
  pop_off();
  return p;
}

int
alloctid()
{
  int tid;
  
  acquire(&tid_lock);
  tid = nexttid;
  nexttid = nexttid + 1;
  release(&tid_lock);

  return tid;
}

// Look in the thread table for an UNUSED thread.
// If found, initialize state required to run in the kernel,
// and return with t->lock held.
// If there are no free threads, or a memory allocation fails, return 0.
struct thread*
allocthread(struct proc* p)
{
  struct thread *t;

  for(t = thread; t < &thread[NTHREAD]; t++) {
    acquire(&t->lock);
    if(t->state == T_UNUSED) {
      goto found;
    } else {
      release(&t->lock);
    }
  }
  return 0;

found:
  t->tid = alloctid();
  t->state = USED;

  // Allocate a trapframe page.
  if((t->trapframe = (struct trapframe *)kalloc()) == 0){
    freethread(t);
    release(&t->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&t->context, 0, sizeof(t->context));
  t->context.ra = (uint64)forkret;
  t->context.sp = t->kstack + PGSIZE;

  if (allocprocthread(p, t) != 0) {
    release(&t->lock);
    return 0;
  }    

  release(&t->lock);
  return t;
}

// free a thread structure and the data hanging from it,
// including user pages.
// t->lock must be held.
void freethread(struct thread *t) {
  if (t == 0) {
    return;
  }    
  if(t->trapframe)
    kfree((void*)t->trapframe);
  t->trapframe = 0;
  t->tid = 0;
  t->name[0] = 0;
  t->chan = 0;
  t->state = UNUSED;
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;
  struct proc *p;
  p = myproc();
  // Still holding p->lock from scheduler.
  release(&p->main_thread->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();
  }

  usertrapret();
}




// Set up first user process thread.
void
userinitthread(struct proc *p)
{

  struct thread *t = p->main_thread;
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  t->trapframe->epc = 0;      // user program counter
  t->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  t->state = T_RUNNABLE;
  safestrcpy(t->name, "main", sizeof(t->name));
}

// Create a new thread, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
// For a thread we need to do a little trick. We should make the thread that calls fork() be the main thread.
void
fork_thread(struct proc *np)
{
  struct thread *t = mythread();
  acquire(&t->lock);

  // copy saved user registers.
  *(np->main_thread->trapframe) = *(t->trapframe);

  // Cause fork to return 0 in the child.
  np->main_thread->trapframe->a0 = 0;
  release(&t->lock);

  safestrcpy(t->name, np->main_thread->name, sizeof(t->name));
}

// This should notify the proccess that it should die.
// Then we call sched() to then to never come back
void exit(int status) {
  struct thread *t = mythread();
  
  acquire(&t->lock);
  t->killed = 1;
  t->state = T_ZOMBIE;
  release(&t->lock);

  proc_exit(t->proc, status);

  acquire(&t->lock);
  // Jump into the scheduler, never to return.

  sched();
  panic("zombie exit");
}  

// Per-CPU thread scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct thread *t;
  struct cpu *c = mycpu();

  c->thread = 0;
  for(;;){
    // The most recent thread to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // threads are waiting.
    intr_on();

    int found = 0;
    for(t = thread; t < &thread[NTHREAD]; t++) {
      acquire(&t->lock);
      if(t->state == T_RUNNABLE) {
        // Switch to chosen thread.  It is the thread's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        t->state = T_RUNNING;
        c->thread = t;
        acquire(&t->proc->lock);
        t->proc->state = RUNNING;
        release(&t->proc->lock);
        swtch(&c->context, &t->context);

        // Thread is done running for now.
        // It should have changed its t->state before coming back.
        c->thread = 0;
        found = 1;
      }
      release(&t->lock);
    }
    if(found == 0) {
      // nothing to run; stop running on this core until an interrupt.
      intr_on();
      asm volatile("wfi");
    }
  }
}

// Switch to scheduler.  Must hold only t->lock
// and have changed t->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be thread->intena and thread->noff, but that would
// break in the few places where a lock is held but
// there's no thread.
void
sched(void)
{
  int intena;
  struct thread *t = mythread();

  if(!holding(&t->lock))
    panic("sched t->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(t->state == T_RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&t->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct thread *t = mythread();
  acquire(&t->lock);
  t->state = T_RUNNABLE;
  sched();
  release(&t->lock);
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct thread *t = mythread();
  
  // Must acquire t->lock in order to
  // change t->state and then call sched.
  // Once we hold t->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks t->lock),
  // so it's okay to release lk.

  acquire(&t->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  t->chan = chan;
  t->state = T_SLEEPING;

  sched();

  // Tidy up.
  t->chan = 0;

  // Reacquire original lock.
  release(&t->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct thread *t;

  for(t = thread; t < &thread[NTHREAD]; t++) {
    if(t != mythread()){
      acquire(&t->lock);
      if(t->state == T_SLEEPING && t->chan == chan) {
        t->state = T_RUNNABLE;
      }
      release(&t->lock);
    }
  }
}

void kill_thread(struct thread *t) {
  if (t == 0) {
    return;
  }    
  acquire(&t->lock);
  t->killed = 1;
  t->state = T_RUNNABLE;
  release(&t->lock);
}  

int
killed_thread(struct thread *t)
{
  int k;
  
  acquire(&t->lock);
  k = t->killed;
  release(&t->lock);
  return k;
}


// Print a thread listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void threaddump(struct thread *t) {

  if (t == 0) {
    return;
  }    
  static char *states[] = {
  [T_UNUSED]    "unused",
  [T_USED]      "used",
  [T_SLEEPING]  "sleep ",
  [T_RUNNABLE]  "runble",
  [T_RUNNING]   "run   ",
  [T_ZOMBIE]    "zombie"
  };
  ;
  char *state;

  printf("\n");
  if(t->state == T_UNUSED)
    return;
  if(t->state >= 0 && t->state < NELEM(states) && states[t->state])
    state = states[t->state];
  else
    state = "???";
  printf("\t%d %s %s", t->tid, state, t->name);
  printf("\n");
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t thread_trapframe(struct thread *t, pagetable_t pagetable) {

  if (t == 0) {
    return pagetable;
  }    

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(t->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

uint64 thread_spawn(void (*fnptr)(void *), void *arg, char *thread_name) {
  struct proc *p = myproc();
  acquire(&p->lock);
  pagetable_t pagetable = p->pagetable;
  struct thread *t = allocthread(p);
  acquire(&t->lock);

  thread_trapframe(t, pagetable);
  uint64 sp;
  if ((sp = uvmalloc(pagetable, 0, (USERSTACK + 1) * PGSIZE, PTE_W)) == 0)
    return 0;
  uvmclear(pagetable, -(USERSTACK+1)*PGSIZE);
  
  t->trapframe->epc = (uint64)fnptr;
  t->trapframe->sp = sp; // initial stack pointer
  t->trapframe->a0 = (uint64)arg;
  if (thread_name)
    safestrcpy(t->name, thread_name, 13);
  uint64 tid = t->tid;
  release(&t->lock);
  release(&p->lock);
  return tid;
}  
