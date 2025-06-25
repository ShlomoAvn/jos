#include <inc/ns.h>
#include <inc/lib.h>
#include <inc/error.h>

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	int r;
	envid_t from_env;
	int perm;

	while (1) {
		// Receive IPC message from network server
		r = ipc_recv(&from_env, &nsipcbuf, &perm);
		if (r < 0) {
			cprintf("output: ipc_recv failed: %e\n", r);
			continue;
		}

		// Check if message is from the network server
		if (from_env != ns_envid) {
			cprintf("output: received IPC from wrong environment %08x (expected %08x)\n", 
					from_env, ns_envid);
			continue;
		}

		// Check if it's an output request
		if (r != NSREQ_OUTPUT) {
			cprintf("output: received unexpected request type %d\n", r);
			continue;
		}

		// Validate packet length
		if (nsipcbuf.pkt.jp_len < 0 || nsipcbuf.pkt.jp_len > TX_PKT_SIZE) {
			cprintf("output: invalid packet length %d\n", nsipcbuf.pkt.jp_len);
			continue;
		}

		// Send packet to device driver
		// Keep trying until successful (handle E1000_TX_FULL case)
		while (1) {
			r = sys_e1000_transmit(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
			if (r == 0) {
				// Packet sent successfully
				break;
			} else if (r == -E_TX_FULL) {
				// Transmit queue is full, yield and try again
				// This prevents blocking the network server indefinitely
				sys_yield();
			} else {
				// Other error occurred
				cprintf("output: sys_e1000_transmit failed: %e\n", r);
				break;
			}
		}
	}
}

// Alternative implementation with more sophisticated error handling
void
output_robust(envid_t ns_envid)
{
	binaryname = "ns_output";

	int r;
	envid_t from_env;
	int perm;
	int retry_count;
	const int MAX_RETRIES = 1000; // Maximum retries before giving up

	cprintf("output: starting network output server\n");

	while (1) {
		// Receive IPC message from network server
		r = ipc_recv(&from_env, &nsipcbuf, &perm);
		if (r < 0) {
			cprintf("output: ipc_recv failed: %e\n", r);
			continue;
		}

		// Validate sender
		if (from_env != ns_envid) {
			cprintf("output: ignoring IPC from environment %08x (expected %08x)\n", 
					from_env, ns_envid);
			continue;
		}

		// Validate request type
		if (r != NSREQ_OUTPUT) {
			cprintf("output: ignoring request type %d (expected %d)\n", r, NSREQ_OUTPUT);
			continue;
		}

		// Validate packet
		if (nsipcbuf.pkt.jp_len < 0) {
			cprintf("output: invalid negative packet length %d\n", nsipcbuf.pkt.jp_len);
			continue;
		}

		if (nsipcbuf.pkt.jp_len > PGSIZE - sizeof(struct jif_pkt)) {
			cprintf("output: packet too large %d bytes (max %d)\n", 
					nsipcbuf.pkt.jp_len, PGSIZE - (int)sizeof(struct jif_pkt));
			continue;
		}

		// Handle zero-length packets
		if (nsipcbuf.pkt.jp_len == 0) {
			cprintf("output: warning - zero length packet\n");
			continue;
		}

		// Transmit packet with retry logic
		retry_count = 0;
		while (retry_count < MAX_RETRIES) {
			r = sys_e1000_transmit(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
			
			if (r == 0) {
				// Success
				break;
			} else if (r == -E_TX_FULL) {
				// Queue full - yield and retry
				retry_count++;
				if (retry_count % 100 == 0) {
					cprintf("output: transmit queue full, retry %d/%d\n", 
							retry_count, MAX_RETRIES);
				}
				sys_yield();
			} else {
				// Permanent error
				cprintf("output: sys_e1000_transmit failed with error %e\n", r);
				break;
			}
		}

		// Check if we exceeded retry limit
		if (retry_count >= MAX_RETRIES) {
			cprintf("output: gave up after %d retries, dropping packet\n", MAX_RETRIES);
		}
	}
}

// Debug version with packet inspection
void
output_debug(envid_t ns_envid)
{
	binaryname = "ns_output";

	int r;
	envid_t from_env;
	int perm;
	uint32_t packet_count = 0;

	cprintf("output: starting DEBUG network output server\n");

	while (1) {
		// Receive IPC message
		r = ipc_recv(&from_env, &nsipcbuf, &perm);
		if (r < 0) {
			cprintf("output: ipc_recv failed: %e\n", r);
			continue;
		}

		// Validate and process
		if (from_env != ns_envid || r != NSREQ_OUTPUT) {
			cprintf("output: invalid IPC: from=%08x (expect %08x), type=%d (expect %d)\n",
					from_env, ns_envid, r, NSREQ_OUTPUT);
			continue;
		}

		packet_count++;
		
		// Validate packet
		if (nsipcbuf.pkt.jp_len < 0 || nsipcbuf.pkt.jp_len > 1600) {
			cprintf("output: packet #%u invalid length %d\n", 
					packet_count, nsipcbuf.pkt.jp_len);
			continue;
		}

		// Print packet info
		cprintf("output: packet #%u, len=%d bytes\n", 
				packet_count, nsipcbuf.pkt.jp_len);

		// Optionally dump first few bytes
		if (nsipcbuf.pkt.jp_len > 0) {
			cprintf("  data: ");
			int i;
			for ( i = 0; i < 16 && i < nsipcbuf.pkt.jp_len; i++) {
				cprintf("%02x ", (unsigned char)nsipcbuf.pkt.jp_data[i]);
			}
			cprintf("\n");
		}

		// Transmit
		while (1) {
			r = sys_e1000_transmit(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
			if (r == 0) {
				cprintf("  transmitted successfully\n");
				break;
			} else if (r == -E_TX_FULL) {
				cprintf("  queue full, retrying...\n");
				sys_yield();
			} else {
				cprintf("  transmission failed: %e\n", r);
				break;
			}
		}
	}
}