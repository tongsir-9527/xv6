// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

static struct kmem kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    char name[8];
    snprintf(name, sizeof(name), "kmem_%d", i);
    initlock(&kmem[i].lock, name);
    kmem[i].freelist = 0;
  }
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  int cpu = cpuid();
  push_off();
  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cpu = cpuid();
  push_off();

  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;
  if(r) {
    kmem[cpu].freelist = r->next;
    release(&kmem[cpu].lock);
    pop_off();
    memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
  }
  release(&kmem[cpu].lock);

  // 当前 CPU 空闲链表为空，尝试从其他 CPU 窃取一半
  for (int i = 0; i < NCPU; i++) {
    if (i == cpu) continue;
    acquire(&kmem[i].lock);
    struct run *steal = kmem[i].freelist;
    if (steal) {
      // 取走一半（最多 64 页）
      int count = 0;
      struct run *p = steal;
      while (p && count < 64) {
        p = p->next;
        count++;
      }
      // 断开原链表
      kmem[i].freelist = p;
      release(&kmem[i].lock);
      // 将窃取到的页插入当前 CPU 链表
      acquire(&kmem[cpu].lock);
      while (steal) {
        struct run *next = steal->next;
        steal->next = kmem[cpu].freelist;
        kmem[cpu].freelist = steal;
        steal = next;
      }
      // 从当前链表取出一页返回
      r = kmem[cpu].freelist;
      if (r) {
        kmem[cpu].freelist = r->next;
        release(&kmem[cpu].lock);
        pop_off();
        memset((char*)r, 5, PGSIZE);
        return r;
      }
      release(&kmem[cpu].lock);
    } else {
      release(&kmem[i].lock);
    }
  }

  pop_off();
  return 0; // 无可用内存
}
