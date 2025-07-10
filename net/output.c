#include <inc/ns.h>
#include <inc/lib.h>
#include <inc/error.h>
#include "ns.h"

#define N_TX_BUFFERS 32
#define BUF_SIZE  2048 // Size of each transmit buffer, must be large enough for largest packet

static int last_freed = (N_TX_BUFFERS - 1) % N_TX_BUFFERS; // Last index used for transmission

struct TxBuffer {
	void *va;
	int in_use;
	//envid_t pending_response;  // Environment waiting for this buffer to be free
};

struct TxBuffer tx_buffers[N_TX_BUFFERS];

void
tx_buffers_init(void)
{
	int i;
	for (i = 0; i < N_TX_BUFFERS; i++) {
		void *va = (void *)UTEMP + i * PGSIZE;
		int r = sys_page_alloc(thisenv->env_id, va, PTE_W | PTE_U | PTE_P);
		if (r < 0)
			panic("Failed to allocate tx buffer %d: %e", i, r);
		
		tx_buffers[i].va = va;
		tx_buffers[i].in_use = 0;
		//tx_buffers[i].pending_response = 0;
	}
}

// Find a free buffer, mark it in use, and return its index, or -1 if none
int
tx_buffer_alloc(void **va)
{
	int i;
	for (i = 0; i < N_TX_BUFFERS; i++) {
		if (!tx_buffers[i].in_use) {
			tx_buffers[i].in_use = 1;
			*va = tx_buffers[i].va;
			return i;
		}
	}
	return -1;
}

void
tx_buffer_free(int idx)
{
	if (idx >= 0 && idx < N_TX_BUFFERS) {
		tx_buffers[idx].in_use = 0;
	}
}

// Check for completed transmissions and free buffers
void
check_tx_completions(void)
{
	int tdh = sys_e1000_get_TDH(); // Get the current Transmit Descriptor Head
	if(tdh < last_freed){
		tdh += N_TX_BUFFERS; 
	} 
	else if (tdh -last_freed == 1) {
		//can be confused with no completed transmissions
		return;
	}
	int i;
	for (i = last_freed + 1; i < tdh; i++) {
		if (tx_buffers[i % N_TX_BUFFERS].in_use) {
			// Check if this buffer's transmission is complete
			cprintf("Freeing tx buffer %d\n", i % N_TX_BUFFERS);
				tx_buffer_free(i % N_TX_BUFFERS);
		}
	}
	last_freed = (tdh -1) % N_TX_BUFFERS; // Update last freed index
}


 void
sleep_output(int msec)
{
       unsigned now = sys_time_msec();
       unsigned end = now + msec;

       if ((int)now < 0 && (int)now > -MAXERROR)
               panic("sys_time_msec: %e", (int)now);

       while (sys_time_msec() < end)
               sys_yield();
}

// Alternative version that sends immediate response but tracks completion
void
output(envid_t ns_envid)
{
	sleep_output(200); // Allow time for initialization
	binaryname = "ns_output";
	tx_buffers_init();

	while (1) {
		envid_t whom;
		int perm;
		
		// Check for any completed transmissions first
		check_tx_completions();
		
		// Wait for a packet from user
		cprintf("Waiting for packet...last_transmit_idx: %d\n", last_freed);
		int r = ipc_recv(&whom, (void*)REQVA, &perm);
		if (r < 0)
			continue;

		// Check if we received a valid page
		if (!(perm & PTE_P)) {
			ipc_send(whom, -E_INVAL, 0, 0);
			continue;
		}
		cprintf("Received packet from %d\n", whom);
		struct jif_pkt *pkt = (struct jif_pkt*)REQVA;
		
		// Validate packet length
		if (pkt->jp_len > BUF_SIZE) {
			ipc_send(whom, -E_INVAL, 0, 0);
			continue;
		}
		cprintf("Packet length: %d\n", pkt->jp_len);
		void *va;
		int idx = tx_buffer_alloc(&va);
		
		if (idx < 0) {
			// No buffers available - send error
			ipc_send(whom, -E_NO_MEM, 0, 0);
			continue;
		}

		// Copy packet data to our buffer
		memmove(va, pkt->jp_data, pkt->jp_len);
		cprintf("Copied packet data to buffer %d at %p\n", idx, va);
		// Send to hardware using virtual address
		r = sys_e1000_transmit(va, pkt->jp_len);
		
		if (r < 0) {
			// Transmission failed immediately
			tx_buffer_free(idx);
			ipc_send(whom, r, 0, 0);
		} 
		//last_transmit_idx = (last_transmit_idx + 1) % N_TX_BUFFERS;
		cprintf("Transmitted packet %d, last_transmit_idx now %d\n", last_freed, last_freed);
	}
}