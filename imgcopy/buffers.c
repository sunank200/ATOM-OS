#include<stdio.h>
#include "buffers.h"

struct buffer_header buffers[MAX_BUFFERS];
struct buffer_header * hash_queue_buffers[BUFF_HASHNUMBER];
struct buffer_header * free_buff_list_head;
struct buffer_header * free_buff_list_tail;
struct buffer_header * delayed_write_q_head;
struct buffer_header * delayed_write_q_tail;

extern struct disk_request * enqueue_disk_request(char device, 
				unsigned long blockno, char *buff, int op);
void init_buffers(void);
struct buffer_header * getblk(char device,unsigned long blockno);
void brelease(struct buffer_header * b);
struct buffer_header * bread(char device,unsigned long blockno);

static void enqueue_to_hashlist(int hash,struct buffer_header * b);
static void delete_from_hashlist(int hash,struct buffer_header * b);
static void delete_from_freelist(struct buffer_header * b);
static void append_to_freelist(struct buffer_header * b);
static void prepend_to_freelist(struct buffer_header * b);
static void append_to_write_queue(struct buffer_header * b);

void display_buff_hashq(void)
{
	int i;
	struct buffer_header *b;

	for (i=0; i<BUFF_HASHNUMBER; i++)
	{
		b = hash_queue_buffers[i];

		if (b != NULL)
		{
			while (b != NULL)
			{
				b = b->hash_q_next;
			}
		}
	}
}

void init_buffers(void)
{
	int i;
	/*
	 * Initialising the buffer headers, hash list, write_q and
	 * free list.
	 */
	for (i=0; i<BUFF_HASHNUMBER; i++)
	 	hash_queue_buffers[i] =  NULL;
	for (i=0; i<MAX_BUFFERS; i++)
	{
		buffers[i].hash_q_prev = NULL;
		buffers[i].hash_q_next = NULL;
		if (i < MAX_BUFFERS-1)
			buffers[i].free_list_next = &buffers[i+1];
		if (i > 0)
			buffers[i].free_list_prev = &buffers[i-1];
		buffers[i].write_q_next = NULL;

		/*
		 * memory allocation. must be filled later.
		 */
		/*
		 * Flags and block numbers are ok.
		 */
		buffers[i].flags = 0;
	}
	buffers[0].free_list_prev = NULL;
	buffers[MAX_BUFFERS-1].free_list_next = NULL;
	free_buff_list_head = &buffers[0];
	free_buff_list_tail = &buffers[MAX_BUFFERS-1];
	delayed_write_q_head = NULL;
	delayed_write_q_tail = NULL;
}
static void enqueue_to_hashlist(int hash,struct buffer_header *b)
{
	// display_buff_hashq();
	b->hash_q_next = hash_queue_buffers[hash];
	b->hash_q_prev = NULL;
	if (hash_queue_buffers[hash] != NULL)
		hash_queue_buffers[hash]->hash_q_prev = b;
	hash_queue_buffers[hash] = b;
	return;
}


static void delete_from_hashlist(int hash,struct buffer_header * b)
{
	struct buffer_header * n,* p;

	n = b->hash_q_next;
	p = b->hash_q_prev;

	if (p == NULL && n == NULL) // Not in hash list
		return;

	if (p != NULL)
		p->hash_q_next = n;
	else
		hash_queue_buffers[hash] = n;
	if (n != NULL)
		n->hash_q_prev = p;
	return;
}


static void delete_from_freelist(struct buffer_header * b)
{
	struct buffer_header * n, * p;

	n = b->free_list_next;
	p = b->free_list_prev;
	if (free_buff_list_head != b)
		p->free_list_next = n;
	else
		free_buff_list_head =  n;

	if (free_buff_list_tail != b)	
		n->free_list_prev = p;
	else
		free_buff_list_tail = p;
	return;
}

static void append_to_freelist(struct buffer_header * b)
{
	b->free_list_next = NULL;
	b->free_list_prev = free_buff_list_tail;
	if (free_buff_list_tail != NULL)
		free_buff_list_tail->free_list_next = b;
	else
		free_buff_list_head = b;
		
	free_buff_list_tail = b;
	return;
}
	
static void prepend_to_freelist(struct buffer_header * b)
{
	b->free_list_next = free_buff_list_head;
	b->free_list_prev = NULL;
	if (free_buff_list_head != NULL)
		free_buff_list_head->free_list_prev = b;
	free_buff_list_head = b;
}
static void append_to_write_queue(struct buffer_header * b)
{
	b->write_q_next = NULL;
	if (delayed_write_q_tail != NULL) 
		delayed_write_q_tail->write_q_next = b;
	else /* No block is in the write queue already */
	{
		delayed_write_q_head = b;
		delayed_write_q_tail = b;
	}
}

/*
 * Allocates a buffer block.
 */
struct buffer_header * getblk(char device,unsigned long blockno)
{

	struct buffer_header * b;
	char found_in_hashq;
	int hash_index = blockno % BUFF_HASHNUMBER;

	while (1)
	{
		/* 
		 * Search buffer cache for this block.
		 */
		b = hash_queue_buffers[hash_index];

		found_in_hashq=0;
		while (b != NULL && !found_in_hashq)
		{
			if (b->device == device && b->blockno == blockno)
				found_in_hashq = 1;
			else
				b = b->hash_q_next;
		}

		if (found_in_hashq)
		{
			if (b->flags & BUFF_BUSY)
			{
				continue;
			}

			/*
			 * block found and not busy so allocate it.
			 * Remove from list of free buffers.
			 */
			b->flags |= BUFF_BUSY;
			delete_from_freelist(b);
			return (b);
		}
		else
		{
			/*
			 * Remove a buffer from the free buffers list.
			 */
			if (free_buff_list_head == NULL)
			{
				continue;
			}

			b = free_buff_list_head;
			delete_from_freelist(b);

			/*
			 * Buffer contains delayed write data.
			 */
			if (b->flags & BUFF_WRITEDELAY)
			{
				append_to_write_queue(b);
				continue;
			}

			/*
			 * Remove from old hash and add to new hash.
			 */
			delete_from_hashlist(b->blockno % BUFF_HASHNUMBER,b);
			b->blockno = blockno;
			b->device = device;
			b->flags = (b->flags | BUFF_BUSY) & (~BUFF_DATAVALID);
			enqueue_to_hashlist(hash_index,b);
			return(b);
		}
	}
}

void brelease(struct buffer_header * b)
{
	/* Add this buffer to free list */
	if (b->flags & BUFF_DATAVALID)
		append_to_freelist(b);
	else
		prepend_to_freelist(b);

	b->flags = b->flags & (~BUFF_BUSY);

	return;
}

struct buffer_header* bread(char device,unsigned long blockno)
{
	struct buffer_header *b;
	struct disk_request *dreq;

	/* Get a buffer block and see if data valid */
	b = getblk(device,blockno);
	if (b->flags & BUFF_DATAVALID)
	{
		return b;
	}

	/* Make a disk read request and wait for completion */
	dreq = enqueue_disk_request(device, blockno, b->buff, DISK_READ);
	if (dreq->status == 0)
		b->flags = b->flags | BUFF_DATAVALID;
	b->io_status = dreq->status;
	dequeue_disk_request(dreq);
	return b;
}

void flush_buffers(void)
{
	struct buffer_header *b1,*b2;
	struct disk_request *dr[20];
	struct buffer_header *blist[20];
	int i, count = 0;

	b1 = delayed_write_q_head;
	delayed_write_q_head = delayed_write_q_tail = NULL;
	while (b1 != NULL)
	{
		dr[count] = enqueue_disk_request(b1->device, b1->blockno, b1->buff, DISK_WRITE);
		blist[count++] = b1;
		// Instaed of sleeping busy wait on completion status
		b2 = b1->write_q_next;
		b1->write_q_next = NULL;
		b1 = b2;

		if (count == 19 || b1 == NULL)
		{	// Wait for previous write operations to complete
			for (i=0; i<count; i++)
			{
				// busy wait on completion
				while (dr[i]->status == DISK_OPERATION_NOT_COMPLETE) ;
				blist[i]->flags = blist[i]->flags & (~BUFF_WRITEDELAY);
			}
			count = 0;
		}
	}
}

	
