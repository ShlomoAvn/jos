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

#define TX_RING_SIZE 64
#define TX_PKT_SIZE 1518

#define E1000_TDLEN    0x03808
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */


#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_CT_SHIFT    4  
#define E1000_TCTL_COLD_SHIFT  12


//#define E1000_TXD_CMD_EOP    0x01000000 /* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02000000 /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC     0x04000000 /* Insert Checksum */
//#define E1000_TXD_CMD_RS     0x08000000 /* Report Status */
#define E1000_TXD_CMD_EOP (1 << 0)  // End of Packet
#define E1000_TXD_CMD_RS  (1 << 3)  // Report Status

#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */

#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */
struct tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
};

int e1000_attach(struct pci_func *pcif);
int e1000_transmit(void *data, size_t len);
#endif	// JOS_KERN_E1000_H

