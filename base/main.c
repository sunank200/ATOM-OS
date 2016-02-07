
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

#include "syscalls.h"
#include "mklib.h"
#include "setup.h"
#include "fdc.h"
#include "alfunc.h"

extern struct process system_process;
extern volatile struct kthread *current_thread;
extern volatile struct process * current_process;
int syscall_sync(int dev);
extern int no_of_ram_segments;
extern struct memory_map_descriptor ram_segments[];
extern char floppy_info[12];
extern char harddisk_info[16];
extern volatile unsigned int secs, initsecs;
void init_pkts(void);
int init_sequence_number(int srcmachine, int sourceport, int socktype);

struct event_t sync_event;

void mount_thread(void)
{
	for ( ;  ; )
	{
		event_timed_sleep(&sync_event, NULL, 30000);	
		syscall_sync(0);
	}
}

void fat_updater(void) // Updated FAT table is written at some intervals.
{	
	void write_fat12(int dev);

	for ( ; ; )
	{
		syscall_tsleep(30*1000);
		write_fat12(0);
	}
}

int mksysthread(void (* func)(void), char thrname[], int priority, unsigned short threadno)
{
	printk(" %s ",thrname);

	create_thread(&system_process, thrname, SCHED_CLASS_SYSTEM, priority, 0, (unsigned long)func, threadno);

	return 0;
}

void create_shell_process(void)
{
	char *argv[] = { "shell", NULL };
	char *envp[] = {"PATH=.", "USER=chapram", NULL};

	syscall_exectask("shell", argv, envp, PROC_TYPE_APPLICATION);
}

void bsend(void)
{
	char *mesg = "TEST";
	struct isockaddr dest;
	extern struct lock_t socklist_lock;

	dest.saddr_host = -1;
	dest.saddr_port = 17;
	dest.saddr_type = SOCK_DGRAM;	
	//current_thread->kt_schedinf.priority = 10; // Slightly reduce
	for ( ; ; )
	{
		_lock(&socklist_lock);
		_lock(&(current_thread->kt_defsock->psk_busy));
		sock_send((struct proc_sock *)(current_thread->kt_defsock), &dest, mesg, 5, PKTTYPE_MCCONSISTENCY, 0, -1, 1);
		unlock(&(current_thread->kt_defsock->psk_busy));
		unlock(&socklist_lock);
		
		syscall_tsleep(500);
	}
}

void brecv(void)
{
	char buf[200];
	int len;
	struct isockaddr from;
	//int i;
	
	// Slightly reduce
	for ( ; ; )
	{
		len = recv_fromsock_dgram((struct proc_sock *)(current_thread->kt_defsock), &from, buf, 200, 0);
		printk("brecv: Recevied message : %d from : %d, %d\n",len, from.saddr_host, from.saddr_port);	
	}
}
void lockserver_setup(void);
void remote_exec_server_setup(void);
void mcmanager_setup(void);
struct vnode * mountroot(void);
extern FDD fdd;
int floppy_setup(FDD *fdd);
extern unsigned short int this_hostid;
void nicthread(void);
void floppy_thread(void);
void mcmanager_localreq(void);
void mcmanager_master(void);
void mcmanager_remotereqreply(void);
void mcmanager_timeout(void);
void nicthread_setup(void);
extern PageDir KERNEL_PAGEDIR;
void do_indirect_jump(unsigned int seg, unsigned int proc);
void do_indirectjump_target(void);
struct lock_t lck1, lck2, lck3, lck4;
struct event_t e1, e2;
int count=0;
void dumm(void)
{
	int i;

	for ( ; ; )
	{
		syscall_puts("dumm1");
		for (i=0; i<0x3000000; i++);
	}
}
void dumm1(void)
{
	int i;

	for ( ; ; )
	{
		syscall_puts("dumm2");
		for (i=0; i<0x3000000; i++);
	}
}
void dummythread(void)
{
	int i;

	for ( ; ; )
	{
		_lock(&lck1);
		count++;
		event_wakeup(&e1);
		unlock(&lck1);
		for (i=0; i<0x64000; i++) ;
		_lock(&lck1);
		if (count % 2 == 1)
			event_sleep(&e2, &lck1);
		unlock(&lck1);
	}
}
void dummythread4(void)
{
	int j, i;

	for ( ; ; )
	{
		_lock(&lck1);
		if (count % 2 == 0)
			event_sleep(&e1, &lck1);
		j = count;
		unlock(&lck1);
		for (i=0; i<0x65000; i++) ;
		printk("count : %d\n",j);
		_lock(&lck1);
		count++;
		event_wakeup(&e2);
		unlock(&lck1);
	}
}
void dummythread1(void)
{
	int i, count = 0;

	for ( ; ; )
	{
		_lock(&lck1);
		_lock(&lck2);
		for (i=0; i<0x10000; i++) ;
		unlock(&lck1);
		unlock(&lck2);

		_lock(&lck3);
		for (i=0; i<0x10000; i++) ;
		unlock(&lck3);
		_lock(&lck4);
		for (i=0; i<0x10000; i++) ;
		unlock(&lck4);
	}
	for ( ; ; )
	{
		for ( ; ; );
		for (i=0; i<0x800000; i++) ;
		count++;
		printk("%s ==> %d\n",current_thread->kt_name, count);
		_lock(&lck1);
		event_timed_sleep(&e1, &lck1, 60);
		for (i=0; i<0x800000; i++) ;
		unlock(&lck1);
	}
}

void display_file(void)
{
	char buf[100];
	int i, n, fd;

	fd = syscall_open("test",O_RDONLY, 0);
	if (fd >= 0)
	{
		while ((n = syscall_read(fd, buf, 100)) > 0)
		{
			for (i=0; i<n; i++) syscall_putchar(buf[i]);
		}
	}
	syscall_close(fd);
}

void do_test(void);
void kbd_init(void);	
void timer_init(void);	
void thread_terminate_handler(void);
extern void print_gdt(int start, int n);
void terminate_initialize_system_thread(void);
extern volatile struct kthread *ready_qhead[];
void print_lock(struct lock_t *lck);
void initialize_display(void);
void initialize_clock(void);
void initialize_keyboard(void);
void initialize_nic(void);
void initialize_paging(unsigned int pagediraddr);
extern struct lock_t ftlck;
void init_system_service_thread(void)
{
	//int i;
	//void *m1, *m2;

	initialize_display();
	initialize_clock();
	initialize_keyboard();

	mksysthread(timerthread, "TIMERSTHREAD", SP_PRIORITY_MAX - 1, 3);

	mksysthread(thread_terminate_handler, "TERMINATER", SP_PRIORITY_MIN, 25);

	sti_intr();

	eventobj_init(&sync_event);

//	mksysthread(dummythread, "DUMMY 1", 0, 35);
//	mksysthread(dummythread4, "DUMMY 2", 0, 36);
//	mksysthread(dummythread1, "DUMMY 3", 0, 27);
//	mksysthread(dummythread1, "DUMMY 4", 0, 28);

	floppy_setup(&fdd);

	mksysthread(floppy_thread, "FLOPPY", SP_PRIORITY_MIN + 15, 4);

	current_process->proc_fileinf->root_dir = current_process->proc_fileinf->current_dir = mountroot();

	mksysthread(mount_thread, "MOUNT", SP_PRIORITY_MIN + 10, 5);

	mksysthread(fat_updater, "FATUPDATER", SP_PRIORITY_MIN + 5, 6);

	initialize_nic();
	mksysthread(nicthread, "NIC", SP_PRIORITY_MIN + 20, 7);

	mksysthread(assemble, "ASSEMBLE", SP_PRIORITY_MIN + 20, 9);

	mksysthread(pkttransmitter, "TRANSMITTER1", SP_PRIORITY_MIN + 20, 8);
	mksysthread(pkttransmitter, "TRANSMITTER2", SP_PRIORITY_MIN + 20, 30);

	printk("\n\n");
	// This is the right place for starting the shell
	create_shell_process();
	printk("\ncreating shell process over\n");
	printk("\n Use Alt-A to switch between screens"); 
	printk("\n Use Ctrl-A to disply some debug information"); 
	printk("\n Use exec <externcmd> <args> ... for executing external program."); 
	printk("\n\n          MODIFY    EXPERIMENT  and  ENJOY\n\n");
	// This is the right place for starting the shell

	terminate_initialize_system_thread();
}


void cmain(void)
{
	//int i;

	initialize_paging((unsigned int)&KERNEL_PAGEDIR);

	// Here all system service providing threads are created 
	// including shell.

	init_system_service_thread();
}	

void get_sys_data(void)
{
	unsigned char *sysdata = (unsigned char *)(CONFIG_DATA_SEGMENT << 4);
	int i,j,n,*iptr;
	unsigned char *cptr;
	struct date_time dt;

	/*
	 * The memory map information
	 */
	n = sysdata[E820_FLAG]; /* No of memor segments */
	if (n == 0)
	{
		printk("INT 15 - E820 not supported\n");
		no_of_ram_segments = 0;
		ram_segments[no_of_ram_segments].base_addr = 0;
		ram_segments[no_of_ram_segments].length = 0xa0000;
		ram_segments[no_of_ram_segments].type = 1;
		no_of_ram_segments ++;
		iptr = (int *) &sysdata[MEMSIZE_ADDR];
		ram_segments[no_of_ram_segments].base_addr = 0x00100000;
		ram_segments[no_of_ram_segments].length = (*(iptr) - 1024) * 1024;
		ram_segments[no_of_ram_segments].type = 1;
		no_of_ram_segments ++;
	}
	else {
		iptr = (int *)&sysdata[MEM_SEGMENT_DESC];
		j = 0;
		no_of_ram_segments = 0;
		for (i=0; i<n; i++)
		{
			ram_segments[no_of_ram_segments].base_addr = iptr[0];
			ram_segments[no_of_ram_segments].length = iptr[2];
			ram_segments[no_of_ram_segments].type = iptr[4];
			no_of_ram_segments++;
			iptr = iptr + 5;
		}
	}	
	/* 
	 * Floppy disk information.(2)
	 * At present not used.
	 */
	cptr = &sysdata[FD0_DATA_ADDR]; 
	for (i=0; i<12; i++)
	{
		floppy_info[i] = *cptr;
		cptr++;
	}
	/*
	 * Hard disk information.
	 */
	cptr = &sysdata[HD0_DATA_ADDR];
	for (i=0; i<16; i++)
	{
		harddisk_info[i] = *cptr;
		cptr++;
	}
	/*
	 * Time and Date information.
	 */
	dt.seconds = sysdata[SECONDS];
	dt.minutes = sysdata[MINUTES];
	dt.hours = sysdata[HOURS];
	dt.date = sysdata[DATE];
	dt.month = sysdata[MONTH];
	dt.year = sysdata[YEAR] + 2000;
	/* System time in seconds */
	printk("Current date : %hd/%hd/%hd - %2hd:%2hd:%2hd\n",dt.date,dt.month,dt.year,dt.hours,dt.minutes,dt.seconds);
	secs = date_to_secs(dt);
	initsecs = secs;
	return;
}


