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

// Vitrual memory functions

#include "mklib.h"
#include "process.h"
#include "vmmap.h"
#include "errno.h"
#include "alfunc.h"


static struct memsegment * vm_creatememsegment(PageDir *pd, struct memregion *mreg, short memflag);
static int vm_destroysegment(struct memsegment *mseg);
static int vm_getattrlistpages(struct vm *vmem, int *buf, int size, int type, int attr);
static void make_PDE(	PageDir *pd, unsigned int i, unsigned long addr, unsigned char avail, unsigned char rw, unsigned char present, unsigned char global	);
static void make_PTE(	PageTable *pt, unsigned int i, unsigned long addr, unsigned char avail, unsigned char rw, unsigned char present, unsigned char global	);
static int is_valid_block(struct core_header ch);
static struct free_mem_block * find_mem_block_before(struct core_header ch);
static void insert_avail_node(struct mem_list *p);
static void delete_avail_node(struct mem_list *p);
static struct mem_list *create_mem_list_node(void);

// Data related to core/page management
struct lock_t core_lock;
volatile struct free_mem_block *first_block=NULL;
struct memory_map_descriptor ram_segments[MAX_RAM_SEGMENTS];
int no_of_ram_segments=0;

struct lock_t kmalloc_lock;
volatile char *kmmalloc_info = NULL;
volatile struct mem_list *kmavail_list = NULL;
volatile struct mem_list *kmused_list = NULL;
int kmcount = 0;

PageDir KERNEL_PAGEDIR __attribute__ ((aligned(4096))); 
PageTable *KERNEL_PAGE_TABLE; 
struct PageTableList ptlist[(KERNEL_MEMMAP >> 22) & 0x3ff];

extern struct segdesc gdt_table[GDT_SIZE]; 

extern struct table_desc gdt_desc; 
extern struct LDT ldt_table[MAXPROCESSES];

#define INT_GATE_TYPE	(INT_286_GATE | DESC_386_BIT)
#define TSS_TYPE	(AVL_286_TSS  | DESC_386_BIT)
#define BEG_LDT_ADDR	&ldt_table[0]
#define END_LDT_ADDR	&ldt_table[MAXPROCESSES - 1]

unsigned char bitmap[32*256]; // Bit map for 32 MB private memory. (4k size pages)

extern volatile struct process *current_process;
extern volatile struct kthread *current_thread;

// Initializes the page directory of the vm with empty maps.
struct vm * vm_createvm(void)
{
	struct vm *vmem;
	struct core_header ch;
	unsigned int oldcr0;


	oldcr0 = clear_paging();	
	vmem = (struct vm *)kmalloc(sizeof(struct vm));
	if (vmem == NULL) 
	{
		printk("Malloc failure in vm_createvm\n");
		restore_paging(oldcr0, current_thread->kt_tss->cr3);
		return NULL;
	}

	lockobj_init(&vmem->vm_lock);
	ch = core_alloc(1);	// System accessible range
	if (ch.no_blocks != 1)
	{
		kfree(vmem);
		restore_paging(oldcr0, current_thread->kt_tss->cr3);
		return NULL;
	}
	vmem->vm_pgdir = (PageDir *)ch.phys_addr;
	// Add kernel memory region
	bzero((char *)vmem->vm_pgdir, 4096);
	bcopy((char *)&KERNEL_PAGEDIR, (char *)vmem->vm_pgdir, ((KERNEL_MEMMAP >>22) & 0x000003ff) * 4);
	vmem->vm_mregion  = NULL;
	restore_paging(oldcr0, current_thread->kt_tss->cr3);
	
	return vmem;
}

// Destroy virtual memory , free all space
int vm_destroy(struct vm *vmem)
{
	struct memregion *mreg1,*mreg2;
	struct core_header ch;
	unsigned int oldcr0;

	oldcr0 = clear_paging();	
	_lock(&(vmem->vm_lock));
	mreg1 = (struct memregion *)vmem->vm_mregion;
	vmem->vm_mregion = NULL;
	unlock(&(vmem->vm_lock));
	
	// Destroy all memory regions	 
	while (mreg1 != NULL)
	{
		_lock(&mreg1->mreg_reglock);
		if (mreg1->mreg_segment != NULL)
			vm_releasesegment((struct memsegment *)mreg1->mreg_segment);
		unlock(&mreg1->mreg_reglock);
		mreg2 = mreg1;
		mreg1 = (struct memregion *)mreg1->mreg_next;
		kfree(mreg2);
	}

	ch.phys_addr = (unsigned int)vmem->vm_pgdir;
	ch.no_blocks = 1;
	core_free(ch);
	kfree(vmem);
	restore_paging(oldcr0, current_thread->kt_tss->cr3);
	return 0;
}

// Adds a memory region of specified type, initializes page tables (optional), 
// allocation of memory (optional). Assumes given range is not overlapping
// with other regions.lstart at page boundary, 
// but lend need not be at the boundary
int vm_addregion(struct vm *vmem, short type, unsigned int lstart, unsigned int lend, short ptflag, short memflag)
{
	struct memregion *mreg, *mreg1, *mreg2;
	
	mreg = (struct memregion *)kmalloc(sizeof(struct memregion));
	if (mreg == NULL) 
	{
		printk("Malloc failure in vm_addregion\n");
		return ENOMEM;
	}

	mreg->mreg_startaddr = lstart;
	mreg->mreg_endaddr = lend;
	mreg->mreg_type = type;
	lockobj_init(&(mreg->mreg_reglock));
	if (ptflag == 0) mreg->mreg_segment = NULL;
		// Allocate a new segment
	else if (vm_creatememsegment(vmem->vm_pgdir, mreg, memflag) == NULL) // Not allocated
	{
		kfree(mreg);
		return ENOMEM;
	}
		
	// Add this to the list of regions in order
	_lock(&(vmem->vm_lock));
	mreg1 = (struct memregion *)vmem->vm_mregion;
	mreg2 = NULL;
	while (mreg1 != NULL && mreg1->mreg_startaddr < mreg->mreg_startaddr)
	{
		mreg2 = mreg1;
		mreg1 = (struct memregion *)mreg1->mreg_next;
	}

	mreg->mreg_next = mreg1;
	if (mreg2 != NULL)
		mreg2->mreg_next = mreg;
	else
		vmem->vm_mregion = mreg; // First region
	unlock(&(vmem->vm_lock));

	return 0;
}

int vm_delregionend(struct vm *vmem, unsigned int lend)
{
	struct memregion *mreg1, *mreg2;
	unsigned int lstart;
	
	_lock(&(vmem->vm_lock));
	mreg1 = (struct memregion *)vmem->vm_mregion;
	mreg2 = NULL;
	while (mreg1 != NULL && mreg1->mreg_endaddr != lend)
		mreg1 = (struct memregion *)mreg1->mreg_next;
	if (mreg1 != NULL)
	{
		lstart = mreg1->mreg_startaddr;
		unlock(&(vmem->vm_lock));
		return vm_delregion(vmem, lstart);
	}
	else
	{
		unlock(&(vmem->vm_lock));
		return -1;
	}
}
// Delete a region, may be useful for stack regions 
// which are deleted as a whole
int vm_delregion(struct vm *vmem, unsigned int lstart)
{
	struct memregion *mreg1, *mreg2;

	_lock(&(vmem->vm_lock));
	mreg1 = (struct memregion *)vmem->vm_mregion;
	mreg2 = NULL;
	while (mreg1 != NULL && mreg1->mreg_startaddr != lstart)
	{
		mreg2 = mreg1;
		mreg1 = (struct memregion *)mreg1->mreg_next;
	}

	if (mreg1 != NULL)
	{
		// Found. unlink it.
		if (mreg2 == NULL) vmem->vm_mregion = mreg1->mreg_next;
		else mreg2->mreg_next = mreg1->mreg_next;
		unlock(&(vmem->vm_lock));

		_lock(&mreg1->mreg_reglock);
		if (mreg1->mreg_segment != NULL)
			vm_releasesegment((struct memsegment *)mreg1->mreg_segment);
		unlock(&mreg1->mreg_reglock);
		kfree(mreg1);
		return 0;
	}
	else unlock(&(vmem->vm_lock));

	return -1;
}

// Expand up/down/(missing ?) page tables, Assumes already segment is acquired 
// and locked
static int vm_expandpagetables(PageDir *pd, struct memsegment *mseg, unsigned long logical_start, unsigned long logical_end, int avail, int rw, int global)
{
	struct core_header ch;
	char *cp1;
	struct PageTableList *ptl, *ptl1, *ptl2;
	char *ptbaddr;
	int pdind;
	unsigned int oldcr0;
	

	ptl1 = (struct PageTableList *)mseg->mseg_pgtable;
	ptl2 = NULL;

	oldcr0 = clear_paging();
	logical_start = logical_start & 0xffc00000; // Round downwards to 4mb boundary
	while (logical_start <= logical_end)
	{
		pdind = ((logical_start >> 22) & 0x3ff);
		ptbaddr = (char *)(((unsigned int)pd->entry[pdind].page_table_base_addr) << 12);
		if (ptbaddr == NULL)
		{
			// Create a page table
			ch = core_alloc(1);

			if (ch.no_blocks < 1)
			{
				restore_paging(oldcr0, current_thread->kt_tss->cr3);
				return ENOMEM;
			}
			ptl = (struct PageTableList *)kmalloc(sizeof(struct PageTableList));
			if (ptl == NULL)
			{
				printk("Malloc failure in vm_expandpagetables\n");
				core_free(ch);
				restore_paging(oldcr0, current_thread->kt_tss->cr3);
				return ENOMEM;
			}
			cp1 = (char *)ch.phys_addr;
			ptl->pgtable = (PageTable *)ch.phys_addr;
			bzero(cp1, 4096);

			make_PDE(pd, pdind, (((unsigned int)cp1) >> 12), avail, rw, 1, global);
			// Insert it at the current position.
			ptl->next = ptl1;
			if (ptl2 == NULL) // At the beginning
				mseg->mseg_pgtable = ptl;
			else ptl2->next = ptl;
			ptl1 = ptl;
		}
		ptl2 = ptl1;
		ptl1 = (struct PageTableList *)ptl1->next;
		logical_start += 0x00400000;
	}
	restore_paging(oldcr0, current_thread->kt_tss->cr3);
	return 0;
}
// Expand region if not overlapping and shrink region
int vm_expandregion(struct vm *vmem, unsigned int lstart, unsigned int size, short pgflag, short memflag)
{
	struct core_header chs[32];
	int retval;
	struct memregion *mreg;
	struct memsegment *mseg;
	unsigned long oend, curendaddr; //, finalendaddr;
	unsigned short rw;
	int i, j, np, n, ptind, pdind;
	unsigned long phys_addr;
	PageTable *pt;
	unsigned int oldcr0;

	_lock(&vmem->vm_lock);
	mreg = (struct memregion *)vmem->vm_mregion;
	while (mreg != NULL && mreg->mreg_startaddr < lstart) mreg = (struct memregion *)mreg->mreg_next;
	if (mreg == NULL || mreg->mreg_startaddr != lstart) // Not found
	{
		unlock(&vmem->vm_lock);
		return -1;
	}
	// otherwise Found. Is it overlapping with next region?
	if ((mreg->mreg_next != NULL) && (mreg->mreg_endaddr + size >= (mreg->mreg_next)->mreg_startaddr)) 
	{
		unlock(&vmem->vm_lock);
		return -1;
	}

	// OK. Expand it
	oend = mreg->mreg_endaddr;
	mreg->mreg_endaddr += size;

	// If page tables required, create
	if (pgflag || memflag)
	{
		// If no segment is attached then create
		mseg = vm_getmemsegment(mreg);
		_lock(&mreg->mreg_reglock);
		if (mseg == NULL)
		{
			mseg = vm_creatememsegment(vmem->vm_pgdir, mreg, memflag);
			unlock(&mreg->mreg_reglock);
			unlock(&vmem->vm_lock);
			if (mseg == NULL) return -1;
			else return 0;
		}
	
		// Expand page tables if necessary
		rw = (mreg->mreg_type == SEGMENTTYPE_CODE) ? 0 : 1;
		_lock(&mseg->mseg_lock);	
		if ((oend & 0xffc00000) != (mreg->mreg_endaddr & 0xffc00000)) // In diffrent 4 mb regions
		{
			retval = vm_expandpagetables(vmem->vm_pgdir, mseg, mreg->mreg_startaddr, mreg->mreg_endaddr, 0, rw, 0); 
			if (retval < 0)
			{
				printk("Pagetable expansion failure\n");
				unlock(&mseg->mseg_lock);
				vm_releasesegment(mseg);
				unlock(&mreg->mreg_reglock);
				unlock(&vmem->vm_lock);
				return ENOMEM;
			}
		}
	
		// Allocate memory if memflag is set
		if (memflag != 0)	
		{
			// Make a call to allocate no of requierd pages
			// at once.
			np = (((unsigned int)((mreg->mreg_endaddr & 0xfffff000) - (oend & 0xfffff000))) >> 12) & 0x000fffff;
			if (np <= 0)	// Nothing needs to be done.
			{
				unlock(&mseg->mseg_lock);
				vm_releasesegment(mseg);
				unlock(&mreg->mreg_reglock);
				unlock(&vmem->vm_lock);
				return 0; 
			}
			curendaddr = oend;
			
			// For all the core segments map them
			n = core_alloc_n(np, chs, 32);
			if (n < 0) // Failure in memory allocation
			{
				unlock(&mseg->mseg_lock);
				vm_releasesegment(mseg);
				unlock(&mreg->mreg_reglock);
				unlock(&vmem->vm_lock);
				return n; 
			}

			// For each page of core segment map them
			oldcr0 = clear_paging();
			for (i=0; i<n; i++)
			{
				phys_addr = chs[i].phys_addr;
				j = chs[i].no_blocks;
				while (j > 0)
				{
					// Get the page table base address
					pdind = ((curendaddr + 4095) >> 22) & 0x3ff;
					ptind = ((curendaddr + 4095) >> 12) & 0x3ff;
					pt = (PageTable *)(((unsigned int)vmem->vm_pgdir->entry[pdind].page_table_base_addr) << 12);


					// Set the page table entry, using 
					// the index numbers of logical address.
					make_PTE(pt, ptind, ((phys_addr >> 12) & 0x000fffff), 0, rw, 1, 0);
					j--;
					phys_addr += 4096;
					curendaddr += 4096;
				}
			}
			restore_paging(oldcr0, current_thread->kt_tss->cr3);
		}
		unlock(&mseg->mseg_lock);
		vm_releasesegment(mseg);
		unlock(&mreg->mreg_reglock);
	}
	unlock(&vmem->vm_lock);
	return 0;
}

// Expand region if not overlapping 
int vm_expanddownregion(struct vm *vmem, unsigned int lstart, unsigned int lend, short pgflag, short memflag)
{
	struct core_header chs[32];
	int retval;
	struct memregion *mreg, *mreg1;
	struct memsegment *mseg;
	unsigned long ostart, curstartaddr; //, finalendaddr;
	unsigned short rw;
	int i, j, np, n, ptind, pdind;
	unsigned long phys_addr;
	PageTable *pt;
	unsigned int oldcr0;

	_lock(&vmem->vm_lock);
	mreg = (struct memregion *)vmem->vm_mregion;
	mreg1 = NULL;
	while (mreg != NULL && mreg->mreg_endaddr != lend)
	{
		mreg1 = mreg;
		mreg = (struct memregion *)mreg->mreg_next;
	}

	if (mreg == NULL) // Not found
	{
		unlock(&vmem->vm_lock);
		return -1;
	}

	// otherwise Found. Is it overlapping with previous region?
	if ((mreg1 != NULL) && (mreg1->mreg_endaddr >= lstart))
	{
		unlock(&vmem->vm_lock);
		return -1;
	}

	// OK. Expand it
	ostart = mreg->mreg_startaddr;
	mreg->mreg_startaddr = lstart;

	// If page tables required, create
	if (pgflag || memflag)
	{
		// If no segment is attached then create
		mseg = vm_getmemsegment(mreg);
		_lock(&mreg->mreg_reglock);
		if (mseg == NULL)
		{
			mseg = vm_creatememsegment(vmem->vm_pgdir, mreg, memflag);
			unlock(&mreg->mreg_reglock);
			unlock(&vmem->vm_lock);
			if (mseg == NULL) return -1;
			else return 0;
		}
	
		// Expand page tables if necessary

		rw = (mreg->mreg_type == SEGMENTTYPE_CODE) ? 0 : 1;
		
		_lock(&mseg->mseg_lock);	
		if ((ostart & 0xffc00000) != (mreg->mreg_endaddr & 0xffc00000)) // In diffrent 4 mb regions
		{
			retval = vm_expandpagetables(vmem->vm_pgdir, mseg, mreg->mreg_startaddr, mreg->mreg_endaddr, 0, rw, 0); 
			if (retval < 0)
			{
				printk("Pagetable expansion failure\n");
				unlock(&mseg->mseg_lock);
				vm_releasesegment(mseg);
				unlock(&mreg->mreg_reglock);
				unlock(&vmem->vm_lock);
				return ENOMEM;
			}
		}
	
		// Allocate memory if memflag is set
		if (memflag != 0)	
		{
			// Make a call to allocate no of requierd pages
			// at once.
			np = (((unsigned int)((mreg->mreg_endaddr & 0xfffff000) - (ostart & 0xfffff000))) >> 12) & 0x000fffff;
			if (np <= 0)	// Nothing needs to be done.
			{
				unlock(&mseg->mseg_lock);
				vm_releasesegment(mseg);
				unlock(&mreg->mreg_reglock);
				unlock(&vmem->vm_lock);
				return 0; 
			}
			curstartaddr = mreg->mreg_startaddr;
			
			// For all the core segments map them
			n = core_alloc_n(np, chs, 32);
			if (n < 0) // Failure in memory allocation
			{
				unlock(&mseg->mseg_lock);
				vm_releasesegment(mseg);
				unlock(&mreg->mreg_reglock);
				unlock(&vmem->vm_lock);
				return n; 
			}

			// For each page of core segment map them
			oldcr0 = clear_paging();
			for (i=0; i<n; i++)
			{
				phys_addr = chs[i].phys_addr;
				j = chs[i].no_blocks;
				while (j > 0)
				{
					// Get the page table base address
					pdind = ((curstartaddr + 4095) >> 22) & 0x3ff;
					ptind = ((curstartaddr + 4095) >> 12) & 0x3ff;
					pt = (PageTable *)(((unsigned int)vmem->vm_pgdir->entry[pdind].page_table_base_addr) << 12);

					// Set the page table entry, using 
					// the index numbers of logical address.
					make_PTE(pt, ptind, ((phys_addr >> 12) & 0x000fffff), 0, rw, 1, 0);
					j--;
					phys_addr += 4096;
					curstartaddr += 4096;
				}
			}
			restore_paging(oldcr0, current_thread->kt_tss->cr3);
		}
		unlock(&mseg->mseg_lock);
		vm_releasesegment(mseg);
		unlock(&mreg->mreg_reglock);
	}
	unlock(&vmem->vm_lock);
	return 0;
}

int vm_shrinkregion(struct vm *vmem, unsigned int lstart, unsigned int size)
{
	struct core_header ch;
	struct memregion *mreg;
	struct memsegment *mseg;
	unsigned long oend, curendaddr; 
	int j,n,np,pdind,ptind;
	PageTable *pt;
	unsigned int start_addr, prev_addr, phys_addr;
	unsigned int oldcr0;

	_lock(&vmem->vm_lock);
	mreg = (struct memregion *)vmem->vm_mregion;
	while (mreg != NULL && mreg->mreg_startaddr < lstart) mreg = (struct memregion *)mreg->mreg_next;
	if (mreg == NULL || mreg->mreg_startaddr != lstart) // Not found
	{
		unlock(&vmem->vm_lock);
		return -1;
	}

	// Other shrink it
	oend = mreg->mreg_endaddr;
	mreg->mreg_endaddr -= size;

	np = (((oend & 0xfffff000) - (mreg->mreg_endaddr & 0xfffff000)) >> 12) & 0x000fffff;
	mseg = vm_getmemsegment(mreg);
	if (np > 0 && mseg != NULL)
	{
		start_addr = 0;
		prev_addr = 0;
		n = 0;
		curendaddr = ((oend + 4095) >> 12) << 12; // Round upwards

		_lock(&mreg->mreg_reglock);		
		_lock(&(mseg->mseg_lock));
		oldcr0 = clear_paging();
		for (j=0; j<np; j++)
		{
			pdind = (curendaddr >> 22) & 0x3ff;
			ptind = (curendaddr >> 12) & 0x3ff;
			curendaddr += 4096;
			pt = (PageTable *)(((unsigned int)vmem->vm_pgdir->entry[pdind].page_table_base_addr) << 12);
			if (pt == NULL) continue;
			phys_addr = pt->entry[ptind].page_base_addr;
			if (phys_addr == 0)
			{
				if (start_addr != 0)
				{
					ch.phys_addr = (start_addr << 12);
					ch.no_blocks = n;
					core_free(ch);
					start_addr = prev_addr = 0;
					n = 0;
				}
				continue;
			}
			if (prev_addr == 0)
			{
				start_addr = prev_addr = phys_addr;
				n = 1;
			}
			else
			{
				if (prev_addr + 1 == phys_addr) n++;
				else
				{
					// Another region, so release 
					// previous region.
					ch.phys_addr = (start_addr << 12);
					ch.no_blocks = n;
					core_free(ch);
					start_addr = prev_addr = phys_addr;
					n = 1;
				}
			}
		}
		if (n > 0)
		{
			ch.phys_addr = (start_addr << 12);
			ch.no_blocks = n;
			core_free(ch);
		}
		restore_paging(oldcr0, current_thread->kt_tss->cr3);
		mreg->mreg_segment->mseg_npages -= np;
		unlock(&(mseg->mseg_lock));
		unlock(&mreg->mreg_reglock);		
	}
	vm_releasesegment(mseg);
	unlock(&vmem->vm_lock);
	
	return 0;
}
		
// Return the segment attached to the region
struct memsegment * vm_getmemsegment(struct memregion *mreg)
{
	struct memsegment *msegment;

	_lock(&(mreg->mreg_reglock));
	msegment = (struct memsegment *)mreg->mreg_segment;

	if (msegment != NULL)
	{
		_lock(&msegment->mseg_lock);
		msegment->mseg_sharecount++;
		unlock(&msegment->mseg_lock);
	}
	unlock(&(mreg->mreg_reglock));
	
	return msegment;
}
int vm_releasesegment(struct memsegment *mseg)
{
	_lock(&mseg->mseg_lock);
	mseg->mseg_sharecount--;
	if (mseg->mseg_sharecount <= 0) 
	{
		unlock(&mseg->mseg_lock);
		vm_destroysegment(mseg);
	}
	else unlock(&mseg->mseg_lock);
	return 0;
}

// Attach the given segment to the region, size should match
int vm_attachmemsegment(struct memregion *mreg, struct memsegment *msegment)
{
	_lock(&(mreg->mreg_reglock));
	mreg->mreg_segment = msegment;
	msegment->mseg_sharecount++;
	unlock(&(mreg->mreg_reglock));
	return 0;
}
// Reduces the reefrnce count of the segment, if necessary frees the segment
int vm_detachmemsegment(struct memregion *mreg)
{
	_lock(&(mreg->mreg_reglock));
	vm_releasesegment((struct memsegment *)mreg->mreg_segment);
	mreg->mreg_segment = NULL;
	unlock(&(mreg->mreg_reglock));
	return 0;
}

// Create a new memory segment for a region, if not exists already
static struct memsegment * vm_creatememsegment(PageDir *pd, struct memregion *mreg, short memflag)
{
	struct core_header chs[32];
	struct memsegment *mseg;
	unsigned long curendaddr; 
	unsigned short rw;
	int ret;
	int i, j, n, np, pdind, ptind;
	PageTable *pt;
	unsigned int phys_addr, oldcr0;

	oldcr0 = clear_paging();
	mseg = (struct memsegment *)kmalloc(sizeof(struct memsegment));
	if (mseg == NULL) printk("Malloc failure in vm_creatememsegment\n");
	mseg->mseg_sourcefile = 0;
	mseg->mseg_sourcedev = 0;
	mseg->mseg_sharecount = 1;
	mseg->mseg_npages = ((((mreg->mreg_endaddr + 4095) >> 12) & 0x000fffff) - ((mreg->mreg_startaddr >> 12) & 0x000fffff));
	mseg->mseg_pgtable = NULL;
	lockobj_init(&mseg->mseg_lock); 

	rw = (mreg->mreg_type == SEGMENTTYPE_CODE) ? 0 : 1;
	ret = vm_expandpagetables(pd, mseg, mreg->mreg_startaddr, mreg->mreg_endaddr, 0, rw, 0);
	if (ret < 0)
	{
		printk("Pagetable expansion failure\n");
		vm_destroysegment(mseg);
		restore_paging(oldcr0, current_thread->kt_tss->cr3);
		return NULL; 
	}
	
	// If page tables required, create
	_lock(&mreg->mreg_reglock);
	if (memflag != 0)
	{

		// Make a call to allocate no of requierd pages at once.
		// For all the core segments map them
		np = mseg->mseg_npages;
		n = core_alloc_n(np, chs, 32);
			
		if (n < 0) // Failure in memory allocation
		{
			printk("segment memory allocation failure : %d pages\n", np);
			unlock(&mreg->mreg_reglock);
			vm_destroysegment(mseg);
			restore_paging(oldcr0, current_thread->kt_tss->cr3);
			return NULL; 
		}

		curendaddr = mreg->mreg_startaddr;
		// For each page of core segment map them
		for (i=0; i<n; i++)
		{
			phys_addr = chs[i].phys_addr;
			j = chs[i].no_blocks;
			while (j > 0)
			{
				// Get the page table base address
				pdind = ((curendaddr + 4095) >> 22) & 0x3ff;
				ptind = ((curendaddr + 4095) >> 12) & 0x3ff;
				pt = (PageTable *)(((unsigned int)pd->entry[pdind].page_table_base_addr) << 12);

				// Set the page table entry, using 
				// the index numbers of logical address.
				make_PTE(pt, ptind, ((phys_addr >> 12) & 0x000fffff), 0, rw, 1, 0);
				j--;
				phys_addr += 4096;
				curendaddr += 4096;
			}
		}
	}
	
	mreg->mreg_segment = mseg;
	unlock(&mreg->mreg_reglock);
	restore_paging(oldcr0, current_thread->kt_tss->cr3);
	return mseg;
}

static int vm_destroysegment(struct memsegment *mseg)
{
	struct core_header ch;
	struct PageTableList *ptl1, *ptl2;
	PageTable *ptaddr;
	unsigned int start_addr, prev_addr,phys_addr;
	int n, j; 
	unsigned int oldcr0;

	oldcr0 = clear_paging();
	ptl1 = (struct PageTableList *)mseg->mseg_pgtable;
	while (ptl1 != NULL)
	{
		start_addr = 0;
		prev_addr = 0;
		n = 0;

		ptaddr = (PageTable *)ptl1->pgtable;
		for (j=0; j<1024; j++)
		{
			phys_addr = ptaddr->entry[j].page_base_addr;
			if (phys_addr == 0)
			{
				if (start_addr != 0)
				{
					ch.phys_addr = (start_addr << 12);
					ch.no_blocks = n;
					core_free(ch);
					start_addr = prev_addr = 0;
					n = 0;
				}
				continue;
			}
			if (prev_addr == 0)
			{
				start_addr = prev_addr = phys_addr;
				n = 1;
			}
			else
			{
				if (prev_addr + 1 == phys_addr) n++;
				else
				{
					// Another region, so release 
					// previous region.
					ch.phys_addr = (start_addr << 12);
					ch.no_blocks = n;
					core_free(ch);
					start_addr = prev_addr = phys_addr;
					n = 1;
				}
			}
		}
		if (n > 0)
		{
			ch.phys_addr = (start_addr << 12);
			ch.no_blocks = n;
			core_free(ch);
		}
		ptl2 = ptl1;
		ptl1 = (struct PageTableList *)ptl1->next;
		ch.phys_addr = (unsigned int)ptl2->pgtable;
		ch.no_blocks = 1;
		core_free(ch);
		kfree(ptl2);
	}
	kfree(mseg);
	restore_paging(oldcr0, current_thread->kt_tss->cr3);
	return 0;
}

// Assumes that this is called with paging disabled.
// Otherwise such an entry makes no sence.
PTE* vm_getptentry(struct vm *vmem, unsigned int lpage)
{
	int ptind, pdind;
	PageTable *pt;

	pdind = (lpage >> 10) & 0x3ff;
	ptind = (lpage & 0x3ff);

	pt = (PageTable *) (vmem->vm_pgdir->entry[pdind].page_table_base_addr << 12);
	if (pt != NULL) return &(pt->entry[ptind]);
	else return NULL;
}
		
// Assumes that this is called with paging disabled.
// Otherwise such an entry makes no sence.
PDE* vm_getpdentry(struct vm *vmem, unsigned int lpage)
{
	int pdind;

	pdind = (lpage >> 10) & 0x3ff;
	return &(vmem->vm_pgdir->entry[pdind]);
}
		
// Set/Get the attributes which are not null
int vm_setattr(struct vm *vmem, unsigned int lpage, int *avail, int *rw, int *present, int *global, int *accessed, int *dirty)
{
	PTE *pt;
	unsigned int oldcr0;
	int retval;

	oldcr0 = clear_paging();
	pt = vm_getptentry(vmem, lpage);
	if (pt != NULL)
	{
		if (present != NULL) pt->present = *present;
		if (avail != NULL) pt->available = *avail;
		if (rw != NULL) pt->read_write = *rw;
		if (global != NULL) pt->global = *global;
		if (dirty != NULL) pt->dirty = *dirty;
		if (accessed != NULL) pt->accessed = *accessed;	
		retval = 0;
	}
	else retval = -1;
	restore_paging(oldcr0, current_thread->kt_tss->cr3);
	return retval;
}

int vm_getdirtylist(struct vm *vmem, int *buf, int size, int type)
{ return vm_getattrlistpages(vmem, buf, size, type, 2); }
int vm_getaccesslist(struct vm *vmem, int *buf, int size, int type)
{ return vm_getattrlistpages(vmem, buf, size, type, 1); }
// Get the accessed == 1, dirty == 2 list
static int vm_getattrlistpages(struct vm *vmem, int *buf, int size, int type, int attr)
{
	PTE *pt;
	struct memregion *mreg;
	struct memsegment *mseg;
	struct PageTableList *ptl1;
	int i=0,j;
	unsigned int pgno;
	int *physbuf;
	unsigned int oldcr0;

	oldcr0 = clear_paging();
	_lock(&(vmem->vm_lock));	
	mreg = (struct memregion *)vmem->vm_mregion;
	// Actually locking of all the pages corresponding to buf must be done.
	// It must ensure that every page must be present in memory.
	// Buf as we are not using swapping, no need of that.
	physbuf = (int *)logical2physical(vmem->vm_pgdir, (char *)buf);
	while (mreg != NULL && i < size)
	{
		if ((mreg->mreg_type == type) || (type == 0)) // matching region
		{
			mseg = vm_getmemsegment(mreg);
			_lock(&(mreg->mreg_reglock));
			if (mseg != NULL) 
			{
				pgno = (mreg->mreg_startaddr >> 12) & 0x000fffff;
				ptl1 = (struct PageTableList *)mseg->mseg_pgtable;
				while (ptl1 != NULL && i < size)
				{
					pt = (PTE *)ptl1->pgtable->entry;
					for (j=0; i < size && j<1024; j++)
					{
						if ((attr == 2) && (pt+j)->dirty == 1)
						{
							if ((((unsigned int)physbuf) % 0x1000) == 0)
								physbuf = (int *)logical2physical(vmem->vm_pgdir, (char *)&buf[i]);
							*physbuf = pgno+j;
							i++;
							physbuf++;
							(pt+j)->dirty = 0;
						}
						if ((attr == 1) && (pt+j)->accessed == 1)
						{
							if ((((unsigned int)physbuf) % 0x1000) == 0)
								physbuf = (int *)logical2physical(vmem->vm_pgdir, (char *)&buf[i]);
							*physbuf = pgno+j;
							i++;
							physbuf++;
							(pt+j)->accessed = 0;
						}
					}
					ptl1 = (struct PageTableList *)ptl1->next;
					pgno += 1024;
				}
				vm_releasesegment(mseg);
			}
			unlock(&(mreg->mreg_reglock));
		}
		mreg = (struct memregion *)mreg->mreg_next;
	}
	unlock(&(vmem->vm_lock));	
	restore_paging(oldcr0, current_thread->kt_tss->cr3);
	return i;
}

int vm_getattr(struct vm *vmem, unsigned int lpage, int *avail, int *rw, int *present, int *global, int *accessed, int *dirty)
{ return 0; }
// Creates a page directory entry with given parameters
static void make_PDE(	PageDir *pd, unsigned int i, unsigned long addr, unsigned char avail, unsigned char rw, unsigned char present, unsigned char global	)
{
	pd->entry[i].present = present;
	pd->entry[i].read_write = rw;
	pd->entry[i].user = ((global == 1)?0:1);
	pd->entry[i].write_through = 1;
	pd->entry[i].cache_disable = 1; // Always disabled
	pd->entry[i].accessed = 0;
	pd->entry[i].reserved = 0;
	pd->entry[i].page_4MB = 0;
	pd->entry[i].global = global;
	pd->entry[i].available = avail;
	pd->entry[i].page_table_base_addr = addr;
}

// Creates a page table entry with given parameters
static void make_PTE(	PageTable *pt, unsigned int i, unsigned long addr, unsigned char avail, unsigned char rw, unsigned char present, unsigned char global	)
{
	pt->entry[i].present = present;
	pt->entry[i].read_write = rw;
	pt->entry[i].user = ((global == 1)?0:1);
	pt->entry[i].write_through = 1;
	pt->entry[i].cache_disable = 1; // Always disabled
	pt->entry[i].accessed = 0;
	pt->entry[i].reserved = 0;
	pt->entry[i].dirty = 0;
	pt->entry[i].global = global;
	pt->entry[i].available = avail;
	pt->entry[i].page_base_addr = addr;
}

void make_page_table(PageTable *pt, int start_index, int nentries, unsigned long shifted_phys_address, unsigned char attr, unsigned char rw, unsigned char present, unsigned char global)
{
	int i;

	for (i=0; i<nentries; i++)
	{
		make_PTE(pt, start_index+i, shifted_phys_address, attr, rw, present, global);
		shifted_phys_address++;
	}
}


void initialize_kernel_pagetables(void)
{
	int i,n;
	struct core_header ch;

	lockobj_init(&core_lock);
	lockobj_init(&kmalloc_lock);

	n = (((KERNEL_MEMMAP + 0x3fffff) >> 22) & 0x3ff); // Round up and how many 4MB
	ch = core_alloc(n);		// For page tables, must be successful
	KERNEL_PAGE_TABLE = (PageTable *)ch.phys_addr;

	bzero((char *)&KERNEL_PAGEDIR, sizeof(PageDir));
	bzero((char *)KERNEL_PAGE_TABLE, sizeof(n*4096));

	// Set the page dir entries
	for (i=0; i<n; i++)
	{
		make_PDE(&KERNEL_PAGEDIR, i, ((unsigned long) &(KERNEL_PAGE_TABLE[i])) >> 12, 0, 1, 1, 1);
		make_page_table(&KERNEL_PAGE_TABLE[i], 0, 1024, ((i*0x00400000)>>12), 0, 1, 1, 1);
	}

	return;
}

int phys2vmcopy(char *src, struct vm *dvm, char *dest, int nbytes)
{
	int prem, ncopy;
	char *physdest;
	PageDir *dpd = dvm->vm_pgdir;

	while (nbytes > 0)
	{
		// Lock the page and find physical address.
		if (lock_page(dpd, dest) != 0)
		{
			printk("ERROR : phy2vmcopy\n");
			break; // some problem
		}
		physdest = logical2physical(dpd, dest);
		// How many bytes in the current page?
		prem = 4096 - (((int)physdest) % 4096);
		ncopy = (prem < nbytes) ? prem : nbytes;
		bcopy(src, physdest, ncopy);
		unlock_page(dpd, dest);

		// Advance the pointers
		src = src + ncopy;
		dest = dest + ncopy;
		nbytes = nbytes - ncopy;
	}

	if (nbytes == 0) return 0; // success
	else return EMEMCOPY;
}

int vm2physcopy(struct vm *svm, char *src, char *dest, int nbytes)
{
	int prem, ncopy;
	char *physsrc;
	PageDir *spd = svm->vm_pgdir;

	while (nbytes > 0)
	{
		// Lock the page and find physical address.
		if (lock_page(spd, src) != 0) 
		{
			printk("ERROR : vm2physcopy\n");
			break; // some problem
		}
		physsrc = logical2physical(spd, src);
		// How many bytes in the current page?
		prem = 4096 - (((int)physsrc) % 4096);
		ncopy = (prem < nbytes) ? prem : nbytes;
		bcopy(physsrc, dest, ncopy);
		unlock_page(spd, src);

		// Advance the pointers
		src = src + ncopy;
		dest = dest + ncopy;
		nbytes = nbytes - ncopy;
	}

	if (nbytes == 0) return 0; // success
	else return EMEMCOPY;
}

int vm2vmcopy(struct vm *svm, char *src, struct vm *dvm, char *dest, int nbytes)
{
	int prem, prem1, prem2, ncopy;
	char *physdest, *physsrc;
	PageDir *dpd = dvm->vm_pgdir, *spd = svm->vm_pgdir;

	while (nbytes > 0)
	{
		// Lock the pages and find physical addresses.
		if (lock_page(dpd, dest) != 0) break; // some problem
		if (lock_page(spd, src) != 0) 
		{
			unlock_page(spd, dest);
			break; // some problem
		}

		physdest = logical2physical(dpd, dest);
		physsrc = logical2physical(spd, src);

		// How many bytes in the current page?
		prem1 = 4096 - (((int)physdest) % 4096);
		prem2 = 4096 - (((int)physsrc) % 4096);
		prem = (prem1 < prem2) ? prem1 : prem2;
		ncopy = (prem < nbytes) ? prem : nbytes;
		bcopy(src, physdest, ncopy);
		unlock_page(spd, src);
		unlock_page(dpd, dest);

		// Advance the pointers
		src = src + ncopy;
		dest = dest + ncopy;
		nbytes = nbytes - ncopy;
	}

	if (nbytes == 0) return 0; // success
	else return EMEMCOPY;
}

static int page_in(PageDir *pd, unsigned int pgno)
{
	printk("page in is called : %s, pgno : %d\n", current_thread->kt_name, pgno);
	return -1;
}

int lock_page(PageDir *pd, char *logaddr)
{
	int ind;
	PageTable *pt;
	//int oldlock;

	ind = (((unsigned int)logaddr) >> PDE_SHIFT) & 0x3ff;
	pt = (PageTable *)((pd->entry[ind].page_table_base_addr) << 12);
	
	ind = (((unsigned int)logaddr) >> 12) & 0x3ff;
	if (pt->entry[ind].present != 1)
		if (page_in(pd, ((unsigned int)logaddr >> 12) & 0x000fffff) != 0)
			return -1;
	
	return 0;
}

int unlock_page(PageDir *pd, char *logaddr)
{
	int ind;
	PageTable *pt;

	ind = (((int)logaddr) >> PDE_SHIFT) & 0x3ff;
	pt = (PageTable *)((pd->entry[ind].page_table_base_addr) << 12);
	ind = (((unsigned int)logaddr) >> 12) & 0x3ff;

	return 0;
}

char *logical2physical(PageDir *pd, char *logicaladdr)
{
	int ind;
	PageTable *pt;
	unsigned int addr;
	char *physaddr = NULL;

	ind = (((unsigned int)logicaladdr) >> PDE_SHIFT) & 0x3ff;
	addr = pd->entry[ind].page_table_base_addr;

	pt = (PageTable *)(addr << 12);

	ind = (((unsigned int)logicaladdr) >> 12) & 0x3ff;

	if (pt->entry[ind].present == 1)
	{
		addr = pt->entry[ind].page_base_addr;
		physaddr = (char *)((addr << 12) | ((unsigned int)logicaladdr & 0xfff));
	}
		
	return physaddr;
}


/*
 * Core management, low level memory allocation and deallocation.
 * Allocation is done at page granularity(4kb).
 */
struct core_header core_alloc(int npages)
{
	unsigned long baddr = 0;
	long hole_size;
	struct free_mem_block *t,*t1=NULL,*t2=NULL;
	struct core_header chead;
	unsigned long oldcr0;
	/* 
	 * best fit method for allocation, but if sufficient size
	 * is not found maximum available block is sent.
	 */
	chead.phys_addr = 0;
	chead.no_blocks = 0;

	oldcr0 = clear_paging();
	_lock(&core_lock);
	if (first_block == NULL)
	{
		unlock(&core_lock);
		restore_paging(oldcr0, current_thread->kt_tss->cr3);
		return chead;
	}

	baddr = (unsigned long) first_block;
	hole_size = first_block->no_pages - npages;
	t = (struct free_mem_block *)first_block -> next_block;
	t2 = (struct free_mem_block *)first_block;
	t1 = NULL;

	while (t != NULL) 
	{
		// If it is system memory allocation donot allocate above the
		// KERNEL_MEMMAP
		if (hole_size < 0)
		{
			if (hole_size < (long)(t->no_pages - npages))
			{
				baddr = (unsigned long) t;
				hole_size = t->no_pages - npages;
				t1 = t2; /* Previous node. */
			}
		}
		else if ((t->no_pages > npages) && 
			 (hole_size > (t->no_pages - npages)))
		{
			baddr = (unsigned long) t;
			hole_size = t->no_pages - npages;
			t1 = t2; /* Previous node. */
		}
		t2 = t;	
		t = (struct free_mem_block *)t->next_block;
	}
	
	chead.phys_addr = baddr;
	if (hole_size >= 0) chead.no_blocks = npages;
	else chead.no_blocks = npages + hole_size;

	/*
	 * Update the linked list of free blocks.
	 */

	t = (struct free_mem_block *) baddr;

	if (hole_size <= 0) /* Entire node must be deleted */
	{
		if (t == first_block) first_block = t->next_block;
		else t1->next_block = t->next_block;
	}
	else	/* Just modify the node size	*/
	{
		t2 = (struct free_mem_block *)(baddr + npages *4096); /* 4096 */		t2->no_pages = hole_size;
		t2->next_block = t->next_block;
		if (t == first_block) first_block = t2;
		else t1->next_block = t2;
	}

	unlock(&core_lock);
	restore_paging(oldcr0, current_thread->kt_tss->cr3);

	return chead;
}

// Allocate specifired number of pages, which may not be contiguous
// STore their correcsponding core headers of all allocated
// fragments in ch array whose size is assumed to be chmax entries.
int core_alloc_n(int npages, struct core_header ch[], int chmax)
{
	int i = 0, pages = npages;
	unsigned int oldcr0;

	oldcr0 = clear_paging();
	while (pages > 0 && i < chmax)
	{
		ch[i] = core_alloc(pages);
		if (ch[i].no_blocks == 0) // No memory available
		{
			// Free all the memory allocated so far
			while (i > 0)
			{
				core_free(ch[i-1]);
				i--;
			}
			restore_paging(oldcr0, current_thread->kt_tss->cr3);
			return ENOMEM;
		}
		pages = pages - ch[i].no_blocks;
		i++;
	}

	if (pages > 0) // Too Many fragments
	{
		// Free all the memory allocated so far
		while (i > 0)
		{
			core_free(ch[i-1]);
			i--;
		}
		restore_paging(oldcr0, current_thread->kt_tss->cr3);
		return ENOMEM;
	}

	restore_paging(oldcr0, current_thread->kt_tss->cr3);
	return i;
}

void core_free(struct core_header ch)
{
	struct free_mem_block *t1,*t2;
	unsigned long baddr1,baddr2,baddr;
	unsigned int oldcr0;

	/* 
	 * Minimal checking whether the freeing block is 
	 * falling within the usable ram segments ?
	 */
	oldcr0 = clear_paging();
	_lock(&core_lock);
	if (!is_valid_block(ch))
	{
		printk("phys_addr : %x len = %d not valid\n", ch.phys_addr, ch.no_blocks);
		unlock(&core_lock);
		restore_paging(oldcr0, current_thread->kt_tss->cr3);
		return;
	}

	t1 = find_mem_block_before(ch);
	baddr = ch.phys_addr;

	if ( t1 == NULL )
	{
		/* Should be inserted in the begining */
		t2 = (struct free_mem_block *)first_block;
		first_block = (struct free_mem_block *) ch.phys_addr;
		first_block->no_pages = ch.no_blocks;
		if ((t2 != NULL) && (baddr + (ch.no_blocks * 4096) == (unsigned long) t2))
		{
			first_block->no_pages += t2->no_pages;
			first_block->next_block = t2->next_block;
		}
		else first_block->next_block = t2;
	}
	else
	{
		/* Should be inserted after t1 */
		baddr1 = (unsigned long)t1;
		t2 = (struct free_mem_block *)t1->next_block;
		baddr2 = (unsigned long) t2;
		if ((baddr1 + (t1->no_pages * 4096)) == baddr)
		{
			/* Adjacent to previous block. */
			t1->no_pages += ch.no_blocks;
			if ((t2 != NULL) && (baddr1 + (t1->no_pages * 4096) == baddr2))
			{
				/* Adjacent to next block. */
				t1->no_pages += t2->no_pages;
				t1->next_block = t2->next_block;
			}
		}
		else if ((t2 != NULL) && (baddr + (ch.no_blocks * 4096) == baddr2)) 
		{   /* if t2 is NULL this part will not be executed. */
			/* Adjacent to next block. */
			t1->next_block = (struct free_mem_block *)baddr;
			t1 = (struct free_mem_block *)t1->next_block;
			t1->no_pages = ch.no_blocks + t2->no_pages;
			t1->next_block = t2->next_block;
		}
		else {
			/* Create a new node */
			t1->next_block = (struct free_mem_block *)baddr;
			t1 = (struct free_mem_block *)t1->next_block;
			t1->no_pages = ch.no_blocks;
			t1->next_block = t2;
		}
					
	}
	unlock(&core_lock);
	restore_paging(oldcr0, current_thread->kt_tss->cr3);
}

static struct free_mem_block * find_mem_block_before(struct core_header ch)
{
	struct free_mem_block *t1, *t2;

	t2 = (struct free_mem_block *)first_block;
	t1 = NULL;
	while ((t2 != NULL) && ((unsigned long)t2 < ch.phys_addr))
	{
		t1 = t2;
		t2 = (struct free_mem_block *)t2->next_block;
	}
	return t1;
}

void initialize_core_info(char *first_free_loc)
{
	int i;
	struct free_mem_block *t=NULL;
	int n=0;

	lockobj_init(&core_lock); // Initialize lock
	/* First segment is base memory starting at base address 0x0 */

	i = 0;	
	while (((ram_segments[i].base_addr + ram_segments[i].length) < (long) first_free_loc) > 0) i++;


	first_block = (struct free_mem_block *)first_free_loc;
	first_block->no_pages = (ram_segments[i].base_addr + ram_segments[i].length - (long) first_free_loc) / 4096;

	t = (struct free_mem_block *)first_block;
	t->next_block = NULL;
	n++;
	
	/*
	 * Assuming the aboove condition is success, other wise the 
	 * kernel should not have been started (loaded).
	 */
	for ( ; i<MAX_RAM_SEGMENTS; i++)
	{
		if (ram_segments[i].type == 1) /* Usable memory map */
		{
			t->next_block = (struct free_mem_block *) ram_segments[i].base_addr;
			t = (struct free_mem_block *)t->next_block;
			t->no_pages = ram_segments[i].length / 4096; // 4096;
			n++;
		}
		else if (ram_segments[i].type < 1) break;
	}
	no_of_ram_segments = i;

	t->next_block = NULL;
#ifdef DEBUG
	print_core_info();
#endif
}

static int is_valid_block(struct core_header ch)
{
	int valid=0,i;

	if (ch.no_blocks <= 0) return 0;
	for (i=0; i<no_of_ram_segments; i++)
	{
		if (ram_segments[i].type == 1)
		{
			if ((ch.phys_addr >= ram_segments[i].base_addr) && 
			    (ch.phys_addr <= ram_segments[i].base_addr + ram_segments[i].length))
			{
				valid = 1;
				return valid;
			}
		}
	}

	return valid;
}

/*
static void check_release_core(void)
{
	struct core_header ch;
	struct mem_list *m1, *m2;
	unsigned int npages;
	unsigned int startaddr;

	// Releasing of the memory can be done only at the end.
	// For kernel memory management releasing any block of memory is valid
	// But for user process only at the end can be done.
	m1 = (struct mem_list *)kmavail_list;
	while (m1 != NULL)
	{
		
		m2 = (struct mem_list *)m1->next;
		// If the block is starting at the page boundary
		// or ending at the page boundary, then release it.
		if ((m1->size >= 0x1000) && ((((unsigned int)m1->addr & 0x00000fff) == 0) || ((((unsigned int)m1->addr + m1->size) & 0x00000fff) == 0))) 
		{
			startaddr = ((unsigned int)m1->addr + 4095) & 0xfffff000;
			ch.phys_addr = startaddr;
			npages = (((unsigned int)m1->addr + m1->size) - startaddr) / 4096;
			ch.no_blocks = npages;
			core_free(ch);
			// check if the total node is to be deleted
			if (npages * 0x1000 == m1->size) delete_avail_node(m1);
			else
			{
				m1->size = m1->size - npages * 0x1000;
				// is it freed at the beginning or at the end?
				if (((unsigned int)m1->addr & 0x00000fff) == 0)
					m1->addr = m1->addr + npages * 0x1000;
				// else (Otherwise starting at the same address)
			}
		}
		m1 = m2;
	}
}
*/
static void insert_avail_node(struct mem_list *p)
{
	struct mem_list *q1, *q2 = NULL;

	// Add p to kmavail_list
	p->next = NULL;
	if (kmavail_list == NULL)
	{
		kmavail_list = p;
		return;
	}
	else
	{
		// Insertion in the beginning?
		if (kmavail_list->addr > p->addr)
		{
			if (p->addr + p->size == kmavail_list->addr)
			{
				kmavail_list->addr = p->addr;
				kmavail_list->size += p->size;
				// Free p.
				p->size = 0;
				kmcount--;
			}
			else
			{
				p->next = kmavail_list;
				kmavail_list = p;
			}
			return;
		}

		// Insertion in the middle or ...
		q1 = (struct mem_list *)kmavail_list;
		while (q1 != NULL && q1->addr+q1->size < p->addr)
		{
			q2 = q1;
			q1 = (struct mem_list *)q1->next;
		}

		if (q1 != NULL)
		{
		       	if (q1->addr + q1->size == p->addr)
			{
				q1->size += p->size;
				p->size = 0;
				kmcount--;
				// Look for next node
				q2 = (struct mem_list *)q1->next;
				if ((q2 != NULL)  && (q1->addr+q1->size == q2->addr))
				{
					q1->size += q2->size;
					q2->size = 0;
					kmcount--;
					q1->next = q2->next;
				}
			}
			else if (p->addr + p->size == q1->addr)
			{
				q1->addr = p->addr;
				q1->size += p->size;
				p->size = 0;
				kmcount--;
			}
			else
			{
				p->next = q1;
				q2->next = p;
			}
		}
		else q2->next = p; // At the end
		return;
	}
}

static void delete_avail_node(struct mem_list *p)
{
	struct mem_list *q2;

	p->size = 0;
	if (kmavail_list == p)
	{
		kmavail_list = kmavail_list->next;
		p->next = NULL;
		return;
	}

	q2 = (struct mem_list *)kmavail_list;

	while (q2 != NULL && q2->next != p)
		q2 = (struct mem_list *)q2->next;

	if (q2 != NULL) q2->next = p->next; // p is removed
	else printk("Error : delete_avail_node : %x\n",p);
	p->next = NULL;
	return;
}

static struct mem_list *create_mem_list_node(void)
{
	struct mem_list *p1;
	struct core_header chead;
	char *ptr1, *ptr2=NULL;
	int i;

	// If no meminfo block allocated
	if (kmmalloc_info == NULL)
	{
		chead = core_alloc(1); // 1 - page,  4096 bytes
		if (chead.no_blocks < 1) return NULL;
		kmmalloc_info = (char *)chead.phys_addr;

		bzero((char *)kmmalloc_info,4096); // Including last pointer NULL
	}

	// Search for freenode in that block
	ptr1 = (char *)kmmalloc_info;
	p1 = (struct mem_list *)ptr1;
	
	while (p1 != NULL)
	{
		for (i=0; i<((4096-4)/sizeof(struct mem_list));  i++)
		{
			if (p1->size == 0) return p1;
			p1 = p1 + 1;
		}
		ptr2 = ptr1;
		bcopy(ptr2+4092,(char *)&ptr1,4); // Pointer to next block
		p1 = (struct mem_list *)ptr1;
	}

	// No free node found ... allocate new.
	chead = core_alloc(1); // 1- page
	if (chead.no_blocks < 1) return NULL;
	ptr1 = (char *)chead.phys_addr;

	bcopy((char *)&ptr1, ptr2+4092,4);
	bzero((char *)ptr1, 4096);
	p1 = (struct mem_list *)ptr1;

	return p1;
}

void *kmalloc(unsigned int size)
{
	struct mem_list *p1 = NULL, *p2 = NULL, *p3 = NULL;
	char avl = 0;
	struct core_header chead;
	int hole = 0xefffffff;

	if (size <= 0) 
	{
		return NULL;
	}
	if (size > MALLOC_MAX) return NULL; // Greater than maximum allocatable.
	// Search the available list for bestfit
	_lock(&kmalloc_lock);	
kmalloc_1:
	if (kmavail_list != NULL)
	{
		p1 = (struct mem_list *)kmavail_list;

		while (p1 != NULL)
		{
			if (p1->size >= size)
			{
				avl = 1;
				if ((p1->size - size) < hole)
				{
					p3 = p1;
					hole = p1->size - size;
				}
			}

			p2 = p1;
			p1 = (struct mem_list *)p1->next;
		}
	}

	p1 = p3; // If available
	// If not available allocate core.
	if (avl == 0)
	{
		chead = core_alloc((size + 4095) / 4096); //, 1); 
		if (chead.no_blocks * 4096 < size)
		{
			// Release that and return NULL
			if (chead.no_blocks > 0) core_free(chead);
			unlock(&kmalloc_lock);	
			printk("kmalloc returning (1) NULL\n");
			return NULL;
		}
		// add a mem_list node to the kmavail_list
		p1 = create_mem_list_node();
		kmcount++;
		if (p1 == NULL)
		{
			core_free(chead);
			unlock(&kmalloc_lock);	
			printk("kmalloc returning (2) NULL\n");
			return NULL;
		}

		p1->addr = (char *)chead.phys_addr;
		p1->size = chead.no_blocks * 4096;
		p1->next = NULL;
		insert_avail_node(p1);
		goto kmalloc_1;
	}
	
	// Allocate requested size of memory from p1
	if (p1 != NULL && p1->size > size) 
	{
		// Used list node
		p3 = create_mem_list_node();
		kmcount++;
		if (p3 == NULL)
		{
			unlock(&kmalloc_lock);	
			printk("kmalloc returning (3) NULL\n");
			return NULL;
		}
		p3->addr = p1->addr;
		p3->size = size;
		p3->next = NULL;
		p1->size = p1->size - size;
		p1->addr += size;
	}
	else // Exact size
	{
		p3 = p1;
		delete_avail_node(p1);
		// Size must be set back to original size (because set to 0)
		p3->size = size;
	}

	// Insert allocated information into used list
	p3->next = kmused_list;
	kmused_list = p3;

	// For safety initialize to zeroes
	unlock(&kmalloc_lock);
	return (struct mem_list *)p3->addr;
}

void kfree(void *ptr)
{
	struct mem_list *p1, *p2 = NULL;

	if (ptr == NULL)
	{
		printk("KFREE freeing NULLLLLLLLLLLLLL\n");
		return;
	}
	// Remove the corresponding node from used list
	_lock(&kmalloc_lock);	
	p1 = (struct mem_list *)kmused_list;
	while (p1 != NULL)
	{
		if (p1->addr == ptr)
		{
			if (p1 == kmused_list) kmused_list = p1->next;
			else p2->next = p1->next;
			p1->next = NULL;

			insert_avail_node(p1);
			//check_release_core();
			unlock(&kmalloc_lock);	
			return;
		}

		p2 = p1;
		p1 = (struct mem_list *)p1->next;
	}
	unlock(&kmalloc_lock);	
	printk("kfree cannot find node ............... : %x\n",ptr);
	return;
}

int syscall_brk(void *end_data_addr)
{
	struct memregion *ureg1;
	unsigned int oldendaddr, startaddr;
	int diffsize, ret;

	_lock(&current_process->proc_vm->vm_lock);
	// Locate the data segment
	
	ureg1 = (struct memregion *)current_process->proc_vm->vm_mregion;
	while (ureg1 != NULL && ureg1->mreg_type != SEGMENTTYPE_DATA) 
		ureg1 = (struct memregion *)ureg1->mreg_next;
	oldendaddr = ureg1->mreg_endaddr;	
	startaddr = ureg1->mreg_startaddr;	
	unlock(&current_process->proc_vm->vm_lock);
	if (end_data_addr == NULL) return oldendaddr;

	// Should not be below the starting of data segment.
	if ((unsigned int)end_data_addr < startaddr) return EINVALIDADDRESS;

	// Otherwise it must be either shrinking or expanding
	diffsize = oldendaddr - (unsigned int)end_data_addr;

	if (diffsize < 0) // Expansion
		ret = vm_expandregion(current_process->proc_vm, startaddr, -diffsize, 1, 1);
	else ret = vm_shrinkregion(current_process->proc_vm, startaddr, diffsize);
	if (ret == 0) return 0;
	else return ENOMEM;
}
		
// Only one page memory is allocated
// Later should be modified to have support for 
// even page table extension ...
int alloc_frame_memory(struct process *proc, unsigned int pageno)
{
	struct core_header ch;
	PageTable *pt;

	ch = core_alloc(1); //, 0); // user process.

	if (ch.no_blocks < 1) return ENOMEM;

	pt = (PageTable *)((proc->proc_vm->vm_pgdir->entry[(pageno >> 10) & 0x3ff].page_table_base_addr) << 12);
	if (pt == NULL) 
	{
		// Allocate even that page table memory also. 
		// Entire page table must be replaced and enlarged. Now not ...
		core_free(ch);
		printk("Error : page table is NULL for page no : %d\n",pageno);
		return ENOMEM;
	}
	make_PTE(pt, (pageno & 0x3ff), ((ch.phys_addr >> 12) & 0x000fffff), 0, 0, 0, 0);
	return 0;
}

void initialize_gdt(void)
{
	struct LDT *ldtp;
	struct segdesc_s *gdtp;
	int i;
	extern struct table_desc gdt_desc;

	/* Segment descriptors for code and data are already hard coded */
	/* Build local descriptors in GDT for LDT's in process table.
	 * The LDT's are allocated at compile time in the process table, and
	 * initialized whenever a process' map is initialized or changed.
	 */
	gdt_desc.limit = GDT_SIZE * 8 - 1;
	gdt_desc.base = (unsigned long ) gdt_table;
	gdt_desc.unused = 0;
	gdtp = (struct segdesc_s *)gdt_table;
	i = LDTDESC_START;
	for (ldtp = BEG_LDT_ADDR; ldtp <= END_LDT_ADDR; ldtp = ldtp + 1)
	{
		initialize_dataseg(&gdtp[i], (unsigned int)ldtp, sizeof(struct LDT), INTR_PRIVILEGE);
		gdtp[i].access = PRESENT | LDT_TYPE;
		i++; 
	}
}

void initialize_codeseg(struct segdesc_s *segdp, unsigned int base, unsigned int size, int privilege)
{
	/* Build descriptor for a code segment. */
	sdesc(segdp, base, size);
	segdp->access = (privilege << DPL_SHIFT) | (PRESENT | SEGMENT | EXECUTABLE | READABLE);
	/* CONFORMING = 0, ACCESSED = 0 */
}

/*=========================================================================*
 *				init_dataseg				   *
 *=========================================================================*/
void initialize_dataseg(struct segdesc_s *segdp, unsigned int base, unsigned int size, int privilege)
{
	/* Build descriptor for a data segment. */
	sdesc(segdp, base, size);
	segdp->access = (privilege << DPL_SHIFT) | (PRESENT | SEGMENT | WRITEABLE);
	/* EXECUTABLE = 0, EXPAND_DOWN = 0, ACCESSED = 0 */
}

void sdesc(struct segdesc_s *segdp, unsigned int base, unsigned int size)
{
	/* Fill in the size fields (base, limit and granularity) of a descriptor. */
	segdp->base_low = base;
	segdp->base_middle = base >> BASE_MIDDLE_SHIFT;

	segdp->base_high = base >> BASE_HIGH_SHIFT;
	--size;			/* convert to a limit, 0 size means 4G */
	if (size > BYTE_GRAN_MAX) 
	{
		segdp->limit_low = size >> PAGE_GRAN_SHIFT;
		segdp->granularity = GRANULAR | (size >> (PAGE_GRAN_SHIFT + GRANULARITY_SHIFT));
	} 
	else 
	{
		segdp->limit_low = size;
		segdp->granularity = size >> GRANULARITY_SHIFT;
	}
	segdp->granularity |= DEFAULT;	/* means BIG for data seg */
}


int syscall_prifree(void *addr, unsigned int size)
{
	int npages;
	unsigned int start_page;
	unsigned int old_cr0;
	int i;

	npages = ((size + 4095) >> 12);
	start_page = (((unsigned int) addr) >> 12) - 0xFDC00;

	old_cr0 = clear_paging();
	if (vm_delregion(current_process->proc_vm, (unsigned int)addr) == 0)
	{
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
		for (i=0; i<npages; i++)
			bitmap[i+start_page] = 0;
		return 0;
	}
	restore_paging(old_cr0, current_thread->kt_tss->cr3);

	return 0;
}

int syscall_primalloc(unsigned int size)
{
	unsigned int old_cr0;
	int i, j, count, npages;
	unsigned int logical_addr, start_page;
	int found;
	int ret;

	if (size > PRIMALLOC_MAX) return 0; // Failed

	// This is above the stack segment
	// Allocate memory above 0xC0000000
	npages = ((size + 4095) >> 12);
	// find npages in the bitmap

	i = 0;
	found = 0;
	while (!found && i < 32 * 256)
	{
		if (bitmap[i] == 0)
		{
			j = i + 1;
			count = 1;
			while ((count < npages) && (bitmap[j] == 0) && (j < 32 * 256))
			{
				j++; count++;
			}
			if (count == npages)
			{
				found = 1;
				break;
			}
			else i = j+1;
		}
		else i++;
	}
	if (found == 1)
	{
		// What is its logical start
		start_page = i;
		logical_addr = 0xFDC00000 + start_page * 0x1000;

		old_cr0 = clear_paging();
		ret = vm_addregion(current_process->proc_vm, SEGMENTTYPE_PRIVATE, logical_addr, logical_addr + npages * 0x1000, 1, 1);
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
		if (ret == 0) // Success
		{
			for (i=0; i<npages; i++)
				bitmap[i+start_page] = 1;
			return logical_addr;
		}
	}
	return NULL;
}
	
#ifdef DEBUG
void print_tss(void)
{
	printk("TSS Descriptor : %X : %X \n",gdt_table[3].a, gdt_table[3].b);
}

void disp_PTE(	PageTable *pt,
		unsigned int i)
{
	printk("PTE [%d] { present : %d RD = %d global = %d page base addr = %x}\n",i, pt->entry[i].present, pt->entry[i].read_write, pt->entry[i].global, pt->entry[i].page_base_addr);
}

void disp_page_table(PageTable *pt, int nent)
{
	int i;

	for (i=0; i<1024; i++)
		if (pt->entry[i].page_base_addr != 0) disp_PTE(pt, i);
}

void disp_page_dir(PageDir *pd)
{
	int i;
	PageTable *pt;

	for (i=4; i<1024; i++)
	{
		if (pd->entry[i].page_table_base_addr != 0)
		{
			printk("PD entry[%d] { present : %d RD = %d global = %d page table base addr = %x}\n",i, pd->entry[i].present, pd->entry[i].read_write, pd->entry[i].global, pd->entry[i].page_table_base_addr);
			pt = (PageTable *)((unsigned long)(pd->entry[i].page_table_base_addr)<<12);
			disp_page_table(pt, 1024);
		}
	}
}
void print_core_info(void)
{
	struct free_mem_block *p = (struct free_mem_block *)first_block;

	printk("Core information :\n");
	while (p != NULL)
	{
		printk("Addr = %x pages = %d\n",p,p->no_pages);
		p = (struct free_mem_block *)p->next_block;
	}
}
void print_ramsegments(void)
{
	int i;

	for (i=0; i<no_of_ram_segments; i++)
		printk("seg[%d] = addr = %x -- length = %d\n",i,ram_segments[i].base_addr, ram_segments[i].length);
}
void print_list(struct mem_list *l)
{
	struct mem_list *p;

	p = l;
	while (p != NULL)
	{
		printk("node = %x addr = %x size = %d\n",p,p->addr,p->size);
		p = (struct mem_list *)p->next;
	}
}

void print_gdt(int start, int n)
{
	int i;

	for (i=start; i<start+n; i++)
		printk("[%x] --> a : %x b : %x\n",i,gdt_table[i].a, gdt_table[i].b);
}

void print_minf_statistics(void)
{
	struct mem_list *p;
	int count, size, i;

	count = 0; size = 0;
	p = (struct mem_list *)kmavail_list;
	while (p != NULL)
	{
		printk(" (%x, %d, %x)",p,p->size,p->addr);
		count++;
		size += p->size;
		p = (struct mem_list *)p->next;
	}
	printk("\nAvailable mem info : count of blocks = %d total size = %d\n", count, size);

	count = 0; size = 0;
	p = (struct mem_list *)kmused_list;
	printk("kmcount : %d\n",kmcount);
	while (p != NULL)
	{
		printk(" (%x, %d, %x)",p,p->size,p->addr);
		for (i=0; i<0x800000; i++) ;
		count++;
		size += p->size;
		p = (struct mem_list *)p->next;
	}
	printk("\nUsed mem info : count of blocks = %d total size = %d\n", count, size);
}


void display_vm(struct vm *v)
{
	struct memregion *mr1; //, *mr2;
	struct memsegment *mseg1; //, *mseg2;

	printk("vm : %x, vm_pgdir : %x\n", v, v->vm_pgdir);
	mr1 = (struct memregion *)v->vm_mregion;

	while (mr1 != NULL)
	{
		printk("mreg : %x, start : %x, end : %x\n",mr1, mr1->mreg_startaddr, mr1->mreg_endaddr);
		mseg1 = (struct memsegment *)mr1->mreg_segment;
		if (mseg1 != NULL)
		{
			printk("source : %x, dev : %x, share count : %d, npages : %d, pgtable : %x\n", mseg1->mseg_sourcefile, mseg1->mseg_sourcedev, mseg1->mseg_sharecount, mseg1->mseg_npages, mseg1->mseg_pgtable);
		}
		else printk("-NULL-\n");

		mr1 = (struct memregion *)mr1->mreg_next;
	}
}

#endif

