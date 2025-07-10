#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;
// 128 page-aligned buffers for zero-copy receive
#define NBUFS 128
#define BUFSZ 2048
static char __attribute__((aligned(PGSIZE))) packet_buffers[NBUFS][BUFSZ];
 void
sleep_input(int msec)
{
       unsigned now = sys_time_msec();
       unsigned end = now + msec;

       if ((int)now < 0 && (int)now > -MAXERROR)
               panic("sys_time_msec: %e", (int)now);

       while (sys_time_msec() < end)
               sys_yield();
}






// Alternative simpler implementation without acknowledgments
void
input(envid_t ns_envid)
{
    binaryname = "ns_input";
    // Register all buffers with the kernel/E1000 driver
    sys_e1000_register_rx_buffer(packet_buffers, BUFSZ);
    int buf_idx = 0;
   //cprintf("NS Input: Simple mode started\n");
    
    while (1) {
        char *packet_buffer = packet_buffers[buf_idx];
        int packet_len, r;
        
        // Receive packet from driver
        packet_len = sys_net_recv(packet_buffer, BUFSZ);
        if (packet_len < 0) {
            continue;  // Error or no packet
        }
        
        // Fill IPC structure
        nsipcbuf.pkt.jp_len = packet_len;

        if (packet_len > BUFSZ) {
            cprintf("NS Input: Packet too large (%d bytes), skipping\n", packet_len);
            continue;  // Skip oversized packets
        }

        // Copy packet data to IPC buffer, happen here because of test's expectations,
        // but can be anywhere and not only copy, just need to syscall after, to tell the receiver it was copied and liberate the buffer
        memcpy(nsipcbuf.pkt.jp_data, packet_buffer, packet_len);
        sys_advance_register_rx_buffer();
        
        
        // Send to network server (blocking until accepted)
        while ((r = sys_ipc_try_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_W | PTE_U)) < 0) {
            cprintf("NS Input: Send error: %d\n", r);
            if (r != -E_IPC_NOT_RECV) {
               //cprintf("NS Input: Send error: %e\n", r);
                break;
            }
            sys_yield();
        }
        buf_idx = (buf_idx + 1) % NBUFS;
        sleep_input(50);
        
    }
}


