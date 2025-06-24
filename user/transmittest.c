#include <inc/lib.h>
#include <inc/x86.h>


void
umain(int argc, char **argv)
{
    char pkt[60] = "Hello, E1000!";
    int r = sys_e1000_transmit(pkt, 60);
    if (r < 0)
        cprintf("Transmit failed: %e\n", r);
    else
        cprintf("Packet transmitted successfully!\n");
}