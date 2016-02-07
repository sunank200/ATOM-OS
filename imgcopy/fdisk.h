/*
 * Floppy disk related inofrmation and disk request structures
 */
#ifndef __FDISK_H
#define __FDISK_H

#define MAX_DISK_REQUESTS	50

/*
 * Disk operations
 */
#define DISK_READ	1
#define DISK_WRITE	2

struct disk_request {
	int head;
	int track;
	int sector;
	int noblocks;
	char * buff;
	//struct event_t complete;
	struct disk_request *next;
	char operation;
	char status;
#define EDISK_READ	-1
#define EDISK_WRITE	-2
#define DISK_OPERATION_NOT_COMPLETE	1
#define DISK_OPERATION_SUCCESS	0
};



/*
 * Functions provided by fdisk.c
 */

void init_disk_request_bufs(void);
struct disk_request * enqueue_disk_request(char device, 
			unsigned long blockno, char *buff, int op);
void dequeue_disk_request(struct disk_request * dr);

#endif /* End of __FDISK_H */

