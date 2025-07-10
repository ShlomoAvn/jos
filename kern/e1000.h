#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#include <inc/x86.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/types.h>
#include <kern/pci.h>

// Register Offsets
#define E1000_STATUS   0x00008
#define E1000_TCTL     0x00400
#define E1000_RCTL     0x00100

// Bit masks
#define E1000_TCTL_EN  0x00000002
#define E1000_RCTL_EN  0x00000002

#define TX_RING_SIZE 32
#define TX_PKT_SIZE 1518
#define RX_RING_SIZE    128   // Number of receive descriptors

#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */
#define E1000_ICR_RXT0          0x00000080 /* rx timer intr (ring 0) */
#define E1000_ICR_RXDMT0        0x00000010 /* rx desc min. threshold (0) */
#define E1000_ICR_TXDW          0x00000001 /* Transmit desc written back */
#define E1000_ICR_SRPD          0x00010000
#define E1000_ICR_RXO           0x00000040 /* rx overrun */
#define E1000_ICR_RXSEQ         0x00000008 /* rx sequence error */
#define E1000_ICR_LSC           0x00000004 /* Link Status Change */


#define E1000_ICR      0x000C0  /* Interrupt Cause Read - R/clr */

#define E1000_RAL       0x05400  /* Receive Address - RW Array */
#define E1000_RAH       0x05404  /* Receive Address - RW Array */
#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */

#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */

//#define E1000_RCTL_BSIZE_2048   (0 << 16)   // 2048 byte buffers
#define E1000_RCTL_BSIZE_4096   ((3 << 16) | (1 << 25))
#define E1000_RCTL_BSIZE_8192   ((2 << 16) | (1 << 25))
#define E1000_RCTL_BSIZE_16384  ((1 << 16) | (1 << 25))

#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */

#define E1000_TDLEN    0x03808
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */

#define E1000_RCTL     0x00100  // Receive Control Register
#define E1000_RDBAL    0x02800  // Receive Descriptor Base Address Low
#define E1000_RDBAH    0x02804  // Receive Descriptor Base Address High
#define E1000_RDLEN    0x02808  // Receive Descriptor Length
#define E1000_RDH      0x02810  // Receive Descriptor Head
#define E1000_RDT      0x02818  // Receive Descriptor Tail
#define E1000_RDTR     0x02820  /* RX Delay Timer - RW */
#define E1000_MTA      0x05200  /* Multicast Table Array - RW Array */
#define E1000_MAC_SIZE 6

#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_CT_SHIFT    4  
#define E1000_TCTL_COLD_SHIFT  12

#define E1000_EERD    0x00014
#define E1000_EERD_START (1 << 0)
#define E1000_EERD_DONE  (1 << 4)

#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */


//#define E1000_TXD_CMD_EOP    0x01000000 /* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02000000 /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC     0x04000000 /* Insert Checksum */
//#define E1000_TXD_CMD_RS     0x08000000 /* Report Status */
#define E1000_TXD_CMD_EOP (1 << 0)  // End of Packet
#define E1000_TXD_CMD_RS  (1 << 3)  // Report Status

#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */

#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */

// Environment blocked on receive
volatile static struct Env *recv_blocked_env = NULL;
static uint32_t recv_syscall_dstva;
static size_t recv_syscall_len;

volatile static struct Env *transmit_blocked_env = NULL;
static uint32_t transmit_syscall_dstva;
static size_t transmit_syscall_len;

static char kernel_rx_buffer[2048];
static int recv_result_len = 0;  

extern int e1000_irq; 

struct tx_desc {
    uint64_t addr;      // Buffer physical address
    uint16_t length;    // Packet length
    uint8_t cso;        // Checksum offset
    uint8_t cmd;        // Command field
    uint8_t status;     // Status field
    uint8_t css;        // Checksum start
    uint16_t special;   // Special field
} __attribute__((packed));

// Receive descriptor structure
struct rx_desc {
    uint64_t addr;      // Buffer physical address
    uint16_t length;    // Packet length
    uint16_t checksum;  // Packet checksum
    uint8_t status;     // Descriptor status
    uint8_t errors;     // Descriptor errors
    uint16_t special;   // Special field
} __attribute__((packed));

// int e1000_attach(struct pci_func *pcif);
// int e1000_transmit(void *data, size_t len);

int e1000_rx(void *data, size_t len);
// void e1000_init_tx(void);
// void e1000_init_rx(void);
void e1000_rx_status(void);
// void e1000_rx_status(void);
// void e1000_write_reg(uint32_t reg, uint32_t value);
// uint32_t e1000_read_reg(uint32_t reg);
int e1000_attach(struct pci_func *pcif);
int e1000_transmit(void *data, size_t len);
void e1000_intr(void);

void e1000_set_recv_blocked_env(struct Env *env, uintptr_t dstva, size_t len);
struct Env *e1000_get_recv_blocked_env(void);
void print_all_status_rx(void);
void print_icr(void);
void e1000_get_mac(uint8_t *mac);

void e1000_set_transmit_blocked_env(struct Env *env, uintptr_t dstva, size_t len);

void e1000_set_recv_buffers(struct Env *env, uintptr_t dstva, size_t len);

void e1000_advance_rx_tail(void);
int e1000_get_idx_of_transmitted_packet(void);
#endif	// JOS_KERN_E1000_H

