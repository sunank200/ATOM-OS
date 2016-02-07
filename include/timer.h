
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

#ifndef __TIMER_H__
#define __TIMER_H__

/* Date and time structure.	*/
struct date_time {
	short seconds; 
	short minutes; 
	short hours; 
	short date; 
	short month; 
	short year;
};

struct timeval {
	int tv_sec;
	int tv_usec;
};

struct timespec {
	int tv_sec;
	int tv_nsec;
};

#ifndef TIMEROBJECT
#define TIMEROBJECT
struct timerobject {
	long timer_count;
	int (* timer_handler)(void *);
	void *timer_handlerparam;
	struct kthread *timer_ownerthread;
	volatile struct timerobject *timer_next, *timer_prev;
	struct event_t timer_event;
};
#endif

void rr_cl_tick(struct kthread *thr);
void timer_handler (int irq);
void syscall_tsleep(unsigned int msec);
time_t syscall_ptime(time_t *t);
time_t syscall_time(time_t *t);
void timer_init(void);
void timerobj_init(struct timerobject *tobj);
int starttimer(struct timerobject *tobj, unsigned long msec, int (*timeouthandler)(void *), void *handlerparam);
int stoptimer(struct timerobject *tobj);
void timerthread(void);
int syscall_gettsc(unsigned int *hi, unsigned int *lo, unsigned int *msecs);
struct date_time secs_to_date(unsigned int secs);
unsigned int date_to_secs(struct date_time dt);
void scheduler(void);
void context_switch(struct kthread * new_thread);

#endif


