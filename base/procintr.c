
/////////////////////////////////////////////////////////////////////
// Copyright (C) 2008 Department of Computer SCience & Engineering
// National Institute of Technology - Warangal
// Andhra Pradesh 
// INDIA
// http://www.nitw.ac.in
//
// This software is developed for software based DSM system Project
// This is an experimental platform, freely useable and modifiable
// 
//
// Team Members:
// Prof. T. Ramesh 			Chapram Sudhakar
// A. Sundara Ramesh  			T. Santhosi
// K. Suresh 				G. Konda Reddy
// Vivek Kumar 				T. Vijaya Laxmi
// Angad 				Jhahnavi
//////////////////////////////////////////////////////////////////////

/* 
 * This file contains routines for handling processor exceptions.
 */

#include "mklib.h"
#include "process.h"
#include "vmmap.h"
#include "alfunc.h"

extern volatile struct kthread *current_thread;
extern volatile struct process *current_process;
void (* IRQ_handlers[16])(int irq_no);

void syscall_pthread_exit(void *retval);
void default_irq_handler(int irq_no);
void do_divide_error(struct PAR_REGS * regs, int error_code)
{
	print_p(" Error: Divide by zero error !");
	print_context(regs,error_code);
	syscall_pthread_exit(NULL);
}

void do_coprocessor_error(struct PAR_REGS * regs, int error_code)
{
	struct fpu_state fps, *fpsp;

	fpsp = &fps;
	__asm__ ( "fnsave %0" : "=m" (* ((long *) fpsp)) );
	/* save_fpu_state(fpsp); */

	restore_eflags(regs->eflags); /* Reenable interrupts */
	print_p("status : %x\n",fpsp->fpuenv.swd);
	if (fpsp->fpuenv.swd & 0x0001)
		print_p(" Error: Invalid FPU operation code !");
	if (fpsp->fpuenv.swd & 0x0002)
		print_p(" Error: Denormalized operand !");
	if (fpsp->fpuenv.swd & 0x0004)
		print_p(" Error: Divide by zero !");
	if (fpsp->fpuenv.swd & 0x0008)
		print_p(" Error: FPU stack overflow !");
	if (fpsp->fpuenv.swd & 0x0010)
		print_p(" Error: FPU stack underflow !");
	if (fpsp->fpuenv.swd & 0x0020)
		print_p(" Error: Floating point operand domain error !");

	/*
	 * Some of these must be corrected and can be continued.
	 * But present this process will be terminated.
	 */
	print_context(regs,error_code);
	syscall_pthread_exit(NULL);
}

void do_device_not_available(struct PAR_REGS *regs, int error_code)
{
	//printk("do device not available : %s\n", current_thread->kt_name);
	math_state_restore((struct fpu_state *)&current_thread->kt_fps);
}

void do_simd_coprocessor_error(struct PAR_REGS *regs, int error_code)
{
	/*
	 * No support for simd extensions
	 */
	print_p(" Error: SIMD extensions are not supported !");
	syscall_pthread_exit(NULL);
}

void do_debug(struct PAR_REGS *regs, int error_code)
{
	/*
	 * No support for debugging.
	 */
	print_p(" Sorry. No support for debugging !");

	syscall_pthread_exit(NULL);
}

void do_nmi(struct PAR_REGS *regs, int error_code)
{
	unsigned char reason = in_byte(0x61);
	restore_eflags(regs->eflags); /* Reenable iterrupts */

	if (reason & 0xc0)
	{
		if (reason & 0x80)
			printk(" Fatal Error: Memory parity error !");
		if (reason & 0x40)
			printk(" Fatal Error: IO - Check error !");
		syscall_pthread_exit(NULL);
	}

	printk(" Warning: Unknown NMI interrupt !");
	return;
}

void do_inter3(struct PAR_REGS *regs, int error_code)
{
	/*
	 * Do nothing and return.
	 */
	return;
}

void do_overflow(struct PAR_REGS *regs, int error_code)
{
	/*
	 * Just terminate the task.
	 */
	print_p(" Error: Divide overflow !");
	syscall_pthread_exit(NULL);
}

void do_bounds(struct PAR_REGS * regs, int error_code)
{
	/*
	 * Just terminate the task.
	 */
	print_p(" Error: Bounds checking fault !");
	syscall_pthread_exit(NULL);
}

void do_invalid_TSS(struct PAR_REGS * regs, int error_code)
{
	/*
	 * This is caused only by the kernel. Should not happen.
	 */
	print_p(" Fatal Error: Accessing invalid TSS !");
	printk(" Fatal Error: Accessing invalid TSS !");
	syscall_pthread_exit(NULL);
}

void do_invalid_op(struct PAR_REGS * regs, int error_code)
{
	print_p(" Error: Invalid opcode execution !");
	print_context(regs,error_code);
	syscall_pthread_exit(NULL);
}

void do_coprocessor_segment_overrun(struct PAR_REGS *regs, int error_code)
{
	print_p(" Error: Coprocessor segment overrun !");
	syscall_pthread_exit(NULL);
}

void do_double_fault(struct PAR_REGS *regs, int error_code)
{
	print_p(" Fatal Error: Double fault occured !");
	print_context(regs,error_code);
}

void print_gdt(int start, int n);
void do_segment_not_present(struct PAR_REGS * regs, int error_code)
{
	/*
	 * Segment is always marked as present. If this is
	 * arised means some mistake with user program.
	 */

        print_gdt(1, 5);
        print_gdt(20, 5);

	
	print_p(" Fatal Error: Segmentation violation. !");
	print_context(regs,error_code);
	syscall_pthread_exit(NULL);
}

void do_stack_segment( struct PAR_REGS * regs, int error_code)
{
	/*
	 * Stack segment not present case will not araise. Other 
	 * possibility is limit exceeded. For this stack must be 
	 * extended ( more space must be allotted). Errorcode 
	 * contains segment selector for not present stack.
	 * other wise error code will be zero for normal overflow.
	 */
	
	restore_eflags(regs->eflags); /* Reenable iterrupts */
	if (error_code != 0)
	{
		print_p(" Fatal error: Stack segment not present. ! %s\n",current_thread->kt_name);
		syscall_pthread_exit(NULL);
	}
}

void do_general_protection(struct PAR_REGS * regs, int error_code)
{
	print_p(" Error: General protection fault!");
	print_context(regs,error_code);
	syscall_pthread_exit(NULL);;
}

void do_alignment_check(struct PAR_REGS *regs, int error_code)
{
	print_p(" Error: Un aligned memory access. !");
	syscall_pthread_exit(NULL);
}	

void do_machine_check(struct PAR_REGS *regs, int error_code)
{
	printk(" Fatal Error: Unexpected machine check exception. !");
}

void do_spurious_interrupt_bug(struct PAR_REGS * regs, int error_code)
{
	printk(" Warning: Unknown interrupt occured. !");
}

#define PGERROR_PRESENT		0x00000001
#define PGERROR_WRITE		0x00000002
#define PGERROR_USERMODE	0x00000004
unsigned long get_cr2(void);
void enqueue_pagerequest(unsigned long pageno, short event, short rw_req);

//-----------------------------------

// If data segment related :
// (1) Check for validity of the address.
// (2) Check weather it is a parallel process 
// (3) If parallel process make a call to consistency specific 
// 	page fault handler
//
// Stack related page faults:
// (1) Same thread's stack, then what ever is the instruction if that 
// 	reference is within its range (4 MB) then enlarge it.
// (2) Current thread referring to the stack of other thread then
//    (i) If writing instruction then declare error. Otherwise
//    (ii) If that target thread is local (should not happen at prsent, 
//    		because no vm support). Delare wrong
//    (iii) If that target thread is remote send a message for 
//    		getting a copy of that page. (using memory consistency model)
void do_page_fault(struct PAR_REGS *regs, int error_code)
{
	unsigned long cr3, cr2, newstart;
	int rwreq;
	int ret; //, i;
	unsigned int old_cr0;
	struct memregion *mreg;
	unsigned int upperlimit, stackend;
	PTE *pte;
	PDE *pde;
	int oldbusy;

	cr3 = get_cr3();
	cr2 = get_cr2();

	old_cr0 = clear_paging();
	// Linear address is in kernel range
	// Read instruction or Write instruction?
	if (error_code & PGERROR_WRITE) rwreq = 1;
	else rwreq = 0;

	pte = vm_getptentry(current_process->proc_vm, (cr2 >> 12) & 0x000fffff);
	oldbusy =  pte_mark_busy((unsigned long *)pte);
	while (oldbusy && pte_mark_busy((unsigned long *)pte))
	{
		scheduler();
	}

	if (oldbusy) // do not process it now, some state might havebeen changed
	{
		pte_unmark_busy((unsigned long *)pte);
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
		return;
	}
	pde = vm_getpdentry(current_process->proc_vm, (cr2 >> 12) & 0x000fffff);

	if (cr2 < KERNEL_MEMMAP) // Should not happen!
		goto err_return;

	// Linear address is in code segment range?
	if (cr2 < USERDATA_SEGMENT_START) // At present should not happen (No VM)
		goto err_return;

	mreg = (struct memregion *)current_process->proc_vm->vm_mregion;
	while (mreg != NULL && mreg->mreg_type != SEGMENTTYPE_DATA) mreg = (struct memregion *)mreg->mreg_next;	
	if (mreg != NULL) upperlimit = mreg->mreg_endaddr;
	else upperlimit = USERDATA_SEGMENT_START;
	// Linear address is in data segment range
	if (cr2 < upperlimit)
	{
		// At present no VM support
		goto err_return;
	} // Linear address is in stack segment range
	else if (cr2 < 0xFDC00000)
	{
		// Is it current stack
		if ((current_thread->kt_ustackstart & 0xFFC00000) == (cr2 & 0xFFC00000)) // Stack range
		{
			// Ignoring all possibilities, just expand the size of the stack.
			stackend = (((current_thread->kt_ustackstart + 0x00400000 - 1) >> 22) << 22) - 1; // Round upwards to 4MB boundary - 1
			// What is the new start?
			newstart = cr2 & 0xfffff000;
			ret = vm_expanddownregion(current_process->proc_vm, newstart, stackend, 1, 1);
			printk("Stack being expanded : %x, %x, %x : ret : %d\n", stackend, newstart, current_thread->kt_ustackstart, ret);
			// What is amount of memory required?
			if (ret < 0)
			{
				// No memory available
				printk("No memory available!\n");
				goto err_return;
			}

			pte_unmark_busy((unsigned long *)pte);
			restore_paging(old_cr0, current_thread->kt_tss->cr3);
			// resume execution
			return;
		}
		else // Other thread's stack is referenced
		{
			// This is not required for normal processes,
			// other tha parallel processes so return error
			if (rwreq == 1) // Wrong 
				goto err_return;
			else goto err_return;
		}
	}
	// else error?

err_return:
	pte_unmark_busy((unsigned long *)pte);
	restore_paging(old_cr0, current_thread->kt_tss->cr3);
#ifdef	DEBUG
	syscall_pageinfo(current_process->proc_id, ((cr2 >> 12) & 0x000fffff));
	printk("Page fault [ %s ] at: %x error type : %x\n",current_thread->kt_name, cr2, error_code);
	print_context(regs,error_code);
#endif
	syscall_pthread_exit(NULL);
	return; // Control never comes here.
}
void default_irq_handler(int irq_no)
{
	printk("Unhandled irq : %x\n",irq_no);
	enable_irq(irq_no);
	return;
}
void do_unhandled_interrupt(struct PAR_REGS *regs, int vector_no)
{
	printk("Unhandled interrupt - Source : unknown\n");
}

void irq_handler(struct PAR_REGS *regs, int irq_no)
{
	IRQ_handlers[irq_no](irq_no);
}


void math_emulate(struct PAR_REGS *regs, int error_code)
{
	print_p("Math emulation is not yet supported. \n");
	return;
}

