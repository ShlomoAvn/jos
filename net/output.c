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
			cprintf("[%08x] output: ipc_recv failed: %e\n", thisenv->env_id, r);
			continue;
		}

		// Check if message is from the network server
		if (from_env != ns_envid) {
			cprintf("[%08x] output: received IPC from wrong environment %08x (expected %08x)\n", 
					ns_envid, from_env, ns_envid);
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

