#ifndef __PTHREADTYPES_H__
#define __PTHREADTYPES_H__
#include "mtype.h"
#include "lock.h"

#define MUTEX

/* Thread descriptors */

typedef int clockid_t;

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
	struct event_t c_event;
} pthread_cond_t;


/* Attribute for conditionally variables.  */
typedef struct
{
  int __dummy;
} pthread_condattr_t;

/* Keys for thread-specific data */
typedef unsigned int pthread_key_t;

#define PTHREAD_MUTEX_RECURSIVE	1

/* Mutexes (not abstract because of PTHREAD_MUTEX_INITIALIZER).  */
/* (The layout is unnatural to maintain binary compatibility
    with earlier releases of LinuxThreads.) */
typedef struct
{
	int m_kind;      /* Mutex kind: fast, recursive or errcheck */
	int m_id;
	struct lock_t m_lock;
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
	volatile unsigned short ba_spinlock;
	volatile struct kthread * ba_spinlock_owner;
	volatile int ba_required;
	volatile int ba_present;
	volatile struct kthread *ba_waitq;
} pthread_barrier_t;

/* barrier attribute */
typedef struct {
	int __pshared;
} pthread_barrierattr_t;


/* Thread identifiers */
typedef unsigned long int pthread_t;

#endif	/* pthreadtypes.h */

