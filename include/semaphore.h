#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include "process.h"
#include "timer.h"
#include "mconst.h"

typedef union {
	char __size[16];	// This is the actual seminternal_t
	long int __align;	// Used incase semaphore is placed 
				// in shared page. (addr of semaphore.)
} sem_t;			// This is opaque structure for the user


struct seminternal_t {
	int sem_ind;		// Process shared type or thread shared type.
	int sem_no;
	char sem_encinfo[sizeof(sem_t) - 8]; // Some encrypted info?
};

struct semtable_entry {
	struct sleep_q sement_slq;
	int sement_type;		// Sharable or Unsharable (Within a process)
	int sement_no;
	int sement_refcount;
	int sement_destroyflag;
	volatile struct process* sement_ownerproc;
	volatile int sement_val;
	char sement_name[SYNCHNAMEMAX];	// For named semaphores (sharable)
};

#define SEM_FAILED	((sem_t *) 0)
	
int syscall_sem_init(sem_t *sem, int pshared, unsigned int val);
int syscall_sem_destroy(sem_t *sem);
sem_t * sem_open(const char *name, int oflag, ...);
int syscall_sem_close(sem_t *sem);
int syscall_sem_unlink(const char *name);
int syscall_sem_wait(sem_t *sem);
int syscall_sem_trywait(sem_t *sem);
int syscall_sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
int syscall_sem_post(sem_t *sem);
int syscall_sem_getvalue(sem_t *sem, int *val);

int sem_init(sem_t *sem, int pshared, unsigned int val);
int sem_destroy(sem_t *sem);
sem_t * syscall_sem_open(const char *name, int oflag, mode_t mode, unsigned int value, sem_t *sem);
int sem_close(sem_t *sem);
int sem_unlink(const char *name);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
int sem_post(sem_t *sem);
int sem_getvalue(sem_t *sem, int *val);
#endif

