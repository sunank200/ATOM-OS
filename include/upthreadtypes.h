#ifndef __PTHREADTYPES_H__
#define __PTHREADTYPES_H__
#include "mtype.h"

#define MUTEX

/* Thread descriptors */

typedef int clockid_t;

struct timespec {
        int tv_sec;
        int tv_nsec;
};

struct sched_param {
	int __sched_priority;
};

/* Attributes for threads.  */
typedef struct pthread_attr_s
{
  int __detachstate;
  int __schedpolicy;
  struct sched_param __schedparam;
  int __inheritsched;
  int __scope;
  size_t __guardsize;
  int __stackaddr_set;
  void *__stackaddr;
  size_t __stacksize;
} pthread_attr_t;


typedef struct
{
	int c_id;
	char cond[24];
} pthread_cond_t;


/* Attribute for conditionally variables.  */
typedef struct
{
  int __dummy;
} pthread_condattr_t;

/* Keys for thread-specific data */
typedef unsigned int pthread_key_t;


/* Mutexes (not abstract because of PTHREAD_MUTEX_INITIALIZER).  */
/* (The layout is unnatural to maintain binary compatibility
    with earlier releases of LinuxThreads.) */
typedef struct
{
	int m_kind;      /* Mutex kind: fast, recursive or errcheck */
	int m_id;
	char lck[28];
} pthread_mutex_t;


/* Attribute for mutex.  */
typedef struct
{
  int mutexkind;
} pthread_mutexattr_t;


/* Once-only execution */
typedef int pthread_once_t;


/* POSIX spinlock data type.  */
typedef volatile int pthread_spinlock_t;

/* POSIX barrier. */
typedef struct {
	int ba_id;
	unsigned short int ba_spinlock;
	int ba_required;
	int ba_present;
	struct kthread *ba_waitq;
} pthread_barrier_t;

/* barrier attribute */
typedef struct {
	int __pshared;
} pthread_barrierattr_t;


/* Thread identifiers */
typedef unsigned long int pthread_t;

#endif	/* pthreadtypes.h */

