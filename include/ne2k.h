
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



#ifndef	__NE2K_H__
#define __NE2K_H__

#include "lock.h"

typedef struct snic snic;
typedef struct nicframe_header nicframe_header;
typedef struct frame_buffer frame_buffer;

#define LEN_ADDR		6
#define MAX_TX			2
#define MAX_PAGESPERPACKET	6
#define TXPAGES			(MAX_TX*MAX_PAGESPERPACKET)
#define LEN_PROM		16
#define LEN_HEADER		14
#define MIN_LENGTH              46      /* minimum length for packet data */
#define MAX_LENGTH              1500    /* maximum length for packet data area */

frame_buffer *alloc_buffer(unsigned int size);
void free_buffer(frame_buffer *ptr);

struct nicframe_header {
	unsigned char status;
	unsigned char next;
	unsigned short count;	/* length of header and packet */
};

struct frame_buffer {
	unsigned int freethis;
	volatile int status;
	unsigned int page;
	unsigned int  len;
	unsigned int  len1;
	char *buf1;
	unsigned int len2;
	char *buf2;
	unsigned int len3;
	char *buf3;
	short int completed;
	struct lock_t frbuflock;
	struct event_t threadwait;
	volatile struct frame_buffer *next;
};

struct snic {
	int irq;
	int iobase;	/* NULL if uninitialized */
	int boundary;
	unsigned int pstart, pstop, wordlength, current_page;
	volatile frame_buffer *tx_frame;
	struct lock_t busy;
	char prom[32];
};

#endif

