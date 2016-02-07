
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

#ifndef __MKLIB_H
#define __MKLIB_H

#include "mtype.h"
#include "stdarg.h"
#include "change.h"
#include "segment.h"
/*
 * Function prototypes of kernel library routines.
 */

unsigned int in_byte(port_t port); /* Read a byte from the i/o port */
unsigned int in_word(port_t port); /* Read a word from the i/o port */
void out_byte(port_t port, int value); /* Write a byte to the I/O port */
void out_word(port_t port, int value); /* Write a word to the I/O port */
void port_read(port_t port, phys_bytes destination, unsigned bytcount);
				/* Transfer data from port to memory
				 * one word at a time.
				 */ 
void port_read_byte(port_t port, phys_bytes destination, unsigned bytcount); 
				/* Transfer data from port to memory
				 * one byte at a time.
				 */ 
void port_write(port_t port, phys_bytes source, unsigned bytcount); 
				/* Transfer data from memory to port
				 * one word at a time.
				 */ 
void port_write_byte(port_t port, phys_bytes source, unsigned bytcount); 
				/* Transfer data from memory to port
				 * one byte at a time.
				 */ 

unsigned short int get_irqstatus(void);
void enable_irq(int irq_no);
				/* Enabling a particular irq */
int disable_irq(int irq_no);
				/* Disables and returns true if not 
				 * already disabled 
				 */
int test_and_set(volatile unsigned short int * var);
			     /* Atomic test_and_set first bit of given
			      * 2 - byte (integer number).
			      */ 
unsigned long get_eflags(void);	/* Return flags value */
void restore_eflags(unsigned long eflags); /* Set flags to eflags value */
unsigned long clear_paging(void);
void restore_paging(unsigned long old_cr0, unsigned long pgdir);

void bcopy(char *dest,const char *src, int no_bytes);
void bzero(const char *loc, int no_bytes);
int memcmp(const char *s1, const char *s2, int nbytes);
int asm_spinunlock(volatile unsigned short int *var);

void sti_intr(void);		/* STI instruction */
void cli_intr(void);		/* CLI instruction */

char * strcpy(char *dest, const char *src);
char * strcat(char *dest, const char *src);
char * strncat(char *dest, const char *src, unsigned int n);
int strcmp(const char *dest, const char *src);
int strlen(const char *src);
char * strncpy(char *dest,const char *src, unsigned int n);
int strncmp(const char *dest,const char *src, unsigned int n);
int strnlen(const char *src, int n);
char *strstr (register char *buf, register char *sub);
char *strchr(const char *s, int c);

int toupper(int  ch);
int tolower(int  ch);
int isspace(int  ch);
int isalpha(int  ch);
int isnum(int ch);
int isdigit(int ch);
int isxdigit(int ch);
void init_timer(void);
void init_8259s(void);
void enable_timer(void);
void do_context_switch(unsigned int newtssdesc, unsigned int newcr0, unsigned int cr3);
void init_paging(unsigned long pgdiraddr);
unsigned long clear_paging(void);
void restore_paging(unsigned long oldcr0, unsigned long pgdir);
unsigned long get_tr(void);
unsigned long get_cr3(void);
unsigned long get_cr2(void);
unsigned long get_cr0(void);
void mark_tsc(unsigned int *hi, unsigned int *lo);
int pte_mark_busy(unsigned long *pteaddr);
int pte_unmark_busy(unsigned long *pteaddr);
int test_and_set_bitn(unsigned int *val, int bitn);
int clear_bitn(unsigned int *val, int bitn);

int scanf(const char *fmt, ...);
int fscanf(FILE *fp, const char *fmt, ...);
int printf(const char *fmt, ...);
int printk(const char *fmt, ...);
int print_p(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int print_context(struct PAR_REGS *regs, int error_code);

#endif

