#include "syscalls.h"
#include "stdarg.h"
#include "errno.h"
#include "pthread.h"

void *malloc(unsigned int size);
void free(void *ptr);

DIR *opendir_internal(const char *name, DIR *dir);
DIR *opendir(const char *name)
{
	DIR *dir;

	dir = (DIR *)malloc(sizeof(DIR));
	if (dir == NULL) return NULL;

	bzero((char *)dir, sizeof(DIR));
	if (opendir_internal(name, dir) == NULL)
	{
		free(dir);
		return NULL;
	}
	else return dir;
}

int closedir_internal(DIR *dir);
int closedir(DIR *dir)
{
	int ret = EINVALID_ARGUMENT;
	if (dir != NULL)
	{
		ret = closedir_internal(dir);
		free(dir);
	}
	return ret;
}

sem_t *sem_openv(const char *name, int oflag, mode_t mode, unsigned int value, sem_t *sem); // Real system call
sem_t * sem_open(const char *name, int oflag, ...)
{
        sem_t *sem;
        mode_t mode;
        unsigned int value;
	sem_t *ret;
        va_list args;

	sem = (sem_t *)malloc(sizeof(sem_t));

	if (sem == NULL) return NULL;

	bzero((char *)sem, sizeof(sem_t));
        va_start(args, oflag);
        mode = (mode_t)va_arg(args, int);
        value = va_arg(args, unsigned int);
        va_end(args);

        ret = (sem_t *)sem_openv(name, oflag, mode, value, sem); // Real system call
	if (ret == NULL) free(sem);
	return ret;
}

int sem_close_internal(sem_t *sem);
int sem_close(sem_t *sem)
{
	int ret;

	ret = sem_close_internal(sem);
	if (sem != NULL) free(sem);
	return ret;
}

int sem_destroy_internal(sem_t *sem);
int sem_destroy(sem_t *sem)
{
	int ret;

	ret = sem_destroy_internal(sem);
	if (sem != NULL) free(sem);
	return ret;
}

int pthread_barrier_spl(pthread_barrier_t *barr, int count)
{
	if (barr != NULL)
	{
		barr->ba_required = count;
		return pthread_barrier_wait(barr);
	}
	else return -1;
}

int uname(struct utsname *buf) { return 0; }

#ifdef DEBUG
void debug_mesg(int addr)
{
        int i;

        printf("thread-> %x\n", addr);
        for (i=0; i<0x8000; i++) ;
}
#endif

