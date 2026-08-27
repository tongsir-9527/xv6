// Network stack

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "net.h"

// Define some constants
#define IPPROTO_UDP 17
#define MAX_PORTS 16
#define MAX_QUEUE 16

// UDP packet queue entry
struct udp_packet {
  struct mbuf *m;
  uint32_t src_ip;
  uint16_t src_port;
  uint16_t len;
};

// Per-port state
struct udp_port {
  int used;                 // bound?
  int head, tail;           // queue indices
  struct udp_packet queue[MAX_QUEUE];
  struct spinlock lock;
  struct proc *waiting;     // process waiting for recv
};

static struct udp_port ports[MAX_PORTS];

// Initialize UDP ports
void
net_init(void)
{
  // (existing net_init code may exist; we add port initialization)
  for (int i = 0; i < MAX_PORTS; i++) {
    ports[i].used = 0;
    ports[i].head = ports[i].tail = 0;
    initlock(&ports[i].lock, "udp_port");
    ports[i].waiting = 0;
  }
}

// Bind a port (system call)
uint64
sys_bind(void)
{
  int port;
  if (argint(0, &port) < 0)
    return -1;
  if (port < 0 || port >= MAX_PORTS)
    return -1;
  struct udp_port *p = &ports[port];
  acquire(&p->lock);
  if (p->used) {
    release(&p->lock);
    return -1;
  }
  p->used = 1;
  p->head = p->tail = 0;
  release(&p->lock);
  return 0;
}

// Receive a UDP packet (system call)
uint64
sys_recv(void)
{
  int dport;
  uint64 src_ip_ptr, src_port_ptr, buf_ptr;
  int maxlen;
  if (argint(0, &dport) < 0) return -1;
  if (argaddr(1, &src_ip_ptr) < 0) return -1;
  if (argaddr(2, &src_port_ptr) < 0) return -1;
  if (argaddr(3, &buf_ptr) < 0) return -1;
  if (argint(4, &maxlen) < 0) return -1;

  struct udp_port *p = &ports[dport];
  if (!p->used) return -1;

  acquire(&p->lock);
  while (p->head == p->tail) {
    // no packet waiting
    p->waiting = myproc();
    sleep(&p->waiting, &p->lock);
    if (myproc()->killed) {
      release(&p->lock);
      return -1;
    }
  }

  // Retrieve the earliest packet
  struct udp_packet *pk = &p->queue[p->head];
  uint16_t src_port = pk->src_port;
  uint32_t src_ip = pk->src_ip;
  uint16_t len = pk->len;
  if (len > maxlen) len = maxlen;

  // Copy source IP and source port to user space
  if (copyout(myproc()->pagetable, src_ip_ptr, (char*)&src_ip, sizeof(src_ip)) < 0) {
    release(&p->lock);
    return -1;
  }
  if (copyout(myproc()->pagetable, src_port_ptr, (char*)&src_port, sizeof(src_port)) < 0) {
    release(&p->lock);
    return -1;
  }

  // Copy payload to user buffer
  if (copyout(myproc()->pagetable, buf_ptr, pk->m->head, len) < 0) {
    release(&p->lock);
    return -1;
  }

  // Free the mbuf
  mbuffree(pk->m);
  pk->m = 0;

  // Advance head
  p->head = (p->head + 1) % MAX_QUEUE;

  // Clear waiting process (if any)
  p->waiting = 0;

  release(&p->lock);
  return len;
}

// Process incoming IP packets
void
ip_rx(struct mbuf *m)
{
  // Assume m contains an IP packet (20-byte header + payload)
  struct ip *iph = (struct ip*)m->head;
  // 检查版本和长度
  if ((iph->ver_len & 0xf0) != 0x40) { // IP version 4, header length >= 5
    mbuffree(m);
    return;
  }
  // 检查协议
  if (iph->protocol != IPPROTO_UDP) {
    mbuffree(m);
    return;
  }
  // 提取 UDP 头部（在 IP 头之后）
  uint16_t iph_len = (iph->ver_len & 0x0f) * 4;
  struct udp *udph = (struct udp*)(m->head + iph_len);
  uint16_t dport = ntohs(udph->dport);
  if (dport >= MAX_PORTS) {
    mbuffree(m);
    return;
  }

  struct udp_port *p = &ports[dport];
  acquire(&p->lock);
  if (!p->used) {
    release(&p->lock);
    mbuffree(m);
    return;
  }

  // Check if queue is full
  int next_tail = (p->tail + 1) % MAX_QUEUE;
  if (next_tail == p->head) {
    // queue full, drop packet
    release(&p->lock);
    mbuffree(m);
    return;
  }

  // Store the packet
  struct udp_packet *pk = &p->queue[p->tail];
  pk->m = m;
  pk->src_ip = ntohl(iph->src);
  pk->src_port = ntohs(udph->sport);
  // UDP length includes UDP header, but we only need payload length for recv
  uint16_t udp_len = ntohs(udph->len);
  pk->len = udp_len - 8; // UDP payload length (8-byte header)
  // Move tail
  p->tail = next_tail;

  // Wake up waiting process if any
  if (p->waiting) {
    wakeup(p->waiting);
  }
  release(&p->lock);
}

// Other functions (e.g., sys_send, arp handling) are already provided.
// Please ensure that net.c's existing code (like net_rx, arp_rx, etc.) remains.
