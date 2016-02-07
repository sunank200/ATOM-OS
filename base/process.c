
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
#include "lock.h"
#include "mklib.h"
#include "errno.h"
#include "vmmap.h"
#include "elf.h"
#include "fs.h"
#include "timer.h"
#include "syscalls.h"
#include "alfunc.h"

extern struct proc_sock *create_defsock(int port);
extern struct MemoryConsistencyOperations *  mc_create(int mctype, unsigned int ds_start, unsigned int ds_size, int attrib);

void initialize_ldt(struct LDT *ldt, struct process_param *tparam, int type);
static struct process *alloc_process(void);
struct kthread *alloc_thread(void);
static int alloc_task_index(void);
int create_thread(struct process *proc, char thrname[], int sched_class_id, int priority, unsigned long stck_top, unsigned long start_routine, short threadno);
void add_readyq(struct kthread *thr);
int del_readyq(struct kthread *thr);
int terminate_thread(struct kthread *t);
void terminate_process(struct process *p);
static void destroy_process(struct process *proc);
void destroy_thread(struct kthread *t);
static int alloc_ldt_index(void);
static int free_task_index(int ind);
static int free_ldt_index(int ind);
void increment_ref_count(int handle);

extern PageDir KERNEL_PAGEDIR;
extern PageTable KERNEL_PAGE_TABLE[];
struct segdesc gdt_table[GDT_SIZE] = { {0x00000000, 0x00000000},
                                       {0x0000ffff, 0x00cf9a00},
                                       {0x0000ffff, 0x00cf9200},
                                       {0x00000067, 0x0000e900} };
struct table_desc gdt_desc={ GDT_SIZE * 8 - 1, (int )gdt_table, 0 };

extern volatile int runrun;
extern volatile int switchthr;
extern volatile unsigned int secs, millesecs;
struct fpu_state initial_fpustate;
volatile struct process *kbd_focus;
extern unsigned int this_hostid;
extern struct lock_t kbd_wait_lock;
extern struct segdesc_s idt_table[];
extern struct lock_t semtable_lock;

struct kthread idle_thread, initialize_system; 
	// Two special threads. Idle thread for executing during idle time.
	// system initialize thread is useful for starting many of system 
	// service providning threads.

struct process system_process;
struct input_info sysproc_inputinfo;
struct output_info sysproc_outputinfo;
struct files_info sysproc_fileinfo;
struct vm sysproc_vm;

struct memregion sysproc_datareg;
struct memsegment sysproc_datasegment;
extern struct PageTableList ptlist[(KERNEL_MEMMAP >> 22) & 0x3ff];

volatile unsigned short ready_qlock = 0;
volatile struct kthread * ready_qlock_owner = NULL;
struct lock_t used_proclock;
struct lock_t suspended_proclock;
struct lock_t unused_proclock;
struct lock_t used_threadlock;
struct lock_t termthr_lock;
struct lock_t unused_threadlock;
struct lock_t gdt_lock;
struct lock_t ldt_lock;
struct event_t terminatehandler_event;

volatile struct kthread * ready_qhead[MAXREADYQ] = { NULL, NULL, NULL, NULL}, *ready_qtail[MAXREADYQ] = { NULL, NULL, NULL, NULL };
volatile struct process * used_processes = NULL;
volatile struct process * suspended_processes = NULL;
volatile struct process * unused_processes = NULL;
volatile int unusedproc_count = 0;
volatile struct kthread * used_threads = NULL;
volatile struct kthread * terminated_threads = NULL;
volatile struct kthread * unused_threads = NULL;
volatile int unusedthr_count = 0;
volatile struct kthread *current_thread = NULL;
volatile struct process *current_process = NULL;
unsigned long initial_eflags = 0x00000200;
struct LDT ldt_table[MAXPROCESSES] __attribute__ ((aligned(8)));
struct TSS tss_table[MAXTHREADS] __attribute__ ((aligned(8)));
struct lock_t tsstable_lock;
struct kthread * intr_threads[16];

extern void rr_cl_tick(struct kthread *thr);
extern void rr_cl_fork(struct kthread *thr);
extern void rr_cl_forkret(struct kthread *thr);
extern void rr_cl_enterclass(struct kthread *thr);
extern void rr_cl_exitclass(struct kthread *thr);
extern void rr_cl_sleep(struct kthread *thr);
extern void rr_cl_wakeup(struct kthread *thr);
extern void change_screen(struct process *newp);
extern int syscall_sync(int dev);	

struct sched_events SCHED_CLASS_SYSTEM_FUNCS = { rr_cl_tick, rr_cl_fork, rr_cl_forkret, rr_cl_enterclass, rr_cl_exitclass, rr_cl_sleep, rr_cl_wakeup };

static int alloc_tss(void);

// idlethread routine that does nothing
void idlethread_routine(void)
{
	int i, count = 0;

	for ( ; ; ) 
	{
		for (i=0; i<0x800000; i++) ;
		count++;
	}
}

// System process is initialized
// Current thread becomes the system initializing thread
// and a new thread idle thread is created.
void initialize_systemprocess(void)
{
	int pslot = alloc_ldt_index();
	char *pname="SYSTEMPROC";
	char *thrname="SYSTEMINITTHR", *idlethr="IDLETHR";
	int i;
	struct process * proc = &system_process;
	struct segdesc_s * gdtp = (struct segdesc_s *)gdt_table;
	int thrslot = 3; 
	struct kthread * thr = &initialize_system;

	// Initialize all locks, events and rlocks
	ready_qlock = 0;
	lockobj_init(&suspended_proclock);
	lockobj_init(&used_proclock);
	lockobj_init(&unused_proclock);
	lockobj_init(&used_threadlock);
	lockobj_init(&unused_threadlock);
	lockobj_init(&termthr_lock);
	lockobj_init(&gdt_lock);
	lockobj_init(&ldt_lock);
	lockobj_init(&semtable_lock);
	eventobj_init(&terminatehandler_event);
	
	thr->kt_tss = &tss_table[alloc_tss()];

	// virtual memory information
	sysproc_vm.vm_pgdir = &KERNEL_PAGEDIR;
	lockobj_init(&(sysproc_vm.vm_lock)); 
	sysproc_vm.vm_mregion = &sysproc_datareg;

	sysproc_datareg.mreg_startaddr = 0x0;
	sysproc_datareg.mreg_endaddr = KERNEL_MEMMAP-1;
	sysproc_datareg.mreg_type = SEGMENTTYPE_DATA;
	lockobj_init(&sysproc_datareg.mreg_reglock);
	sysproc_datareg.mreg_segment = &sysproc_datasegment;
	sysproc_datareg.mreg_next = NULL;

	sysproc_datasegment.mseg_sourcefile = 0;
	sysproc_datasegment.mseg_sourcedev = 0;
	sysproc_datasegment.mseg_sharecount = 1;
	sysproc_datasegment.mseg_npages = (KERNEL_MEMMAP >> 12) & 0x000fffff;
	sysproc_datasegment.mseg_sharecount = 1;
	lockobj_init(&sysproc_datasegment.mseg_lock);
	for (i=0; i<((KERNEL_MEMMAP >> 22) & 0x3ff); i++)
	{
		ptlist[i].pgtable = &KERNEL_PAGE_TABLE[i];
		if (i<((KERNEL_MEMMAP >> 22) & 0x3ff) - 1)
			ptlist[i].next = &ptlist[i+1];
		else ptlist[i].next = NULL;
	}
	sysproc_datasegment.mseg_pgtable = &ptlist[0];
	proc->proc_vm = &sysproc_vm;

	// Input kbd buffer info
	proc->proc_inputinf = &sysproc_inputinfo;
	proc->proc_inputinf->kbhead = proc->proc_inputinf->kbtail = 0;
	lockobj_init(&proc->proc_inputinf->input_buf_lock);
	eventobj_init(&proc->proc_inputinf->input_available);

	// Output screen buffer info
	proc->proc_outputinf = &sysproc_outputinfo;
	proc->proc_outputinf->video_mem_base = (char *)0x000b8000;
	proc->proc_outputinf->cursor_x = 24;
	proc->proc_outputinf->cursor_y = 0;
	proc->proc_outputinf->attrib = 0x07;
	lockobj_init(&proc->proc_outputinf->output_lock);

	// Initializing file table information	
	proc->proc_fileinf = &sysproc_fileinfo;
	proc->proc_fileinf->current_dir = NULL; // Mount floppy return handle
	proc->proc_fileinf->root_dir = NULL;

	proc->proc_fileinf->fhandles[0] = -1;
	proc->proc_fileinf->fhandles[1] = -2;
	proc->proc_fileinf->fhandles[2] = -3; // Special handles for console
	for (i=3; i<MAXPROCFILES; i++)
		proc->proc_fileinf->fhandles[i] = -4; // Invalid handle nos
	lockobj_init(&proc->proc_fileinf->fhandles_lock);

	// Initialize process structure
	strncpy(proc->proc_name, pname, 32);
	proc->proc_id = 0;
	proc->proc_type = PROC_TYPE_SYSTEM;
	proc->proc_state = PROC_STATE_READY;
	proc->proc_ldtindex = pslot;
	proc->proc_thrno = 0;
	proc->proc_waitable = 0;
	lockobj_init(&proc->proc_exitlock);
	eventobj_init(&proc->proc_exitevent);
	proc->proc_exitstatus = 0;
	proc->proc_starttimesec = secs;
	proc->proc_starttimemsec = millesecs;
	proc->proc_totalcputime = 0;
	proc->proc_childproccputime = 0;

	lockobj_init(&proc->proc_timerobjlock);
	proc->proc_timedout = NULL;
	
	lockobj_init(&proc->proc_thrlock);
	proc->proc_threadptr = thr;
	proc->proc_parent = NULL;
	lockobj_init(&proc->proc_childlistlock);
	eventobj_init(&proc->proc_childtermevent);
	proc->proc_childlist = NULL;
	proc->proc_sib = NULL;
	proc->proc_next = NULL;
	proc->proc_prev = NULL;
	used_processes = proc; // First one so no locking is required.

	// Initialize thread structure
		
	// Name & type
	strncpy(thr->kt_name, thrname, 32);
	thr->kt_threadid = 1; // Thread no
	thr->kt_type = proc->proc_type;  // Type of the thread
	thr->kt_tssdesc = thrslot * 8;

	/* Build current thread TSS. 
	 */
	bzero((char *)thr->kt_tss,sizeof(struct TSS));
	thr->kt_tss->ss0 = KERNEL_DS;
	thr->kt_tss->esp0 = (unsigned int)(&(thr->kt_kstack[KERNEL_STACK_SIZE/4 - 1])) - 1;
	thr->kt_tss->cr3 = (unsigned int)&KERNEL_PAGEDIR;
	thr->kt_tss->iomapbase = sizeof(struct TSS);
	thr->kt_tss->trapflag = 0;
	// Other fieldes will be initialised (stored) when task switch happens.
	initialize_dataseg(&gdtp[thrslot], (unsigned int)thr->kt_tss, sizeof(struct TSS), INTR_PRIVILEGE);
	gdtp[thrslot].access = PRESENT | (INTR_PRIVILEGE << DPL_SHIFT) | TSS_TYPE;

	// FPU state	
	bcopy((char *)&initial_fpustate, (char *)&(thr->kt_fps), sizeof(initial_fpustate));
	thr->kt_cr0 = 0x00000011;
	thr->kt_ustackstart = 0; // Not used

	// Sceduling information
	thr->kt_schedinf.sc_cid = SCHED_CLASS_SYSTEM; 
	thr->kt_schedinf.sc_usrpri = 0;
	thr->kt_schedinf.sc_cpupri = SP_PRIORITY_MAX;
	thr->kt_schedinf.sc_inhpri = 0;
	
	thr->kt_schedinf.sc_state = THREAD_STATE_RUNNING;
	thr->kt_schedinf.sc_tqleft = TIME_QUANTUM;
	thr->kt_schedinf.sc_totcpuusage = 0;
	thr->kt_schedinf.sc_slpstart = 0;
	thr->kt_schedinf.sc_thrreadystart = (secs & 0x000FFFFF) * 1000 + millesecs;
	thr->kt_schedinf.sc_clfuncs = &SCHED_CLASS_SYSTEM_FUNCS;
	// Other scheduling related operations need to be initialized

	thr->kt_defsock = create_defsock((proc->proc_id << 16) + thr->kt_threadid); //NULL;
	thr->kt_terminatelock = 0;
	lockobj_init(&thr->kt_exitlock);
	eventobj_init(&thr->kt_threadexit);
	thr->kt_exitstatus = NULL;
	thr->kt_threadjoinable = 1;
	thr->kt_synchobjlock = 0;
	thr->kt_synchobjlist = NULL;
	thr->kt_slpchannel = NULL;
	thr->synchtry = 0;
	thr->synchobtained = 0;

	// initilize this thread related pointers.
	thr->kt_process = proc;
	thr->kt_sib = NULL;

	thr->kt_uselist = (struct kthread *)used_threads;
	used_threads = thr;
	thr->kt_qnext = NULL;	
	thr->kt_qprev = NULL;	

	current_thread = thr;

	kbd_focus = proc; // Initially this is the keyboard focus task.
	current_process = proc;

	// Initialize the task structure of idle thread now
	thrslot = alloc_task_index(); // No need of checking
	thr = &idle_thread;
	thr->kt_tss = &tss_table[alloc_tss()];
	// Name
	strncpy(thr->kt_name, idlethr, 32);
	thr->kt_threadid = 0; // Thread no
	thr->kt_type = proc->proc_type;  // Type of the thread

	thr->kt_tssdesc = thrslot * 8;
	// TSS initialization
	bzero((char *)(thr->kt_tss), sizeof(struct TSS));
	thr->kt_tss->esp0 = (unsigned int)(&(thr->kt_kstack[KERNEL_STACK_SIZE/4 -1]))-1; 
	thr->kt_tss->ss0 = KERNEL_DS;
	thr->kt_tss->cr3 = (unsigned int)(proc->proc_vm->vm_pgdir);
	thr->kt_tss->eip = (unsigned int)idlethread_routine;
	thr->kt_tss->eflags = initial_eflags;

	thr->kt_tss->esp = (unsigned int)(&(thr->kt_kstack[KERNEL_STACK_SIZE/4 -1]))-1; 
	thr->kt_tss->es = 0x10;
	thr->kt_tss->cs = 0x08;
	thr->kt_tss->ss = 0x10;
	thr->kt_tss->ds = 0x10;
	thr->kt_tss->fs = 0x10;
	thr->kt_tss->gs = 0x10;

	thr->kt_tss->ldtsel = (LDTDESC_START + proc->proc_ldtindex) * 8;
	thr->kt_tss->iomapbase = sizeof(struct TSS);

	initialize_dataseg(&gdtp[thrslot], (unsigned int)(thr->kt_tss), sizeof(struct TSS), INTR_PRIVILEGE);
	gdtp[thrslot].access = PRESENT | (INTR_PRIVILEGE << DPL_SHIFT) | TSS_TYPE;
	
	bcopy((char *)&initial_fpustate, (char *)&(thr->kt_fps), sizeof(initial_fpustate));

	thr->kt_ustackstart = 0;	// Only kernel stack
	thr->kt_cr0 = 0x00000011;

	// Sceduling information
	thr->kt_schedinf.sc_cid = SCHED_CLASS_IDLE; 
	thr->kt_schedinf.sc_usrpri = 0;
	thr->kt_schedinf.sc_cpupri = IDLE_PRIORITY_MIN;
	thr->kt_schedinf.sc_inhpri = 0;
	thr->kt_schedinf.sc_state = THREAD_STATE_READY;
	thr->kt_schedinf.sc_tqleft = TIME_QUANTUM;
	thr->kt_schedinf.sc_totcpuusage = 0;
	thr->kt_schedinf.sc_slpstart = 0;
	thr->kt_schedinf.sc_thrreadystart = 0;
	thr->kt_schedinf.sc_clfuncs = &SCHED_CLASS_SYSTEM_FUNCS;
	thr->kt_defsock = NULL;

	thr->kt_terminatelock = 0;
	lockobj_init(&thr->kt_exitlock);
	eventobj_init(&thr->kt_threadexit);
	thr->kt_exitstatus = NULL;

	thr->kt_threadjoinable = 0;
	thr->kt_synchobjlock = 0;
	thr->kt_synchobjlist = NULL;
	thr->kt_slpchannel = NULL;
	thr->synchtry = 0;
	thr->synchobtained = 0;

	// initilize this thread related pointers.
	thr->kt_process = proc;
	thr->kt_sib = proc->proc_threadptr;
	proc->proc_threadptr = thr;

	thr->kt_uselist = (struct kthread *)used_threads;
	used_threads = thr;

	thr->kt_qnext = thr->kt_qprev = NULL;
	/*
	 * Add this thread to ready_q, this is the only thread in ready q now.
	 */
	ready_qhead[MAXREADYQ-1] = ready_qtail[MAXREADYQ-1] = thr;
}

void terminate_initialize_system_thread(void)
{
	struct kthread *t1;
	unsigned long oflags;

	// Remove it from the used list
	_lock(&used_threadlock);
	if (used_threads == current_thread)
		used_threads = current_thread->kt_uselist;
	else
	{
		t1 = (struct kthread *)used_threads;
		while (t1 != NULL && t1->kt_uselist != current_thread) t1 = (struct kthread *)t1->kt_uselist;

		if (t1 != NULL) t1->kt_uselist = current_thread->kt_uselist;
	}
	unlock(&used_threadlock);

	CLI;
	current_thread->kt_schedinf.sc_state = THREAD_STATE_TERMINATING;
	scheduler(); // Never returns back here.
	STI;	// Useless
}

/*
 * Creates a process and one task context and marks it as in ready state.
 * All memory regions addresses sizes are logical addresses.
 * Page directory is prepared by the pageing program, if used.
 * eflags and starting routine address are also passed.
 */
int create_process(struct process_param *tparam, char *name, int type, int mctype, int priority, unsigned short procno)
{
	/*
	 * At present no error checking is performed.
	 */
	int pslot;
	struct process *proc; 
	char thrname[35];
	int i, ret;
	int scid;

	pslot = alloc_ldt_index();
	if (pslot < 0) return EMAXPROCESSES;

	proc = alloc_process();
	if (proc == NULL) 
	{
		free_ldt_index(pslot);
		return ENOMEM;
	}	
	// Name & type
	strncpy(proc->proc_name, name, 32);
	proc->proc_id = procno;
	proc->proc_type = type;
	if (type == PROC_TYPE_SYSTEM) scid = SCHED_CLASS_SYSTEM;
	else scid = SCHED_CLASS_USER;
	proc->proc_state = PROC_STATE_READY;
	/*
	 * initialize ldt 
	 */
	proc->proc_ldtindex = pslot;
	initialize_ldt(&ldt_table[pslot], tparam, type);

	proc->proc_thrno = tparam->mainthrno;
	proc->proc_waitable = 1;
	lockobj_init(&proc->proc_exitlock);
	eventobj_init(&proc->proc_exitevent);
	proc->proc_exitstatus = 0;
	proc->proc_starttimesec = secs;
	proc->proc_starttimemsec = millesecs;
	proc->proc_totalcputime = 0;
	proc->proc_childproccputime = 0;

	proc->proc_vm = tparam->vminfo;

	// Paging info, Dynamic memory allocation info
	// Paging information is already filled in the calling function

	// Input kbd buffer info
	proc->proc_inputinf->kbhead = proc->proc_inputinf->kbtail = 0;
	lockobj_init(&proc->proc_inputinf->input_buf_lock);
	eventobj_init(&proc->proc_inputinf->input_available);

	// Output screen buffer info
	proc->proc_outputinf->video_mem_base = proc->proc_outputinf->video_mem_backup;
	proc->proc_outputinf->cursor_x = proc->proc_outputinf->cursor_y = 0;
	proc->proc_outputinf->attrib = 0x07;
	lockobj_init(&proc->proc_outputinf->output_lock);

	// Initializing file table information	
	proc->proc_fileinf->current_dir = get_curdir(); 
	if (proc->proc_fileinf->current_dir != NULL)
	{
		proc->proc_fileinf->current_dir->v_count++;
		release_vnode((struct vnode *)proc->proc_fileinf->current_dir);
	}
	proc->proc_fileinf->root_dir = get_rootdir(); 
	if (proc->proc_fileinf->root_dir != NULL)
	{
		proc->proc_fileinf->root_dir->v_count++;
		release_vnode((struct vnode *)proc->proc_fileinf->root_dir);
	}
	// Increment the reference count of the current directory

	for (i=0; i<MAXPROCFILES; i++)
	{
		proc->proc_fileinf->fhandles[i] = current_process->proc_fileinf->fhandles[i];
		// Increment the reference count of the open files 
		if (proc->proc_fileinf->fhandles[i] >= 0)
			increment_ref_count(proc->proc_fileinf->fhandles[i]);
	}
	lockobj_init(&proc->proc_fileinf->fhandles_lock);


	// Initialize other pointers of the process
	lockobj_init(&proc->proc_timerobjlock);
	proc->proc_timedout = NULL;
	lockobj_init(&proc->proc_thrlock);
	proc->proc_threadptr = NULL;
	proc->proc_parent = (struct process *)current_process;
	lockobj_init(&proc->proc_childlistlock);
	eventobj_init(&proc->proc_childtermevent);
	proc->proc_childlist = NULL;
	_lock((struct lock_t *)&current_process->proc_childlistlock);
	proc->proc_sib = current_process->proc_childlist;
	current_process->proc_childlist = proc;
	unlock((struct lock_t *)&current_process->proc_childlistlock);
	proc->proc_prev = NULL;
	_lock(&used_proclock);
	if (used_processes != NULL) used_processes->proc_prev = proc;
	proc->proc_next = (struct process *)used_processes;
	used_processes = proc;
	unlock(&used_proclock);

	sprintf(thrname,"%s#%d",proc->proc_name,tparam->mainthrno);
	if ((ret = create_thread(proc, thrname, scid, priority, tparam->ss3_start + tparam->ss3_size , tparam->start_routine, tparam->mainthrno)) < 0)
		terminate_process(proc);
	
	return ret;
}

int create_thread(struct process *proc, char thrname[], int scid, int priority, unsigned long stck_top, unsigned long start_routine, short threadno)
{
	int thrslot;
	struct kthread *thr;
	struct segdesc_s * gdtp = (struct segdesc_s *)gdt_table;

	thrslot = alloc_task_index();
	if (thrslot < 0) return EMAXTHREADS;
	thr = alloc_thread();
	if (thr == NULL)
	{
		free_task_index(thrslot);
		return ENOMEM;
	}
	
	// Name
	strncpy(thr->kt_name, thrname, 32);
	thr->kt_threadid = threadno; // Thread no
	thr->kt_type = proc->proc_type;  // Type of the thread

	thr->kt_tssdesc = thrslot * 8;
	// TSS initialization
	bzero((char *)(thr->kt_tss), sizeof(struct TSS));
	thr->kt_tss->esp0 = (unsigned int)(&(thr->kt_kstack[KERNEL_STACK_SIZE/4 -1]))-1; 
	thr->kt_tss->ss0 = KERNEL_DS;
	thr->kt_tss->cr3 = (unsigned int)(proc->proc_vm->vm_pgdir);
	thr->kt_tss->eip = start_routine;
	thr->kt_tss->eflags = initial_eflags;
	if (proc->proc_type != PROC_TYPE_SYSTEM)
	{
		thr->kt_tss->esp = stck_top - 16;
		thr->kt_tss->es = 0x0f;
		thr->kt_tss->cs = 0x07;
		thr->kt_tss->ss = 0x0f;
		thr->kt_tss->ds = 0x0f;
		thr->kt_tss->fs = 0x0f;
		thr->kt_tss->gs = 0x0f;
		thr->kt_ustackstart = stck_top-1;
	}
	else
	{
		thr->kt_tss->esp = thr->kt_tss->esp0; // stck_top - 16;
		thr->kt_tss->es = 0x10;
		thr->kt_tss->cs = 0x08;
		thr->kt_tss->ss = 0x10;
		thr->kt_tss->ds = 0x10;
		thr->kt_tss->fs = 0x10;
		thr->kt_tss->gs = 0x10;
		thr->kt_ustackstart = 0;	// Only kernel stack
	}
	thr->kt_tss->ldtsel = (LDTDESC_START + proc->proc_ldtindex) * 8;
	thr->kt_tss->iomapbase = sizeof(struct TSS);

	initialize_dataseg(&gdtp[thrslot], (unsigned int)(thr->kt_tss), sizeof(struct TSS), INTR_PRIVILEGE);
	gdtp[thrslot].access = PRESENT | (INTR_PRIVILEGE << DPL_SHIFT) | TSS_TYPE;
	
	bcopy((char *)&initial_fpustate, (char *)&(thr->kt_fps), sizeof(initial_fpustate));
	thr->kt_cr0 = 0x80050033;
	// Sceduling information
	thr->kt_schedinf.sc_cid = scid; // Default round robin type.
	thr->kt_schedinf.sc_usrpri = priority; // Nice value
	if (scid == SCHED_CLASS_SYSTEM) 
		thr->kt_schedinf.sc_cpupri = priority;
	if (scid == SCHED_CLASS_USER) 
		thr->kt_schedinf.sc_cpupri = RR_PRIORITY_MIN;
	if (scid == SCHED_CLASS_INTERRUPT) 
		thr->kt_schedinf.sc_cpupri = IT_PRIORITY_MIN;
	thr->kt_schedinf.sc_inhpri = 0;
	thr->kt_schedinf.sc_state = THREAD_STATE_READY;
	thr->kt_schedinf.sc_tqleft = TIME_QUANTUM;
	thr->kt_schedinf.sc_totcpuusage = 0;
	thr->kt_schedinf.sc_slpstart = 0;
	thr->kt_schedinf.sc_thrreadystart = 0;
	thr->kt_schedinf.sc_clfuncs = &SCHED_CLASS_SYSTEM_FUNCS;

	/*
	 * Initialize socket structure
	 */
	if (thr->kt_type == PROC_TYPE_PARALLEL) // Default socket required
	{
		thr->kt_defsock = create_defsock((proc->proc_id << 16) + threadno);
	}
	else thr->kt_defsock = create_defsock((proc->proc_id << 16) + threadno); //?

	thr->kt_terminatelock = 0;
	lockobj_init(&(thr->kt_exitlock));	
	eventobj_init(&(thr->kt_threadexit));	
	thr->kt_exitstatus = NULL;

	thr->kt_threadjoinable = 1;

	thr->kt_synchobjlock = 0;
	thr->kt_synchobjlist = NULL;
	thr->kt_slpchannel = NULL;
	thr->synchtry = 0;
	thr->synchobtained = 0;

	// initilize this thread related pointers.
	thr->kt_process = proc;
	_lock(&proc->proc_thrlock); // Locked at higher levels
	thr->kt_sib = proc->proc_threadptr;
	proc->proc_threadptr = thr;
	unlock(&proc->proc_thrlock);

	_lock(&used_threadlock);
	thr->kt_uselist = (struct kthread *)used_threads;
	used_threads = thr;
	unlock(&used_threadlock);

	/*
	 * Add this thread to ready_q.
	 */
	add_readyq(thr);
	return 0;
}

void add_readyq(struct kthread *thr)
{
	unsigned long oflags;
	int i;

	// Insert at the ending of ready_q 
	thr->kt_qnext = NULL;
	thr->kt_slpchannel = NULL;

	CLI;
	spinlock(ready_qlock);
	i = (thr->kt_schedinf.sc_cpupri > thr->kt_schedinf.sc_inhpri) ? ((159 - thr->kt_schedinf.sc_cpupri) / 5) : ((159 - thr->kt_schedinf.sc_inhpri) / 5);
	thr->kt_schedinf.sc_state = THREAD_STATE_READY;
	thr->kt_qprev = (struct kthread *)ready_qtail[i];
	if (ready_qtail[i] != NULL)
	{
		ready_qtail[i]->kt_qnext = thr;
		ready_qtail[i] = thr;
	}
	else ready_qhead[i] = ready_qtail[i] = thr;
	spinunlock(ready_qlock);
	STI;
	if (((current_thread->kt_schedinf.sc_cpupri < thr->kt_schedinf.sc_cpupri)
	 &&(current_thread->kt_schedinf.sc_inhpri < thr->kt_schedinf.sc_cpupri))
         ||((current_thread->kt_schedinf.sc_cpupri < thr->kt_schedinf.sc_inhpri)
	 &&(current_thread->kt_schedinf.sc_inhpri < thr->kt_schedinf.sc_inhpri)))
	runrun = 1;
}

int del_readyq(struct kthread *thr)
{
	unsigned long oflags;
	int i;
	int ret;

	CLI;
	spinlock(ready_qlock);
	i = (thr->kt_schedinf.sc_cpupri > thr->kt_schedinf.sc_inhpri) ? ((159 - thr->kt_schedinf.sc_cpupri) / 5) : ((159 - thr->kt_schedinf.sc_inhpri) / 5);
	if (thr->kt_schedinf.sc_state == THREAD_STATE_READY) // In ready q
	{
		if (thr->kt_qnext != NULL) (thr->kt_qnext)->kt_qprev = thr->kt_qprev;
		else ready_qtail[i] = thr->kt_qprev;

		if (thr->kt_qprev != NULL) (thr->kt_qprev)->kt_qnext = thr->kt_qnext;
		else ready_qhead[i] = thr->kt_qnext;
		
		thr->kt_qnext = NULL;
		thr->kt_qprev = NULL;
		ret = 1;
	}
	else ret = 0;
	spinunlock(ready_qlock);
	STI;

	return ret;
}

void thread_exit(void) { terminate_thread((struct kthread *)current_thread); }

int terminate_thread(struct kthread *t)
{
	struct kthread *t1; 
	unsigned long oflags;
	int status;

	// First get the termination lock on the thread so that multiple 
	// attempts to terminate the thread can be avoided.
	CLI;
	spintrylock(t->kt_terminatelock, status);
	if (status == -1) { STI;  return -1; }// Already in progress
	// Remove it from the used list
	_lock(&used_threadlock);
	if (used_threads == t) used_threads = t->kt_uselist;
	else
	{
		t1 = (struct kthread *)used_threads;
		while (t1 != NULL && t1->kt_uselist != t) t1 = (struct kthread *)t1->kt_uselist;

		if (t1 != NULL) t1->kt_uselist = t->kt_uselist;
	}
	unlock(&used_threadlock);

	if (t == current_thread)
	{
		// Add this to terminated threads list, that are post 
		// processed by termination handler
		// and context switch 
		_lock(&termthr_lock);

		spinlock(terminatehandler_event.e_slq.sl_bitval);
		spinlock(termthr_lock.l_slq.sl_bitval);
		t->kt_uselist = (struct kthread *)terminated_threads;
		terminated_threads = t;	
		t->kt_schedinf.sc_state = THREAD_STATE_TERMINATING;
		sys_event_wakeup(&terminatehandler_event);
		spinunlock(terminatehandler_event.e_slq.sl_bitval);
		sys_unlock((struct lock_t *)&termthr_lock);
		spinunlock(termthr_lock.l_slq.sl_bitval);
		scheduler(); // Never returns back here.
			// Useless
		// Actually when context is switched then current_thread is 
		// changed. That value can be used by the termination 
		// handling thread to ensure that really the thread has 
		// completed its termination. Before that it should not cliam 
		// this thread's resources.
	}

	// OTHERWISE: ONE THREAD IS TERMINATING ANOTHER
	// It is very critical, that the thread may be sleeping, or ready 
	// or even running. Until it is found successfully either on some
	// sleep channel or in the ready q this should go on trying to find it.
	while (1)
	{
		// WE SHOULD WAIT IF THE THREAD IS IN RUNNING STATE 
		// (POSSIBLE IF MULTIPROCESSOR SYSTEM) - Not done now.
		spinlock(t->kt_synchobjlock);
		if (t->kt_schedinf.sc_state == THREAD_STATE_READY && t->kt_process->proc_state == PROC_STATE_READY)
		{
			// Available on the ready queue. remove it safely
			del_readyq(t);
			spinunlock(t->kt_synchobjlock);
			break;
		}
		else if (t->kt_schedinf.sc_state == THREAD_STATE_SLEEP)
		{
			
			if (t->kt_slpchannel != NULL)
			{
				// IMPORTANT, in order to avoid deadlock
				spintrylock((t->kt_slpchannel->sl_bitval), status);
				if (status == -1)
				{
					spinunlock(t->kt_synchobjlock);
					continue; 
				}
				// Check if it is still on the sleep channel
				t1 = (struct kthread *)t->kt_slpchannel->sl_head;
				while (t1 != NULL && t1 != t)
					t1 = (struct kthread *)t1->kt_qnext;
				if (t1 == NULL) // Not found
				{
					// Release the locks and again try to find it
					spinunlock(t->kt_slpchannel->sl_bitval);
					spinunlock(t->kt_synchobjlock);
					continue; 
				}
				else // found
				{
					// Remove it from the sleep channel
					if (t->kt_qnext != NULL)
						(t->kt_qnext)->kt_qprev = t->kt_qprev;
					else
						t->kt_slpchannel->sl_tail = t->kt_qprev;
					if (t->kt_qprev != NULL)
						(t->kt_qprev)->kt_qnext = t->kt_qnext;
					else
						t->kt_slpchannel->sl_head = t->kt_qnext;
					spinunlock(t->kt_slpchannel->sl_bitval);
					spinunlock(t->kt_synchobjlock);
					break;
				}
				
			}
			else
			{
				spinunlock(t->kt_synchobjlock);
			}

		}
		else
		{
			// It cannot be found in a proper state???
			spinunlock(t->kt_synchobjlock);
		}
	}
	STI;
	// Add this to terminated threads list, that are post 
	// processed by termination handler
	_lock(&termthr_lock);

	t->kt_uselist = (struct kthread *)terminated_threads;
	terminated_threads = t;	
	t->kt_schedinf.sc_state = THREAD_STATE_TERMINATING;

	event_wakeup(&terminatehandler_event);
	unlock(&termthr_lock);

	return 0;
	// No need of unlocking kt_terminatelock
	
	// The termination handler then completes other task such as 
	// releasing of resources owned by this thread and adding that to 
	// unused list of threads depending on the joinability
}

// This is going to release all resources owned by the thread
// and also determines the need of termination of process, (if all threads are
// terminated)
void thread_terminate_handler(void)
{
	struct kthread *t, *t1, *t2 = NULL;
	struct process *proc, *p1, *p2, *p3;
	int term_proc_flag;
	int i;
	struct sleep_q *synchobj;

	while (1)
	{
		_lock(&termthr_lock);
		t = (struct kthread *)terminated_threads;
		if (t == NULL)
		{
			event_sleep(&terminatehandler_event, &termthr_lock);
			unlock(&termthr_lock);
			continue;
		}

		terminated_threads = t->kt_uselist;
		unlock(&termthr_lock);

		// Release the resources of the thread t
		proc = (struct process *)t->kt_process;

		// Close the default socket if exists
		if (t->kt_defsock != NULL) delete_socket(t->kt_defsock);

		// Add this threads cumulative time to corresponding process 
		// proc->proc_totalcputime += t->kt_schedinf.total_cpu_usage;

		// free the task descriptor
		free_task_index(t->kt_tssdesc / 8);

		// Release all the owned synch objects
		// No longer synchobjlock is required, because no other
		// thread is manipulating this list
		while (t->kt_synchobjlist != NULL)
		{
			synchobj = (struct sleep_q *)t->kt_synchobjlist;
			t->kt_synchobjlist = synchobj->sl_next;
			releasesynch_object(t, synchobj);
		}

		// The user stack allotted for this thread is no longer in use.
		if (t->kt_ustackstart >= 0)
			vm_delregionend(proc->proc_vm,  t->kt_ustackstart);

		if (t->kt_threadjoinable == 0)
		{
			// Remove from sibling list
			_lock(&proc->proc_thrlock);
			if (proc->proc_threadptr == t)
				proc->proc_threadptr = t->kt_sib;
			else
			{
				t1 = (struct kthread *)proc->proc_threadptr;
				while (t1 != NULL && t1->kt_sib != t) t1 = (struct kthread *)t1->kt_sib;
				if (t1 != NULL) t1->kt_sib = t->kt_sib;
			}
			unlock(&proc->proc_thrlock);

			t->kt_sib = NULL;

			destroy_thread(t);
		}
		else
		{
			// Don't release the memory of thread
			_lock(&t->kt_exitlock);
			t->kt_schedinf.sc_state = THREAD_STATE_TERMINATED;
			event_wakeup(&(t->kt_threadexit));
			unlock(&t->kt_exitlock);
		}

		// Check if the corresponding process must be terminated.
		term_proc_flag = 1;
		
		_lock(&proc->proc_thrlock);
		t1 = (struct kthread *)proc->proc_threadptr;
		
		while (t1 != NULL)
		{
			if (t1->kt_schedinf.sc_state != THREAD_STATE_TERMINATED)
			{
				term_proc_flag = 0;
				break;
			}
			else t1 = (struct kthread *)t1->kt_sib;
		}
		unlock(&proc->proc_thrlock);

		if (term_proc_flag == 1)
		{
			// Release the process owned resources 
			// Release all local threads and remote thread memory blocks
			t1 = (struct kthread *)proc->proc_threadptr;
			while (t1 != NULL)
			{
				t2 = t1;
				t1 = (struct kthread *)t1->kt_sib;
				destroy_thread(t2);
			}

			/*
			 * Close all open file handles
			 */
			for (i=0; i<MAXPROCFILES; i++)
				if (proc->proc_fileinf->fhandles[i] >= 0)
					syscall_close(i);

			if (proc->proc_fileinf->current_dir != NULL)
			{
				get_vnode((struct vnode *)proc->proc_fileinf->current_dir);
				proc->proc_fileinf->current_dir->v_count--;
				release_vnode((struct vnode *)proc->proc_fileinf->current_dir);
			}
			if (proc->proc_fileinf->root_dir != NULL)
			{
				get_vnode((struct vnode *)proc->proc_fileinf->root_dir);
				proc->proc_fileinf->root_dir->v_count--;
				release_vnode((struct vnode *)proc->proc_fileinf->root_dir);
			}

			// Release memory and memory management information
			vm_destroy(proc->proc_vm);

			// Set the child processes parent field point to 
			// parent of this if they are live. 
			// Otherwise release their memory or add to unused list 
			_lock((struct lock_t *)&(proc->proc_parent->proc_childlistlock));	
			_lock(&proc->proc_childlistlock);
			if (proc->proc_childlist != NULL)
			{
				p1 = (struct process *)proc->proc_childlist;
				p2 = NULL;
				while (p1 != NULL)
				{
					_lock(&p1->proc_exitlock);
					if (p1->proc_state == PROC_STATE_TERMINATED)
					{
						// Remove and destroy p1
						if (p2 == NULL) // First node
							proc->proc_childlist = p1->proc_sib;
						else
							p2->proc_sib = p1->proc_sib;
						unlock(&p1->proc_exitlock);
						p3 = p1;
						p1 = (struct process *)p1->proc_sib;
						destroy_process(p3);
					}
					else
					{
						// Live change the parent
						p1->proc_parent = proc->proc_parent;
						unlock(&p1->proc_exitlock);
						p2 = p1;
						p1 = (struct process *)p1->proc_sib;
					}
				}
				if (p2 != NULL)
				{
					p2->proc_sib = (proc->proc_parent)->proc_childlist;
					(proc->proc_parent)->proc_childlist = proc->proc_childlist;
				}
				// else parents child list not modified
				proc->proc_childlist = NULL;
			}
			unlock(&proc->proc_childlistlock);
			// Check if this is a waitable process
			// Wake up the waiting process
			if (proc->proc_waitable == 1)
			{
				_lock(&proc->proc_exitlock);
				proc->proc_state = PROC_STATE_TERMINATED;
				event_wakeup(&proc->proc_exitevent);
				unlock(&proc->proc_exitlock);
				// Inform to parent
				event_wakeup((struct event_t *)&proc->proc_parent->proc_childtermevent);
			}
			else
			{
				// Otherwise remove it from the child process 
				// list of its parent.

				p2 = (struct process *)proc->proc_parent->proc_childlist;
				p1 = NULL;
				while (p2 != proc)
				{
					p1 = p2;
					p2 = (struct process *)p2->proc_sib;
				}

				if (p1 == NULL)
					proc->proc_parent->proc_childlist = proc->proc_sib;
				else p1->proc_sib = proc->proc_sib;
				proc->proc_sib = NULL;
				destroy_process(proc);
			}
			unlock((struct lock_t *)&(proc->proc_parent->proc_childlistlock));	
		}
	}
}

// This is called from other process to terminate a process.
// If it is self termination do not allow.
void terminate_process(struct process *p)
{
	struct kthread *t1, *t2;

	// Change the keyboard focus
	_lock(&kbd_wait_lock);
	if (kbd_focus == p)
	{
		change_screen((struct process *)p->proc_parent);
		kbd_focus = p->proc_parent;
	}
	unlock(&kbd_wait_lock);

	// Terminate all threads of p, this implies termination of
	// process. Really will be completed by termination handler later
	_lock(&p->proc_thrlock);
	t1 = (struct kthread *)p->proc_threadptr;
	while (t1 != NULL)
	{
		t2 = t1;
		t1 = (struct kthread *)t1->kt_sib;
		if (t2 != current_thread && t2->kt_schedinf.sc_state != THREAD_STATE_TERMINATING && t2->kt_schedinf.sc_state != THREAD_STATE_TERMINATED)  
			terminate_thread(t2);
	}
	unlock(&p->proc_thrlock);
	// Current thread is not yet terminated, if this is the calling thread
	if (current_thread->kt_process == p)
		terminate_thread((struct kthread *)current_thread);

	return;
}

static int alloc_task_index(void)
{
	int i;

	// second 4 - bytes can never be zero when a segment is in use.
	// This feature is used to identify free task index.
	_lock(&gdt_lock);
	for (i = TASKDESC_START; i<= TASKDESC_END; i++)
		if (gdt_table[i].b == 0) break;
	if (i <= TASKDESC_END) gdt_table[i].b = 1;
	else	i = -1;
	unlock(&gdt_lock);

	return i;
}

void initialize_tss_table(void)
{
	int i;
	lockobj_init(&tsstable_lock);	
	for (i=0; i<MAXTHREADS; i++) tss_table[i].res1 = 0xffff; // To indicate free slot.
}

static int alloc_tss(void)
{
	int i;

	_lock(&tsstable_lock);
	for (i=0; i<MAXTHREADS; i++)
	{
		if (tss_table[i].res1 == 0xffff) // free
		{
			tss_table[i].res1 = 0;
			break;
		}
	}
	unlock(&tsstable_lock);
	if (i < MAXTHREADS) return i;
	else return -1;
}

static int alloc_ldt_index(void)
{
	int i;

	// second 4 - bytes can never be zero when a segment is in use.
	// This feature is used to identify free ldt index.
	_lock(&ldt_lock);
	for (i = 0; i< MAXPROCESSES; i++)
		if (ldt_table[i].code.b == 0) break;
	if (i < MAXPROCESSES) ldt_table[i].code.b = 1;
	else	i = -1;
	unlock(&ldt_lock);

	return i;
}

static int free_task_index(int ind) 
{ 
	gdt_table[ind].b = 0; 
	return 0;
}
static int free_ldt_index(int ind) 
{ 
	ldt_table[ind].code.b = 0; 
	return 0;
}

static struct process * alloc_process(void)
{
	struct process *proc;
	struct output_info *outinf;
	struct input_info *ininf;
	struct files_info *fileinf;

	_lock(&unused_proclock);
	if (unused_processes != NULL)
	{
		proc = (struct process *)unused_processes;
		unused_processes = proc->proc_next;
		if (unused_processes != NULL)
			unused_processes->proc_prev = NULL;
		unusedproc_count--;
		proc->proc_next = NULL;
		unlock(&unused_proclock);
		bzero((char *)proc->proc_inputinf, sizeof(struct input_info));
		bzero((char *)proc->proc_outputinf, sizeof(struct output_info));;
		bzero((char *)proc->proc_fileinf, sizeof(struct files_info));;
		return proc;
	}	
	unlock(&unused_proclock);

	// Otherwise allocate new block of process memory
	proc = (struct process *) kmalloc(sizeof(struct process));
	outinf = (struct output_info *) kmalloc(sizeof(struct output_info));
	ininf = (struct input_info *) kmalloc(sizeof(struct input_info));
	fileinf = (struct files_info *) kmalloc(sizeof(struct files_info));

	if (proc == NULL || outinf == NULL || ininf == NULL || fileinf == NULL) // Memory allocation problem
	{
		if (proc != NULL)
			kfree(proc);
		if (outinf != NULL)
			kfree(outinf);
		if (ininf != NULL)
			kfree(ininf);
		if (fileinf != NULL)
			kfree(fileinf);
		printk("Malloc failure alloc process\n");
		return NULL;
	}
	proc->proc_inputinf = ininf;
	proc->proc_outputinf = outinf;
	proc->proc_fileinf = fileinf;

	bzero((char *)proc->proc_inputinf, sizeof(struct input_info));
	bzero((char *)proc->proc_outputinf, sizeof(struct output_info));;
	bzero((char *)proc->proc_fileinf, sizeof(struct files_info));;
	return proc;
}

static void destroy_process(struct process *proc)
{
	// relase the process memory or add to unused process list
	_lock(&unused_proclock);
	if (unusedproc_count < UNUSEDPROC_COUNTMAX)
	{
		// Add this process to unused process list
		proc->proc_next = (struct process *)unused_processes;
		proc->proc_prev = NULL;
		if (unused_processes != NULL)
			unused_processes->proc_prev = proc;
		unused_processes = proc;
		unusedproc_count++;
	}
	else
	{
		// Free the memory
		// Release files info, output info and input info
		kfree(proc->proc_outputinf);
		kfree(proc->proc_inputinf);
		kfree(proc->proc_fileinf);
		kfree(proc);
	}
	unlock(&unused_proclock);
}

struct kthread *alloc_thread(void)
{
	struct kthread *thr;
	int i;

	if ((i = alloc_tss()) < 0) return NULL;

	_lock(&unused_threadlock);
	if (unused_threads != NULL)
	{
		thr = (struct kthread *)unused_threads;
		unused_threads = thr->kt_qnext;
		if (unused_threads != NULL)
			unused_threads->kt_qprev = NULL;
		unusedthr_count--;
		thr->kt_qnext = NULL;
		unlock(&unused_threadlock);
		thr->kt_tss = &tss_table[i];
		return thr;
	}	
	unlock(&unused_threadlock);

	thr = ((struct kthread *) kmalloc(sizeof(struct kthread)));

	if (thr != NULL) thr->kt_tss = &tss_table[i];
	else 
	{
		printk("Malloc failure in alloc_thread\n");
		tss_table[i].res1 = 0xffff; // Release tss
	}

	return thr;
}

void destroy_thread(struct kthread *t)
{
	t->kt_tss->res1 = 0xffff; // Release tss

	// Add this to unused list of threads
	
	_lock(&unused_threadlock);
	if (unusedthr_count < UNUSEDTHR_COUNTMAX)
	{
		t->kt_qprev = NULL;
		t->kt_qnext = (struct kthread *)unused_threads;
		unused_threads = t;
		unusedthr_count++;
		t->kt_schedinf.sc_state = THREAD_STATE_TERMINATED;
	}
	else kfree(t);
			
	unlock(&unused_threadlock);
}

void halt_system(void)
{	
	struct process *p;

	/*
	 * Kill all user tasks.
	 */
	_lock(&used_proclock);
	p = (struct process *)used_processes;
	while (p != NULL)
	{
		if (p->proc_type != PROC_TYPE_SYSTEM && (p->proc_state == PROC_STATE_READY || p->proc_state == PROC_STATE_SUSPENDED))
			terminate_process(p);

		p = (struct process *)p->proc_next;
	}
	unlock(&used_proclock);

	// Synching must be done here
	syscall_sync(0);	
	printk("Now the system can be switched off....");
	return;
}

void suspend(struct process *proc)
{
	unsigned long oflags;
	struct kthread *t1, *t2;
	int i;

	_lock(&used_proclock);
	_lock(&suspended_proclock);
	_lock(&proc->proc_exitlock);
	CLI;
	spinlock(ready_qlock);
	proc->proc_state = PROC_STATE_SUSPENDED;
	// Remove from used proc list
	if (proc->proc_prev != NULL)
		(proc->proc_prev)->proc_next = proc->proc_next;
	else
		used_processes = proc->proc_next;

	if (proc->proc_next != NULL)
		(proc->proc_next)->proc_prev =  proc->proc_prev;

	// Add this to supended process list
	proc->proc_prev = NULL;
	proc->proc_next = (struct process *)suspended_processes;
	if (suspended_processes != NULL)
		suspended_processes->proc_prev = proc;
	suspended_processes = proc;

	// Remove all ready threads from the ready queue;
	for (i=0; i<MAXREADYQ; i++)
	{
	t1 = (struct kthread *)ready_qhead[i];
	while (t1 != NULL)
	{
		if (t1->kt_process == proc)
		{
			// Remove it
			t2 = (struct kthread *)t1->kt_qnext;

			if (t1->kt_qnext != NULL)
				(t1->kt_qnext)->kt_qprev = t1->kt_qprev;
			else ready_qtail[i] = t1->kt_qprev;

			if (t1->kt_qprev != NULL)
				(t1->kt_qprev)->kt_qnext = t1->kt_qnext;
			else ready_qhead[i] = t1->kt_qnext;
			
			t1->kt_qnext = NULL;
			t1->kt_qprev = NULL;
		
			t1 = t2;	
		}
		else t1 = (struct kthread *)t1->kt_qnext;
	}
	}
	spinunlock(ready_qlock);
	STI;
	unlock(&proc->proc_exitlock);
	unlock(&suspended_proclock);
	unlock(&used_proclock);
}


void wakeup(struct kthread *t)
{
	t->kt_schedinf.sc_clfuncs->CL_WAKEUP(t);

	if (t->kt_process->proc_state == PROC_STATE_READY)
	{
		t->kt_schedinf.sc_state = THREAD_STATE_READY;
		add_readyq(t);
	}
	// Otherwise do not add this to ready queue.
	return;
}

void initialize_ldt(struct LDT *ldt, struct process_param *tparam, int type) 
{	
	int priv;

	if (type == PROC_TYPE_SYSTEM) priv = 0;
	else priv = 3;

	initialize_codeseg((struct segdesc_s *)&ldt->code, 0, tparam->cs_start+tparam->cs_size, priv);
	initialize_dataseg((struct segdesc_s *)&ldt->data, 0, USERSTACK_SEGMENT_END + PRIMALLOC_MAX, priv); // Even for data segment same range as stack
	initialize_dataseg((struct segdesc_s *)&ldt->stack_user, 0, USERSTACK_SEGMENT_END + PRIMALLOC_MAX, priv);
}


int create_thread_in_proc(struct process *proc, void *attr, void * (*start_routine)(void *), void *arg, unsigned int stackstart, unsigned int initialstacksize, int thrid)
{
	char *kstack = NULL;
	int *iptr;
	unsigned int old_cr0;
	char name[40]; // > 32 for safety
	unsigned int roundedsize;
	int ret;

	// allocate thread memory
	old_cr0 = clear_paging();

	roundedsize = ((initialstacksize + 4095) / 4096) * 4096; 
	// For stack use of rounded size is OK, but not for data segment
	if (vm_addregion(proc->proc_vm, SEGMENTTYPE_STACK, stackstart, stackstart + roundedsize - 1, 1, 1) != 0)
	{
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
		return ENOMEM;
	}

	kstack = (char *)stackstart;

	// Initialize the stack
	iptr = (int *)logical2physical(proc->proc_vm->vm_pgdir, kstack + roundedsize-4);
	iptr[0] = NULL;
	iptr[-1] = NULL;
	iptr[-2] = (int)arg;
	iptr[-3] = (unsigned int)USERCODE_SEGMENT_START; // pthread_exit;
	restore_paging(old_cr0, current_thread->kt_tss->cr3);
	
	
	if (attr != NULL)
	{
		// TODO: Process the attributes
	}

	sprintf(name,"%s#%d",proc->proc_name,thrid);

	// create_thread(...) also locks the proc_thrlock, so unlock it here
	unlock(&proc->proc_thrlock);	
	ret = create_thread(proc, name, SCHED_CLASS_USER, 0, (unsigned int)(kstack + roundedsize), (unsigned int)start_routine, thrid);
	_lock(&proc->proc_thrlock);	
	return ret;
}

static unsigned short int get_global_processno(char name[], int type) // Temporary
{
	static unsigned short int procno=32000;
	static volatile unsigned short procno_lock = 0;
	short int p;
	unsigned long oflags;

	if (type != PROC_TYPE_PARALLEL)
	{
		CLI;
		spinlock(procno_lock);
		p = procno++;
		spinunlock(procno_lock);
		STI;
		return p;
	}
	else	return -1;
}

void sort_hdrs(Elf32_Shdr *hdrs, int n)
{
	Elf32_Shdr t;
	int i,j;

	for (i=0; i<n-1; i++)
	{
		for (j=i+1; j<n; j++)
		{
			if (hdrs[i].sh_addr > hdrs[j].sh_addr)
			{
				t = hdrs[i];
				hdrs[i] = hdrs[j];
				hdrs[j] = t;
			}
		}
	}
}

void display_file(void);
int exectask_internal(char pathname[], char *argv[], char *envp[], int type, int local, void *(* start_routine)(void *), void *arg, unsigned int dssize, unsigned int stack_start, unsigned int initial_stacksize, int pno, int thrno, int srcmachine, int mctype, int commslot)
{
	int fd;
	Elf32_Shdr hdrs[15];
	int hdrcount;
	int sec_sizes[3], len;
	int rdsize, actsize, totalsizeread, arg_count; //, datasectionsize=0;
	unsigned long phys_addr, logical_envpage;
	unsigned long rounded_addr, space_inframe;
	unsigned int *user_stack_end;
	char *args_envp, *args, *env;
	char **penvp, **pargv, *phys_argv, *phys_env;
	char **args_ptrs, **env_ptrs;
	int i, j;
	char name[32];
	unsigned long old_cr0;
	unsigned long startaddr, curaddr;
	int err;
	struct process_param tp;
	Elf32_Ehdr ehdr;
	Elf32_Shdr shdr;
	struct vm *vmem;
	PTE *pte;
	PDE *pde;
	void print_lock(struct lock_t *lck);

	// Extract the file/command name (to be used as process name)
	i = strlen(pathname);
	while (i > 0 && pathname[i] != '/' && pathname[i] != '\\' && pathname[i] != ':') i--;
	if (i>0) i++;
	j = 0;
	while (j < 32 && pathname[i] != 0) name[j++] = toupper(pathname[i++]);
	name[j] = 0;	
	fd = syscall_open(pathname, O_RDONLY, 0);
	
	pte = vm_getptentry(current_process->proc_vm, (((unsigned int)0x200050ed) >> 12) & 0x000fffff);
	pde = vm_getpdentry(current_process->proc_vm, (((unsigned int)0x200050ed) >> 12) & 0x000fffff);
	if (fd < 0) return fd; // File not found.

	// Read the signature
	syscall_read(fd, &ehdr, sizeof(ehdr));
	if (ehdr.e_type != 2) // Not executable file
	{
		err = EEXECSIGNATURE_NOTFOUND;
		syscall_close(fd);
		goto err_execsignature;
	}

	// Read the sections info

	sec_sizes[0] = sec_sizes[1] = sec_sizes[2] = 0;
	syscall_lseek(fd, ehdr.e_shoff, SEEK_SET);	
	if (ehdr.e_shnum > 15)
		printk("Error : More number of sections than supoorted in executable file\n");
	// Read all headers limited to a miximum of 15
	for (i=0, hdrcount=0; i<ehdr.e_shnum && hdrcount < 15; i++)
	{
		syscall_read(fd, &hdrs[hdrcount], sizeof(shdr));
		if (hdrs[hdrcount].sh_flags != 0)
			hdrcount++;
		// Otherwise discard
	}

	sort_hdrs(hdrs, hdrcount);
	// Compute the space required for all together (code, data, stack)
	// Code
	startaddr = 0x20000000;
	curaddr = 0x20000000;
	for (i=0; i<hdrcount; i++)
	{
		if (hdrs[i].sh_type == 1)
		{
			if ((hdrs[i].sh_flags == 6) || (hdrs[i].sh_flags == 32) || (hdrs[i].sh_flags == 12))
			{
				if (i>0) // Check for alignment
					curaddr = ((curaddr + hdrs[i].sh_addralign - 1) / hdrs[i].sh_addralign) * hdrs[i].sh_addralign;
				curaddr += hdrs[i].sh_size;
			}
		}
	}

	sec_sizes[0] = curaddr - startaddr;
	
	// Data
	startaddr = 0x24000000;
	curaddr = 0x24000000;
	for (i=0; i<hdrcount; i++)
	{
		if (hdrs[i].sh_type == 1 || hdrs[i].sh_type == 8)
		{
			if (hdrs[i].sh_flags == 3)
			{
				if (i>0) // Check for alignment
					curaddr = ((curaddr + hdrs[i].sh_addralign - 1) / hdrs[i].sh_addralign) * hdrs[i].sh_addralign;
				curaddr += hdrs[i].sh_size;
			}
		}
	}

	sec_sizes[1] = curaddr - startaddr;	

	if ((sec_sizes[0] < 1) || (sec_sizes[1] < 0)) // No limit on size
	{
		err = EINVALID_SEGMENTTYPESIZE;
		syscall_close(fd);
		goto err_size;
	}

	sec_sizes[2] = initial_stacksize; // Includes env block

	
	old_cr0 = clear_paging(); // Disable paging
	// Allocate necessary memory, page tables ...
	if ((vmem = vm_createvm()) == NULL)
	{
		err = ENOMEM;
		syscall_close(fd);
		goto err_allocprocmemory;
	}
	
	err = vm_addregion(vmem, SEGMENTTYPE_CODE, USERCODE_SEGMENT_START, USERCODE_SEGMENT_START + sec_sizes[0] - 1, 1, 1);

	if (err == 0)
	{
		if (local == 1)
			err = vm_addregion(vmem, SEGMENTTYPE_DATA, USERDATA_SEGMENT_START, USERDATA_SEGMENT_START + sec_sizes[1] - 1, 1, 1);
		else
			err = vm_addregion(vmem, SEGMENTTYPE_DATA, USERDATA_SEGMENT_START, USERDATA_SEGMENT_START + sec_sizes[1] - 1, 1, 0);
	}

	if (err == 0)
		err = vm_addregion(vmem, SEGMENTTYPE_STACK, stack_start, stack_start + initial_stacksize - 1, 1, 1);
	
	if (err != 0)
	{
		vm_destroy(vmem);
		syscall_close(fd);
		goto err_allocprocmemory;
	}	

	// Read code and data sections from file
	for (i=0; i<hdrcount; i++)
	{
		// Initialize Bss section
		if ((local == 1) && (hdrs[i].sh_type == 8))
		{
			startaddr = hdrs[i].sh_addr;
			totalsizeread = 0;
			while (totalsizeread < hdrs[i].sh_size) // Until code section completed
			{
				phys_addr = (unsigned long)logical2physical(vmem->vm_pgdir, (char *)startaddr);
				rounded_addr = (phys_addr + 4096) & 0xfffff000;
				space_inframe = rounded_addr - phys_addr;
		
				rdsize = ((hdrs[i].sh_size - totalsizeread) > space_inframe) ? space_inframe : (hdrs[i].sh_size - totalsizeread);
				bzero((char *)phys_addr, rdsize);
				totalsizeread  += rdsize;
				startaddr += rdsize;
			}
		}
		// If not local process  & data segment, then do not read 
		if (((local == 0) && ((hdrs[i].sh_flags & 1) != 0)) ||
			(hdrs[i].sh_type == 8))continue;

		// Otherwise read the section
		startaddr = hdrs[i].sh_addr;
		totalsizeread = 0;
		syscall_lseek(fd, hdrs[i].sh_offset, SEEK_SET); 

		while (totalsizeread < hdrs[i].sh_size) // Until code section completed
		{
			phys_addr = (unsigned long)logical2physical(vmem->vm_pgdir, (char *)startaddr);
			rounded_addr = (phys_addr + 4096) & 0xfffff000;
			space_inframe = rounded_addr - phys_addr;
		
			rdsize = ((hdrs[i].sh_size - totalsizeread) > space_inframe) ? space_inframe : (hdrs[i].sh_size - totalsizeread);
			actsize = syscall_read(fd, (void *)phys_addr, rdsize);
			if (actsize < 0) // Read error
			{
				printk("Read error : %d\n",actsize);
				err = actsize; // Error code
				syscall_close(fd);
				goto err_fileread;
			}
			totalsizeread  += actsize;
			startaddr += actsize;
		}
	}

	// Successfully read  Close the file	
	syscall_close(fd);

	if (local == 1) // Add args and environment variables
	{
		// Prepare environment variables and 
		// arguments of the new program.
		// Bottom of the stack page is for environment block
		logical_envpage = USERSTACK_SEGMENT_END - 0x00001000;
		args_envp = logical2physical(vmem->vm_pgdir, (char *)logical_envpage);
		// In that page of memory copy arguments into first 2048 bytes
		// and copy environment varaibles into other half.
		args = args_envp;
		args_ptrs = (char **)(args_envp + 1648); // 100 pointers can be allocated.
		i = 0;
		pargv = (char **)logical2physical(current_process->proc_vm->vm_pgdir, (char *)argv);
		while (pargv[i] != NULL && i < 99)	
		{
			phys_argv = logical2physical(current_process->proc_vm->vm_pgdir, pargv[i]);
			len = strlen(phys_argv);
		
			// Large number or size of args, exceeding the limit ?
			if (args + len + 1 > args_envp + 1648)
			{
				err = EARGSLENGTH;
				goto err_argslength;
			}
			vm2physcopy(current_process->proc_vm, pargv[i], args, len+1);

			*args_ptrs = (char *) ((args - args_envp) + logical_envpage);

			args_ptrs++;
			args += (len + 1); // Including null character
			i++;
		}

		*args_ptrs = NULL;
		arg_count = i;

		env = args_envp + 2048;
		env_ptrs = (char **)(args_envp + 3696); // 100 pointers can be allocated.
		i = 0;
		penvp = (char **)logical2physical(current_process->proc_vm->vm_pgdir, (char *)envp);
		while (penvp[i] != NULL && i < 99)	
		{
			phys_env = logical2physical(current_process->proc_vm->vm_pgdir, penvp[i]);
			len = strlen(phys_env);
			
			// Large number or size of env vars, exceeding the limit ?
			if (env + len + 1 > args_envp + 3696)
			{
				err = EARGSLENGTH;
				goto err_argslength;
			}

			vm2physcopy(current_process->proc_vm, penvp[i], env, len+1);
		
			*env_ptrs = (char *)((env - args_envp) + logical_envpage);
			env_ptrs++;
			env += (len + 1); // Including null character
			i++;
		}
		*env_ptrs = NULL;

		// For Local process : Place the env_ptrs, args_pts, 
		// arg_count on the user stack and the exit system call 
		// address as the return address of the new process
		user_stack_end = (unsigned int *) logical2physical(vmem->vm_pgdir, (char *) (stack_start + initial_stacksize - 0x00001000));
		i = 0;
		user_stack_end[-1] = logical_envpage + 3696; // env ptrs
		user_stack_end[-2] = logical_envpage + 1648; // args ptrs
		user_stack_end[-3] = arg_count; // arg count
		user_stack_end[-4] = (unsigned int) USERCODE_SEGMENT_START; // pthread_exit; // default return, exit system call.
	}
	else
	{
		// For remote process place the argument on the stack
		user_stack_end = (unsigned int *) logical2physical(vmem->vm_pgdir, (char *) (stack_start + initial_stacksize - 4));
		user_stack_end[0] = 0; 
		user_stack_end[-1] = 0; 
		user_stack_end[-2] = (unsigned int)arg;
		user_stack_end[-3] = (unsigned int) USERCODE_SEGMENT_START; // default return, exit system call.
	}
		
	// enable paging
	restore_paging(old_cr0, current_thread->kt_tss->cr3);

	// Fill the process parameters and call create_process()
	tp.cs_start = USERCODE_SEGMENT_START;
	tp.cs_size = PAGE_ROUND_UP(sec_sizes[0]);
	tp.ds_start = USERDATA_SEGMENT_START;
	tp.ds_size = PAGE_ROUND_UP(sec_sizes[1]);
	tp.ss3_start = stack_start;
	if (local == 1)
	{
		tp.ss3_size = USER_STACK_SIZE-0x1000;
		tp.start_routine = ehdr.e_entry;
	}
	else
	{
		tp.ss3_size = initial_stacksize;
		tp.start_routine = (unsigned long)start_routine;
	}
	tp.mainthrno = thrno;
	tp.eflags = initial_eflags;
	tp.vminfo = vmem;

	if (pno == 0) // Local process
		pno = get_global_processno(name, type);
	err = create_process(&tp, name, type, mctype, 0, pno);
	if (err < 0)
		goto err_createprocess;
	
	return 0; // Success
	

err_createprocess:
err_argslength:
err_fileread:
	// Release all the process memory, pgdir, pgtables ...
	vm_destroy(vmem);

err_allocprocmemory:
	restore_paging(old_cr0, current_thread->kt_tss->cr3);
err_size:
err_execsignature:
	return err;
}
//To create a new process loaded from the given pathnamed file.
int syscall_exectask(char pathname[], char *argv[], char *envp[], int type)
{
	return exectask_internal(pathname, argv, envp, type, 1, NULL, NULL, 0, (USERSTACK_SEGMENT_END - USER_STACK_SIZE), USER_STACK_SIZE, 0, 0, this_hostid, 0, -1);
}

void syscall_exit(int status)
{
	struct kthread *kt1, *kt2;

	_lock((struct lock_t *)&current_process->proc_thrlock);	
	kt1 = (struct kthread *)current_process->proc_threadptr;

	while (kt1 != NULL)
	{
		kt2 = (struct kthread *)kt1->kt_sib;
		if (kt1 == current_thread) 
		{
			// Terminate it last.
			kt1 = kt2;
			continue; 
		}
		if (kt1->kt_schedinf.sc_state != THREAD_STATE_TERMINATING && kt1->kt_schedinf.sc_state != THREAD_STATE_TERMINATED)
		{
			kt1->kt_threadjoinable = 0;
			terminate_thread((struct kthread *)kt1);
		}
		kt1 = kt2;
	}
	current_process->proc_exitstatus = status;
	unlock((struct lock_t *)&current_process->proc_thrlock);	
	terminate_thread((struct kthread *)current_thread);
}

int syscall_getpids(int pid[], int n)
{
	int count=0;
	struct process *proc;

	if (n <= 0) return 0;

	_lock(&used_proclock);
	_lock(&suspended_proclock);
	proc = (struct process *)used_processes;
	while (proc != NULL && count < n)
	{
		pid[count++] = proc->proc_id;
		proc = (struct process *)proc->proc_next;
	}

	proc = (struct process *)suspended_processes;
	while (proc != NULL && count < n)
	{
		pid[count++] = proc->proc_id;
		proc = (struct process *)proc->proc_next;
	}

	unlock(&used_proclock);
	unlock(&suspended_proclock);
	return count;
}


int syscall_getprocinfo(int pid, struct uprocinfo *pinf)
{
	int i;
	struct process *proc = NULL;
	struct kthread *t;
	struct memregion *mreg;

	
	_lock(&used_proclock);
	_lock(&suspended_proclock);
	proc = (struct process *)used_processes;
	while (proc != NULL)
	{
		if (proc->proc_id == pid) // Matching found 
			break;
		proc = (struct process *)proc->proc_next;
	}

	// If no such pid, search suspended process list
	if (proc == NULL)
	{
		proc = (struct process *)suspended_processes;
		while (proc != NULL)
		{
			if (proc->proc_id == pid) // Matching found 
				break;
			proc = (struct process *)proc->proc_next;
		}
	}

	if (proc != NULL) _lock(&proc->proc_exitlock);
	unlock(&used_proclock);
	unlock(&suspended_proclock);
	if (proc == NULL) return EINVALIDPID;

	// Collect information about the process
	strcpy(pinf->uproc_name, (char *)proc->proc_name);
	pinf->uproc_type = proc->proc_type;
	pinf->uproc_id = proc->proc_id;
	pinf->uproc_ppid = ((proc->proc_parent != NULL) ? ((proc->proc_parent)->proc_id) : 0);
	pinf->uproc_starttimesec = proc->proc_starttimesec;
	pinf->uproc_starttimemsec = proc->proc_starttimemsec;
	// Compute total cpu time usage of the process
	pinf->uproc_totalcputimemsec = proc->proc_totalcputime; 
	pinf->uproc_nthreadslocal = 0;
	//pinf->uproc_nthreadsglobal = 0; // Currently not used (TO BE ...)

	_lock(&proc->proc_thrlock);
	t = (struct kthread *)proc->proc_threadptr;
	while (t != NULL)
	{
		if (t->kt_schedinf.sc_state != THREAD_STATE_TERMINATED && t->kt_schedinf.sc_state != THREAD_STATE_TERMINATING)
			pinf->uproc_nthreadslocal++;
		t = (struct kthread *)t->kt_sib;
	}

	unlock(&proc->proc_thrlock);
	pinf->uproc_status = proc->proc_state;

	// Count no of open files/Sockets
	_lock(&proc->proc_fileinf->fhandles_lock);
	pinf->uproc_nopenfiles = 0;
	for (i=0; i<MAXPROCFILES; i++)
	{
		if (proc->proc_fileinf->fhandles[i] >= 0)
			pinf->uproc_nopenfiles++;
	}
	unlock(&proc->proc_fileinf->fhandles_lock);

	// Memory usage information.
	pinf->uproc_memuse = 0;
	_lock(&proc->proc_vm->vm_lock);
	mreg = (struct memregion *)proc->proc_vm->vm_mregion;
	while (mreg != NULL)
	{
		pinf->uproc_memuse += ((mreg->mreg_endaddr - mreg->mreg_startaddr + 4095) << 12);
		mreg = (struct memregion *)mreg->mreg_next;
	}
	unlock(&proc->proc_vm->vm_lock);
	unlock(&proc->proc_exitlock);
	return 0;
}

int syscall_wait(int *status)
{
	struct process *proc = NULL, *p1, *p2;
	int retval = 0;

	_lock((struct lock_t *)&current_process->proc_childlistlock);

syscall_wait_1:
	proc = (struct process *)current_process->proc_childlist;
	while (proc != NULL)
	{
		if (proc->proc_state == PROC_STATE_TERMINATED)
		{
			*status = proc->proc_exitstatus;
			break;
		}
		proc = (struct process *)proc->proc_sib;
	}

	if (proc == NULL) // No terminated process is found
	{
		if (proc->proc_childlist != NULL)
		{
			// wait until one child is terminated
			event_sleep((struct event_t *)&current_process->proc_childtermevent, (struct lock_t *)&current_process->proc_childlistlock);
			goto syscall_wait_1;
		}
		else retval = ENOCHILD;
	}
	else
	{
		// Some terminated process found, so add it to unused list of processes
		// Otherwise remove it from the child process 
		// list of its parent.

		p2 = (struct process *)current_process->proc_childlist;
		p1 = NULL;
		while (p2 != proc)
		{
			p1 = p2;
			p2 = (struct process *)p2->proc_sib;
		}

		if (p1 == NULL) current_process->proc_childlist = proc->proc_sib;
		else p1->proc_sib = proc->proc_sib;
		proc->proc_sib = NULL;
		destroy_process(proc);
	}

	unlock((struct lock_t *)&current_process->proc_childlistlock);
	return retval;	
}

int syscall_kill(int pid)
{
	// At present this can kill only local process (&threads)
	// No checking for authorization.
	struct process *proc = NULL;
	int retval = 0;

	_lock(&used_proclock);
	_lock(&suspended_proclock);
	proc = (struct process *)used_processes;
	while (proc != NULL && proc->proc_id != pid) proc = (struct process *)proc->proc_next;

	if (proc == NULL)
	{
		proc = (struct process *)suspended_processes;
		while (proc != NULL && proc->proc_id != pid) proc = (struct process *)proc->proc_next;
	}

	// Terminate all the local threads of the process.
	if (proc != NULL) terminate_process(proc);
	else retval = EINVALIDPID; // If no such pid

	unlock(&suspended_proclock);
	unlock(&used_proclock);
	
	return retval;
}

struct kthread * initialize_irqtask(int irqno, char *tname, void (* irq_handler_func)(void), int pri)
{
	int i;
	int thrslot;
	struct kthread *thr;
        struct segdesc_s * gdtp = (struct segdesc_s *)gdt_table;
        struct process *proc = &system_process;

	thrslot = NO_SYS_DESCRIPTORS + irqno + 1;
	thr = alloc_thread();
	if (thr == NULL)
	{
		free_task_index(thrslot);
		return NULL;
	}
	strcpy(thr->kt_name, tname);
	thr->kt_threadid = ++proc->proc_thrno;
	thr->kt_type = proc->proc_type;  // Type of the thread
	thr->kt_tssdesc = thrslot * 8;

	// TSS initialization
	bzero((char *)thr->kt_tss,sizeof(struct TSS));
	i = (KERNEL_STACK_SIZE/4) - 1;
	thr->kt_tss->ss0 = KERNEL_DS;
	thr->kt_tss->esp0 = (unsigned int)(&(thr->kt_kstack[i])) - 1;
	thr->kt_tss->iomapbase = sizeof(struct TSS);
	thr->kt_tss->cr3 = (unsigned int)&(KERNEL_PAGEDIR);
	thr->kt_tss->eip = (unsigned int)irq_handler_func; 
	thr->kt_tss->eflags = initial_eflags;
	thr->kt_tss->esp = (unsigned int)(&(thr->kt_kstack[i])) - 1;
	thr->kt_tss->ss = KERNEL_DS;
	thr->kt_tss->es = KERNEL_DS;
	thr->kt_tss->ds = KERNEL_DS;
	thr->kt_tss->cs = KERNEL_CS;
	thr->kt_tss->fs = KERNEL_DS;
	thr->kt_tss->gs = KERNEL_DS;

	initialize_dataseg(&gdtp[thrslot], (unsigned int)thr->kt_tss, sizeof(struct TSS), INTR_PRIVILEGE);
	gdtp[thrslot].access = PRESENT | (INTR_PRIVILEGE << DPL_SHIFT) | TSS_TYPE;

	bcopy((char *)&initial_fpustate, (char *)&(thr->kt_fps), sizeof(initial_fpustate));

	thr->kt_cr0 = 0x80050033;

	thr->kt_ustackstart = 0;

	// Initialize scheduling information
	thr->kt_schedinf.sc_cid = SCHED_CLASS_INTERRUPT;
	thr->kt_schedinf.sc_usrpri = 0;    
	thr->kt_schedinf.sc_cpupri = pri;  
	thr->kt_schedinf.sc_inhpri = 0;  
	thr->kt_schedinf.sc_state = THREAD_STATE_READY;
	thr->kt_schedinf.sc_tqleft = TIME_QUANTUM;
	thr->kt_schedinf.sc_totcpuusage = 0;
	thr->kt_schedinf.sc_slpstart = 0;
	thr->kt_schedinf.sc_thrreadystart = 0;
	thr->kt_schedinf.sc_clfuncs = &SCHED_CLASS_SYSTEM_FUNCS;

	thr->kt_defsock = NULL;
	thr->kt_terminatelock = 0;
	lockobj_init(&thr->kt_exitlock);
	eventobj_init(&thr->kt_threadexit);
	thr->kt_exitstatus = NULL;
	thr->kt_threadjoinable = 0;
	thr->kt_synchobjlock = 0;
	thr->kt_synchobjlist = NULL;
	thr->kt_slpchannel = NULL;
	thr->synchtry = 0;
	thr->synchobtained = 0;
	thr->kt_process = &system_process;
	_lock(&proc->proc_thrlock);
	thr->kt_sib = proc->proc_threadptr;
	proc->proc_threadptr = thr;
	unlock(&proc->proc_thrlock);

	_lock(&used_threadlock);
	thr->kt_uselist = (struct kthread *)used_threads;
	used_threads = thr;
	unlock(&used_threadlock);

	// Not added to ready q
	thr->kt_qnext = thr->kt_qprev = NULL;
	return thr;
}

#ifdef DEBUG
int syscall_pageinfo(int pid, unsigned int pageno)
{
	struct process *proc;
	char *pt;
	char *p;
	unsigned int old_cr0;
	int pdpresent, pdwrite;
	PTE *ptentry;
	PDE *pdentry;

	_lock(&used_proclock);
	proc = (struct process *)used_processes;
	while (proc != NULL && proc->proc_id != pid) proc = (struct process *)proc->proc_next;
	unlock(&used_proclock);

	// If no such pid
	if (proc == NULL || proc->proc_threadptr == NULL)
		return EINVALIDPID;
	old_cr0 = clear_paging();

	ptentry = vm_getptentry(proc->proc_vm, pageno);
	pdentry = vm_getpdentry(proc->proc_vm, pageno);
	
	pt = (char *)((pdentry->page_table_base_addr) << 12);
	pdpresent = pdentry->present;
	pdwrite = pdentry->read_write;

	p = (char *)(ptentry->page_base_addr << 12);
	restore_paging(old_cr0, current_thread->kt_tss->cr3);
	return 0;
}

#endif


