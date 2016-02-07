
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


#ifndef __PROCESS_H__
#define __PROCESS_H__

#define PROC_TYPE_USER		0x01
#define PROC_TYPE_SUBSYSTEM	0x02
#define PROC_TYPE_SYSTEM	0x04
#define PROC_TYPE_APPLICATION	0x10
#define PROC_TYPE_PARALLEL	0x20
#define PROC_TYPE_MPIPARALLEL	0x40

#define THREAD_STATE_SLEEP	1
#define THREAD_STATE_READY	2
#define THREAD_STATE_CREATE	3
#define THREAD_STATE_TERMINATING	4
#define THREAD_STATE_TERMINATED	5
#define THREAD_STATE_RUNNING	6
#define THREAD_STATE_UNUSED	0

#define PROC_STATE_SUSPENDED	1
#define PROC_STATE_READY	2
#define PROC_STATE_CREATE	3
#define PROC_STATE_TERMINATED	5
#define PROC_STATE_UNUSED	0

#include "mconst.h"
#include "segment.h"
#include "lock.h"

struct input_info {
	struct lock_t input_buf_lock;
	struct event_t input_available;
	short int input_buf[KB_BUF_SIZE];
	short int kbhead, kbtail;
};

struct output_info {
	struct lock_t output_lock;
	char video_mem_backup[4000];
	char *video_mem_base;
	unsigned int cursor_x, cursor_y;
	char attrib;
};

struct files_info {
	volatile struct vnode *root_dir;
	volatile struct vnode *current_dir;
	int fhandles[MAXPROCFILES];
	struct lock_t fhandles_lock;
};

// Event supporting interface is required
struct sched_events
{
	void (* CL_TICK)(struct kthread *thr);		// At present we are going to use only 
					// this event.
	void (* CL_FORK)(struct kthread *thr);
	void (* CL_FORKRET)(struct kthread *thr);
	void (* CL_ENTERCLASS)(struct kthread *thr);
	void (* CL_EXITCLASS)(struct kthread *thr);
	void (* CL_SLEEP)(struct kthread *thr);
	void (* CL_WAKEUP)(struct kthread *thr);
};

// So function pointers about event handling must be added in 
// the following structure. All times are measured in mille seconds.
// Actual scheduling algorithm used is very simple and it may not be using all
// the information shown below.
struct sched_info {
	short int sc_cid;
//	short int sc_pri;	// Overall priority (sc_usrpri + sc_cpupri)
	short int sc_usrpri;	// user controllable priority 
	short int sc_cpupri;	// System controlled priority 
	short int sc_inhpri;
	short int sc_reccpuusage;
	short int sc_state;
	short int sc_tqleft;
	unsigned int sc_totcpuusage;
	unsigned int sc_slpstart;
	unsigned int sc_thrreadystart;
	struct sched_events *sc_clfuncs;
};

#define RR_PRIORITY_MIN		10
#define RR_PRIORITY_MAX		99
#define SP_PRIORITY_MIN		100
#define SP_PRIORITY_MAX		129
#define IT_PRIORITY_MIN		130
#define IT_PRIORITY_MAX		159
#define IDLE_PRIORITY_MIN	0
#define IDLE_PRIORITY_MAX	0

#define SCHED_CLASS_INTERRUPT	1
#define SCHED_CLASS_SYSTEM	2
#define SCHED_CLASS_USER	3
#define SCHED_CLASS_IDLE	4

// Schedulable unit, thread structure.
struct kthread {
	char kt_name[32];
	unsigned short kt_threadid;
	unsigned short kt_type;

	int kt_tssdesc;
	struct TSS *kt_tss;	// Hardware task context is used for thread support
	struct fpu_state kt_fps; 	// FPU state
	unsigned int kt_cr0;
	unsigned int kt_ustackstart;	
				// user stack region logical start address. 
				// Size is limited to 4mb maximum.
	unsigned int kt_kstack[KERNEL_STACK_SIZE / 4];
	struct sched_info kt_schedinf;
	struct proc_sock *kt_defsock;
	volatile unsigned short  kt_terminatelock;
	volatile struct kthread * kt_terminatelock_owner;
	struct lock_t kt_exitlock;
	struct event_t kt_threadexit;
	void *kt_exitstatus;
	unsigned short kt_threadjoinable;
	volatile unsigned short kt_synchobjlock;
	volatile struct kthread * kt_synchobjlock_owner;
	volatile struct sleep_q * kt_synchobjlist;
	volatile struct sleep_q * kt_slpchannel;
	volatile struct process *kt_process;
	volatile struct kthread * kt_sib;
	volatile struct kthread * kt_uselist;
	volatile struct kthread * kt_qnext;
	volatile struct kthread * kt_qprev;
	volatile void * synchtry;
	volatile void * synchobtained;
	volatile void * lockpageobtained;
};
	
struct process {
	char proc_name[32];	
	unsigned short proc_id;
	int proc_type;	
	int proc_state;
	int proc_ldtindex;	// LDT used by this process
	int proc_thrno;		// Next thread id 
	int proc_waitable;
	struct lock_t proc_exitlock;
	struct event_t proc_exitevent;
	int proc_exitstatus;
	int proc_starttimesec;
	int proc_starttimemsec;
	int proc_totalcputime;
	int proc_childproccputime;
	struct vm *proc_vm;	// Virtual memory info
	struct output_info *proc_outputinf;	// Screen buf and information
	struct input_info  *proc_inputinf;	// Keyboard buf etc.
	struct files_info *proc_fileinf;		// Open file, cwd etc.
	struct lock_t proc_timerobjlock;
	struct timerobject *proc_timedout;	// Timed out timer objects
	struct lock_t proc_thrlock;
	volatile struct kthread *proc_threadptr; // Child threads (local)
	volatile struct process * proc_parent;
	struct lock_t proc_childlistlock;
	struct event_t proc_childtermevent;
	volatile struct process * proc_childlist;
	volatile struct process * proc_sib;
	volatile struct process * proc_next;
	volatile struct process * proc_prev;
};

struct uprocinfo {
	char uproc_name[32];	
	int uproc_type;	

	// ids
	unsigned short uproc_id;
	unsigned short uproc_ppid;
	// Time
	int uproc_totalcputimemsec;
	int uproc_starttimesec;
	int uproc_starttimemsec;
	
	// If terminated (thread count == 0)
	int uproc_status;

	// memory usage
	unsigned int uproc_memuse;

	// No of open files
	int uproc_nopenfiles;

	// Threads count
	int uproc_nthreadslocal;	
};

struct process_param {
	unsigned long cs_start; 
	unsigned long cs_size;
	unsigned long ds_start;
	unsigned long ds_size;
	unsigned long ss3_start;
	unsigned long ss3_size;
	unsigned long eflags;
	unsigned long start_routine;
	int mainthrno;
	struct vm *vminfo;	// Virtual memory info
};


void idlethread_routine(void);
void initialize_systemprocess(void);
int create_process(struct process_param *tparam, char *name, int type, int mctype, int priority, unsigned short procno);
int create_thread(struct process *proc, char thrname[], int scid, int priority, unsigned long stck_top, unsigned long start_routine, short threadno);
void add_readyq(struct kthread *thr);
int del_readyq(struct kthread *thr);
void thread_exit(void);
void terminate_process(struct process *p);
struct kthread *alloc_thread(void);
struct rthread * alloc_rthread_slot(void);
void suspend(struct process *proc);
void wakeup(struct kthread *t);
int create_thread_in_proc(struct process *proc, void *attr, void * (*start_routine)(void *), void *arg, unsigned int stackstart, unsigned int initialstacksize, int thrid);
//To create a new process loaded from the given pathnamed file.
int syscall_exectask(char pathname[], char *argv[], char *envp[], int type);
void syscall_exit(int status);
int syscall_getpids(int pid[], int n);
int syscall_getprocinfo(int pid, struct uprocinfo *pinf);
int syscall_wait(int *status);
int syscall_kill(int pid);
struct kthread * init_irqtask(int irqno, char *tname, void (* irq_handler_func)(void), int pri);
int syscall_pageinfo(int pid, unsigned int pageno);

void math_state_restore(struct fpu_state *fps);
void math_state_save(struct fpu_state *fps);
void math_emulate(struct PAR_REGS *regs, int error_code);

#endif
