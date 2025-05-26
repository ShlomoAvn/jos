/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

    TRAPHANDLER_NOEC(divide, T_DIVIDE)
    TRAPHANDLER_NOEC(debug, T_DEBUG)
    TRAPHANDLER_NOEC(nmi, T_NMI)
    TRAPHANDLER_NOEC(brkpt, T_BRKPT)
    TRAPHANDLER_NOEC(oflow, T_OFLOW)
    TRAPHANDLER_NOEC(bound, T_BOUND)
    TRAPHANDLER_NOEC(illop, T_ILLOP)
    TRAPHANDLER_NOEC(device, T_DEVICE)
    TRAPHANDLER(dblflt, T_DBLFLT)
    TRAPHANDLER(tss, T_TSS)
    TRAPHANDLER(segnp, T_SEGNP)
    TRAPHANDLER(stack, T_STACK)
    TRAPHANDLER(gpflt, T_GPFLT)
    TRAPHANDLER(pgflt, T_PGFLT)
    TRAPHANDLER_NOEC(fperr, T_FPERR)
    TRAPHANDLER(align, T_ALIGN)
    TRAPHANDLER_NOEC(mchk, T_MCHK)
    TRAPHANDLER_NOEC(simderr, T_SIMDERR)
	TRAPHANDLER_NOEC(syscall_handler, T_SYSCALL)
    TRAPHANDLER_NOEC(timer_handler, IRQ_OFFSET + 0)      /* Timer interrupt */
    TRAPHANDLER_NOEC(keyboard_handler, IRQ_OFFSET + 1)   /* Keyboard interrupt */
    TRAPHANDLER_NOEC(cascade_handler, IRQ_OFFSET + 2)    /* Cascade for 2nd PIC */
    TRAPHANDLER_NOEC(com2_handler, IRQ_OFFSET + 3)       /* COM2 serial port */
    TRAPHANDLER_NOEC(com1_handler, IRQ_OFFSET + 4)       /* COM1 serial port */
    TRAPHANDLER_NOEC(lpt2_handler, IRQ_OFFSET + 5)       /* LPT2 parallel port */
    TRAPHANDLER_NOEC(floppy_handler, IRQ_OFFSET + 6)     /* Floppy disk */
    TRAPHANDLER_NOEC(lpt1_handler, IRQ_OFFSET + 7)       /* LPT1 / spurious */
    TRAPHANDLER_NOEC(rtc_handler, IRQ_OFFSET + 8)        /* Real-time clock */
    TRAPHANDLER_NOEC(irq9_handler, IRQ_OFFSET + 9)       /* Redirected IRQ2 */
    TRAPHANDLER_NOEC(irq10_handler, IRQ_OFFSET + 10)     /* Reserved */
    TRAPHANDLER_NOEC(irq11_handler, IRQ_OFFSET + 11)     /* Reserved */
    TRAPHANDLER_NOEC(mouse_handler, IRQ_OFFSET + 12)     /* PS/2 mouse */
    TRAPHANDLER_NOEC(fpu_handler, IRQ_OFFSET + 13)       /* FPU / coprocessor */
    TRAPHANDLER_NOEC(primary_ata_handler, IRQ_OFFSET + 14) /* Primary ATA */
    TRAPHANDLER_NOEC(secondary_ata_handler, IRQ_OFFSET + 15) /* Secondary ATA */
/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
    pushl %ds;
    pushl %es;
    pushal;
    pushl $GD_KD;
    popl %ds;
    pushl $GD_KD;
    popl %es;
    pushl %esp;
    call trap;