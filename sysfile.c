#include "fcntl.h"   // 确保 PROT_* 宏已定义

uint64
sys_mmap(void)
{
  uint64 addr, len, off;
  int prot, flags, fd;
  struct file *f;
  struct proc *p = myproc();

  // 1. 提取参数
  if (argaddr(0, &addr) < 0 || argaddr(1, &len) < 0 ||
      argint(2, &prot) < 0 || argint(3, &flags) < 0 ||
      argfd(4, &fd, &f) < 0 || argaddr(5, &off) < 0)
    return -1;

  // 2. 简化实现要求：addr 必须为 0，offset 必须为 0
  if (addr != 0 || off != 0)
    return -1;

  // 3. 文件必须支持读写（根据 prot）
  if (prot & PROT_WRITE) {
    if (!f->writable)
      return -1;
  }
  if (!(prot & PROT_READ) && !(prot & PROT_WRITE))
    return -1;   // 既不可读也不可写

  // 4. 分配 VMA
  struct vma *v = 0;
  for (int i = 0; i < MAX_VMA; i++) {
    if (!p->vma[i].used) {
      v = &p->vma[i];
      break;
    }
  }
  if (v == 0)
    return -1;   // 没有空闲 VMA

  // 5. 选择映射地址（从当前堆顶开始）
  uint64 va = p->sz;
  if (va + len > MAXVA)
    return -1;

  // 6. 记录 VMA
  v->used = 1;
  v->va = va;
  v->len = len;
  v->prot = prot;
  v->flags = flags;
  v->file = filedup(f);   // 增加文件引用计数
  v->offset = off;
  p->vma_count++;

  // 7. 扩大进程地址空间（不分配物理页）
  p->sz = va + len;

  return va;
}

uint64
sys_munmap(void)
{
  uint64 addr, len;
  if (argaddr(0, &addr) < 0 || argaddr(1, &len) < 0)
    return -1;

  struct proc *p = myproc();

  // 查找包含 addr 的 VMA
  struct vma *v = 0;
  for (int i = 0; i < MAX_VMA; i++) {
    if (p->vma[i].used &&
        addr >= p->vma[i].va &&
        addr < p->vma[i].va + p->vma[i].len) {
      v = &p->vma[i];
      break;
    }
  }
  if (v == 0)
    return -1;   // 未找到对应 VMA

  // 如果映射区域是 MAP_SHARED 且可能被修改，则写回脏页
  if (v->flags == MAP_SHARED && v->prot & PROT_WRITE) {
    // 遍历该区域所有页，检查 PTE_D（脏位）
    uint64 start = addr;
    uint64 end = addr + len;
    if (end > v->va + v->len) end = v->va + v->len;
    for (uint64 a = start; a < end; a += PGSIZE) {
      pte_t *pte = walk(p->pagetable, a, 0);
      if (pte && (*pte & PTE_V) && (*pte & PTE_D)) {
        // 将页写入文件
        uint64 pa = PTE2PA(*pte);
        ilock(v->file->ip);
        writei(v->file->ip, 0, (uint64)pa, a - v->va, PGSIZE);
        iunlock(v->file->ip);
        // 清除脏位（可选）
        // *pte &= ~PTE_D;
      }
    }
  }

  // 解除映射（释放物理页）
  uvmunmap(p->pagetable, addr, len / PGSIZE, 1);

  // 释放 VMA
  fileclose(v->file);
  v->used = 0;
  p->vma_count--;

  return 0;
}
