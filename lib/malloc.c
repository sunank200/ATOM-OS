#include "malloc.h"
#define NULL	0
#define MALLOC_MAX	0x2000000	/* 16 MB */

typedef struct pthread_mutex_t 
{
	char _opaque[36];
} pthread_mutex_t;

typedef struct pthread_mutex_attr_t {
	int type;
} pthread_mutex_attr_t;
void uprint_list(struct mem_list *l);
void uinsert_avail_node(struct mem_list *p);
void udelete_avail_node(struct mem_list *p);
struct mem_list *ucreate_mem_list_node(void);
void *malloc(unsigned int size);
void free(void *ptr);
int printf(char *fmt, ...);
void bzero(char *ptr, int n);
void bcopy(char *src, char *dest, int n);
int brk_internal(void *endaddr);

struct memory_info umem_info = {NULL, NULL, NULL};
unsigned int enddataaddr=0;
pthread_mutex_t __malloc_mutex = {{0, 0, 0, 0, 0, 0, 0, 0, 
				   0, 0, 0, 0, 0, 0, 0, 0, 
				   0, 0, 0, 0, 0, 0, 0, 0, 
				   0, 0, 0, 0, 0, 0, 0, 0, 
				   0, 0, 0, 0}};
pthread_mutex_t __brk_mutex =    {{0, 0, 0, 0, 0, 0, 0, 0, 
				   0, 0, 0, 0, 0, 0, 0, 0, 
				   0, 0, 0, 0, 0, 0, 0, 0, 
				   0, 0, 0, 0, 0, 0, 0, 0,
				   0, 0, 0, 0}};
extern int pthread_mutex_init(pthread_mutex_t *mut, pthread_mutex_attr_t *attr);
extern int pthread_mutex_lock(pthread_mutex_t *mut);
extern int pthread_mutex_unlock(pthread_mutex_t *mut);

void ucheck_release_core(void)
{
	struct mem_list *m1, *m2 = NULL;

	// Releasing of the memory can be done only at the end.
	// For kernel memory management releasing any block of memory is valid
	// But for user process only at the end can be done.
	m1 = (struct mem_list *)umem_info.avail_list;

	// Find the last node	
	while (m1->next != NULL)
	{
		m2 = m1;
		m1 = (struct mem_list *)m1->next;
	}
	
	// If the block is more than one page and at the end of the segment
	// then release it.
	if ((m1->size >= 0x1000) && ((((unsigned int)m1->addr) + m1->size) == (enddataaddr + 1)))
	{
		// release memory, by shrinking
		brk((void * )(m1->addr - 1));
		enddataaddr = (unsigned int) m1->addr -1;
		m1->size = 0;
		if (umem_info.avail_list == m1)
			umem_info.avail_list = NULL;

	}
}
void uinsert_avail_node(struct mem_list *p)
{
	struct mem_list *q1, *q2 = NULL;

	// Add p to avail_list
	p->next = NULL;
	if (umem_info.avail_list == NULL)
	{
		umem_info.avail_list = p; return;
	}
	else
	{
		// Insertion in the beginning?
		if (umem_info.avail_list->addr > p->addr)
		{
			if (p->addr + p->size == umem_info.avail_list->addr)
			{
				umem_info.avail_list->addr = p->addr;
				umem_info.avail_list->size += p->size;
				// Free p.
				p->size = 0;
			}
			else
			{
				p->next = umem_info.avail_list;
				umem_info.avail_list = p;
			}
			return;
		}

		// Insertion in the middle or ...
		q1 = (struct mem_list *)umem_info.avail_list;
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

				// Look for next node
				q2 = (struct mem_list *)q1->next;
				if ((q2 != NULL)  && (q1->addr+q1->size == q2->addr))
				{
					q1->size += q2->size;
					q2->size = 0;
					q1->next = q2->next;
				}
			}
			else if (p->addr + p->size == q1->addr)
			{
				q1->addr = p->addr;
				q1->size += p->size;
				p->size = 0;
			}
			else
			{
				p->next = q1;
				q2->next = p;
			}
		}
		else // At the end
		{
			q2->next = p;
		}
		
		return;
	}
}

void udelete_avail_node(struct mem_list *p)
{
	struct mem_list *q2;

	p->size = 0;
	if (umem_info.avail_list == p)
	{
		umem_info.avail_list = umem_info.avail_list->next;
		p->next = NULL;
		return;
	}

	q2 = (struct mem_list *)umem_info.avail_list;

	while (q2 != NULL && q2->next != p)
		q2 = (struct mem_list *)q2->next;

	if (q2 != NULL) 
		q2->next = p->next; // p is removed
	p->next = NULL;
	return;
}

struct mem_list *ucreate_mem_list_node(void)
{
	struct mem_list *p1;
	char *ptr1, *ptr2=NULL;
	int i;

	// If no meminfo block allocated
	if (umem_info.malloc_info == NULL)
	{
		enddataaddr = brk(NULL); // Not yet initialized
		// Increase the size by one page
		if (brk((void *)(enddataaddr + 4096)) < 0) // Successfully expanded?
			return NULL;
		umem_info.malloc_info = (char *)(enddataaddr+1);
		enddataaddr += 4096;

		bzero((char *)umem_info.malloc_info,4096); // Including last pointer NULL
	}

	// Search for freenode in that block
	ptr1 = (char *)umem_info.malloc_info;
	p1 = (struct mem_list *)ptr1;

	while (p1 != NULL)
	{
		for (i=0; i<((4096-4)/sizeof(struct mem_list));  i++)
		{
			if (p1->size == 0) return p1;
			p1 = p1 + 1;
		}
		ptr2 = ptr1;
		bcopy(ptr1+4092,(char *)&ptr1,4); // Pointer to next block
		p1 = (struct mem_list *)ptr1;
	}

	// No free node found ... allocate new.
	if (brk((void *)(enddataaddr + 4096)) < 0) // Successfully expanded?
		return NULL;
	ptr1 = (char *)(enddataaddr+1);
	enddataaddr += 4096;

	bcopy((char *)&ptr1, ptr2+4092,4);
	bzero((char *)ptr1, 4096);
	p1 = (struct mem_list *)ptr1;

	return p1;
}

#ifdef DEBUG
void uprint_list(struct mem_list *l)
{
	struct mem_list *p;

	p = l;
	while (p != NULL)
	{
		printf("node = %x addr = %x size = %d\n",p,p->addr,p->size);
		p = (struct mem_list *)p->next;
	}
}

void uprint_minf(void)
{
	printf("minf= %x available :\n",umem_info.avail_list);
	uprint_list((struct mem_list *)umem_info.avail_list);
	printf("used :\n");
	uprint_list((struct mem_list *)umem_info.used_list);
}

void uprint_minf_statistics(volatile struct memory_info *minf)
{
	struct mem_list *p;
	int count, size;

	count = 0; size = 0;
	p = (struct mem_list *)umem_info.avail_list;
	while (p != NULL)
	{
		count++;
		size += p->size;
		p = (struct mem_list *)p->next;
	}
	printf("Available mem info : count of blocks = %d total size = %d\n", count, size);

	count = 0; size = 0;
	p = (struct mem_list *)umem_info.used_list;
	while (p != NULL)
	{
		count++;
		size += p->size;
		p = (struct mem_list *)p->next;
	}
	printf("Used mem info : count of blocks = %d total size = %d\n", count, size);
}
#endif

void *malloc(unsigned int size)
{
	struct mem_list *p1 = NULL, *p2 = NULL, *p3 = NULL;
	char avl = 0;
	int hole = 0xefffffff; // INT_MAX
	unsigned int mem;
	int i;

	//printf("Malloc : %d\n",size);
	if (size > MALLOC_MAX) // Greater than maximum allocatable.
	{
		printf("MALLOC ERROR >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		return NULL;
	}

	//printf("malloc called\n");
	i = pthread_mutex_lock(&__malloc_mutex);
	//printf("malloc called-2\n");
	// Search the available list for bestfit
malloc_1:
	if (umem_info.avail_list != NULL)
	{
		p1 = (struct mem_list *)umem_info.avail_list;

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

	//if (p3 != NULL) ;
	//else ;

	p1 = p3; // If available

	// If not available allocate core.
	if (avl == 0)
	{
		//printf("malloc before brk \n");
		if (enddataaddr == 0)
			enddataaddr = brk(NULL);

		//printf("malloc 2\n");
		if (brk((void *)(enddataaddr + size)) < 0)
		{
			pthread_mutex_unlock(&__malloc_mutex);
			printf("MALLOC ERROR >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
			return NULL;
		}
		//printf("malloc 3 : %x\n",enddataaddr);
		mem = enddataaddr + 1;
		enddataaddr += size;
	
		// add a mem_list node to the avail_list
		p1 = ucreate_mem_list_node();

		//printf("malloc 31 : %x\n",p1);
		if (p1 == NULL)
		{
			brk((void *)(mem-1));
			enddataaddr = mem -1;
			pthread_mutex_unlock(&__malloc_mutex);
			printf("MALLOC ERROR >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
			return NULL;
		}
		//printf("malloc 4 : %x\n", enddataaddr);

		p1->addr = (char *)mem;
		p1->size = size;
		p1->next = NULL;
		uinsert_avail_node(p1);
		goto malloc_1;
	}

	// Allocate requested size of memory from p1
		//printf("malloc 41 : %x\n", p1);
	if (p1 != NULL && p1->size > size) 
	{
		// Used list node
		p3 = ucreate_mem_list_node();
		if (p3 == NULL)
		{
			pthread_mutex_unlock(&__malloc_mutex);
			printf("MALLOC ERROR >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
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
		udelete_avail_node(p1);
		// Size must be set back to original size (because set to 0)
		p3->size = size;
	}

	//printf("malloc 5 : %x\n", p3);
	// Insert allocated information into used list
	p3->next = umem_info.used_list;
	umem_info.used_list = p3;

	// For safety initialize to zeroes
	// bzero(p3->addr, size);
	
	//printf("malloc called-3\n");
	i = pthread_mutex_unlock(&__malloc_mutex);
	//printf("malloc called-4\n");
	//printf("malloc 6 : %d\n", i);
	return ((void *)p3->addr);
}

void free(void *ptr)
{
	struct mem_list *p1, *p2 = NULL;
	
	if (ptr == NULL) return;

	// Remove the corresponding node from used list
	pthread_mutex_lock(&__malloc_mutex);
	p1 = (struct mem_list *)umem_info.used_list;
	while (p1 != NULL)
	{
		if (p1->addr == ptr)
		{
			if (p1 == umem_info.used_list)
				umem_info.used_list = p1->next;
			else
				p2->next = p1->next;
			p1->next = NULL;

			uinsert_avail_node(p1);

			pthread_mutex_unlock(&__malloc_mutex);
			return;
		}

		p2 = p1;
		p1 = (struct mem_list *)p1->next;
	}

	pthread_mutex_unlock(&__malloc_mutex);
	printf("failure in memory freeing : %x\n",ptr);
	return;
}

int brk(void *enddataaddr)
{
	int retval;

	pthread_mutex_lock(&__brk_mutex);
	retval = brk_internal(enddataaddr);
	pthread_mutex_unlock(&__brk_mutex);
	return retval;
}

int primalloc_internal(unsigned int size);
void * primalloc(unsigned int size)
{
	int retval;

	pthread_mutex_lock(&__brk_mutex);
	retval = primalloc_internal(size);
	pthread_mutex_unlock(&__brk_mutex);
	return (void *)retval;
}
int prifree_internal(void *addr, unsigned int size);
int prifree(void *addr, unsigned int size)
{
	int retval;

	pthread_mutex_lock(&__brk_mutex);
	retval = prifree_internal(addr, size);
	pthread_mutex_unlock(&__brk_mutex);
	return retval;
}

void *calloc(unsigned int nmemb, unsigned int size)
{
	unsigned int totsize = nmemb * size;
	char *ptr;

	ptr = malloc(totsize);
	if (ptr != NULL) bzero(ptr, totsize);
	return ptr;
}

void *realloc(void *ptr, unsigned int size)
{
	char *ptr1;
	unsigned int osize = 0;
	struct mem_list *p1; //, *p2 = NULL;
	
	if (ptr != NULL) 
	{
		// Find the ptr block size 
		pthread_mutex_lock(&__malloc_mutex);
		p1 = (struct mem_list *)umem_info.used_list;
		while (p1 != NULL)
		{
			if (p1->addr == ptr)
			{
				osize = p1->size;
				break;
			}
			else p1 = (struct mem_list *)p1->next;
		}
		pthread_mutex_unlock(&__malloc_mutex);
	}
		
	if (osize < size)
	{
		
		ptr1 = malloc(size);
		if (osize > 0)
		{
			bcopy(ptr, ptr1, osize);
			free (ptr);
		}
	}
	else	ptr1 = ptr;	// No reduction in size
	
	return ptr1;
}

