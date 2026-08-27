// E1000 NIC driver

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "net.h"
#include "e1000_dev.h"

#define RX_RING_SIZE 16
#define TX_RING_SIZE 16

// Memory-mapped I/O registers
static volatile uint32 *regs;

// Descriptor rings
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));

// Buffers for descriptors
static struct mbuf *rx_mbufs[RX_RING_SIZE];
static struct mbuf *tx_mbufs[TX_RING_SIZE];

static struct spinlock e1000_lock;

// Read a 32-bit register
static uint32
e1000_read(uint32 index)
{
  return regs[index];
}

// Write a 32-bit register
static void
e1000_write(uint32 index, uint32 val)
{
  regs[index] = val;
}

// Initialize the E1000
void
e1000_init(uint32 *regs_base)
{
  regs = regs_base;

  // Acquire lock
  initlock(&e1000_lock, "e1000");

  // 1. Reset the E1000
  e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);

  // 2. Initialize transmit descriptor ring
  memset(tx_ring, 0, sizeof(tx_ring));
  for (int i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].addr = 0;
    tx_ring[i].length = 0;
    tx_ring[i].cmd = 0;
    tx_ring[i].status = 0;
    tx_mbufs[i] = 0;
  }
  e1000_write(E1000_TDBAL, (uint64)tx_ring);
  e1000_write(E1000_TDBAH, (uint64)tx_ring >> 32);
  e1000_write(E1000_TDLEN, TX_RING_SIZE * sizeof(struct tx_desc));
  e1000_write(E1000_TDH, 0);
  e1000_write(E1000_TDT, 0);

  // 3. Initialize receive descriptor ring
  memset(rx_ring, 0, sizeof(rx_ring));
  for (int i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000_init: mbufalloc");
    rx_ring[i].addr = (uint64)rx_mbufs[i]->head;
    rx_ring[i].length = 0;
    rx_ring[i].status = 0;
  }
  e1000_write(E1000_RDBAL, (uint64)rx_ring);
  e1000_write(E1000_RDBAH, (uint64)rx_ring >> 32);
  e1000_write(E1000_RDLEN, RX_RING_SIZE * sizeof(struct rx_desc));
  e1000_write(E1000_RDH, 0);
  e1000_write(E1000_RDT, RX_RING_SIZE - 1);

  // 4. Configure receive control
  e1000_write(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO | E1000_RCTL_RDMTS_HALF |
                         (0 << E1000_RCTL_MO_SHIFT) | E1000_RCTL_BSIZE_2048);

  // 5. Configure transmit control
  e1000_write(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << E1000_TCTL_CT_SHIFT) |
                         (0x40 << E1000_TCTL_COLD_SHIFT));

  // 6. Enable interrupts
  e1000_write(E1000_IMS, E1000_IMS_RXT0);
}

// Transmit a packet
int
e1000_transmit(struct mbuf *m)
{
  acquire(&e1000_lock);

  // 1. Get the current tail index
  int idx = e1000_read(E1000_TDT) % TX_RING_SIZE;

  // 2. Check if the descriptor is free
  if (!(tx_ring[idx].status & E1000_TXD_STAT_DD)) {
    release(&e1000_lock);
    return -1; // ring is full
  }

  // 3. Free the previous buffer (if any)
  if (tx_mbufs[idx]) {
    mbuffree(tx_mbufs[idx]);
    tx_mbufs[idx] = 0;
  }

  // 4. Fill in the descriptor
  tx_ring[idx].addr = (uint64)m->head;
  tx_ring[idx].length = m->len;
  tx_ring[idx].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS | E1000_TXD_CMD_IFCS;
  tx_mbufs[idx] = m;

  // 5. Update the tail index
  e1000_write(E1000_TDT, (idx + 1) % TX_RING_SIZE);

  release(&e1000_lock);
  return 0;
}

// Receive packets (called from interrupt handler)
void
e1000_recv(void)
{
  acquire(&e1000_lock);

  while (1) {
    // Get the next index after the last processed
    int idx = (e1000_read(E1000_RDT) + 1) % RX_RING_SIZE;
    struct rx_desc *desc = &rx_ring[idx];

    if (!(desc->status & E1000_RXD_STAT_DD))
      break; // no more packets

    // Deliver the packet to the network stack
    rx_mbufs[idx]->len = desc->length;
    net_rx(rx_mbufs[idx]);

    // Allocate a new buffer and replace in descriptor
    struct mbuf *new_mbuf = mbufalloc(0);
    if (!new_mbuf) {
      // Should not happen; if it does, we drop the packet and keep the old buffer
      // but we cannot reuse the old one because it's now owned by net_rx.
      // To keep things simple, we'll panic.
      panic("e1000_recv: mbufalloc");
    }
    rx_mbufs[idx] = new_mbuf;
    desc->addr = (uint64)new_mbuf->head;
    desc->length = 0;
    desc->status = 0;

    // Update the RDT
    e1000_write(E1000_RDT, idx);
  }

  release(&e1000_lock);
}

// Interrupt handler
void
e1000_intr(void)
{
  uint32 icr = e1000_read(E1000_ICR);
  if (icr & E1000_ICR_RXT0) {
    e1000_recv();
  }
}
