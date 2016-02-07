#ifndef __MALLOC_H__
#define __MALLOC_H__

struct mem_list {
	volatile char *addr;
	volatile unsigned int size;
	volatile struct mem_list *next;
};

struct memory_info {
	volatile char *malloc_info;	// dynamic requests for memory
	volatile struct mem_list *avail_list;	// information related
	volatile struct mem_list *used_list;	// data structures
};

void *malloc(unsigned int size);
void free(void *ptr);
int brk(void *end_data_addr);

// For used program global memory info structure is required
extern struct memory_info umem_info;
extern unsigned int enddataaddr;

#endif
