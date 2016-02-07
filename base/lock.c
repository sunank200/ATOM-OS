
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


#include "lock.h"
#include "errno.h"
#include "mklib.h"
#include "process.h"
#include "timer.h"
#include "alfunc.h"

// Lock, Recursive lock, Event and Spin lock - Kernel internal synchronization
// tools.
// Spin locks are used along with each type of other synchronization objects
// for protecting the wait queues of each one of them.
// Thread will have one spin lock which protects the list of owned 
// synchronization objects of that thread. Spin lock is not stored in the
// list of synchronization objects owned. In very few cases only the spinlocks
// must be used with care. Because they are not tracked for the holder.
// The calls related to these are not used directly by the applications from
// user code.

/*
 * Using test_and_set sets the lock_val first bit. 
 * If that is already 1 and if type = LOCK_BLOCKING then the calling process 
 * is blocked and added to list of waiting processes.
 * Returns a 0 on successful locking otherwise a -1.
 */
extern volatile struct kthread * current_thread;
extern struct lock_t timerq_lock;
extern volatile struct timerobject *timers;
extern volatile unsigned long prevmillesecs, prevsecs;
extern volatile unsigned int secs, millesecs;
extern volatile unsigned short ready_qlock;

extern void insert_timerobj(struct timerobject *tobj);
extern void remove_timerobj(struct timerobject *tobj);

void lockobj_init(struct lock_t *l)
{
	bzero((char *)l, sizeof(struct lock_t));
	l->l_slq.sl_type = SYNCH_TYPE_LOCK;
}
void rlockobj_init(struct rlock_t *rl)
{
	bzero((char *)rl, sizeof(struct rlock_t));
	rl->rl_slq.sl_type = SYNCH_TYPE_RLOCK;
}
void eventobj_init(struct event_t *e)
{
	bzero((char *)e, sizeof(struct event_t));
	e->e_slq.sl_type = SYNCH_TYPE_EVENT;
}

void lend_priority(struct genlockobj *gl)
{
	struct kthread *thr;
	int __pri__, ret;

	do 
	{
		if (gl->sl_type != SYNCH_TYPE_LOCK && gl->sl_type != SYNCH_TYPE_RLOCK)  // Do nothing
			return;
		thr = (struct kthread *)gl->owner_thread;
		__pri__ = (current_thread->kt_schedinf.sc_cpupri > current_thread->kt_schedinf.sc_inhpri) ?  current_thread->kt_schedinf.sc_cpupri : current_thread->kt_schedinf.sc_inhpri;
		if (thr->kt_schedinf.sc_cpupri < __pri__ && thr->kt_schedinf.sc_inhpri < __pri__)
		{
			ret = del_readyq(thr);	// Was that on ready queue.
			thr->kt_schedinf.sc_inhpri = __pri__;
			if (ret == 1) add_readyq(thr);

			if (thr->kt_schedinf.sc_state == THREAD_STATE_SLEEP) // Transitively inherit the priority
				gl = (struct genlockobj *)thr->kt_slpchannel;
			else	gl = NULL;
		}
		else gl = NULL;
	} while (gl != NULL);
}
// Blocked locking.
int _lock(struct lock_t *l)
{
	unsigned long oflags;

	CLI;
_lock_1:
	spinlock(current_thread->kt_synchobjlock); 
	if (l->l_owner_thread != NULL && l->l_owner_thread == current_thread)
		printk("Thread : [%s] trying to lock again : %x\n",current_thread->kt_name, l);
	// This thread synchlist lock 
	spinlock(l->l_slq.sl_bitval);
	if (test_and_set(&(l->l_val)) == 1)
	{
		// Block this thread
		// Add it to the waiting q of lock	
		ADD_SLQ(l->l_slq, current_thread);
		// Set this lock as the sleep channel for this thread
		// The thread should not be interrupted until it schedules 
		// another thread.
		current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
		current_thread->kt_slpchannel = (struct sleep_q *)l;
		spinunlock(l->l_slq.sl_bitval);	
		spinunlock(current_thread->kt_synchobjlock); 
		lend_priority((struct genlockobj *)l);
		scheduler();

		// Retry to lock it when this resumes
		goto _lock_1;
	}
	else 
	{
		l->l_owner_thread = (struct thread *)current_thread; /* Owner of lock */
		ADD_SYNCHOBJ(l->l_slq, current_thread);
		current_thread->kt_slpchannel = NULL;
		spinunlock(l->l_slq.sl_bitval);	
		spinunlock(current_thread->kt_synchobjlock); 
		STI;
		return ELOCK_SUCCESS;
	}
}

// Try locking, return success/failure
int trylock(struct lock_t *l)
{
	unsigned long oflags;
	int ret;

	CLI;
	// This thread synchlist lock 
	spinlock(current_thread->kt_synchobjlock); 
	spinlock(l->l_slq.sl_bitval);	
	if (test_and_set(&(l->l_val)) == 1)
	{
		ret = ELOCK_FAILURE;
		//Failure
	}
	else 
	{
		l->l_owner_thread = (struct thread *)current_thread; /* Owner of lock */
		ADD_SYNCHOBJ(l->l_slq, current_thread);
		current_thread->kt_slpchannel = NULL;
		ret = ELOCK_SUCCESS;
	}
	spinunlock(current_thread->kt_synchobjlock); 
	spinunlock(l->l_slq.sl_bitval);	
	STI;
	return ret;
}

// Just wakeup the waiting threads
int sys_unlock(struct lock_t *l)
{
	struct kthread *t;
	int count = 0;

	while (l->l_slq.sl_head != NULL)
	{
		count++;
		DEL_SLQ(l->l_slq, t);
		add_readyq(t);
	}
	DEL_SYNCHOBJ(l->l_slq, current_thread);
	l->l_owner_thread = NULL;
	l->l_val = 0;
	// May not be correct when there are no more synch objects owned
	// set inherited priority to 0.
	if (current_thread->kt_synchobjlist == NULL)
		current_thread->kt_schedinf.sc_inhpri = 0;
	return count;
}
/*
 * Wake up a process that is at the end of waiting queue if any
 * processes are waiting.
 */
int unlock(struct lock_t * l)
{
	struct kthread *t = NULL;
	int ret = ELOCK_SUCCESS;
	unsigned long oflags;

	CLI;
	spinlock(current_thread->kt_synchobjlock); // This thread termination is unsafe
	spinlock(l->l_slq.sl_bitval);	
	if (l->l_owner_thread == current_thread)
	{
		while (l->l_slq.sl_head != NULL)
		{
			DEL_SLQ(l->l_slq, t);
			add_readyq(t);
		}
		DEL_SYNCHOBJ(l->l_slq, current_thread);
		l->l_owner_thread = NULL;
		l->l_val = 0;
	}
	else 
	{
		printk("[%s]-%x, trying to unlock without owning\n", current_thread->kt_name, l);
		ret = ELOCK_NOTOWNED;
	}

	spinunlock(l->l_slq.sl_bitval);	
	spinunlock(current_thread->kt_synchobjlock);
	STI;
	if (current_thread->kt_synchobjlist == NULL)
		current_thread->kt_schedinf.sc_inhpri = 0;
	return ret;
}

// Recursive lock operation
int rlock(struct rlock_t *l)
{
	unsigned long oflags;

	CLI;
_lock_1:
	spinlock(current_thread->kt_synchobjlock); 
	// This thread synchlist lock 
	spinlock(l->rl_slq.sl_bitval);	
	if (test_and_set(&(l->rl_val)) == 1)
	{
		// Check if this is the owner
		if (l->rl_owner_thread == current_thread)
		{
			l->rl_level++;
			spinunlock(l->rl_slq.sl_bitval);	
			spinunlock(current_thread->kt_synchobjlock); 
			STI;
			return ELOCK_SUCCESS;
		}
		// Block this thread
		// Add it to the waiting q of lock	
		ADD_SLQ(l->rl_slq, current_thread);
		// Set this lock as the sleep channel for this thread
		// The thread should not be interrupted until it schedules 
		// another thread.
		current_thread->kt_slpchannel = (struct sleep_q *)l;
		current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
		spinunlock(l->rl_slq.sl_bitval);	
		spinunlock(current_thread->kt_synchobjlock); 
		lend_priority((struct genlockobj *)l);
		scheduler();

		// Retry to lock it when this resumes
		goto _lock_1;
	}
	else 
	{
		l->rl_owner_thread = (struct thread *)current_thread; /* Owner of lock */
		l->rl_level++;
		ADD_SYNCHOBJ(l->rl_slq, current_thread);
		spinunlock(l->rl_slq.sl_bitval);	
		spinunlock(current_thread->kt_synchobjlock); 
		current_thread->kt_slpchannel = NULL;
		STI;
		return ELOCK_SUCCESS;
	}
}

/*
 * Unlocking a recursive lock
 *
 */
int runlock(struct rlock_t * l)
{
	unsigned long oflags;
	struct kthread *t;
	int ret = ELOCK_SUCCESS;

	CLI;
	// This thread termination is unsafe
	spinlock(current_thread->kt_synchobjlock); // This thread termination is unsafe
	spinlock(l->rl_slq.sl_bitval);	
	if (l->rl_owner_thread == current_thread)
	{
		l->rl_level--;
		// Unlocked equal number of locking times?
		if (l->rl_level <= 0)
		{
			// Give up th ownership and wakeup others
			while (l->rl_slq.sl_head != NULL)
			{
				DEL_SLQ(l->rl_slq, t);
				add_readyq(t);
			}
			DEL_SYNCHOBJ(l->rl_slq, current_thread);
			l->rl_owner_thread = NULL;
			l->rl_val = 0;
		}
	}
	else 
	{
		printk("[%s ] Unlocking without owning the lock : %x\n", current_thread->kt_name, l);
		ret = ELOCK_NOTOWNED;
	}

	spinunlock(l->rl_slq.sl_bitval);	
	spinunlock(current_thread->kt_synchobjlock); 

	if (current_thread->kt_synchobjlist == NULL)
		current_thread->kt_schedinf.sc_inhpri = 0;
	STI;

	return ret;
}

// Try lock is not given for recursive locks, not required at present....

int sys_event_wakeup(struct event_t *e)
{
	struct kthread *t;
	int count = 0;

	while (e->e_slq.sl_head != NULL)
	{
		DEL_SLQ(e->e_slq, t);
		add_readyq(t);
		t->kt_slpchannel = NULL;
		count++;
	}
	e->e_val = 0;

	return count;
}

/*
 * Wakes up all the processes waiting for event to happen.
 * Returns the number of processes waken up.
 */
int event_wakeup(struct event_t *e)
{
	struct kthread *t;
	int count=0;
	unsigned long oflags;

	CLI;
	// This thread termination is unsafe
	spinlock(current_thread->kt_synchobjlock); 
	spinlock(e->e_slq.sl_bitval);
	while (e->e_slq.sl_head != NULL)
	{
		DEL_SLQ(e->e_slq, t);
		add_readyq(t);
		t->kt_slpchannel = NULL;
		count++;
	}
	e->e_val = 0;

	spinunlock(e->e_slq.sl_bitval);
	spinunlock(current_thread->kt_synchobjlock); 
	STI;
	return count;
}

/*
 * Adds the current process to event waiting q and schedules a
 * new process.
 */
int event_sleep(struct event_t *e, struct lock_t *lck)
{
	int ret=0;
	unsigned long oflags;
	struct kthread *t;

	CLI;
	spinlock(current_thread->kt_synchobjlock); 
	spinlock(e->e_slq.sl_bitval);

	if (lck != NULL)
	{
		if (lck->l_owner_thread == current_thread)
		{
			spinlock(lck->l_slq.sl_bitval);	
		}
		else
		{
			spinunlock(e->e_slq.sl_bitval);
			spinunlock(current_thread->kt_synchobjlock); 
			printk("[%s]-%x, event sleep trying to unlock without owning\n", current_thread->kt_name, lck);
			STI;
			return ELOCK_NOTOWNED;
		}
	}

	// Context switching should not happen.
	if (lck != NULL)
	{ 
		// Unlock it
		while (lck->l_slq.sl_head != NULL)
		{
			DEL_SLQ(lck->l_slq, t);
			t->kt_slpchannel = NULL;
			add_readyq(t);
		}
		DEL_SYNCHOBJ(lck->l_slq, current_thread);
		lck->l_owner_thread = NULL;
		lck->l_val = 0;
		spinunlock(lck->l_slq.sl_bitval);	
	}

	// Add this to waiting queue
	ADD_SLQ(e->e_slq, current_thread);
	e->e_val++;
	current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
	current_thread->kt_slpchannel = (struct sleep_q *)e;

	spinunlock(e->e_slq.sl_bitval);
	spinunlock(current_thread->kt_synchobjlock); 

	scheduler();
	STI;

	if (lck != NULL) ret = _lock(lck);				

	return ret;
}

struct eventtimedsleep_arg {
	struct event_t *e;
	struct kthread *thr;
	int efailed;
};

int event_wait_remove_thread(void *arg)
{
	struct eventtimedsleep_arg *timerarg = (struct eventtimedsleep_arg *) arg;;
	struct kthread *t, *t1;
	struct event_t *e;
	unsigned long oflags;

	t = timerarg->thr;
	e = timerarg->e;

	CLI;
	spinlock(e->e_slq.sl_bitval);
	// First check for whether still the thread is in 
	// waiting queue

	t1 = (struct kthread *)e->e_slq.sl_head;
	while (t1 != NULL && t1 != t)
		t1 = (struct kthread *)t1->kt_qnext;
	if (t1 != NULL) // found, delete it
	{
		if (t->kt_qnext != NULL) (t->kt_qnext)->kt_qprev = t->kt_qprev;
		else e->e_slq.sl_tail = t->kt_qprev;
		if (t->kt_qprev != NULL) (t->kt_qprev)->kt_qnext = t->kt_qnext;
		else e->e_slq.sl_head = t->kt_qnext;
		e->e_val--;
		timerarg->efailed = 1;
		spinunlock(e->e_slq.sl_bitval);
		t->kt_slpchannel = NULL;
		add_readyq(t);
	}
	else 
	{
		spinunlock(e->e_slq.sl_bitval);
	}
	STI;

	return 0;
}

int event_timed_sleep(struct event_t *e, struct lock_t *lck, unsigned int msecs)
{
	int ret=0;
	unsigned long oflags;
	struct eventtimedsleep_arg timerarg;
	struct timerobject t3;
	struct kthread * t;

	timerarg.thr = (struct kthread *)current_thread;
	timerarg.e = e;
	timerarg.efailed = 0;

	timerobj_init(&t3);	

	// Initialize the timer object
	t3.timer_count = msecs;
	t3.timer_handler =  event_wait_remove_thread;
	t3.timer_handlerparam = &timerarg;
	t3.timer_ownerthread = (struct kthread *)current_thread;
	t3.timer_next = t3.timer_prev = NULL;

	_lock(&timerq_lock);
	CLI;
	spinlock(timerq_lock.l_slq.sl_bitval);	
	spinlock(current_thread->kt_synchobjlock); 
	spinlock(e->e_slq.sl_bitval);

	if (lck != NULL)
	{
		if (lck->l_owner_thread == current_thread)
		{
			spinlock(lck->l_slq.sl_bitval);	
		}
		else
		{
			spinunlock(e->e_slq.sl_bitval);
			spinunlock(current_thread->kt_synchobjlock); 
			spinunlock(timerq_lock.l_slq.sl_bitval);	
			unlock(&timerq_lock);
			STI;
			return ELOCK_NOTOWNED;
		}
	}

	// add this object to timer q
	insert_timerobj(&t3);

	// Context switching should not happen.
	// Acquire timers slq spinlock
		
	if (lck != NULL)
	{ 
		// Unlock it
		while (lck->l_slq.sl_head != NULL)
		{
			DEL_SLQ(lck->l_slq, t);
			add_readyq(t);
		}
		DEL_SYNCHOBJ(lck->l_slq, current_thread);
		lck->l_owner_thread = NULL;
		lck->l_val = 0;
	}

	// Add this to waiting queue
	ADD_SLQ(e->e_slq, current_thread);
	e->e_val++;
	current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
	current_thread->kt_slpchannel = (struct sleep_q *)e;
	
	// Modify this unlock
	while (timerq_lock.l_slq.sl_head != NULL)
	{
		DEL_SLQ(timerq_lock.l_slq, t);
		add_readyq(t);
	}
	DEL_SYNCHOBJ(timerq_lock.l_slq, current_thread);
	timerq_lock.l_owner_thread = NULL;
	timerq_lock.l_val = 0;
	spinunlock(timerq_lock.l_slq.sl_bitval);	

	if (lck != NULL) 
	{
		spinunlock(lck->l_slq.sl_bitval);	
	}
	spinunlock(e->e_slq.sl_bitval);
	spinunlock(current_thread->kt_synchobjlock);
	scheduler();

	// If still timer is running then stop it.
	STI;
	_lock(&timerq_lock);
	if (t3.timer_count >= 0)
	{
		remove_timerobj(&t3); // May be found or not?
		timerarg.efailed = 0;
	}
	unlock(&timerq_lock);

	if (lck != NULL)
		ret = _lock(lck);				

	if (timerarg.efailed == 0) return 0; // Successfully waiting completed
	else return ETIMEDOUT;	// Failure
}

void releasesynch_object(struct kthread *t, struct sleep_q *synchobj)
{
	struct lock_t *l;
	struct rlock_t *r;
	struct event_t *e;
	struct sem_t *s;
	//int __pri__;
	unsigned long oflags;

	switch (synchobj->sl_type)
	{
	case SYNCH_TYPE_LOCK:
		l = (struct lock_t *)synchobj;
		CLI;
		spinlock(l->l_slq.sl_bitval);
		sys_unlock(l);
		spinunlock(l->l_slq.sl_bitval);
		STI;
		break;

	case SYNCH_TYPE_RLOCK:
		r = (struct rlock_t *)synchobj;
		break;
	case SYNCH_TYPE_EVENT:
		e = (struct event_t *)synchobj;
		break;
	case SYNCH_TYPE_SEM:
		s = (struct sem_t *)synchobj;
		break;
	}
	return;
}

#ifdef DEBUG
void print_lock(struct lock_t *lck)
{
	printk("lck [%x] --> owner : %s, lval : %hd, sl_head : %x, sl_tail : %x ", lck, ((lck->l_owner_thread == NULL) ? "NULL" : ((struct kthread *)lck->l_owner_thread)->kt_name), lck->l_val, lck->l_slq.sl_head, lck->l_slq.sl_tail);
}
#endif

