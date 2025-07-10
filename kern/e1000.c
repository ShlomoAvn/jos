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

static uint8_t mac[E1000_MAC_SIZE];

// Transmit structures
static struct tx_desc tx_desc_array[TX_RING_SIZE] __attribute__((aligned(128)));
static char tx_pkt_bufs[TX_RING_SIZE][TX_PKT_SIZE];
static uint32_t tx_desc_tail = 0;

// Receive structures  
static struct rx_desc rx_desc_array[RX_RING_SIZE] __attribute__((aligned(16)));
static char rx_pkt_bufs[RX_RING_SIZE][2048];
static uint32_t rx_desc_tail = 0;
static uint32_t rx_desc_tail_zero_copy = 0;

static int zero_copy = 0; // Flag for zero-copy mode

int e1000_irq = -1; // IRQ line for E1000 device

static int transmitted_packets = 0; // pointer for transmitted packets

static void
e1000_init_tx(void);
static void
e1000_init_rx(void);


uint16_t
e1000_eeprom_read(uint16_t addr)
{
    e1000_reg_base[E1000_EERD / 4] = (addr << 8) | E1000_EERD_START;

    while (!(e1000_reg_base[E1000_EERD / 4] & E1000_EERD_DONE))
        ;

    return (e1000_reg_base[E1000_EERD / 4] >> 16) & 0xFFFF;
}

void
e1000_read_mac(uint8_t *mac)
{
    uint16_t word;

    word = e1000_eeprom_read(0);
    mac[0] = word & 0xFF;
    mac[1] = (word >> 8) & 0xFF;

    word = e1000_eeprom_read(1);
    mac[2] = word & 0xFF;
    mac[3] = (word >> 8) & 0xFF;

    word = e1000_eeprom_read(2);
    mac[4] = word & 0xFF;
    mac[5] = (word >> 8) & 0xFF;
}
void e1000_get_mac(uint8_t *mac1){
    memmove(mac1, mac, 6);
}


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

static int
e1000_rx_packet_available(void)
{
    struct rx_desc *desc = &rx_desc_array[(rx_desc_tail + 1) % RX_RING_SIZE];
    //cprintf("desc->status & 0x01: %x\n", desc->status & 0x01);
    return (desc->status & 0x01) != 0;
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
    cprintf("E1000: MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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
    
    
    e1000_read_mac(mac);

    // Set Receive Address Low and High registers
    uint32_t ral = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
    uint32_t rah = mac[4] | (mac[5] << 8);

    // Write the MAC address to the Receive Address Low and High registers
    e1000_write_reg(E1000_RAL, ral);
    e1000_write_reg(E1000_RAH, rah | E1000_RAH_AV);

    cprintf("E1000: Receive descriptor base address set to %08x\n", PADDR(rx_desc_array));

    // Set receive descriptor length
    e1000_write_reg(E1000_RDLEN, sizeof(rx_desc_array));
    
    // Initialize head and tail pointers
    e1000_write_reg(E1000_RDH, 0);
    e1000_write_reg(E1000_RDT, RX_RING_SIZE - 1); // Tail points to last available descriptor
    rx_desc_tail = RX_RING_SIZE - 1;
    rx_desc_tail_zero_copy = RX_RING_SIZE - 1;
    //cprintf("E1000 rx_desc_tail initialized to %d\n", rx_desc_tail);
    
    // Configure RCTL register
    // EN: Enable receive
    // BAM: Broadcast Accept Mode
    // BSIZE: Buffer size (2048 bytes)
    // SECRC: Strip Ethernet CRC
    uint32_t rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC | E1000_RCTL_SZ_2048; // Set buffer size to 2048 bytes
    // uint32_t rctl = (1 << 1) |    // EN
    //                 (1 << 15) |   // BAM
    //                 (0 << 16) |   // BSIZE = 2048
    //                 (1 << 26);    // SECRC
    e1000_write_reg(E1000_RCTL, rctl);
    cprintf("E1000: RCTL set to %08x\n", rctl);
    //e1000_write_reg(E1000_IMS, E1000_ICR_RXT0);
    e1000_write_reg(E1000_IMS, E1000_ICR_RXT0 | E1000_ICR_RXDMT0 | E1000_ICR_TXDW | E1000_ICR_RXO | E1000_ICR_RXSEQ | E1000_ICR_LSC);


    cprintf("E1000: Receive initialized\n");
}

physaddr_t
user_va_to_pa(volatile struct Env *env, void *va) {
    pte_t *pte = NULL;
    struct PageInfo *pp = page_lookup(env->env_pgdir, va, &pte);
    if (!pp || !pte || !(*pte & PTE_P)) {
        panic("user_va_to_pa: Address %p not mapped in env %08x\n", va, env->env_id);
    }
    physaddr_t pa = (PTE_ADDR(*pte)) | ((uintptr_t)va & 0xFFF);
    return pa;
}

// Transmit a packet
int
e1000_transmit(void *data, size_t len)
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
        return -E_TX_FULL;
    }
    

    cprintf("E1000 tx: Transmitting packet number %d of length %d\n", tail, len);
    desc->addr = user_va_to_pa(curenv, data); // Set physical address of buffer
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

// Get transmit queue status for debugging
void
e1000_rx_status(void)
{
    uint32_t head = e1000_read_reg(E1000_RDH);
    uint32_t tail = e1000_read_reg(E1000_RDT);

    
    cprintf("E1000 TX: head=%d, tail=%d, sw_tail=%d\n", 
            head, tail, rx_desc_tail);

    // Print descriptor status
    int i;
    for (i = 0; i < 6; i++) {
        if (rx_desc_array[i].status & E1000_RXD_STAT_DD) {
            cprintf("  desc[%d] at addr %p: DONE\n", i, rx_desc_array[i].addr);
        } else {
            cprintf("  desc[%d] at addr %p: PENDING\n", i, rx_desc_array[i].addr);
        }
    }
}

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
            cprintf("  desc[%d] at addr %p: DONE\n", i, tx_desc_array[i].addr);
        } else {
            cprintf("  desc[%d] at addr %p: PENDING\n", i, tx_desc_array[i].addr);
        }
    }
}


// Receive a packet
int
e1000_rx(void *data, size_t len)
{
    if(!zero_copy) {
        cprintf("E1000: Zero-copy mode is disabled, cannot receive packets\n");
        return -E_INVAL;
    }
    uint32_t next_tail = (rx_desc_tail + 1) % RX_RING_SIZE;
    cprintf("E1000 rx: Checking descriptor %d\n", next_tail);
    struct rx_desc *desc = &rx_desc_array[next_tail];
    if (!(desc->status & 0x01)) { // DD bit
        return -E_RX_EMPTY;
    }
    
    size_t pkt_len = desc->length;
    if (pkt_len > len) {
        return -E_BUF_TOO_SMALL;
    }

    __asm__ volatile ("" ::: "memory");
    len = pkt_len;
    
    // Clear descriptor status for reuse
    desc->status = 0;
    rx_desc_tail = next_tail;

    //will update the tail pointer when server finished to use it

    return pkt_len; // Return length of received packet
}


// E1000 interrupt handler
void
e1000_intr(void)
{
    uint32_t icr = e1000_read_reg(E1000_ICR);
    cprintf ("E1000 245: Interrupt received\n");  
    

 // Handle transmit interrupts
    if (icr & E1000_ICR_TXDW) {
        //find the last packet that was transmitted
        cprintf("E1000 250: Transmit done interrupt received\n");
        transmitted_packets = e1000_read_reg(E1000_TDH);

        //cprintf("E1000 267: Transmit done interrupt\n");
        // Wake up any environment blocked on transmit, if applicable
        if (transmit_blocked_env) {
            int result = e1000_transmit((void *)transmit_syscall_dstva, transmit_syscall_len);
            transmit_blocked_env->env_tf.tf_regs.reg_eax = 0; // Success
            transmit_blocked_env->env_status = ENV_RUNNABLE;
            transmit_blocked_env = NULL;
        }
     }

    // Handle receive interrupts
    if (icr & (E1000_ICR_RXT0 | E1000_ICR_RXDMT0)) {
        if(!zero_copy) {
        cprintf("E1000: Zero-copy mode is disabled, cannot handle interrupts\n");
        return;
      }
        // Check if there's an environment blocked on receive
      //  cprintf("E1000 252: Receive interrupt\n");
        if (recv_blocked_env && e1000_rx_packet_available()) {
            // Receive packet into kernel buffer
            int result = e1000_rx(kernel_rx_buffer, sizeof(kernel_rx_buffer));
            //hexdump("E1000: Received packet data: ", kernel_rx_buffer, result);
            if (result > 0) {
                cprintf("E1000: Received %d bytes, waking up blocked environment\n", result);
                
                // Store result for syscall to copy later
                recv_result_len = result;
                
                // Set return value and mark environment as runnable
                physaddr_t phys_addr = user_va_to_pa(recv_blocked_env, (void *)recv_syscall_dstva);
                //void *virt_addr_ptr = KADDR(phys_addr);
                //memmove((void *)KADDR(phys_addr), kernel_rx_buffer, result);
                recv_blocked_env->env_tf.tf_regs.reg_eax = result;
                recv_blocked_env->env_status = ENV_RUNNABLE;
                
                // Clear blocked environment
                recv_blocked_env = NULL;
            } else {
                cprintf("E1000: Failed to receive packet, result = %d\n", result);
            }
    }
}
    
   
    cprintf("E1000 275: Interrupt handled, clearing EOI\n");
    // Clear interrupt on LAPIC
    //lapic_eoi();
}

void print_all_status_rx(void) {
    cprintf("E1000 RX Status:\n");
    cprintf("  Head: %d, Tail: %d, Software Tail: %d\n", 
            e1000_read_reg(E1000_RDH), e1000_read_reg(E1000_RDT), rx_desc_tail);
    
    // Print each descriptor's status
    int i;
    cprintf("  sizeof(void*) = %d and sizeof(rx_desc_array[i].addr) = %d\n", 
            sizeof(void*), sizeof(rx_desc_array[0].addr)); 
    for (i = 0; i < 7; i++) {
        cprintf("  Descriptor %d: addr=%08x, length=%d, status=%02x\n", 
                i, KADDR(rx_desc_array[i].addr), rx_desc_array[i].length, rx_desc_array[i].status);
    }
}


void e1000_set_recv_blocked_env(struct Env *env, uintptr_t dstva, size_t len) {
    recv_syscall_dstva = dstva;
    recv_blocked_env = env;
    recv_syscall_len = len;
}

void e1000_set_transmit_blocked_env(struct Env *env, uintptr_t dstva, size_t len) {
    transmit_syscall_dstva = dstva;
    transmit_blocked_env = env;
    transmit_syscall_len = len;
}

struct Env *e1000_get_recv_blocked_env(void) {
    return (struct Env *)recv_blocked_env;
}




void e1000_set_recv_buffers(struct Env *env, uintptr_t dstva, size_t len) {
    //set the buffers to be physical addresses of va dstva
    int i =0;
    for (i = 0; i < RX_RING_SIZE; i++) {
        physaddr_t pa = user_va_to_pa(env, (void *)dstva + i * len);
        rx_desc_array[i].addr = pa;
    }
    zero_copy = 1; // Enable zero-copy mode
    cprintf("E1000: rx_desc_array[0].addr = %08x\n", rx_desc_array[0].addr);
    cprintf("E1000: Set receive buffers for env %08x at dstva %p with len %d\n", 
            env->env_id, (void *)dstva, len);
}


void e1000_advance_rx_tail(void) {
    // Advance the RX tail pointer
    rx_desc_tail_zero_copy = (rx_desc_tail_zero_copy + 1) % RX_RING_SIZE;
    e1000_write_reg(E1000_RDT, rx_desc_tail_zero_copy);
    cprintf("E1000: Advanced RX tail to %d\n", rx_desc_tail_zero_copy);
}


int e1000_get_idx_of_transmitted_packet(void) {
    // Return the index of the last transmitted packet
    return transmitted_packets;
}