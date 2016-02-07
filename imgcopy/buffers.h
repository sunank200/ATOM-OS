#ifndef	__BUFFERS_H
#define __BUFFERS_H

#include "fdisk.h"

#define MAX_BUFFERS	1000
#define BUFF_HASHNUMBER	5

struct buffer_header {
	unsigned long blockno;
	struct buffer_header * hash_q_next;
	struct buffer_header * hash_q_prev;
	struct buffer_header * free_list_next;
	struct buffer_header * free_list_prev;
	struct buffer_header * write_q_next;
	struct process * waitfor;
	//struct event_t release_event;	
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

