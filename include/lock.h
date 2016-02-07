
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

#ifndef __LOCK
#define __LOCK

#include "mtype.h"
#include "mklib.h"

#define SYNCH_TYPE_LOCK		0
#define SYNCH_TYPE_RLOCK	1
#define SYNCH_TYPE_EVENT	2
#define SYNCH_TYPE_SEM		3

#define STI	restore_eflags(oflags) 
#define CLI	oflags = get_eflags(); cli_intr(); 

struct genlockobj {
	unsigned short int sl_type;		// Type of sleep queue object
	volatile unsigned short sl_bitval;
	volatile struct kthread * sl_head;	// Waiting thread's head and tail
	volatile struct kthread * sl_tail;
	volatile struct sleep_q * sl_next;	// To maintain current owned list 
	volatile struct sleep_q * sl_prev;
	volatile struct kthread * owner_thread; /* Lock owning process */
};

struct sleep_q {
	unsigned short int sl_type;		// Type of sleep queue object
	volatile unsigned short sl_bitval;
	volatile struct kthread * sl_head;	// Waiting thread's head and tail
	volatile struct kthread * sl_tail;
	volatile struct sleep_q * sl_next;	// To maintain current owned list 
	volatile struct sleep_q * sl_prev;
};

/* IMPORTANT : Always whereever wait qs can be formed the head and tail 
 * pointers must be the first two in that order. Immediately followed 
 * by the spinlock. This property is used while operating with the sleep queues.
 */
struct lock_t {
	struct sleep_q l_slq;
	volatile void *l_owner_thread; /* Lock owning process */
	volatile unsigned short int l_val; /* Value that is used for lock yes/no */
};

#define LOCK_INITIALIZER	{{SYNCH_TYPE_LOCK, 0, NULL, NULL, NULL, NULL}, NULL, 0 }
struct rlock_t {
	struct sleep_q rl_slq;
	volatile void *rl_owner_thread; /* Lock owning process */
	volatile unsigned short int rl_val; /* Value that is used for lock yes/no */
	volatile unsigned short int rl_level;
};
#define LOCK_BLOCKING		0
#define LOCK_NONBLOCKING	1


/* Event structure	*/
struct event_t {
	struct sleep_q e_slq;
	volatile unsigned short int e_val;
};

// During the spin lock holding period, there should not be any context switch. 
// Other wise the advantage of spinlock - with busy wait is violated. 
// So before using spinlock interrupts must be disabled. After spinunlock 
// again interrupts can be enabled. This is only to avoid the context switch 
// on the spinlock holding processor

#ifdef SMP
#define spintrylock(l, status)	{ \
		if (test_and_set(&l) != 0) \
		{ \
			status = -1; \
		} \
		else status = 0; \
	}

#define spinlock(l) { \
		while (test_and_set(&l) != 0) \
			while (l != 0) ; \
	}

#define spinunlock(l) { l = 0; }

#else

#define spintrylock(l, status)	{ l = 1; status = 0; }
#define spinlock(l) 		{ l = 1; }
#define spinunlock(l) 		{ l = 0; }

#endif

// Add a thread at the end of the sleep q
#define ADD_SLQ(slq, t) { 	t->kt_qnext = NULL; t->kt_qprev = slq.sl_tail; \
				if (slq.sl_tail != NULL) {\
					slq.sl_tail->kt_qnext = (struct kthread *)t; \
					slq.sl_tail = t; \
				} \
				else \
					slq.sl_head = slq.sl_tail = (struct kthread *)t; \
			}

// Remove a thread from the head of the sleep q
#define DEL_SLQ(slq, t) { 	if (slq.sl_head == NULL) t = NULL; \
				else \
				{ \
					t = (struct kthread *)slq.sl_head; \
					slq.sl_head = t->kt_qnext; \
					if (slq.sl_head == NULL) \
						slq.sl_tail = NULL; \
					else	slq.sl_head->kt_qprev = NULL; \
					t->kt_qnext = t->kt_qprev = NULL; \
				} \
			}

#define ADD_SYNCHOBJ(slq, t)	{	slq.sl_next = t->kt_synchobjlist; \
					slq.sl_prev = NULL; \
					if (t->kt_synchobjlist != NULL) \
						t->kt_synchobjlist->sl_prev = &slq; \
					t->kt_synchobjlist = &slq; \
				}

#define DEL_SYNCHOBJ(slq, t)	{ \
				if (slq.sl_next != NULL) \
					(slq.sl_next)->sl_prev = slq.sl_prev; \
				if (slq.sl_prev != NULL) \
					(slq.sl_prev)->sl_next = slq.sl_next; \
				else \
					t->kt_synchobjlist = slq.sl_next; \
				slq.sl_next = slq.sl_prev = NULL; \
			} 

/* Lock and event related calls */
void lockobj_init(struct lock_t *l);
int _lock(struct lock_t *l);
int trylock(struct lock_t *l);
int unlock(struct lock_t * l);
void rlockobj_init(struct rlock_t *l);
int rlock(struct rlock_t *l);
int runlock(struct rlock_t *l);

void eventobj_init(struct event_t *e);
int event_wakeup(struct event_t *e);
int event_sleep(struct event_t *e, struct lock_t *l);
int event_timed_sleep(struct event_t *e, struct lock_t *l, unsigned int msec);

void releasesynch_object(struct kthread *t, struct sleep_q *synchobj);
#endif
