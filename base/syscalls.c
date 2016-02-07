
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
#include "errno.h"
#include "alfunc.h"
#include "pthread.h"

#define INTPAR(x)	(int)(*((int *)(x)))
#define INTPPAR(x)	(int *)(* ((int **)(x)))
#define CHARPPAR(x)	(char *)(* ((char **)(x)))
#define CHARPAR(x)	(char)(*((char *)(x)))
#define SHORTPAR(x)	(short)(*((short *)(x)))
#define SHORTPPAR(x)	(short*)(*((short **)(x)))

#define KERNELEND	(char *) KERNEL_MEMMAP
int syscall_notimplemented(int num);
extern volatile struct process *current_process;
extern volatile struct kthread *current_thread;
extern volatile unsigned int secs, millesecs;
extern int syscall_pthread_mutex_destroy (pthread_mutex_t *mutex);

int syscall_test(void)
{
	int i;

	printk("syscall test : %x\n",&i);
	return 0;
}
int systemcall(struct PAR_REGS *regs)
{
	char *cpp1, *cpp2, **cppp1; //3, *cpp4, *cpp5, *cpp6, **cppp1, **cppp2;
	int i, ip1, ip2;
	//unsigned long old_cr0;
	
	switch (regs->eax) // Sys call number ?
	{
		case 0: return syscall_test();
		case 1:
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_open(CHARPPAR(regs->ebx), INTPAR(regs->ebx+4), INTPAR(regs+4+4));
		case 2: return syscall_close(INTPAR(regs->ebx));
		case 3:
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_read(INTPAR(regs->ebx), cpp1, INTPAR(regs->ebx+4+4));
		case 4: 
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_write(INTPAR(regs->ebx), cpp1, INTPAR(regs->ebx+4+4));
		case 5: return syscall_lseek(INTPAR(regs->ebx), INTPAR(regs->ebx+4), INTPAR(regs->ebx+4+4));
		case 6: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_mkdir(cpp1, INTPAR(regs->ebx+4));
		case 7: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_rmdir(cpp1);
		case 8: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_chdir(cpp1);
		case 9: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_creat(cpp1, INTPAR(regs->ebx+4));
		case 10:
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_rename(CHARPPAR(regs->ebx), CHARPPAR(regs->ebx+4));
		case 11: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return (int) syscall_opendir(CHARPPAR(regs->ebx), (DIR *)CHARPPAR(regs->ebx+4));
		case 12: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_closedir((DIR *) CHARPPAR(regs->ebx));
		case 13: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return (int) syscall_readdir((DIR *)CHARPPAR(regs->ebx));
		case 14: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_stat(CHARPPAR(regs->ebx),(struct stat *)CHARPPAR(regs->ebx+4));
		case 15: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_unlink(CHARPPAR(regs->ebx));
		case 16: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_chmod(CHARPPAR(regs->ebx),(mode_t)INTPAR(regs->ebx+4));
		case 17:
			ip1 = INTPAR(regs->ebx);
			return syscall_dup(ip1);
		case 18:
			ip1 = INTPAR(regs->ebx);
			ip2 = INTPAR(regs->ebx + 4);
			return syscall_dup2(ip1, ip2);
		case 19: syscall_setcursor( INTPAR(regs->ebx),INTPAR(regs->ebx+4)); return 0;
		case 20: return syscall_getchar();
		case 21: return syscall_getchare();
		case 22: return syscall_getscanchar();
		case 23: return syscall_putchar(CHARPAR(regs->ebx));
		case 24: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_puts(CHARPPAR(regs->ebx));
		case 25:
			cpp1 = CHARPPAR(regs->ebx);
			ip1 = INTPAR(regs->ebx + 4);
			syscall_seekdir((DIR *)cpp1, ip1);
			return 0;
		case 26:
			cpp1 = CHARPPAR(regs->ebx);
			return syscall_telldir((DIR *)cpp1);
		case 27:
			cpp1 = CHARPPAR(regs->ebx);
			syscall_rewinddir((DIR *)cpp1);
			return 0;
		case 32: 
			cpp1 = CHARPPAR(regs->ebx+12);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_readblocks(INTPAR(regs->ebx), INTPAR(regs->ebx+4), INTPAR(regs->ebx+8), CHARPPAR(regs->ebx+12));
		case 33: 
			cpp1 = CHARPPAR(regs->ebx+12);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_writeblocks(INTPAR(regs->ebx), INTPAR(regs->ebx+4), INTPAR(regs->ebx+8), CHARPPAR(regs->ebx+12));
		case 34: syscall_sync(0); return 0;
		case 50: syscall_tsleep(INTPAR(regs->ebx)); return 0;
		case 60: 
			cpp1 = CHARPPAR(regs->ebx + 4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_tsendto(SHORTPAR(regs->ebx), CHARPPAR(regs->ebx+4), INTPAR(regs->ebx+8), INTPAR(regs->ebx+12), INTPAR(regs->ebx+16), SHORTPAR(regs->ebx+20));
		case 62: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx + 8);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_trecvfrom(CHARPPAR(regs->ebx), INTPAR(regs->ebx+4), SHORTPPAR(regs->ebx+8), INTPAR(regs->ebx+12));
		case 65: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cppp1 = (char **)CHARPPAR(regs->ebx+4);
			i = 0;
			while (cppp1[i] != NULL)
			{
				if (cppp1[i] < KERNELEND) return EINVALIDPTR;
				i++;
			}
			cppp1 = (char **)CHARPPAR(regs->ebx+8);
			i = 0;
			while (cppp1[i] != NULL)
			{
				if (cppp1[i] < KERNELEND) return EINVALIDPTR;
				i++;
			}
			return syscall_exectask(CHARPPAR(regs->ebx), (char **)CHARPPAR(regs->ebx+4), (char **)CHARPPAR(regs->ebx+8), INTPAR(regs->ebx+12));
		case 66: syscall_exit(INTPAR(regs->ebx)); return 0;
		case 70:
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+8);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+12);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			return syscall_pthread_create((pthread_t *)INTPPAR(regs->ebx), (pthread_attr_t *)CHARPPAR(regs->ebx+4), (void *(*)(void *))INTPPAR(regs->ebx+8), (void *)CHARPPAR(regs->ebx+12));
		case 71: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			syscall_pthread_exit((void *)CHARPPAR(regs->ebx)); return 0;
		case 72: 
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			return syscall_pthread_join((pthread_t)INTPAR(regs->ebx), (void **)CHARPPAR(regs->ebx+4));
		case 73: return syscall_pthread_detach((pthread_t)INTPAR(regs->ebx));
		case 74: return syscall_pthread_yield();
		case 75: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			return syscall_pthread_mutex_init((pthread_mutex_t *)INTPAR(regs->ebx), (pthread_mutexattr_t *)CHARPPAR(regs->ebx+4));
		case 76: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_pthread_mutex_trylock((pthread_mutex_t *)INTPAR(regs->ebx));
		case 77: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_pthread_mutex_lock((pthread_mutex_t *)INTPAR(regs->ebx));
		case 78: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_pthread_mutex_unlock((pthread_mutex_t *)INTPAR(regs->ebx));
		case 79: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			return syscall_pthread_cond_init((pthread_cond_t *)INTPAR(regs->ebx), (pthread_condattr_t *)CHARPPAR(regs->ebx+4));
		case 80: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			return syscall_pthread_cond_wait((pthread_cond_t *)INTPAR(regs->ebx), (pthread_mutex_t *)INTPPAR(regs->ebx+4));
		case 81: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_pthread_cond_signal((pthread_cond_t *)INTPAR(regs->ebx));
		case 82: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_pthread_cond_broadcast((pthread_cond_t *)INTPAR(regs->ebx));
		case 83: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			return syscall_pthread_barrier_init((pthread_barrier_t *)INTPAR(regs->ebx), (pthread_barrierattr_t *)INTPPAR(regs->ebx+4), INTPAR(regs->ebx+8));
		case 84: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_pthread_barrier_wait((pthread_barrier_t *)INTPAR(regs->ebx));
		case 90: return syscall_socket(INTPAR(regs->ebx), INTPAR(regs->ebx+4), INTPAR(regs->ebx+8));
		case 91: 
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_bind(INTPAR(regs->ebx), (struct sockaddr *)INTPPAR(regs->ebx+4), SHORTPAR(regs->ebx+8));
		case 92: 
			cpp1 = CHARPPAR(regs->ebx + 4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_recv(INTPAR(regs->ebx), (void *)CHARPPAR(regs->ebx+4), (size_t)INTPAR(regs->ebx+8), INTPAR(regs->ebx+12));
		case 93: 
			cpp1 = CHARPPAR(regs->ebx + 4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+16);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+20);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			return syscall_recvfrom(INTPAR(regs->ebx), (void *)CHARPPAR(regs->ebx+4), (size_t)INTPAR(regs->ebx+8), INTPAR(regs->ebx+12), (struct sockaddr *)CHARPPAR(regs->ebx+16), (socklen_t *)SHORTPPAR(regs->ebx+20));
		case 94: 
			cpp1 = CHARPPAR(regs->ebx + 4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_send(INTPAR(regs->ebx), (void *)CHARPPAR(regs->ebx+4), (size_t)INTPAR(regs->ebx+8), INTPAR(regs->ebx+12));
		case 95: 
			cpp1 = CHARPPAR(regs->ebx + 4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx + 16);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_sendto(INTPAR(regs->ebx), (void *)CHARPPAR(regs->ebx+4), (size_t)INTPAR(regs->ebx+8), INTPAR(regs->ebx+12), (struct sockaddr *)CHARPPAR(regs->ebx+16), (socklen_t)SHORTPAR(regs->ebx+20));
		case 96: 
			cpp1 = CHARPPAR(regs->ebx + 4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_connect(INTPAR(regs->ebx), (struct sockaddr *)CHARPPAR(regs->ebx+4), (socklen_t)SHORTPAR(regs->ebx+8));
		case 97: 
			cpp1 = CHARPPAR(regs->ebx + 4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx + 8);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_accept(INTPAR(regs->ebx), (struct sockaddr *)CHARPPAR(regs->ebx+4), (socklen_t *)CHARPPAR(regs->ebx+8));
		case 98: return syscall_listen(INTPAR(regs->ebx), INTPAR(regs->ebx+4));
		case 99: return syscall_kill(INTPAR(regs->ebx));
		case 100: 
			cpp1 = CHARPPAR(regs->ebx + 4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_getprocinfo(INTPAR(regs->ebx), (struct uprocinfo *)INTPPAR(regs->ebx+4));
		case 101: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_getpids(INTPPAR(regs->ebx), INTPAR(regs->ebx+4));
		case 102: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_sem_init((sem_t *)(CHARPPAR(regs->ebx)), INTPAR(regs->ebx+4), (unsigned int)INTPAR(regs->ebx+8));
		case 103: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_sem_destroy((sem_t *)(CHARPPAR(regs->ebx)));
		case 104: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+16);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return (int)syscall_sem_open(CHARPPAR(regs->ebx), INTPAR(regs->ebx+4), (mode_t)INTPAR(regs->ebx+8), INTPAR(regs->ebx+12), (sem_t *)CHARPPAR(regs->ebx+16));
		case 105: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_sem_close((sem_t *)(CHARPPAR(regs->ebx)));
		case 106: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_sem_unlink(CHARPPAR(regs->ebx));
		case 107: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_sem_wait((sem_t *)(CHARPPAR(regs->ebx)));
		case 108: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_sem_trywait((sem_t *)(CHARPPAR(regs->ebx)));
		case 109: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_sem_timedwait((sem_t *)(CHARPPAR(regs->ebx)), (const struct timespec *)CHARPPAR(regs->ebx+4));
		case 110: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_sem_post((sem_t *)(CHARPPAR(regs->ebx)));
		case 111: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_sem_getvalue((sem_t *)(CHARPPAR(regs->ebx)), INTPPAR(regs->ebx+4));
		case 112: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			return (int)syscall_time((time_t *)(CHARPPAR(regs->ebx)));
		case 113: return (int)syscall_pthread_self();
		case 114: 
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND && cpp1 != NULL) return EINVALIDPTR;
			return syscall_brk((void *)CHARPPAR(regs->ebx));
		case 115:
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+4);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp1 = CHARPPAR(regs->ebx+8);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_gettsc((unsigned int *)INTPPAR(regs->ebx), (unsigned int *)INTPPAR(regs->ebx+4), (unsigned int *)INTPPAR(regs->ebx+8));

		case 140:
			print_core_info();
			print_minf_statistics();
			return 0;

		case 147:
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_wait(INTPPAR(regs->ebx));
		case 148:
			return syscall_pageinfo(INTPAR(regs->ebx), INTPAR(regs->ebx+4));
		case 153:
			syscall_machid();
			return 0;
		case 154:
			return syscall_getcharf(INTPAR(regs->ebx));
		case 155:
			return syscall_ungetcharf(INTPAR(regs->ebx), INTPAR(regs->ebx+4));
		case 156:
			printk("broadcast called\n");
			return syscall_broadcast(INTPAR(regs->ebx), CHARPPAR(regs->ebx+4), INTPAR(regs->ebx+8));
		case 157:
			return syscall_primalloc((unsigned int)INTPAR(regs->ebx));

		case 158:
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_prifree((void *)CHARPPAR(regs->ebx), (unsigned int)INTPAR(regs->ebx+4));
		case 202:
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			cpp2 = CHARPPAR(regs->ebx + 4);
			if (cpp2 < KERNELEND) return EINVALIDPTR;
			mark_tsc((unsigned int *)cpp1, (unsigned int *)cpp2);
			return 0;

		case 203:
			return (secs * 1000 + millesecs);
			
		case 204:
			cpp1 = CHARPPAR(regs->ebx);
			if (cpp1 < KERNELEND) return EINVALIDPTR;
			return syscall_pthread_mutex_destroy((pthread_mutex_t *) cpp1);
		
		default: return syscall_notimplemented(regs->eax);
	}
}

int syscall_notimplemented(int num)
{
	printk("Not implemented : %d\n", num);
	return 0;
}

