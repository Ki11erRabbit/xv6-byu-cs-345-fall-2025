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
void kfree_init(void *pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

#define TRANSFORM(addr) ((addr - KERNBASE) / PGSIZE)

unsigned char ref_count[TRANSFORM(PHYSTOP)] = {0};

static void _init_count(uint64 phys_mem) {
  uint64 index = TRANSFORM(phys_mem);
  ref_count[index] = 1;
}

static unsigned char _increment_ref(uint64 phys_mem) {
  uint64 index = TRANSFORM(phys_mem);
  ref_count[index] += 1;
  //printf("incremented ref: %d\n", ref_count[index]);
  return ref_count[index];
}

static unsigned char _decrement_ref(uint64 phys_mem) {
  uint64 index = TRANSFORM(phys_mem);
  if (ref_count[index] == 0) {
    // printf("ref count already at 0\n");
    panic("ref count hit zero again");
    return 0;
  }    
  ref_count[index] -= 1;
  //printf("decremented ref: %d\n", ref_count[index]);
  return ref_count[index];
}

unsigned char increment_ref(uint64 phys_mem) {
  acquire(&kmem.lock);

  unsigned char value = _increment_ref(phys_mem);

  release(&kmem.lock);
  return value;
}

unsigned char decrement_ref(uint64 phys_mem) {
  acquire(&kmem.lock);

  unsigned char value = _decrement_ref(phys_mem);

  release(&kmem.lock);

  return value;
}

unsigned char _get_count(uint64 phys_mem) {
  uint64 index = TRANSFORM(phys_mem);
  return ref_count[index];
}

unsigned char get_ref_count(uint64 phys_mem) {
  acquire(&kmem.lock);

  unsigned char value = _get_count(phys_mem);

  release(&kmem.lock);

  return value;
}


void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_init(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree_init(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if (_decrement_ref((uint64)pa) != 0) {
    //printf("Refcount not 0 yet\n");
    return;
  }    

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  if (r)
    _init_count((uint64)r);
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
