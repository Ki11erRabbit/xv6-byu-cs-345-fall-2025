// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
#define MAXCPUS 3

struct run {
  struct run *next;
};

struct {
  struct run *starting_freelist;
  struct spinlock locks[MAXCPUS];
  struct run *freelists[MAXCPUS];
  int free_list_lens[MAXCPUS];
} kmem;


void
kfree_init(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  r->next = kmem.starting_freelist;
  kmem.starting_freelist = r;
}


void
freerange_init(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_init(p);
}

void kinit() {
  freerange_init((void *)end, (void *)PHYSTOP);
  int cpuid = 0;
  while (kmem.starting_freelist) {
    if (kmem.freelists[cpuid]) {
      struct run *next = kmem.starting_freelist->next;
      kmem.starting_freelist->next = kmem.freelists[cpuid];
      kmem.freelists[cpuid] = kmem.starting_freelist;
      kmem.starting_freelist = next;
      kmem.free_list_lens[cpuid]++;
    } else {
      struct run *next = kmem.starting_freelist->next;
      kmem.starting_freelist->next = 0;
      kmem.freelists[cpuid] = kmem.starting_freelist;
      kmem.starting_freelist = next;
      kmem.free_list_lens[cpuid]++;
    }      
    cpuid = (cpuid + 1) % MAXCPUS;
  }    
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int cpu_id = cpuid();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.locks[cpu_id]);
  r->next = kmem.freelists[cpu_id];
  kmem.freelists[cpu_id] = r;
  kmem.free_list_lens[cpu_id]++;
  release(&kmem.locks[cpu_id]);
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cpu_id = cpuid();

  //printf("accquired lock in %d\n", cpu_id);
  acquire(&kmem.locks[cpu_id]);
  r = kmem.freelists[cpu_id];
  if(r) {
    kmem.freelists[cpu_id] = r->next;
    kmem.free_list_lens[cpu_id]--;
  } else {
    int max = -1;
    int max_value = 0;
    for (int i = 0; i < MAXCPUS; i++) {
      if (max_value < kmem.free_list_lens[i]) {
        max = i;
        max_value = kmem.free_list_lens[i];
      }
    }
    if (max != -1) {
      //printf("accquired lock in max %d\n", max);
      acquire(&kmem.locks[max]);
      r = kmem.freelists[max];

      kmem.freelists[max] = r->next;
      kmem.free_list_lens[max]--;
      
      release(&kmem.locks[max]);
    }      
  }
  release(&kmem.locks[cpu_id]);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
