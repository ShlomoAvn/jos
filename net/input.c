#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;
static char packet_buffer[2048];

 void
sleep(int msec)
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
    
   //cprintf("NS Input: Simple mode started\n");
    
    while (1) {
        int packet_len, r;
        
        // Receive packet from driver
        packet_len = sys_net_recv(packet_buffer, sizeof(packet_buffer));
        if (packet_len < 0) {
            continue;  // Error or no packet
        }
        
        // Fill IPC structure
        nsipcbuf.pkt.jp_len = packet_len;
        memmove(nsipcbuf.pkt.jp_data, packet_buffer, packet_len);
        
        // Send to network server (blocking until accepted)
        while ((r = sys_ipc_try_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_W | PTE_U)) < 0) {
            cprintf("NS Input: Send error: %d\n", r);
            if (r != -E_IPC_NOT_RECV) {
               //cprintf("NS Input: Send error: %e\n", r);
                break;
            }
            sys_yield();
        }
        sleep(50);
        
       //cprintf("NS Input: Packet forwarded (%d bytes)\n", packet_len);
    }
}


