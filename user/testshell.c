#include <inc/x86.h>
#include <inc/lib.h>

void wrong(int, int, int);

void
umain(int argc, char **argv)
{
	char c1, c2;
	int r, rfd, wfd, kfd, n1, n2, off, nloff;
	int pfds[2];
/*
	if ((r = exec(argv[1], (const char **)&argv[1])) < 0) {
        cprintf("testexec: exec fail: %e\n", r);
        }*/

	close(0);
	close(1);
	opencons();
	opencons();
	
	
	if ((rfd = open("testshell.sh", O_RDONLY)) < 0)
		panic("open testshell.sh: %e", rfd);
	if ((wfd = pipe(pfds)) < 0)
		panic("pipe: %e", wfd);
	wfd = pfds[1];

	cprintf("running sh -x < testshell.sh | cat\n");
	if ((r = fork()) < 0)
		panic("fork: %e", r);
	//cprintf("right before r==0 GGGGGG\n\n\n");
	if (r == 0) {
		//cprintf("entering r==0 GGGGGG\n\n\n");
		dup(rfd, 0);
		dup(wfd, 1);
		close(rfd);
		close(wfd);
		//cprintf("right before spawnl GGGGGG\n\n\n");
		if ((r = spawnl("/sh", "sh", "-x", 0)) < 0)
{
			//cprintf("SPAWN FAILED GGGGGG %d \n\n\n", r);
			panic("spawn: %e", r);
}
		//cprintf("right after spawnl GGGGGG\n\n\n");
		close(0);
		close(1);
		wait(r);
		exit();
	}
	
	//cprintf("right after r==0 GGGGGG\n\n\n");
	close(rfd);
	close(wfd);

	rfd = pfds[0];
	if ((kfd = open("testshell.key", O_RDONLY)) < 0)
		panic("open testshell.key for reading: %e", kfd);
	//cprintf("right after open testshell.key GGGGGG\n\n\n");
	nloff = 0;
	//cprintf("before loop\n\n\n");
	for (off=0;; off++) {
		n1 = read(rfd, &c1, 1);
		n2 = read(kfd, &c2, 1);
		//cprintf("n1 text is %s\n", c1);
		//cprintf("n2 text is %s\n", c2);
		if (n1 < 0)
			panic("reading testshell.out: %e", n1);
		if (n2 < 0)
			panic("reading testshell.key: %e", n2);
		if (n1 == 0 && n2 == 0)
			break;
		//cprintf("in loop\n");
		//cprintf("n1 is %d\n",n1);
		//cprintf("n2 is %d\n\n",n2);
		if (n1 != 1 || n2 != 1 || c1 != c2)
			wrong(rfd, kfd, nloff);
		if (c1 == '\n')
			nloff = off+1;
	}
	cprintf("shell ran correctly\n");
	
	
	
	breakpoint();
}

void
wrong(int rfd, int kfd, int off)
{
	char buf[100];
	int n;

	seek(rfd, off);
	seek(kfd, off);

	cprintf("shell produced incorrect output.\n");
	cprintf("expected:\n===\n");
	while ((n = read(kfd, buf, sizeof buf-1)) > 0)
		sys_cputs(buf, n);
	cprintf("===\ngot:\n===\n");
	while ((n = read(rfd, buf, sizeof buf-1)) > 0)
		sys_cputs(buf, n);
	cprintf("===\n");
	exit();
}

