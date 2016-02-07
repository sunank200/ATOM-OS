
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


/* Constants and data structures for protected mode. */
#ifndef __SEGMENT_H
#define __SEGMENT_H
/* Privileges. */
#define INTR_PRIVILEGE   0	/* kernel and interrupt handlers */
#define TASK_PRIVILEGE   1
#define USER_PRIVILEGE   3

/* Selector bits. */
#define TI            0x04	/* table indicator */
#define RPL           0x03	/* requester privilege level */

/* Descriptor structure offsets. */
#define DESC_BASE        2	/* to base_low */
#define DESC_BASE_MIDDLE 4	/* to base_middle */
#define DESC_ACCESS      5	/* to access byte */
#define DESC_SIZE        8	/* sizeof (struct segdesc_s) */

/* Base and limit sizes and shifts. */
#define BASE_MIDDLE_SHIFT   16	/* shift for base --> base_middle */

/* Access-byte and type-byte bits. */
#define PRESENT       0x80	/* set for descriptor present */
#define DPL           0x60	/* descriptor privilege level mask */
#define DPL_SHIFT        5
#define SEGMENT       0x10	/* set for segment-type descriptors */

/* Access-byte bits. */
#define EXECUTABLE    0x08	/* set for executable segment */
#define CONFORMING    0x04	/* set for conforming segment if executable */
#define EXPAND_DOWN   0x04	/* set for expand-down segment if !executable*/
#define READABLE      0x02	/* set for readable segment if executable */
#define WRITEABLE     0x02	/* set for writeable segment if !executable */
#define TSS_BUSY      0x02	/* set if TSS descriptor is busy */
#define ACCESSED      0x01	/* set if segment accessed */

/* Special descriptor types. */
#define AVL_286_TSS      1	/* available 286 TSS */
#define LDT_TYPE         2	/* local descriptor table */
#define BUSY_286_TSS     3	/* set transparently to the software */
#define CALL_286_GATE    4	/* not used */
#define TASK_GATE        5	/* only used by debugger */
#define INT_286_GATE     6	/* interrupt gate, used for all vectors */
#define TRAP_286_GATE    7	/* not used */
#define TSS_TYPE	(AVL_286_TSS | DESC_386_BIT)
/* Descriptor structure offsets. */
#define DESC_GRANULARITY     6	/* to granularity byte */
#define DESC_BASE_HIGH       7	/* to base_high */

/* Base and limit sizes and shifts. */
#define BASE_HIGH_SHIFT     24	/* shift for base --> base_high */
#define BYTE_GRAN_MAX   0xFFFFFL   /* maximum size for byte granular segment */
#define GRANULARITY_SHIFT   16	/* shift for limit --> granularity */
#define OFFSET_HIGH_SHIFT   16	/* shift for (gate) offset --> offset_high */
#define PAGE_GRAN_SHIFT     12	/* extra shift for page granular limits */

/* Type-byte bits. */
#define DESC_386_BIT      0x08	/* 386 types are obtained by ORing with this */
				/* LDT's and TASK_GATE's don't need it */

/* Granularity byte. */
#define GRANULAR          0x80	/* set for 4K granularilty */
#define DEFAULT           0x40	/* set for 32-bit defaults (executable seg) */
#define BIG               0x40	/* set for "BIG" (expand-down seg) */
#define AVL               0x10	/* 0 for available */
#define LIMIT_HIGH        0x0F	/* mask for high bits of limit */

/* 
 * Task state structure.
 */
struct TSS {
	unsigned short tss_prevlink, res1;
	unsigned int esp0;
	unsigned short ss0, res2;
	unsigned int esp1;
	unsigned short ss1, res3;
	unsigned int esp2;
	unsigned short ss2, res4;
	unsigned int cr3;
	unsigned int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
	unsigned short es, res5, cs, res6, ss, res7, ds, res8;
	unsigned short fs, res9, gs, res10, ldtsel, res11;
	unsigned short trapflag, iomapbase;
} __attribute__ ((aligned(8)));

/* FPU environment structure.  */

struct fpu_environment {
	unsigned short int cwd, res1;
	unsigned short int swd, res2;
	unsigned short int tag, res3;
	unsigned int fpuip;
	unsigned short int fpucs;
	unsigned short int last_opcode;
	unsigned int last_operand_offset;
	unsigned short int last_operand_selector, res4;
};

/* Complete FPU state	*/
struct fpu_state {
	struct fpu_environment fpuenv;
	long double st0,st1,st2,st3,st4,st5,st6,st7;
};

/*
 * Parameter register structure, passed on stack to system call or,
 * interrupt handlers.
 */
struct PAR_REGS {
	unsigned long edi, esi, ebp, etemp, ebx, edx, ecx, eax;
	unsigned long gs,fs,es,ds;
	unsigned long error_code;
	unsigned long eip;
	unsigned long cs;
	unsigned long eflags;
	unsigned long oldsp;
	unsigned long oldss;
};
/*
 * Any Gate or segment descriptor
 */
struct segdesc {
	unsigned int a,b;
} __attribute__ ((aligned(8)));

struct gate_desc {
	unsigned int a,b;
} __attribute__ ((aligned(8)));

struct LDT {
	struct segdesc code, data, stack_user;
} __attribute__ ((aligned(8)));

/* 
 * Table descriptor (IDT or LDT or GDT).
 */
struct table_desc {
	unsigned short limit;
	unsigned long base;
	unsigned short unused;
} __attribute__ ((aligned(8)));

struct segdesc_s {	/* segment descriptor for protected mode */
	unsigned short limit_low;
	unsigned short base_low;
	unsigned char base_middle;
	unsigned char access;			/* |P|DL|1|X|E|R|A| */
	unsigned char granularity;		/* |G|X|0|A|LIMT| */
	unsigned char base_high;
} __attribute__ ((aligned(8)));

struct intr_gate {
	unsigned short entry_15_0;
	unsigned short code_sel;
	unsigned char  unused;
	unsigned char  access;
	unsigned short entry_31_16;
} __attribute__ ((aligned(8)));

void init_dataseg(struct segdesc_s *segdp, unsigned int base, unsigned int size,int privilege);
void init_codeseg(struct segdesc_s *segdp, unsigned int base, unsigned int size,int privilege);

#endif

