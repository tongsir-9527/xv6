#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"

// 处理 mmap 缺页异常
int
mmap_fault(struct proc *p, uint64 va) {
  va = PGROUNDDOWN(va);
  for (int i = 0; i < MAX_VMA; i++) {
    if (!p->vma[i].used) continue;
    struct vma *v = &p->vma[i];
    if (va >= v->va && va < v->va + v->len) {
      char *mem = kalloc();
      if (mem == 0) return 0;
      uint64 offset = v->offset + (va - v->va);
      ilock(v->file->ip);
      readi(v->file->ip, 0, (uint64)mem, offset, PGSIZE);
      iunlock(v->file->ip);
      int perm = PTE_U | PTE_V;
      if (v->prot & PROT_READ) perm |= PTE_R;
      if (v->prot & PROT_WRITE) perm |= PTE_W;
      if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, perm) != 0) {
        kfree(mem);
        return 0;
      }
      return 1;
    }
  }
  return 0;
}

// 写回 MAP_SHARED 脏页（供 munmap 和 exit 调用）
void
mmap_writeback(struct proc *p, uint64 start, uint64 end, struct file *f) {
  for (uint64 a = start; a < end; a += PGSIZE) {
    pte_t *pte = walk(p->pagetable, a, 0);
    if (pte && (*pte & PTE_V) && (*pte & PTE_D)) {
      uint64 pa = PTE2PA(*pte);
      ilock(f->ip);
      writei(f->ip, 0, (uint64)pa, a - start, PGSIZE);
      iunlock(f->ip);
    }
  }
}

// 解除 VMA 映射并释放资源
void
mmap_unmap_vma(struct proc *p, struct vma *v) {
  if (v->flags == MAP_SHARED && (v->prot & PROT_WRITE)) {
    mmap_writeback(p, v->va, v->va + v->len, v->file);
  }
  uvmunmap(p->pagetable, v->va, v->len / PGSIZE, 1);
  fileclose(v->file);
  v->used = 0;
}
