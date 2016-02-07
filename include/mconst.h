
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
// K. Sudhakar
// A. Sundara Ramesh  			T. Santhosi
// K. Suresh 				G. Konda Reddy
// Vivek Kumar 				T. Vijaya Laxmi
// Angad 				Jhahnavi
//////////////////////////////////////////////////////////////////////

#ifndef __MCONST_H
#define __MCONST_H

#define SYSCALL_VECTOR  0x30
#define IRQ0_7 		0x20
#define IRQ8_F		(IRQ0_7 + 8)


#define INT_CTL         0x20
#define INT_CTLMASK     0x21
#define INT2_CTL        0xa0
#define INT2_CTLMASK    0xa1
#define	ENABLE		0x20

#define KB_BUF_SIZE	256
#define NO_SYS_DESCRIPTORS	4

#define MAXDRIVE	4

#define MAXHOSTS		256
#define MAX_PKTSIZE	4194304 // 131072	// 128 KB
	/* Null desc, code desc, data desc, idle tss desc */
#define MAXPROCESSES	32
#define MAXTHREADS	256
#define MAXAPPLICATIONS	(MAXPROCESSES - 2) // One for sys, one for shell

#define MAXLOCKS	8192
#define MAXCONDITIONS	128
#define MAXBARRIERS	32
#define MAXPROCTHREADS	128

#define GDT_SIZE	(NO_SYS_DESCRIPTORS+MAXTHREADS+MAXPROCESSES + 16)
	/* Includes LDTs for user processes and Sys tasks and user tasks and interrupt tasks */
#define LDTDESC_START	(NO_SYS_DESCRIPTORS + MAXTHREADS + 16)	
#define LDTDESC_END	(GDT_SIZE - 1)
#define TASKDESC_START	(NO_SYS_DESCRIPTORS + 16)
#define TASKDESC_END	(NO_SYS_DESCRIPTORS + MAXTHREADS + 16 - 1)

#define IDT_SIZE	(SYSCALL_VECTOR + 8)	
	/* 8 for any other User level tasks */

#define KERNEL_CS	8
#define KERNEL_DS	16

#define NR_SYSCALLS	220	/* May not be correct now ******** */

#define TIME_QUANTUM	100	/* 100 Milleseconds */

#define MAX_RAM_SEGMENTS	10
#define KERNEL_STACK_SIZE	0x00004000 /* 16K - 4 pages */	

#define USER_STACK_SIZE	0x00010000	// 64 KB
#define KERNEL_MEMMAP   0x20000000      // Upto 512 MB reserved for kernel
#define MAX_PROCSIZE	0xD0000000	// 3.5 GB

#define MALLOC_MAX	0x01000000
#define PRIMALLOC_MAX	0x02000000
#define MAXPROCFILES	20
#define FTABLE_SIZE	100
#define MAXSOCKETS	50
#define NAME_MAX	255
#define PATH_MAX	500
#define SYNCHNAMEMAX	32
#define MAXACTIVEPACKETS	100
#define MAXWINDOWSIZE	8
#define MAXPRIORITY	160
#define MAXREADYQ	(MAXPRIORITY / 5)
#define MAX_SLCHANNELS	50

#define SOCKHANDLEBEGIN	FTABLE_SIZE
#define SOCKHANDLEEND	(SOCKHANDLEBEGIN+MAXSOCKETS)

#define UNUSEDPROC_COUNTMAX	10
#define UNUSEDTHR_COUNTMAX	25
#define MAXSEM			50
#define DEBUG	1
//#define SMP	0

#endif
