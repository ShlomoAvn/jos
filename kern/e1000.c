#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/pci.h>
#include <inc/string.h>
#include <inc/error.h>
volatile uint32_t *e1000_regs;  
uint32_t tx_tail = 0;

struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(128)));
char tx_bufs[TX_RING_SIZE][TX_PKT_SIZE];


void test_e1000_transmit(); 

int
e1000_attach(struct pci_func *pcif)
{
    int i = 0;
    pci_func_enable(pcif);
    e1000_regs = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    e1000_regs[E1000_TDLEN] = sizeof(tx_ring);
    e1000_regs[E1000_TDBAL] = PADDR(tx_ring);
    e1000_regs[E1000_TDBAH] = 0;
    e1000_regs[E1000_TDH] = 0;
    //cprintf("E1000 TX Descriptor Length: %d\n", e1000_regs[E1000_TDLEN]);
    e1000_regs[E1000_TDT] = 0;
    //cprintf("E1000 TX Descriptor Base Address Low: 0x%x\n", e1000_regs[E1000_TDBAL]);
    //e1000_regs[E1000_TCTL] = E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << E1000_TCTL_CT_SHIFT) | (0x40 << E1000_TCTL_COLD_SHIFT);
    //e1000_regs[E1000_TDT] = 0;
    e1000_regs[E1000_TIPG] = 10 | (8 << 10) | (12 << 20);
    for (i = 0; i < TX_RING_SIZE; i++) {
        tx_ring[i].status = E1000_TXD_STAT_DD;
    }



    uint32_t status = e1000_regs[2];  // offset 8 bytes = index 2
    cprintf("E1000 device status: 0x%x\n", status);
    test_e1000_transmit();

    return 0;
}



int e1000_transmit(void *data, size_t len){
    if (len > TX_PKT_SIZE)
        return -E_INVAL;

    uint32_t index = tx_tail;
    if (!(tx_ring[index].status & E1000_TXD_STAT_DD))
        return -E_TX_FULL; // תור מלא

    memcpy(tx_bufs[index], data, len);

    tx_ring[index].addr = PADDR(tx_bufs[index]);
    tx_ring[index].length = len;
    tx_ring[index].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
    tx_ring[index].status = 0;

    tx_tail = (tx_tail + 1) % TX_RING_SIZE;
    e1000_regs[E1000_TDT] = tx_tail;

    return 0;

}

void test_e1000_transmit() {
    const char test_pkt[] = "Hello, E1000!";
    cprintf("Transmitting packet: %s\n", test_pkt);
    e1000_transmit((void *) test_pkt, sizeof(test_pkt));
}