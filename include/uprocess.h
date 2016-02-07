
#ifndef __UPROCESS_H__
#define __UPROCESS_H__


#define PROC_TYPE_USER		0x01
#define PROC_TYPE_SUBSYSTEM	0x02
#define PROC_TYPE_SYSTEM	0x04

#define PROC_TYPE_APPLICATION	0x10

#define THREAD_STATE_SLEEP	1
#define THREAD_STATE_READY	2
#define THREAD_STATE_CREATE	3
#define THREAD_STATE_TERMINATE	4
#define THREAD_STATE_RUNNING	5
#define THREAD_STATE_UNUSED	0

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
	int uproc_nthreadsglobal;
};

int getpids(int pid[], int n);
int getprocinfo(int pid, struct uprocinfo *pinf);
int kill(int pid);

#endif
