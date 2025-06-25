#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/pci.h>
#include <kern/env.h>
#include <kern/picirq.h>
#include <inc/string.h>
#include <inc/error.h>
// Transmit descriptor structure (from Intel manual table 3-8)


// Global variables
static volatile uint32_t *e1000_reg_base;  // Memory-mapped registers base address

// Transmit structures
static struct tx_desc tx_desc_array[TX_RING_SIZE] __attribute__((aligned(128)));
static char tx_pkt_bufs[TX_RING_SIZE][TX_PKT_SIZE];
static uint32_t tx_desc_tail = 0;

// Receive structures  
static struct rx_desc rx_desc_array[RX_RING_SIZE] __attribute__((aligned(128)));
static char rx_pkt_bufs[RX_RING_SIZE][TX_PKT_SIZE];
static uint32_t rx_desc_tail = 0;

int e1000_irq = -1; // IRQ line for E1000 device


static void
e1000_init_tx(void);
static void
e1000_init_rx(void);

// Helper function to read E1000 register
static uint32_t
e1000_read_reg(uint32_t reg)
{
    return e1000_reg_base[reg / 4];
}

// Helper function to write E1000 register
static void
e1000_write_reg(uint32_t reg, uint32_t value)
{
    e1000_reg_base[reg / 4] = value;
}

// Initialize E1000 device
int
e1000_attach(struct pci_func *pcif)
{
    // Enable PCI device
    pci_func_enable(pcif);
    
    // Map the E1000's registers into virtual memory
    e1000_reg_base = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    e1000_irq = pcif->irq_line;
    // Initialize transmit
    e1000_init_tx();
    
    // Initialize receive
    e1000_init_rx();
    
    irq_setmask_8259A(irq_mask_8259A & ~(1 << e1000_irq));
    cprintf("E1000: Initialized successfully\n");
    return 0;
}

// Initialize transmit functionality
static void
e1000_init_tx(void)
{
    int i;
    
    // Initialize transmit descriptors
    for (i = 0; i < TX_RING_SIZE; i++) {
        tx_desc_array[i].addr = PADDR(tx_pkt_bufs[i]);
        tx_desc_array[i].cmd = 0;
        tx_desc_array[i].status = E1000_TXD_STAT_DD; // Mark as done initially
    }
    
    // Set transmit descriptor base address
    e1000_write_reg(E1000_TDBAL, PADDR(tx_desc_array));
    e1000_write_reg(E1000_TDBAH, 0); // High 32 bits (assuming < 4GB physical memory)
    
    // Set transmit descriptor length (must be 128-byte aligned)
    e1000_write_reg(E1000_TDLEN, sizeof(tx_desc_array));
    
    // Initialize head and tail pointers
    e1000_write_reg(E1000_TDH, 0);
    e1000_write_reg(E1000_TDT, 0);
    tx_desc_tail = 0;
    
    // Configure TCTL register
    // EN: Enable transmit
    // PSP: Pad short packets to minimum size
    // CT: Collision threshold (IEEE 802.3 standard recommends 15)
    // COLD: Collision distance (IEEE 802.3 standard for full-duplex is 64)
    uint32_t tctl = E1000_TCTL_EN | E1000_TCTL_PSP | 
                    (15 << 4) |     // CT = 15
                    (64 << 12);     // COLD = 64
    e1000_write_reg(E1000_TCTL, tctl);
    
    // Configure TIPG register (Inter Packet Gap)
    // Values from IEEE 802.3 standard (table 13-77)
    // IPGT: 10 for full-duplex
    // IPGR1: 8 
    // IPGR2: 6
    uint32_t tipg = (10 << 0) |   // IPGT
                    (8 << 10) |   // IPGR1  
                    (6 << 20);    // IPGR2
    e1000_write_reg(E1000_TIPG, tipg);
    
    cprintf("E1000: Transmit initialized\n");
}

// Initialize receive functionality
static void
e1000_init_rx(void)
{
    int i;
    cprintf("E1000: Initializing receive...\n");
    // Initialize receive descriptors
    for (i = 0; i < RX_RING_SIZE; i++) {
        rx_desc_array[i].addr = PADDR(rx_pkt_bufs[i]);
        rx_desc_array[i].status = 0; // Hardware will set status
    }
    cprintf("E1000: Receive descriptors initialized\n");
    // Set receive descriptor base address
    e1000_write_reg(E1000_RDBAL, PADDR(rx_desc_array));
    e1000_write_reg(E1000_RDBAH, 0);
    
    cprintf("E1000: Receive descriptor base address set to %08x\n", PADDR(rx_desc_array));

    // Set receive descriptor length
    e1000_write_reg(E1000_RDLEN, sizeof(rx_desc_array));
    
    // Initialize head and tail pointers
    e1000_write_reg(E1000_RDH, 0);
    e1000_write_reg(E1000_RDT, RX_RING_SIZE - 1); // Tail points to last available descriptor
    rx_desc_tail = RX_RING_SIZE - 1;
    
    // Configure RCTL register
    // EN: Enable receive
    // BAM: Broadcast Accept Mode
    // BSIZE: Buffer size (2048 bytes)
    // SECRC: Strip Ethernet CRC
    uint32_t rctl = (1 << 1) |    // EN
                    (1 << 15) |   // BAM
                    (0 << 16) |   // BSIZE = 2048
                    (1 << 26);    // SECRC
    e1000_write_reg(E1000_RCTL, rctl);
    
// Enable receive interrupts
    e1000_write_reg(E1000_IMS, 
        E1000_ICR_RXT0 |       // Receiver timer interrupt
        E1000_ICR_RXDMT0 |     // Receive descriptor minimum threshold
        E1000_ICR_TXDW);       // Also enable transmit interrupts

    cprintf("E1000: Receive initialized\n");
}

// Transmit a packet
int
e1000_transmit(const void *data, size_t len)
{
    // Check packet length
    if (len > TX_PKT_SIZE) {
        return -E_INVAL;
    }
    
    // Get current tail descriptor
    uint32_t tail = tx_desc_tail;
    struct tx_desc *desc = &tx_desc_array[tail];
    
    // Check if descriptor is available (previous transmission completed)
    if (!(desc->status & E1000_TXD_STAT_DD)) {
        // Transmit queue is full
        return -E_TX_FULL;
    }
    
    // Copy packet data to buffer
    memcpy(tx_pkt_bufs[tail], data, len);
    
    // Set up descriptor
    desc->length = len;
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS; // End of packet, report status
    desc->status = 0; // Clear status (hardware will set DD when done)
    
    // Update tail pointer
    tx_desc_tail = (tail + 1) % TX_RING_SIZE;
    
    // Notify hardware of new packet
    e1000_write_reg(E1000_TDT, tx_desc_tail);
    
    return 0;
}

// Receive a packet
int
e1000_rx(void *data, size_t *len)
{
    // Calculate next descriptor to check
    uint32_t next_tail = (rx_desc_tail + 1) % RX_RING_SIZE;
    struct rx_desc *desc = &rx_desc_array[next_tail];
    
    // Check if packet is available
    if (!(desc->status & 0x01)) { // DD bit
        // No packet available
        return -E_RX_EMPTY;
    }
    
    // Copy packet data
    size_t pkt_len = desc->length;
    if (pkt_len > *len) {
        return -E_BUF_TOO_SMALL;
    }
    
    memcpy(data, rx_pkt_bufs[next_tail], pkt_len);
    *len = pkt_len;
    
    // Clear descriptor status for reuse
    desc->status = 0;
    
    // Update tail pointer and notify hardware
    rx_desc_tail = next_tail;
    e1000_write_reg(E1000_RDT, rx_desc_tail);
    
    return 0;
}

// E1000 interrupt handler
void
e1000_intr(void)
{
    uint32_t icr;
    
    // Read interrupt cause register (this also clears it)
    icr = e1000_read_reg(E1000_ICR);
    
    // Handle receive interrupts
    if (icr & (E1000_ICR_RXT0 | E1000_ICR_RXDMT0)) {
        // Check if there's an environment blocked on receive
        if (recv_blocked_env && e1000_rx_packet_available()) {
            int result = e1000_rx_packet_nb((void *)recv_syscall_dstva, recv_syscall_len);
            
            // Set return value and mark environment as runnable
            recv_blocked_env->env_tf.tf_regs.reg_eax = result;
            recv_blocked_env->env_status = ENV_RUNNABLE;
            
            // Clear blocked environment
            recv_blocked_env = NULL;
        }
    }
    
    // Handle transmit interrupts
    if (icr & E1000_ICR_TXDW) {
        // Wake up any environment blocked on transmit, if applicable
        if (transmit_blocked_env) {
            transmit_blocked_env->env_tf.tf_regs.reg_eax = 0; // Success
            transmit_blocked_env->env_status = ENV_RUNNABLE;
            transmit_blocked_env = NULL;
        }
     }
    
    // Clear interrupt on LAPIC
    lapic_eoi();
}

// Get transmit queue status for debugging
void
e1000_tx_status(void)
{
    uint32_t head = e1000_read_reg(E1000_TDH);
    uint32_t tail = e1000_read_reg(E1000_TDT);
    
    cprintf("E1000 TX: head=%d, tail=%d, sw_tail=%d\n", 
            head, tail, tx_desc_tail);
    
    // Print descriptor status
    int i;
    for (i = 0; i < TX_RING_SIZE; i++) {
        if (tx_desc_array[i].status & E1000_TXD_STAT_DD) {
            cprintf("  desc[%d]: DONE\n", i);
        } else {
            cprintf("  desc[%d]: PENDING\n", i);  
        }
    }
}