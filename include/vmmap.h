
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

/* System definitions and constants for Paging */

#ifndef __PAGING_DEFS
#define __PAGING_DEFS

#include "mconst.h"
#define PAGE_SIZE       4096	/* in bytes */

#define PAGE_SHIFT      12
#define PAGE_MASK       0x3FF

#define PDE_SHIFT       22
#define PTE_SHIFT       12
                                                                                                                             
#define TABLE_ENTRIES   1024

/* These are the status macros */
#define INVALID 100
#define UNAVAIL 200

#define PAGE_ROUND(x)	 ((x) & 0xFFFFF000)
#define PAGE_ROUND_UP(x) (((unsigned long)(((x)+PAGE_SIZE-1)>>PAGE_SHIFT))<<PAGE_SHIFT)
#define ROUND_4(x)	 (( x + 3) & ~ 3)

/* Hardware paging data structures */
/* Page Directory Entry */
typedef struct {
	unsigned present:1;
	unsigned read_write:1;
	unsigned user:1;
	unsigned write_through:1;
	unsigned cache_disable:1;
	unsigned accessed:1;
	unsigned reserved:1;
	unsigned page_4MB:1;
	unsigned global:1;
	unsigned available:3;
	unsigned page_table_base_addr:20;
} PDE;

#define PDE_PRESENT		0x1
#define PDE_READWRITE		0x2
#define PDE_USER		0x4
#define PDE_WRITETHROUGH	0x8
#define PDE_CACHEDISABLE	0x10
#define PDE_ACCESSED		0x20
#define PDE_RESERVED		0x40
#define PDE_PAGE4MB		0x80
#define PDE_GLOBAL		0x100
#define PDE_AVBLMASK		(0x200|0x400|0x800)
#define PDE_PAGETABLEADDRMASK	(0xFFFFF000) 

typedef struct {
	unsigned present:1;
	unsigned read_write:1;
	unsigned user:1;
	unsigned write_through:1;
	unsigned cache_disable:1;
	unsigned accessed:1;
	unsigned dirty:1;
	unsigned reserved:1;
	unsigned global:1;
	unsigned available:3;
	unsigned page_base_addr:20;
} PTE;

#define PTE_PRESENT		0x1
#define PTE_READWRITE		0x2
#define PTE_USER		0x4
#define PTE_WRITETHROUGH	0x8
#define PTE_CACHEDISABLE	0x10
#define PTE_ACCESSED		0x20
#define PTE_DIRTY		0x40
#define PTE_RESERVED		0x80
#define PTE_GLOBAL		0x100
#define PTE_AVBLMASK		(0x200|0x400|0x800)
#define PTE_PAGEADDRMASK	(0xFFFFF000) 

/* Values for avail field */
#define PAGE_COPYONWRITE	0x1
#define PAGE_INVALID		0x2
#define PAGE_LOCK		0x4


typedef struct {
	PDE entry[TABLE_ENTRIES] ;
}PageDir;

typedef struct {
	PTE entry[TABLE_ENTRIES] ;
}PageTable;

struct PageTableList {
	volatile PageTable *pgtable;
	volatile struct PageTableList *next;
};

#include "lock.h"
/* Process level memory segments, regions, vm information */

#define SEGMENTTYPE_CODE	1	// Section types in exe file
#define SEGMENTTYPE_DATA	2
#define SEGMENTTYPE_STACK	4
#define SEGMENTTYPE_PRIVATE	8

#define USERCODE_SEGMENT_START	KERNEL_MEMMAP
#define USERDATA_SEGMENT_START (USERCODE_SEGMENT_START + 0x04000000) // 64MB
#define USERSTACK_SEGMENT_END	(0xFDC00000)	// Excluding last 4MB + 32MB(private mem) range

// Several processes may share the same memory segment 
// (Must start at 4MB) boundary. This can be mapped into the region of
// any process at any address
struct memsegment {
	unsigned long mseg_sourcefile;	// Inode no of the source file
	unsigned short mseg_sourcedev;	// device number of the source
        unsigned short mseg_sharecount;
        unsigned int mseg_npages;
        volatile struct PageTableList *mseg_pgtable;
	struct lock_t mseg_lock; 
	// Protects all modifications to this segment 
	// including pgtable list.
};

// Within a process description of one region of memory
struct memregion {
        unsigned int mreg_startaddr;
        unsigned int mreg_endaddr;
        short mreg_type;
	struct lock_t mreg_reglock;
	// Protects any changes to this region including segment pointer
	volatile struct memsegment *mreg_segment;
	// unsigned void * mreg_owner;	// Possibly the owner process
	volatile struct memregion *mreg_next;
};

struct vm {
        PageDir *vm_pgdir;
	struct lock_t vm_lock;
	// Protects from changes to pgdir or memory regions list.
	volatile struct memregion *vm_mregion;
};

/* Low level kernel memory and core management data structures */	

#define DYNMEMORY_THRESHOLDMAX	0x00020000	// 128 KB
struct mem_list {
	volatile char *addr;
	volatile unsigned int size;
	volatile struct mem_list *next;
};

// Dynamically allocated kernel memory information ( Small portions only )
// When free memory is above DYNMEMORY_THRESHOLDMAX then whole pages are added
// to free core.

struct km {
        char * km_mallocinfo;      	// dynamic requests for memory
        struct mem_list *km_availlist;    // information related
        struct mem_list *km_usedlist;     // data structures
	unsigned int km_availsize;
	unsigned int km_usedsize;
};

/* Core management related data structures */
/* System address MAP region types */
#define ADDRESS_RANGE_MEMORY	1
#define ADDRESS_RANGE_RESERVED	2
#define ADDRESS_RANGE_ACPI	3
#define ADDRESS_RANGE_NVS	4

struct core_header {
	unsigned long phys_addr;/* Starting of pages.	*/
	unsigned int no_blocks;		/* No of pages of 4k	*/
};

/* Structure used to describe the physical memory map regions. */
struct memory_map_descriptor {
		unsigned long base_addr;
		unsigned long length;
		unsigned char type;
};

struct free_mem_block {
	unsigned int no_pages;
	volatile struct free_mem_block * next_block;
};

// Initializes the page directory of the vm with empty maps.
struct vm * vm_createvm(void);
// Destroy virtual memory , free all space
int vm_destroy(struct vm *vmem);
// Adds a memory region of specified type, initializes page tables (optional), 
// allocation of memory (optional). Assumes given range is not overlapping
// with other regions.lstart at page boundary, 
// but lend need not be at the boundary
int vm_addregion(struct vm *vmem, short type, unsigned int lstart, unsigned int lend, short ptflag, short memflag);
// Delete a region, may be useful for stack regions 
// which are deleted as a whole
int vm_delregion(struct vm *vmem, unsigned int lstart);
int vm_delregionend(struct vm *vmem, unsigned int lend);
// Expand region if not overlapping and shrink region
int vm_expandregion(struct vm *vmem, unsigned int lstart, unsigned int size, short pgflag, short memflag);
int vm_expanddownregion(struct vm *vmem, unsigned int lstart, unsigned int lend, short pgflag, short memflag);
int vm_shrinkregion(struct vm *vmem, unsigned int lstart, unsigned int size);
// Attach the given segment to the region, size should match
int vm_attachmemsegment(struct memregion *mreg, struct memsegment *msegment);
// Reduces the reefrnce count of the segment, if necessary frees the segment
int vm_detachmemsegment(struct memregion *mreg);
PTE* vm_getptentry(struct vm *vmem, unsigned int lpage);
PDE* vm_getpdentry(struct vm *vmem, unsigned int lpage);
// Set/Get the attributes which are not null
int vm_setattr(struct vm *vmem, unsigned int lpage, int *avail, int *rw, int *present, int *global, int *accessed, int *dirty);
int vm_getattr(struct vm *vmem, unsigned int lpage, int *avail, int *rw, int *present, int *global, int *accessed, int *dirty);
// Get dirty list of pages, accessed list of pages (it clears those flags)
int vm_getdirtylist(struct vm *vmem, int *buf, int size, int type);
int vm_getaccesslist(struct vm *vmem, int *buf, int size, int type);
// First initialization of kernel page tables
void init_kernel_pagetables(void);
// Memory copy operations.
int phys2vmcopy(char *src, struct vm *dvm, char *dest, int nbytes);
int vm2physcopy(struct vm *svm, char *src, char *dest, int nbytes);
int vm2vmcopy(struct vm *svm, char *src, struct vm *dvm, char *dest, int nbytes);
// Locking and unlocking of pages
int lock_page(PageDir *pd, char *logaddr);
int unlock_page(PageDir *pd, char *logaddr);
// translate a given logical address to physical address.
char *logical2physical(PageDir *pd, char *logicaladdr);


struct core_header core_alloc(int npages);//, int sys);
int core_alloc_n(int npages, struct core_header *chs, int maxn);
void core_free(struct core_header ch);
void core_init(char *first_free_loc);

int alloc_frame_memory(struct process * proc, unsigned int pageno);
void *kmalloc(unsigned int size);
void kfree(void *ptr);
int syscall_primalloc(unsigned int size);
int syscall_prifree(void *addr, unsigned int size);

void print_core_info(void);
void print_minf_statistics(void);
#endif

