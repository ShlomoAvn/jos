// program to cause a breakpoint trap

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	asm volatile("int $3");
	int a=5;
	cprintf("Hello, world! %d\n", a);
}

