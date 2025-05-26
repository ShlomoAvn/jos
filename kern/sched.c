#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Choose a user environment to run and run it.
void
sched_yield(void)
{
<<<<<<< HEAD
	struct Env *idle=curenv;
=======
	struct Env *idle = curenv;
	int idle_envid = (idle == NULL) ? 0 : ENVX(idle->env_id);
	int start_idx = (idle_envid + 1) % NENV;
	int i;
>>>>>>> recover-lost

	// DEBUG: Print current state
	// cprintf("sched_yield: current env %08x (idx %d)\n", 
	//         idle ? idle->env_id : 0, idle_envid);

	// Search for runnable environment starting after current one
	for (i = 0; i < NENV; i++) {
		int idx = (start_idx + i) % NENV;
		if (envs[idx].env_status == ENV_RUNNABLE) {
			// cprintf("sched_yield: switching to env %08x (idx %d)\n", 
			//         envs[idx].env_id, idx);
			env_run(&envs[idx]);
			// Never returns
		}
	}

<<<<<<< HEAD

	int idle_envid = (idle == NULL) ? -1 : ENVX(idle->env_id);
	int i=0, index=0;

	// search envs after idle
	for (i = idle_envid + 1; i < (NENV+idle_envid); i++) {
		if (envs[i%NENV].env_status == ENV_RUNNABLE) {
			env_run(&envs[i%NENV]);
		}
	}
/*
	// find from 1st env if not found
	for (i = 0; i < idle_envid; i++) {;
		if (envs[i].env_status == ENV_RUNNABLE) {
			env_run(&envs[i]);
		}
	}
*/
	// if still not found, try idle
	if(idle  && idle->env_status == ENV_RUNNING) {
		env_run(idle);
	}

=======
	// If no runnable environment found, try to continue with current one
	if (idle && idle->env_status == ENV_RUNNING) {
		//cprintf("sched_yield: continuing with current env %08x\n", idle->env_id);
		env_run(idle);
		// Never returns
	}

	//cprintf("sched_yield: no runnable environments, halting\n");
>>>>>>> recover-lost
	// sched_halt never returns
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		//cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}

