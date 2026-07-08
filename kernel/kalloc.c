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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int count[PHYSTOP / PGSIZE];
} refs;

static int
refindex(void *pa)
{
  return ((uint64)pa) / PGSIZE;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refs.lock, "refs");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Increase reference count of a physical page.
void
kaddref(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kaddref");

  acquire(&refs.lock);
  refs.count[refindex(pa)]++;
  release(&refs.lock);
}

// Free the page of physical memory pointed at by pa.
// If the page is shared, only decrease reference count.
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&refs.lock);
  if(refs.count[refindex(pa)] > 1){
    refs.count[refindex(pa)]--;
    release(&refs.lock);
    return;
  }
  refs.count[refindex(pa)] = 0;
  release(&refs.lock);

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
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    acquire(&refs.lock);
    refs.count[refindex((void*)r)] = 1;
    release(&refs.lock);
  }

  return (void*)r;
}
