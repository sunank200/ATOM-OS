
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

#include "mconst.h"
#include "mklib.h"
#include "errno.h"
#include "lock.h"
#include "semaphore.h"
#include "process.h"
#include "fs.h"
#include "alfunc.h"

struct lock_t semtable_lock;
struct semtable_entry semtable[MAXSEM];
volatile static int next_semno = 1;

extern volatile struct process *current_process;
extern volatile struct kthread *current_thread;
extern unsigned short this_hostid;
extern struct lock_t timerq_lock;
extern volatile struct timerobject *timers;
extern volatile unsigned long prevmillesecs, prevsecs;
extern volatile unsigned long secs, millesecs;
extern volatile unsigned short ready_qlock;
extern void insert_timerobj(struct timerobject *tobj);
extern void remove_timerobj(struct timerobject *tobj);

int semobj_init(struct semtable_entry *s_ent, int pshared, int val)
{
	s_ent->sement_slq.sl_type = SYNCH_TYPE_SEM;
	s_ent->sement_slq.sl_bitval = 0;
	s_ent->sement_slq.sl_head = NULL;
	s_ent->sement_slq.sl_tail = NULL;
	s_ent->sement_slq.sl_next = NULL;
	s_ent->sement_slq.sl_prev = NULL;
	s_ent->sement_type = (pshared == 0) ? 0 : 1; // Not shared | shared
	s_ent->sement_refcount = 1;
	s_ent->sement_destroyflag = 0;
	s_ent->sement_ownerproc = current_process;
	s_ent->sement_val = val;
	s_ent->sement_name[0] = 0; // Unnamed sem
	return 0;
}

int syscall_sem_init(sem_t *sem, int pshared, unsigned int val)
{
	struct seminternal_t *s;
	struct semtable_entry *s_ent = NULL;
	int i; //, retval;
	//char sname[32];
	unsigned long oflags;

	_lock(&semtable_lock);
	// Otherwise this is local process.
	// find one free semaphore table entry
	for (i=0; i<MAXSEM; i++)
		if (semtable[i].sement_no == 0)
		{
			s_ent = &semtable[i];
			s_ent->sement_no = next_semno++;
			break;
		}
	unlock(&semtable_lock);

	CLI;
	spinlock(current_thread->kt_synchobjlock);
	if (i >= MAXSEM) 
	{
		spinunlock(current_thread->kt_synchobjlock);
		STI;
		return -1; // return EMAXRESOURCE; // Not available.
	}
	s = (struct seminternal_t *) sem;
	// Initialize the seminternal_t structure
	s->sem_ind = i;
	s->sem_no = s_ent->sement_no;
	// s->sem_encinfo[..] = ... (Currently not used)

	// Initialize the specified semaphore structure
	semobj_init(s_ent, pshared, val);
	spinunlock(current_thread->kt_synchobjlock);
	STI;

	return 0;
}

// Destroy the semaphore, wakeup all threads sleeping on it,
int syscall_sem_destroy(sem_t *sem)
{
	struct seminternal_t *s;
	struct semtable_entry *s_ent;
	struct kthread *t1;
	// int ret;
	unsigned long oflags;

	s = (struct seminternal_t *)sem->__size;
	// Otherwise local semaphore.
	// For now only minimal checking with index range, actually
	// should check with encoded data as well.
	CLI;
	spinlock(current_thread->kt_synchobjlock);
	if (s->sem_ind < 0 || s->sem_ind > MAXSEM)
	{
		spinunlock(current_thread->kt_synchobjlock);
		STI;
		return EINVALID_ARGUMENT;
	}

	s_ent = &semtable[s->sem_ind];
	if ((s->sem_no != s_ent->sement_no) || // Slot may be under reuse
	    ((s_ent->sement_type == 0) && (s_ent->sement_ownerproc != current_process))) // This process cannot destroy
	{
		spinunlock(current_thread->kt_synchobjlock);
		STI;
		return EINVALID_ARGUMENT;
	}

	// Wakeup all sleeping threads.
	spinlock(s_ent->sement_slq.sl_bitval);
	while (s_ent->sement_slq.sl_head != NULL)
	{
		DEL_SLQ(s_ent->sement_slq, t1);
		wakeup(t1);
	}

	// Uninitialize the specified semaphore structure
	s_ent->sement_slq.sl_type = SYNCH_TYPE_SEM;
	s_ent->sement_slq.sl_head = NULL;
	s_ent->sement_slq.sl_tail = NULL;
	s_ent->sement_slq.sl_next = NULL;
	s_ent->sement_slq.sl_prev = NULL;
	s_ent->sement_type = 0;
	s_ent->sement_no = 0;
	s_ent->sement_refcount = 0;
	s_ent->sement_ownerproc = NULL;
	s_ent->sement_val = 0;
	s_ent->sement_name[0] = 0;
	s_ent->sement_destroyflag = 0;
	spinunlock(s_ent->sement_slq.sl_bitval);
	spinunlock(current_thread->kt_synchobjlock);
	STI;
	return 0;
}

sem_t * syscall_sem_open(const char *name, int oflag, mode_t mode, unsigned int value, sem_t *sem)
{
	struct seminternal_t *s;
	struct semtable_entry *s_ent;
	int i; //, retval;
	unsigned long oflags;

	_lock(&semtable_lock);
	CLI;
	spinlock(current_thread->kt_synchobjlock);
	// Check if already one exists
	for (i=0; i<MAXSEM; i++)
		if (strcmp(semtable[i].sement_name, name) == 0) break;

	if (((oflag & O_CREAT) == 0) && (i >= MAXSEM)) // Trying to open an non-existing sem
	{
		spinunlock(current_thread->kt_synchobjlock);
		STI;
		unlock(&semtable_lock);
		return NULL; // ESEMNOTFOUND;
	}
	
	// otherwise trying to create/reinitialize	
	if (i < MAXSEM) // Found
	{
		if ((oflag & O_EXCL) != 0) return NULL; // EALREADYEXISTS
		s_ent = &semtable[i];

		spinlock(s_ent->sement_slq.sl_bitval);	
		// Allocate in-process semaphore structure
		s = (struct seminternal_t *) sem;
		// Initialize the seminternal_t structure
		s->sem_ind = i;
		s->sem_no = s_ent->sement_no;
		s_ent->sement_refcount++;
		// s->sem_encinfo[..] = ... (Currently not used)
		spinunlock(s_ent->sement_slq.sl_bitval);
		spinunlock(current_thread->kt_synchobjlock);
		STI;
		unlock(&semtable_lock);
		return sem;
	}
	else
	{
		// find one free semaphore table entry
		for (i=0; i<MAXSEM; i++)
			if (semtable[i].sement_no == 0)
			{
				s_ent = &semtable[i];
				s_ent->sement_no = next_semno++;
				break;
			}

		if (i >= MAXSEM) 
		{
			spinunlock(current_thread->kt_synchobjlock);
			STI;
			unlock(&semtable_lock);
			return NULL; // return EMAXRESOURCE; // Not available.
		}

		// Allocate in-process semaphore structure
		s = (struct seminternal_t *) sem;
		// Initialize the seminternal_t structure
		s->sem_ind = i;
		s->sem_no = s_ent->sement_no;
		// s->sem_encinfo[..] = ... (Currently not used)

		// Initialize the specified semaphore structure
		semobj_init(s_ent, 1, value);
		strncpy(s_ent->sement_name, name, 99);
		spinunlock(current_thread->kt_synchobjlock);
		STI;
		unlock(&semtable_lock);
		
		return sem;
	}
}

int syscall_sem_close(sem_t *sem)
{
	struct seminternal_t *s;
	struct semtable_entry *s_ent;
	unsigned long oflags;

	// For now only minimal checking with index range, actually
	// should check with encoded data as well.
	s = (struct seminternal_t *)sem->__size;
	if (s->sem_ind < 0 || s->sem_ind > MAXSEM)
		return EINVALID_ARGUMENT;

	_lock(&semtable_lock);
	CLI;
	spinlock(current_thread->kt_synchobjlock);
	s_ent = &semtable[s->sem_ind];
	// Slot may be under reuse or Invalid type of semaphore
	if ((s->sem_no != s_ent->sement_no) || (s_ent->sement_type != 1))
	{
		spinunlock(current_thread->kt_synchobjlock);
		STI;
		unlock(&semtable_lock);
		return EINVALID_ARGUMENT;
	}

	spinlock(s_ent->sement_slq.sl_bitval);
	// Decrement reference count
	s_ent->sement_refcount--;
	if (s_ent->sement_refcount <= 0 && s_ent->sement_destroyflag == 1)
	{
		s_ent->sement_type = 0;
		s_ent->sement_ownerproc = NULL;
		s_ent->sement_val = 0;
		s_ent->sement_refcount = 0;
		s_ent->sement_name[0] = 0;
		s_ent->sement_destroyflag = 0;
		s_ent->sement_no = 0;
	}

	spinunlock(s_ent->sement_slq.sl_bitval);
	spinunlock(current_thread->kt_synchobjlock);
	STI;
	unlock(&semtable_lock);
	// Otherwise just free the memory allocated for it
	return 0;
} 

int syscall_sem_unlink(const char *name)
{
	struct semtable_entry *s_ent;
	int i;
	unsigned long oflags;

	_lock(&semtable_lock);
	CLI;
	spinlock(current_thread->kt_synchobjlock);
	// Check if one exists
	for (i=0; i<MAXSEM; i++)
		if (strcmp(semtable[i].sement_name, name) == 0) break;

	if (i < MAXSEM) // Found
	{
		s_ent = &semtable[i];
		spinlock(s_ent->sement_slq.sl_bitval);
		// mark it as destroyed and remove its name
		s_ent = &semtable[i];
		s_ent->sement_destroyflag = 1;
		s_ent->sement_name[0] = 0;

		// If no refernces then destroy here itself
		if (s_ent->sement_refcount <= 0)
		{
			s_ent->sement_type = 0;
			s_ent->sement_no = 0;
			s_ent->sement_ownerproc = NULL;
			s_ent->sement_val = 0;
			s_ent->sement_name[0] = 0;
			s_ent->sement_refcount = 0;
			s_ent->sement_destroyflag = 0;
		}
		spinunlock(s_ent->sement_slq.sl_bitval);
		spinunlock(current_thread->kt_synchobjlock);
		STI;
		unlock(&semtable_lock);
		return 0;
	}
	else	
	{
		spinunlock(current_thread->kt_synchobjlock);
		STI;
		unlock(&semtable_lock);
		return ESEMNOTFOUND;
	}
}

int syscall_sem_wait(sem_t *sem)
{
	struct seminternal_t *s;
	struct semtable_entry *s_ent;
	int retval;
	unsigned long oflags;

	s = (struct seminternal_t *)sem->__size;

	// For now only minimal checking with index range, actually
	// should check with encoded data as well.
	if (s->sem_ind < 0 || s->sem_ind > MAXSEM)
		return EINVALID_ARGUMENT;

	s_ent = &semtable[s->sem_ind];
	// Slot may be under reuse ...
	if (s->sem_no != s_ent->sement_no) 
		return EINVALID_ARGUMENT;

	CLI;
	spinlock(current_thread->kt_synchobjlock);
	spinlock(s_ent->sement_slq.sl_bitval);
	s_ent->sement_val--;
	if (s_ent->sement_val < 0)
	{
		// Add this to waiting q
		ADD_SLQ(s_ent->sement_slq, current_thread);
		current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
		current_thread->kt_slpchannel = (struct sleep_q *)s_ent;
		spinunlock(s_ent->sement_slq.sl_bitval);
		spinunlock(current_thread->kt_synchobjlock);
		scheduler();
		// Check if semaphore still exists or destroyed
		if (s->sem_no != s_ent->sement_no) retval = -1;
		else
		{
			spinlock(s_ent->sement_slq.sl_bitval);
			spinlock(current_thread->kt_synchobjlock);
			retval =  0;
			ADD_SYNCHOBJ(s_ent->sement_slq, current_thread);
			spinunlock(s_ent->sement_slq.sl_bitval);
			spinunlock(current_thread->kt_synchobjlock);
		}
		STI;
		return retval;
	}
	else retval = 0;
	ADD_SYNCHOBJ(s_ent->sement_slq, current_thread);
	spinunlock(s_ent->sement_slq.sl_bitval);
	spinunlock(current_thread->kt_synchobjlock);
	STI;
	return retval;
}

int syscall_sem_trywait(sem_t *sem)
{
	struct seminternal_t *s;
	struct semtable_entry *s_ent;
	int retval;
	unsigned long oflags;

	s = (struct seminternal_t *)sem->__size;

	// For now only minimal checking with index range, actually
	// should check with encoded data as well.
	if (s->sem_ind < 0 || s->sem_ind > MAXSEM)
		return EINVALID_ARGUMENT;

	s_ent = &semtable[s->sem_ind];
	// Slot may be under reuse ...
	if (s->sem_no != s_ent->sement_no) 
		return EINVALID_ARGUMENT;

	CLI;
	spinlock(current_thread->kt_synchobjlock);
	spinlock(s_ent->sement_slq.sl_bitval);
	s_ent->sement_val--;
	if (s_ent->sement_val < 0)
	{
		// Not possible to wait
		s_ent->sement_val++;
		retval = -1;
	}
	else
	{
		retval = 0;
		ADD_SYNCHOBJ(s_ent->sement_slq, current_thread);
	}
	spinunlock(s_ent->sement_slq.sl_bitval);
	spinunlock(current_thread->kt_synchobjlock);
	STI;
	return retval;
}

struct removethread_fromsemwait_arg {
	struct semtable_entry *s_ent;
	struct kthread *thr;
	int lock_failed;
};

int removethread_fromsemwait(void *varg)
{
	struct kthread *th1, *th2;
	struct semtable_entry *s_ent;
	struct removethread_fromsemwait_arg *arg = (struct removethread_fromsemwait_arg *) varg;
	unsigned long oflags;

	th1 = arg->thr;
	s_ent = arg->s_ent;

	CLI;
	spinlock(s_ent->sement_slq.sl_bitval);
	// Check if still the thread is in the waiting list
	th2 = (struct kthread *)s_ent->sement_slq.sl_head;
	while (th2 != NULL && th2 != th1) th2 = (struct kthread *)th2->kt_qnext;
	if (th2 != NULL) // found
	{
		if (th1->kt_qnext != NULL) (th1->kt_qnext)->kt_qprev = th1->kt_qprev;
		if (th1->kt_qprev != NULL) (th1->kt_qprev)->kt_qnext = th1->kt_qnext;
		if (th1 == s_ent->sement_slq.sl_head) s_ent->sement_slq.sl_head = th1->kt_qnext;
		if (th1 == s_ent->sement_slq.sl_tail) s_ent->sement_slq.sl_tail = th1->kt_qprev;
		th1->kt_qnext = th1->kt_qprev = NULL;
		spinunlock(s_ent->sement_slq.sl_bitval);
		arg->lock_failed = 1;
		wakeup(th1);
	}
	else spinunlock(s_ent->sement_slq.sl_bitval);
	STI;
	return 0;
}

int syscall_sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)
{
	struct seminternal_t *s;
	struct semtable_entry *s_ent;
	struct timerobject t3;
	struct removethread_fromsemwait_arg timerarg;
	unsigned long timedelaymsec;
	int retval;
	unsigned long oflags;
	struct kthread *t;

	s = (struct seminternal_t *)sem->__size;

	// For now only minimal checking with index range, actually
	// should check with encoded data as well.
	if (s->sem_ind < 0 || s->sem_ind > MAXSEM) return EINVALID_ARGUMENT;

	s_ent = &semtable[s->sem_ind];
	// Slot may be under reuse ...
	if (s->sem_no != s_ent->sement_no) return EINVALID_ARGUMENT;

	// Initialize the timer object
	timerobj_init(&t3);
	t3.timer_handler =  removethread_fromsemwait;
	t3.timer_handlerparam = (void *)&timerarg;
	t3.timer_ownerthread = (struct kthread *)current_thread;
	t3.timer_next = t3.timer_prev = NULL;

	// Wait on the corresponding lock val
	_lock(&timerq_lock);
	CLI;
	spinlock(timerq_lock.l_slq.sl_bitval);
	spinlock(current_thread->kt_synchobjlock);
	spinlock(s_ent->sement_slq.sl_bitval);
	s_ent->sement_val--;
	if (s_ent->sement_val < 0)
	{
		// Check for delaytime in mille seconds
		timedelaymsec = (abs_timeout->tv_sec - secs) * 1000 + (abs_timeout->tv_nsec / 1000000);
		if (timedelaymsec < 0)
		{
			s_ent->sement_val++;
			spinunlock(s_ent->sement_slq.sl_bitval);
			spinunlock(current_thread->kt_synchobjlock);
			spinunlock(timerq_lock.l_slq.sl_bitval);
			STI;
			unlock(&timerq_lock);
			return ETIMEDOUT;
		}
		// Also start the timer
		timerarg.thr = (struct kthread *)current_thread;
		timerarg.lock_failed = 0;
		timerarg.s_ent = s_ent;

		t3.timer_count = timedelaymsec;
		// add this object to timer q
		insert_timerobj(&t3);

		// Add this to waiting q
		ADD_SLQ(s_ent->sement_slq, current_thread);
		current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
		current_thread->kt_slpchannel = (struct sleep_q *)s_ent;

		// Unlocking of timer q
		DEL_SYNCHOBJ(timerq_lock.l_slq, current_thread);
		timerq_lock.l_owner_thread = NULL;
		while (timerq_lock.l_slq.sl_head != NULL)
		{
			DEL_SLQ(timerq_lock.l_slq, t);
			add_readyq(t);
		}
		timerq_lock.l_val = 0;
		spinunlock(timerq_lock.l_slq.sl_bitval);
		spinunlock(s_ent->sement_slq.sl_bitval);
		spinunlock(current_thread->kt_synchobjlock);

		scheduler();
		STI;
		_lock(&timerq_lock);
		if (t3.timer_count >= 0)
		{
			remove_timerobj(&t3);
			timerarg.lock_failed = 0;
		}
		unlock(&timerq_lock);
		// Check if semaphore still exists or destroyed
		if (s->sem_no != s_ent->sement_no) return -1;
		else if (timerarg.lock_failed == 1) return ETIMEDOUT;
		else
		{
			CLI;
			spinlock(current_thread->kt_synchobjlock);
			spinlock(s_ent->sement_slq.sl_bitval);
			ADD_SYNCHOBJ(s_ent->sement_slq, current_thread);
			spinunlock(s_ent->sement_slq.sl_bitval);
			spinunlock(current_thread->kt_synchobjlock);
			STI;
			return 0;
		}
	}
	else retval =  0;
	ADD_SYNCHOBJ(s_ent->sement_slq, current_thread);
	spinunlock(s_ent->sement_slq.sl_bitval);
	spinunlock(current_thread->kt_synchobjlock);
	STI;
	unlock(&timerq_lock);
	return retval;
}

int syscall_sem_post(sem_t *sem)
{
	struct seminternal_t *s;
	struct semtable_entry *s_ent;
	struct kthread *thr;
	unsigned long oflags;

	s = (struct seminternal_t *)sem->__size;

	// For now only minimal checking with index range, actually
	// should check with encoded data as well.
	s = (struct seminternal_t *)sem->__size;
	if (s->sem_ind < 0 || s->sem_ind > MAXSEM)
		return EINVALID_ARGUMENT;

	s_ent = &semtable[s->sem_ind];
	// Slot may be under reuse ...
	if (s->sem_no != s_ent->sement_no) 
		return EINVALID_ARGUMENT;

	CLI;
	spinlock(current_thread->kt_synchobjlock);
	spinlock(s_ent->sement_slq.sl_bitval);
	DEL_SYNCHOBJ(s_ent->sement_slq, current_thread);
	s_ent->sement_val++;
	if (s_ent->sement_val <= 0)
	{
		// wake up waiting thread
		DEL_SLQ(s_ent->sement_slq, thr);
		wakeup(thr);
	}

	spinunlock(s_ent->sement_slq.sl_bitval);
	spinunlock(current_thread->kt_synchobjlock);
	STI;
	return 0;
}

int syscall_sem_getvalue(sem_t *sem, int *val)
{
	struct seminternal_t *s;
	struct semtable_entry *s_ent;

	// For now only minimal checking with index range, actually
	// should check with encoded data as well.
	s = (struct seminternal_t *)sem->__size;
	if (s->sem_ind < 0 || s->sem_ind > MAXSEM)
		return EINVALID_ARGUMENT;

	s_ent = &semtable[s->sem_ind];
	// Slot may be under reuse ...
	if (s->sem_no != s_ent->sement_no) 
		return EINVALID_ARGUMENT;

	*val = s_ent->sement_val;
	return 0;
}


