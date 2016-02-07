
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

#include "process.h"
#include "timer.h"
#include "mklib.h"
#include "lock.h"
#include "alfunc.h"


// Total 160 levels each in interval of 5 (for simplifying, otherwise should be 1).
// 0 - 79 for user threads and 80-119 system threads, 120-159 interrupt handlers
// index = global priority (quantum, tqexp, slpret = lwait)

volatile int runrun = 0, kprunrun = 0, priocompute = 0, timeoutflag = 0;
struct lock_t timerq_lock;
volatile struct timerobject *timers = NULL;
volatile unsigned int millesecs = 0;
volatile unsigned int secs = 0;
volatile unsigned int initsecs;
unsigned char basepri[4] = {IT_PRIORITY_MIN, SP_PRIORITY_MIN, RR_PRIORITY_MIN, IDLE_PRIORITY_MIN}, maxpri[4] = {IT_PRIORITY_MAX, SP_PRIORITY_MAX, RR_PRIORITY_MAX, IDLE_PRIORITY_MAX};

struct event_t timerthr_sleep;
struct kthread *timer_intr_thread, *timed_event_thread = NULL;

extern struct segdesc gdt_table[];
extern volatile unsigned short ready_qlock;
extern volatile struct kthread * ready_qlock_owner;
extern volatile struct kthread * ready_qhead[], *ready_qtail[];
extern volatile struct kthread * current_thread;
extern volatile struct process * current_process;
extern struct process system_process;
extern struct kthread * nic_intr_thread;

extern void irq_entry_0_0(void);
void check_urgent_flags(void);

time_t syscall_ptime(time_t *t)
{
	extern struct kthread * nicthr, *assembthread, *trans;
	unsigned int t1;

	t1 = current_thread->kt_schedinf.sc_totcpuusage;
	t1 += nicthr->kt_schedinf.sc_totcpuusage;
	t1 += ( 2 * assembthread->kt_schedinf.sc_totcpuusage );
	printk("cur : %d, nic : %d, assem : %d, trans : %d - Total : %d\n",current_thread->kt_schedinf.sc_totcpuusage, nicthr->kt_schedinf.sc_totcpuusage, assembthread->kt_schedinf.sc_totcpuusage, trans->kt_schedinf.sc_totcpuusage, t1);

	if (t!= NULL)
		*t = (time_t)t1;
	return (time_t)t1;
}

time_t syscall_time(time_t *t)
{
	if (t!= NULL)
		*t = (time_t)secs;
	return (time_t)secs;
}

void initialize_clock(void)
{
	void set_timer_frequency(void);

	set_timer_frequency();
	lockobj_init(&timerq_lock);
	eventobj_init(&timerthr_sleep);
}

void timerobj_init(struct timerobject *tobj)
{
	if (tobj != NULL)
	{
		tobj->timer_count = 0;
		tobj->timer_handler = NULL;
		tobj->timer_handlerparam = NULL;
		tobj->timer_ownerthread = NULL;
		tobj->timer_next = NULL;
		tobj->timer_prev = NULL;
		eventobj_init(&tobj->timer_event);
	}
		
}

void insert_timerobj(struct timerobject *tobj)
{
	struct timerobject *t1, *t2=NULL;

	// add this object to timer q
	if (timers == NULL)
	{
		// First waiter
		timers = tobj;
	}
	else
	{
		if (timers->timer_count > tobj->timer_count)
		{
			tobj->timer_next = (struct timerobject *)timers;
			timers->timer_prev = tobj;
			timers->timer_count -= tobj->timer_count;
			timers = tobj;
		}
		else
		{
                      /*new code */
			t1 = (struct timerobject *)timers;
			
			while(t1 != NULL && t1->timer_count <= tobj->timer_count)
			{
				tobj->timer_count -= t1->timer_count;
				t2 = t1;
				t1=(struct timerobject *)t1->timer_next;
			}
			
			tobj->timer_next = t1;
			tobj->timer_prev = t2;
			t2->timer_next = tobj;
			if (t1 != NULL)
			{
				t1->timer_prev = tobj;
				t1->timer_count -= tobj->timer_count;
			}
		}
	}
	return;
}

void remove_timerobj(struct timerobject *tobj)
{
	if (tobj->timer_prev != NULL)
		(tobj->timer_prev)->timer_next = tobj->timer_next;
	else
		timers = tobj->timer_next;

	if (tobj->timer_next != NULL)
	{
		(tobj->timer_next)->timer_prev = tobj->timer_prev;
		(tobj->timer_next)->timer_count += tobj->timer_count;
	}

	tobj->timer_next = tobj->timer_prev = NULL;
}

int starttimer(struct timerobject *tobj, unsigned long msec, int (*timeouthandler)(void *), void *handlerparam)
{
	struct timerobject * t1;
	// Initialize the timer object
	tobj->timer_count = msec;
	tobj->timer_handler =  timeouthandler;	
	tobj->timer_handlerparam = handlerparam;
	tobj->timer_ownerthread = (struct kthread *)current_thread;
	tobj->timer_next = tobj->timer_prev = NULL;

	_lock(&timerq_lock);

#ifdef DEBUG
	// Check if timer is already in the list
	t1 = (struct timerobject *)timers;
	while (t1 != NULL && t1 != tobj) t1 = (struct timerobject *)t1->timer_next;
	if (t1 == tobj)
	{
		syscall_puts((char *)current_thread->kt_name);
		syscall_puts(" TIMER ALREADY IN THE QUEUE\n");
		unlock(&timerq_lock);
		return -1;
	}
#endif

	// Insert this timer object
	insert_timerobj(tobj);
	unlock(&timerq_lock);
	
	return 0;
}

int stoptimer(struct timerobject *tobj)
{	
	// Remove timer object from the queue
	struct timerobject *t1;
	int ret = 0;

	_lock(&timerq_lock);
	
	// Check if it is in the queue
	t1 = (struct timerobject *)timers;
	while (t1 != NULL && t1 != tobj) t1 = (struct timerobject *)t1->timer_next;
	if (t1 != tobj)
	{
		ret = -1;
	}
	else remove_timerobj(tobj);
	unlock(&timerq_lock);

	tobj->timer_next = tobj->timer_prev = NULL;
	tobj->timer_count = -1;
	return ret;
}
void syscall_tsleep(unsigned int msec)
{
	unsigned long oflags;
	struct timerobject tobj;

	// Start timer object
	// Initialize the timer object
	tobj.timer_count = msec;
	tobj.timer_handler =  (int (*)(void *))wakeup;	
	tobj.timer_handlerparam = NULL;
	tobj.timer_ownerthread = (struct kthread *)current_thread;
	tobj.timer_next = tobj.timer_prev = NULL;

	_lock(&timerq_lock);
	// Insert this timer object
	insert_timerobj(&tobj);
	CLI;
	current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
	unlock(&timerq_lock);
	scheduler();
	STI;

	return;
}

int syscall_twakeup(struct kthread *thr)
{
	struct timerobject *t1;
	int ret = 0;

	_lock(&timerq_lock);
	
	// Check if it is in the queue
	t1 = (struct timerobject *)timers;
	while (t1 != NULL && t1->timer_ownerthread != thr) t1 = (struct timerobject *)t1->timer_next;
	if (t1 == NULL)
	{
		ret = -1;
	}
	else
	{
		remove_timerobj(t1);
		t1->timer_count = -1;
		add_readyq(t1->timer_ownerthread);
	}
	unlock(&timerq_lock);

	return ret;
}

extern void mark_tsc(unsigned int *hi, unsigned int *lo);
int syscall_gettsc(unsigned int *hi, unsigned int *lo, unsigned int *msec)
{
	unsigned int t1=1;
	unsigned int h=2, l=2; 

	mark_tsc(&h, &l);
	t1 = (secs - 1220000000) * 1000 + millesecs;	
	*hi = h; *lo = l; *msec = t1;
	return 0;
}

struct date_time secs_to_date(unsigned int secs)
{
	struct date_time dt;
	int days,mdays;
	int month_days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
	days = secs / (24 * 3600);
	secs = secs % (24 * 3600);
	dt.year = 1970  + 4 * (days / 1461);
	days = days % 1461;
	if (days > 1096) 
	{ 
		dt.year += 3; days -= 1096; 
	}
	else if (days > 730) 
	{
		dt.year += 2; days -= 730;
	}
	else if (days > 365) 
	{
		dt.year += 1;
		days -= 365;
	}

	if ((dt.year % 4) == 0)
		month_days[2] = 29;
	else
		month_days[2] = 28;
	dt.month = 1;
	mdays = 0;
	while ( mdays + month_days[dt.month] <= days ) 
		mdays += month_days[dt.month++];
	dt.date = days - mdays + 1;
	dt.hours = secs / (3600);
	secs = secs % (3600);
	dt.minutes = secs / (60);
	dt.seconds = secs % 60;

	return dt;
}

unsigned int date_to_secs(struct date_time dt)
{
	unsigned int secs = 0;
	int i,days;
	int month_days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

	if ((dt.year % 4) == 0) month_days[2] = 29;
	else month_days[2] = 28;
	secs += ((dt.year-1970)/4) * (1461 * 24 * 3600);
	dt.year = (dt.year - 1970) % 4;
	secs += dt.year * (365 * 24 * 3600);
	if (dt.year > 2) days = 1;
	else days = 0;
	for (i=1; i<dt.month; i++)
		days += month_days[i];
	days += dt.date -1; 
	secs += ((days * 24 * 3600) + ( dt.hours * 3600) +
		(dt.minutes * 60) + (dt.seconds)); 
	return secs;
}

void rr_cl_tick(struct kthread *thr)
{
	int i;

	thr->kt_schedinf.sc_tqleft -= 20;
	thr->kt_schedinf.sc_totcpuusage += 20;
	thr->kt_schedinf.sc_reccpuusage += 20;
	if (thr->kt_schedinf.sc_tqleft <= 0)
	{
		// Reduce priority
		thr->kt_schedinf.sc_cpupri -= 10;
		i = thr->kt_schedinf.sc_cid-1;
		if (thr->kt_schedinf.sc_cpupri < basepri[i])
			thr->kt_schedinf.sc_cpupri = basepri[i];
	}
}

/*
void rr_cl_tick(struct kthread *thr)
{
	int i;

	thr->kt_schedinf.sc_tqleft -= 20;
	thr->kt_schedinf.sc_totcpuusage += 20;
	if (thr->kt_schedinf.sc_tqleft <= 0)
	{
		thr->kt_schedinf.sc_cpupri -= 5; // Reduce priority
		i = thr->kt_schedinf.sc_cid-1;
		if (thr->kt_schedinf.sc_cpupri < basepri[i])
			thr->kt_schedinf.sc_cpupri = basepri[i];
		if (thr->kt_schedinf.sc_cpupri > maxpri[i])
			thr->kt_schedinf.sc_cpupri = maxpri[i];
		thr->kt_schedinf.sc_thrreadystart = (secs & 0x000FFFFF) * 1000 + millesecs;
	}
}
*/
void rr_cl_sleep(struct kthread *thr)
{
	thr->kt_schedinf.sc_slpstart = (secs & 0x000FFFFF) * 1000 + millesecs;
	return; // No change in priority
}

void rr_cl_wakeup(struct kthread *thr)
{
	int i;
	i = thr->kt_schedinf.sc_cid-1;
	thr->kt_schedinf.sc_cpupri += 5; // return from sleep
	if (thr->kt_schedinf.sc_cpupri > maxpri[i])
		thr->kt_schedinf.sc_cpupri = maxpri[i];
}
void rr_cl_fork(struct kthread *thr) { return; }
void rr_cl_forkret(struct kthread *thr) { return; }
void rr_cl_enterclass(struct kthread *thr) { return; }
void rr_cl_exitclass(struct kthread *thr) { return; }

// This is executed as part of the current running thread
// Activities to be done :
// 1. System time updating.
// 2. Timer object value decrementing and setting the flag 
//    for time out event processing.
// 3. Set if necessary priority recomputation flag.
// 4. Update the time quantum left and cpu usage for the current thread.
//    If necessary call the scheduler function after enablig the timer.
// flags : runrun, kprunrun, priocompute, timeoutflag 
void timer_handler (int irq)
{
	unsigned long oflags;

	/* Update system time value. HZ = 50 */
	millesecs += 20;
	if (millesecs >= 1000)
	{
		secs++;
		millesecs -= 1000;
		priocompute = 1;// Once in every one second recompute 
				//priorities by applying decay factor.
	}

	// Without locking only the first timer object is observed.
	if (timers != NULL)
	{
		timers->timer_count -=  20;
		if (timers->timer_count <= 0) timeoutflag = 1;
	}

	// Update current thread cpu usage statistics
	rr_cl_tick((struct kthread *)current_thread);
	if (current_thread->kt_schedinf.sc_tqleft <= 0 || runrun == 1 || kprunrun == 1)
	{
		// Start scheduler function
		CLI;
		enable_timer();
		scheduler();
		STI;
	}
	return;
}

// Here urgent flags are checked and if necessary 
// timed events handler is invoked.
void check_urgent_flags(void)
{
	unsigned long oflags;

	// Wakeup the timed event handler thread
	if (timeoutflag == 1 || priocompute == 1)
	{
		if (timed_event_thread != NULL && current_thread != timed_event_thread)
		{
			if (timed_event_thread->kt_schedinf.sc_state == THREAD_STATE_SLEEP);
				event_wakeup(&timerthr_sleep);
		}
	}

	if (runrun == 1 || kprunrun == 1)
	{
		CLI;
		scheduler();
		STI;
	}
}

void scheduler(void)
{
	struct kthread *new_thread = NULL;
	struct kthread *t;
	int priority, p;
	unsigned long oflags;
	int i, cid;

	runrun = kprunrun = 0;
	CLI; // Interrupts are always disabled when scheduler is running
	spinlock(ready_qlock);
	
	for (i=0; i<MAXREADYQ; i++)
	{
		if (ready_qhead[i] == NULL) continue; // Go for the next Q
		t = new_thread = (struct kthread *)ready_qhead[i];
		priority = t->kt_schedinf.sc_usrpri + ((t->kt_schedinf.sc_cpupri > t->kt_schedinf.sc_inhpri) ? t->kt_schedinf.sc_cpupri : t->kt_schedinf.sc_inhpri);

		t = (struct kthread *)t->kt_qnext;
		while (t != NULL)
		{
			p = t->kt_schedinf.sc_usrpri + ((t->kt_schedinf.sc_cpupri > t->kt_schedinf.sc_inhpri) ? t->kt_schedinf.sc_cpupri : t->kt_schedinf.sc_inhpri);
			if (priority > p)
			{
				priority = p;
				new_thread = t;
			}
			t = (struct kthread *)t->kt_qnext;
		}
		break;
	}

	// No thread is chosen, may be idle thread is running and 
	// no other thread is ready.
	// Or if selected thread is idle thread Do not context switch, 
	// if current thread is runnable.
	if ((new_thread == NULL) || 
	    ((new_thread->kt_schedinf.sc_cid == SCHED_CLASS_IDLE) && 
             (current_thread->kt_schedinf.sc_state == THREAD_STATE_RUNNING)))
	{
		current_thread->kt_schedinf.sc_thrreadystart = (secs & 0x000fffff) * 1000 + millesecs;
		current_thread->kt_schedinf.sc_tqleft = TIME_QUANTUM;
		spinunlock(ready_qlock);
		STI;
		return; 
	}

	// If current thread is runnable add this to ready Q	
	if (current_thread->kt_schedinf.sc_state == THREAD_STATE_RUNNING)
	{
		current_thread->kt_schedinf.sc_state = THREAD_STATE_READY;
		cid = (current_thread->kt_schedinf.sc_cpupri > current_thread->kt_schedinf.sc_inhpri) ? ((159 - current_thread->kt_schedinf.sc_cpupri) / 5) : ((159 - current_thread->kt_schedinf.sc_inhpri) / 5);
		current_thread->kt_qnext = NULL;
		current_thread->kt_qprev = (struct kthread *)ready_qtail[cid];
		if (ready_qtail[cid] != NULL)
		{
			ready_qtail[cid]->kt_qnext = (struct kthread *)current_thread;
			ready_qtail[cid] = current_thread;
		}
		else ready_qhead[cid] = ready_qtail[cid] = current_thread;
	}

	// Remove the new  thread from the ready q	
	cid = (new_thread->kt_schedinf.sc_cpupri > new_thread->kt_schedinf.sc_inhpri) ? ((159 - new_thread->kt_schedinf.sc_cpupri) / 5) : ((159 - new_thread->kt_schedinf.sc_inhpri) / 5);
	if (new_thread->kt_qnext != NULL)
		(new_thread->kt_qnext)->kt_qprev = new_thread->kt_qprev;
	else ready_qtail[cid]= new_thread->kt_qprev;

	if (new_thread->kt_qprev != NULL)
		(new_thread->kt_qprev)->kt_qnext = new_thread->kt_qnext;
	else ready_qhead[cid] = new_thread->kt_qnext;

	new_thread->kt_qnext = NULL;
	new_thread->kt_qprev = NULL;
	new_thread->kt_schedinf.sc_tqleft = TIME_QUANTUM;
	new_thread->kt_schedinf.sc_thrreadystart = (secs & 0x000fffff) * 1000 + millesecs;

	spinunlock(ready_qlock);
	context_switch(new_thread);
	STI;
	return;
}

#ifdef DEBUG
void restore_proc(void)
{
	printk("Restore proc called %32s\n",current_thread->kt_name);
}

void debug_mesg(int addr)
{
	int i;

	printk("thread-> %x\n", addr);
	for (i=0; i<0x8000; i++) ;
}
#endif

extern volatile unsigned int nicwait_start;
extern snic nic;
extern volatile frame_buffer *tx_wait_pkt;
// (i) This thread is responsible for waking up of timed out sleeping 
//     threads on timers.
// (ii)Certain threads that are sleeping on certain events must be 
//     waken up (nic thread, fdc thread, ...)
void timerthread(void)
{
	struct timerobject *t1, *t2;
	unsigned long oflags;
	unsigned long tmsec;
	struct kthread *thr, *thr1;
	int i, j, inc, cid;

	// Remember this thread address in a global variable.
	timed_event_thread = (struct kthread *)current_thread;
	while (1)
	{
		// Increase priorities of long awiting threads ( > 3 seconds)
		if (priocompute == 1)
		{
			CLI;
			spinlock(ready_qlock);
			priocompute = 0;
			tmsec = (secs & 0x000FFFFF) * 1000 + millesecs;
			for (i=0; i<31; i++)
			{
				// Total queue i is taken out
				thr = (struct kthread *)ready_qhead[i];
				ready_qhead[i] = NULL; ready_qtail[i] = NULL;
				while (thr != NULL)
				{
					thr1 = (struct kthread *)thr->kt_qnext;
					if (tmsec - thr->kt_schedinf.sc_thrreadystart > 3000) inc = 4;
					else inc = 2;

					// Priority must be increased
					// Insert in the suitable queue.
					thr->kt_schedinf.sc_cpupri += inc; // Irrespective of the class/type
					cid = thr->kt_schedinf.sc_cid - 1;
					if (thr->kt_schedinf.sc_cpupri > maxpri[cid])
						thr->kt_schedinf.sc_cpupri = maxpri[cid];
					j = (thr->kt_schedinf.sc_cpupri > thr->kt_schedinf.sc_inhpri) ? ((159 - thr->kt_schedinf.sc_cpupri) / 5) : ((159 - thr->kt_schedinf.sc_inhpri) / 5);
					thr->kt_qnext = NULL;
					thr->kt_qprev = (struct kthread *)ready_qtail[j];
					if (ready_qtail[j] != NULL)
					{
						ready_qtail[j]->kt_qnext = thr;
						ready_qtail[j] = thr;
					}
					else ready_qhead[j] = ready_qtail[j] = thr;
					thr = thr1;
				}
			}
			spinunlock(ready_qlock);
			STI;
		}
		// Check if nic thread is waiting for transmit interrupt
		// for long time
		if (tx_wait_pkt != NULL)
		{
			_lock(&nic.busy);
			if ((tx_wait_pkt != NULL) && (secs - nicwait_start) > 5)
			{
				event_wakeup((struct event_t *)&tx_wait_pkt->threadwait);
			}
			unlock(&nic.busy);
		}
			

		// Check if fdc thread is waiting for disk interrupt
		//for long time
		
		// Process timer objects
		if (timeoutflag == 1)
		{
			_lock(&timerq_lock);
			timeoutflag = 0;
			if (timers != NULL)
			{
				t1 = (struct timerobject *)timers;
				while (t1 != NULL && t1->timer_count <= 0)
				{
					t2 = (struct timerobject *)t1->timer_next;
					if (t2 != NULL)
					{
						t2->timer_count += (t1->timer_count);
						t2->timer_prev = NULL;
					}
					// Add this timed object to the corresponding process.
					t1->timer_prev = t1->timer_next = NULL;
					t1->timer_count = -1; // Not in use
		
					if (t1->timer_handler == (int (*)(void *))wakeup)
					{ // Wakeup operation
						if (t1->timer_ownerthread->kt_schedinf.sc_state == THREAD_STATE_SLEEP)
							wakeup(t1->timer_ownerthread);
					}
					else if (t1->timer_handler != NULL) // Otherwise discard
					{
						t1->timer_handler(t1->timer_handlerparam);
					}
					t1 = t2;
				}
				timers = t1;
			}
			unlock(&timerq_lock);
		}

		// One round is completed. Sleep on event object
		event_sleep(&timerthr_sleep, NULL);
	}
}

void do_context_switch(unsigned int new_task_desc, unsigned int new_cr0, unsigned int cr3);

void context_switch(struct kthread * new_thread)
{
	/*
	 * For switching to the new task
	 * Math state must be saved, 
	 */
	math_state_save((struct fpu_state *)&current_thread->kt_fps);
	math_state_restore(&new_thread->kt_fps);

	// Save cr0
	current_thread->kt_cr0 = get_cr0();
	// Change current process inf necessary
	current_process = new_thread->kt_process;

	new_thread->kt_schedinf.sc_state = THREAD_STATE_RUNNING;
	gdt_table[new_thread->kt_tssdesc >> 3].b &= 0xfffffdff;
	current_thread = new_thread;

	do_context_switch(new_thread->kt_tssdesc, new_thread->kt_cr0, new_thread->kt_tss->cr3);

	// process any pending timeout events
	return;
}

