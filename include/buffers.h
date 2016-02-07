
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

#ifndef	__BUFFERS_H
#define __BUFFERS_H

#define MAX_BUFFERS	30
#define BUFF_HASHNUMBER	7

struct buffer_header {
	unsigned long blockno;
	volatile struct buffer_header * hash_q_next;
	volatile struct buffer_header * hash_q_prev;
	volatile struct buffer_header * free_list_next;
	volatile struct buffer_header * free_list_prev;
	volatile struct buffer_header * write_q_next;
	volatile struct process * waitfor;
	struct event_t release_event;	
	char  buff[512];
	char device;
	char flags;
	char io_status;
#define	BUFF_BUSY	0x01
#define BUFF_DATAVALID	0x02
#define BUFF_WRITEDELAY	0x04
};

typedef struct buffer_header	buffer_header;

/*
 * Functions provided by buffer.c
 */

void init_buffers(void);
struct buffer_header * getblk(char device,unsigned long blockno);
void brelease(struct buffer_header * b);
struct buffer_header * bread(char device,unsigned long blockno);

#endif	/* __BUFFERS_H */

