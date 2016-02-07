
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

#include "mklib.h"
#include "errno.h"
#include "lock.h"
#include "process.h"
#include "pthread.h"
#include "timer.h"
#include "syscalls.h"
#include "alfunc.h"

extern unsigned int temp_proc_map[];
extern volatile struct process *current_process;
extern volatile struct kthread *current_thread;
extern unsigned short int this_hostid;
extern volatile unsigned short ready_qlock;
extern void destroy_thread(struct kthread *thr);
extern struct rthread * alloc_rthread_slot(void);
extern struct lock_t socklist_lock;
int syscall_pthread_create(pthread_t *threadp, const pthread_attr_t *attr, void *(* start_routine)(void *), void * arg)
{
	// initially simple implementation without considering attributes

	int tslot; 
	int pno, thrno; //, retval;
	unsigned int stack_start;
	PDE *pdentry;
	
	if (threadp == NULL) return EARGS;

	pno = current_process->proc_id;
	// Allocate stack region
			
	_lock((struct lock_t *)&current_process->proc_thrlock);	
	for (stack_start = ((USERSTACK_SEGMENT_END >> 12) & 0x000fffff)-1; stack_start > 0x60000; stack_start = stack_start - 0x400)
	{
		pdentry = vm_getpdentry(current_process->proc_vm, stack_start);
		if (pdentry->page_table_base_addr == 0) // Unused 4 MB region
			break;
	}

	if (stack_start <= 0x60000)
	{
		*threadp = -1;
		unlock((struct lock_t *)&current_process->proc_thrlock);	
		return EMAXTHREADS;
	}

	stack_start = ((stack_start + 1) << 12) - USER_STACK_SIZE;
	// It should have failed even before if more than 16 
	// threads are there.
	thrno = ++current_process->proc_thrno;

	tslot = create_thread_in_proc((struct process *)current_process, (void *)attr, start_routine, arg, stack_start, USER_STACK_SIZE, thrno);
	if (tslot < 0) // Failure
	{
		*threadp = -1;
		unlock((struct lock_t *)&current_process->proc_thrlock);	
		return tslot;
	}
	*threadp = ((pno << 16) + thrno);
	unlock((struct lock_t *)&current_process->proc_thrlock);	
	return 0;
}

/* Obtain the identifier of the current thread.  */
pthread_t syscall_pthread_self (void)
{
	return  (current_process->proc_id << 16) + current_thread->kt_threadid;
} 
                                                                                
/* Compare two thread identifiers.  */
int pthread_equal (pthread_t thread1, pthread_t thread2)
{
	return (thread1 == thread2);
}
                                                                                
/* Terminate calling thread.  */
void syscall_pthread_exit (void *retval)
{ // return value is not being handled
	
	_lock((struct lock_t *)&(current_process->proc_thrlock));
	current_thread->kt_exitstatus = retval;
	unlock((struct lock_t *)&(current_process->proc_thrlock));
	thread_exit();
	return;
}
                                                                                
/* Make calling thread wait for termination of the thread TH.  The
   exit status of the thread is stored in *THREAD_RETURN, if THREAD_RETURN
   is not NULL.  */
int syscall_pthread_join (pthread_t th, void **thread_return)
{
	struct kthread *thr = NULL;
	struct kthread *t1;
	unsigned short int pno, thno;

	pno = (th >> 16) & 0x0000ffff;
	thno = (th & 0x0000ffff);

	if (current_process->proc_id != pno) return -1;

	// Dont allow to join with itself
	if (current_thread->kt_threadid == thno) return -1;

	// checking for validity of pthread number must be done.
	_lock((struct lock_t *)&(current_process->proc_thrlock));
	thr = (struct kthread *)current_process->proc_threadptr;
	while (thr != NULL && thr->kt_threadid != thno) thr = (struct kthread *)thr->kt_sib;
	unlock((struct lock_t *)&(current_process->proc_thrlock));

	if (thr == NULL) return -1;
	
	_lock(&(thr->kt_exitlock));
	if (thr->kt_schedinf.sc_state != THREAD_STATE_TERMINATED)
		event_sleep(&(thr->kt_threadexit), &(thr->kt_exitlock));
	
	if (thread_return != NULL)
		*thread_return = thr->kt_exitstatus;
	unlock(&(thr->kt_exitlock));
	// Remove it from the list of threads
	
	_lock((struct lock_t *)&(current_process->proc_thrlock));
	if (current_process->proc_threadptr == thr)
		current_process->proc_threadptr = thr->kt_sib;
	else
	{
		t1 = (struct kthread *)current_process->proc_threadptr;

		while (t1->kt_sib != thr) t1 = (struct kthread *)t1->kt_sib;
		t1->kt_sib = thr->kt_sib;
	}
	thr->kt_sib = NULL;
	unlock((struct lock_t *)&(current_process->proc_thrlock));
	destroy_thread(thr);
		
	return 0;
}

int syscall_pthread_detach (pthread_t th)
{
	struct kthread *thr, *t1;
	unsigned short int pno, thno;

	pno = (th >> 16) & 0x0000ffff;
	thno = (th & 0x0000ffff);

	if (current_process->proc_id != pno) return -1;
	
	// checking for validity of pthread number must be done.
	_lock((struct lock_t *)&(current_process->proc_thrlock));
	thr = (struct kthread *)current_process->proc_threadptr;
	while (thr != NULL && thr->kt_threadid != thno) thr = (struct kthread *)thr->kt_sib;
	unlock((struct lock_t *)&(current_process->proc_thrlock));

	if (thr == NULL) return -1;

	// Determine whether it is local thread or remote thread.
 	if (thr != NULL) // Local
	{
		_lock(&thr->kt_exitlock);
		if (thr->kt_schedinf.sc_state != THREAD_STATE_TERMINATED)
		{
			thr->kt_threadjoinable = 0;
			unlock(&thr->kt_exitlock);
		}
		else
		{
			// Already terminated
			unlock(&thr->kt_exitlock);
			// Release that thread
			_lock((struct lock_t *)&(current_process->proc_thrlock));
			if (current_process->proc_threadptr == thr)
				current_process->proc_threadptr = thr->kt_sib;
			else
			{
				t1 = (struct kthread *)current_process->proc_threadptr;

				while (t1->kt_sib != thr) t1 = (struct kthread *)t1->kt_sib;
					t1->kt_sib = thr->kt_sib;
			}
			thr->kt_sib = NULL;
			unlock((struct lock_t *)&(current_process->proc_thrlock));
			destroy_thread(thr);
		}
	}
	return 0;
} 
                                                                                
                                                                                
/* Functions for handling attributes.  */
                                                                                
/* Initialize thread attribute *ATTR with default attributes
   (detachstate is PTHREAD_JOINABLE, scheduling policy is SCHED_OTHER,
    no user-provided stack).  */
extern int pthread_attr_init (pthread_attr_t *__attr) ;
                                                                                
/* Destroy thread attribute *ATTR.  */
extern int pthread_attr_destroy (pthread_attr_t *__attr) ;
                                                                                
/* Set the `detachstate' attribute in *ATTR according to DETACHSTATE.  */
extern int pthread_attr_setdetachstate (pthread_attr_t *__attr,
                                        int __detachstate) ;
                                                                                
/* Return in *DETACHSTATE the `detachstate' attribute in *ATTR.  */
extern int pthread_attr_getdetachstate (__const pthread_attr_t *__attr,
                                        int *__detachstate) ;
                                                                                
/* Set scheduling parameters (priority, etc) in *ATTR according to PARAM.  */
extern int pthread_attr_setschedparam (pthread_attr_t *__restrict __attr,
                                       __const struct sched_param *__restrict
                                       __param) ;
                                                                                
/* Return in *PARAM the scheduling parameters of *ATTR.  */
extern int pthread_attr_getschedparam (__const pthread_attr_t *__restrict
                                       __attr,
                                       struct sched_param *__restrict __param)
     ;
                                                                                
/* Set scheduling policy in *ATTR according to POLICY.  */
extern int pthread_attr_setschedpolicy (pthread_attr_t *__attr, int __policy)
     ;
                                                                                
/* Return in *POLICY the scheduling policy of *ATTR.  */
extern int pthread_attr_getschedpolicy (__const pthread_attr_t *__restrict
                                        __attr, int *__restrict __policy)
     ;
                                                                                
/* Set scheduling inheritance mode in *ATTR according to INHERIT.  */
extern int pthread_attr_setinheritsched (pthread_attr_t *__attr,
                                         int __inherit) ;

/* Return in *INHERIT the scheduling inheritance mode of *ATTR.  */
extern int pthread_attr_getinheritsched (__const pthread_attr_t *__restrict
                                         __attr, int *__restrict __inherit)
     ;
                                                                                
/* Set scheduling contention scope in *ATTR according to SCOPE.  */
extern int pthread_attr_setscope (pthread_attr_t *__attr, int __scope)
     ;
                                                                                
/* Return in *SCOPE the scheduling contention scope of *ATTR.  */
extern int pthread_attr_getscope (__const pthread_attr_t *__restrict __attr,
                                  int *__restrict __scope) ;
                                                                                
/* Set the starting address of the stack of the thread to be created.
   Depending on whether the stack grows up or down the value must either
   be higher or lower than all the address in the memory block.  The
   minimal size of the block must be PTHREAD_STACK_MIN.  */
extern int pthread_attr_setstackaddr (pthread_attr_t *__attr,
                                      void *__stackaddr) ;
                                                                                
/* Return the previously set address for the stack.  */
extern int pthread_attr_getstackaddr (__const pthread_attr_t *__restrict
                                      __attr, void **__restrict __stackaddr);


int syscall_pthread_yield (void)
{
	unsigned long oflags;
	CLI;
	scheduler();
	STI;
	return 0;
}
                                                                                
/* Functions for mutex handling.  */
int mutexid = 1;
                                                                                
/* Initialize MUTEX using attributes in *MUTEX_ATTR, or use the
   default values if later is NULL.  */

int syscall_pthread_mutex_init (pthread_mutex_t *__restrict mutex,
                               __const pthread_mutexattr_t *__restrict
                               mutex_attr) 
{
	// Only simple implemenation at present
	if (mutex_attr != NULL) mutex->m_kind = mutex_attr->mutexkind;
	else mutex->m_kind = 0;
	
	lockobj_init(&mutex->m_lock);	
	return 0;
}
                                                                                
/* Destroy MUTEX.  */
int syscall_pthread_mutex_destroy (pthread_mutex_t *mutex)
{
	return 0;
} 
                                                                                
/* Try to lock MUTEX.  */
int syscall_pthread_mutex_trylock (pthread_mutex_t *mutex)
{ return trylock(&(mutex->m_lock)); }
                                                                                
/* Wait until lock for MUTEX becomes available and lock it.  */
int syscall_pthread_mutex_lock (pthread_mutex_t *mutex) 
{ return _lock(&(mutex->m_lock)); } 
                                                                                
/* Wait until lock becomes available, or specified time passes. */
int pthread_mutex_timedlock (pthread_mutex_t *__restrict __mutex,
                                    __const struct timespec *__restrict
                                    __abstime)
{
	// Not implemented
	return 0;
}
                                                                                
/* Unlock MUTEX.  */
int syscall_pthread_mutex_unlock (pthread_mutex_t *mutex)
{ return unlock(&(mutex->m_lock)); }
                                                                                
                                                                                
/* Functions for handling mutex attributes.  */
                                                                                
/* Initialize mutex attribute object ATTR with default attributes
   (kind is PTHREAD_MUTEX_TIMED_NP).  */
int pthread_mutexattr_init (pthread_mutexattr_t *__attr) { return 0; }
                                                                                
/* Destroy mutex attribute object ATTR.  */
int pthread_mutexattr_destroy (pthread_mutexattr_t *__attr) { return 0; }
                                                                                
/* Get the process-shared flag of the mutex attribute ATTR.  */
int pthread_mutexattr_getpshared (__const pthread_mutexattr_t *
				 __restrict __attr,
				 int *__restrict __pshared) { return 0; }
/* Set the process-shared flag of the mutex attribute ATTR.  */
int pthread_mutexattr_setpshared (pthread_mutexattr_t *__attr,
				 int __pshared) { return 0; } 
                                                                                
/* Functions for handling conditional variables.  */
int condid = 1;
/* Initialize condition variable COND using attributes ATTR, or use
   the default values if later is NULL.  */
int syscall_pthread_cond_init (pthread_cond_t *__restrict cond,
                              __const pthread_condattr_t *__restrict
                              __cond_attr)
{
	eventobj_init(&cond->c_event);
	return 0;
}
                                                                                
/* Destroy condition variable COND.  */
int syscall_pthread_cond_destroy (pthread_cond_t *cond)
{
	return 0;
}
                                                                                
/* Wake up one thread waiting for condition variable COND.  */
int syscall_pthread_cond_signal (pthread_cond_t *cond)
{
	struct kthread *t;
	//int __pri__;
	unsigned long oflags;

	if (cond == NULL) return -1;

	// Remove thread from wait q
	CLI;
	spinlock(cond->c_event.e_slq.sl_bitval);
	DEL_SLQ(cond->c_event.e_slq, t);
	spinunlock(cond->c_event.e_slq.sl_bitval);
	STI;
		
	if (t != NULL) wakeup(t); // Wake up t
	return 0;
}
                                                                                
/* Wake up all threads waiting for condition variables COND.  */
int syscall_pthread_cond_broadcast (pthread_cond_t *cond)
{
	return event_wakeup(&(cond->c_event));
}
                                                                                
/* Wait for condition variable COND to be signaled or broadcast.
   MUTEX is assumed to be locked before.  */
int syscall_pthread_cond_wait (pthread_cond_t *cond,
                              pthread_mutex_t *mutex)
{
	if (mutex != NULL)
		return event_sleep(&(cond->c_event), &(mutex->m_lock));
	else return event_sleep(&(cond->c_event), NULL);
}
                                                                                
/* Wait for condition variable COND to be signaled or broadcast until
   ABSTIME.  MUTEX is assumed to be locked before.  ABSTIME is an
   absolute time specification; zero is the beginning of the epoch
   (00:00:00 GMT, January 1, 1970).  */
int pthread_cond_timedwait (pthread_cond_t *__restrict __cond,
                                   pthread_mutex_t *__restrict __mutex,
                                   __const struct timespec *__restrict
                                   __abstime) { return 0; }
                                                                                
/* Functions for handling condition variable attributes.  */
                                                                                
/* Initialize condition variable attribute ATTR.  */
int pthread_condattr_init (pthread_condattr_t *__attr) { return 0; } 
                                                                                
/* Destroy condition variable attribute ATTR.  */
int pthread_condattr_destroy (pthread_condattr_t *__attr) { return 0; }
                                                                                
/* Get the process-shared flag of the condition variable attribute ATTR.  */
int pthread_condattr_getpshared (__const pthread_condattr_t *
                                        __restrict __attr,
                                        int *__restrict __pshared) { return 0; }
                                                                                
/* Set the process-shared flag of the condition variable attribute ATTR.  */
int pthread_condattr_setpshared (pthread_condattr_t *__attr,
                                        int __pshared) { return 0; }
                                                                                
/* The IEEE Std. 1003.1j-2000 introduces functions to implement
   spinlocks.  */
                                                                                
/* Initialize the spinlock LOCK.  If PSHARED is nonzero the spinlock can
   be shared between different processes.  */
int pthread_spin_init (pthread_spinlock_t *__lock, int __pshared) { return 0; }

                                                                                
/* Destroy the spinlock LOCK.  */
int pthread_spin_destroy (pthread_spinlock_t *__lock) { return 0; }
                                                                                
/* Wait until spinlock LOCK is retrieved.  */
int pthread_spin_lock (pthread_spinlock_t *__lock) { return 0; }
/* Try to lock spinlock LOCK.  */
int pthread_spin_trylock (pthread_spinlock_t *__lock) { return 0; }
                                                                                
/* Release spinlock LOCK.  */
int pthread_spin_unlock (pthread_spinlock_t *__lock) { return 0; }
                                                                                

int barrid = 1;
/* Barriers are a also a new feature in 1003.1j-2000. */
                                                                                
int syscall_pthread_barrier_init (pthread_barrier_t *__restrict barrier,
                                 __const pthread_barrierattr_t *__restrict
                                 __attr, unsigned int count)
{
	barrier->ba_spinlock = 0;
	barrier->ba_required = count;
	barrier->ba_present = 0;
	barrier->ba_waitq = NULL;
	return 0;
}
                                                                                
int pthread_barrier_destroy (pthread_barrier_t *barrier) 
{ 
	return 0; 
}
                                                                                
int pthread_barrierattr_init (pthread_barrierattr_t *__attr) { return 0; }
                                                                                
int pthread_barrierattr_destroy (pthread_barrierattr_t *__attr) { return 0; }
                                                                                
int pthread_barrierattr_getpshared (__const pthread_barrierattr_t *
				   __restrict __attr,
				   int *__restrict __pshared) { return 0; }

int pthread_barrierattr_setpshared (pthread_barrierattr_t *__attr,
                                           int __pshared) { return 0; }
                                                                                
int syscall_pthread_barrier_wait (pthread_barrier_t *barrier)
{
	struct kthread *t1, *t2;
	unsigned long oflags;

	CLI;
	spinlock(barrier->ba_spinlock); // At present useless

	if (barrier->ba_present + 1 == barrier->ba_required) // Count completed?
	{
		// Wake up all waiting processes
		t1 = (struct kthread *)barrier->ba_waitq;
		barrier->ba_waitq = NULL;
		while (t1 != NULL)
		{
			t2 = (struct kthread *)t1->kt_qnext;
			wakeup(t1);
			t1 = t2;
		}
		barrier->ba_present = 0;
		spinunlock(barrier->ba_spinlock); // At present useless
	}
	else	// Wait
	{
		current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
		current_thread->kt_qnext = barrier->ba_waitq;
		if (barrier->ba_waitq != NULL)
			(barrier->ba_waitq)->kt_qprev = (struct kthread *)current_thread;
		barrier->ba_waitq = (struct kthread *)current_thread;
		current_thread->kt_qprev = NULL;
		barrier->ba_present++;
		spinunlock(barrier->ba_spinlock);
		scheduler();
	}
	STI;
	return 0;
}

