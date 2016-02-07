#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

#include "mtype.h"
#include "mconst.h"

#define S_IFMT          0x01d0
#define S_IFDIR         0x0010
#define S_IFCHR         0x0040
#define S_IFBLK         0x0080
#define S_IFREG         0x0000  /* Not used */
#define S_IFIFO         0x0100

#define O_RDONLY        0x01
#define O_WRONLY        0x02
#define O_RDWR          0x04
#define O_APPEND        0x08
#define O_TRUNC         0x10
#define O_CREAT         0x20
#define O_EXCL          0x40

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

struct stat {
	dev_t         st_dev;      /* device */
	ino_t         st_ino;      /* inode */
	mode_t        st_mode;     /* protection */
	nlink_t       st_nlink;    /* number of hard links */
	uid_t         st_uid;      /* user ID of owner */
	gid_t         st_gid;      /* group ID of owner */
	dev_t         st_rdev;     /* device type (if inode device) */                  off_t         st_size;     /* total size, in bytes */
	blksize_t     st_blksize;  /* blocksize for filesystem I/O */
	blkcnt_t      st_blocks;   /* number of blocks allocated */
	time_t        st_atime;    /* time of last access */
	time_t        st_mtime;    /* time of last modification */
	time_t        st_ctime;    /* time of last change */
};

struct dirent {
	long d_ino;                 // inode number 
	off_t d_off;		    // Offset to this dirent 
	unsigned short d_reclen;
	char d_name [NAME_MAX+1];   // file name (null-terminated) 
};

struct dir_opaque{
        int dir_handle;
        int dir_offset;
        struct dirent dir_dent;
};

typedef struct dir_opaque DIR;

#define FTYPE_READONLY  0x01
#define FTYPE_HIDDEN    0x02
#define FTYPE_SYSTEM    0x04
#define FTYPE_VOLUME    0x08
#define FTYPE_DIR       0x10
#define FTYPE_ARCHIVE   0x20


#define PF_INET		1
#define AF_INET		1
#define PF_UNIX		2
#define INADDR_ANY	0L

#define SOCK_UNUSED	0
#define SOCK_STREAM	1
#define SOCK_DGRAM	2

#define __SOCK_SIZE__	16

#define PROCSOCK_DEFAULT        0x1
#define PROCSOCK_USER           0x2

struct in_addr {
	unsigned int s_addr;
};

struct sockaddr_in {
        short int     		sin_family;
        unsigned short int      sin_port;
        struct in_addr          sin_addr;
        unsigned char 		__pad[__SOCK_SIZE__ - sizeof(short int) - sizeof(unsigned short int) - sizeof(struct in_addr)];
};
                                                                                
struct sockaddr {
        unsigned short int	sa_family;
        unsigned char		sa_data[14];
};
	
#define MSG_DONTWAIT	0x0001
#define MSG_TRUNC	0x0002
#define MSG_PEEK	0x0004

#include "upthreadtypes.h"
#include "uprocess.h"

typedef union {
	char __size[16];	// This is the actual seminternal_t
	long int __align;	// Used incase semaphore is placed 
				// in shared page. (addr of semaphore.)
} sem_t;			// This is opaque structure for the user

struct exec_reply {
	int 	er_type;
	int	er_id;
	int	er_stackno;
	int 	er_status;
};

#define PROC_TYPE_USER          0x01
#define PROC_TYPE_SUBSYSTEM     0x02
#define PROC_TYPE_SYSTEM        0x04
#define PROC_TYPE_APPLICATION   0x10
#define PROC_TYPE_PARALLEL      0x20

struct date_time {
	short seconds;
	short minutes;
	short hours;
	short date;
	short month;
	short year;
};

struct date_time secs_to_date(unsigned int secs);
unsigned int date_to_secs(struct date_time dt);
	
//---------------------------------------------
int getchar(void);
int getchare(void);
int getscanchar(void);
int putchar(char c);
int puts(char *str);
int setcursor(int x, int y);
int open(const char *pathname, int flags, mode_t mode);
int creat(const char *pathname, mode_t mode);

int write(int fd, void *buf, size_t count);
int read(int fd, void *buf, size_t count);
int close(int fd);

off_t lseek(int fildes, off_t offset, int whence);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
DIR *opendir(const char *name);       
int closedir(DIR *dir);
struct dirent * readdir(DIR *dir);
void seekdir(DIR *dir, off_t offset);
int closedir(DIR *dirptr);
off_t telldir(DIR *dir);
void rewinddir(DIR *dir);
// int scandir(const char *dir, struct dirent ***namelist,
// 	int(*filter)(const struct dirent *),
// 	int(*compar)(const struct dirent **, const struct dirent **));

int mkdir(const char *pathname, mode_t mode);
int rmdir(const char *pathname);
int chdir(const char *path);
// int fchdir(int fd);
int unlink(const char *pathname);
int rename(const char *oldpath, const char *newpath);
int chattrib(const char *pathname, unsigned char attr);
int stat(const char *pathname, struct stat *statbuf);
void tsleep(unsigned int msec);
int readblocks(char device, unsigned int stblk, int count, char *buf);
int writeblocks(char device, unsigned int stblk, int count, char *buf);
void syncfloppy(void);
int tsendto(short dest_procno, char *buf, unsigned int len, short nb);
int broadcast(int port, char *buf, int len);
int trecvfrom(char buf[], int maxlen, unsigned short *from, int nb);
int exectask(char pathname[], char *argv[], char *envp[], int type);
void exit(int status);
time_t time(time_t *t);
////////////////////////////////////////////////

int pthread_create(pthread_t *threadp, const pthread_attr_t *attr, void *(* start_routine)(void *), void * arg);
pthread_t pthread_self (void);
int pthread_equal (pthread_t thread1, pthread_t thread2);
void pthread_exit (void *retval);
int pthread_join (pthread_t th, void **thread_return);
int pthread_detach (pthread_t th);
int pthread_yield (void);
int pthread_mutex_init (pthread_mutex_t *__restrict mutex, __const pthread_mutexattr_t *__restrict mutex_attr) ;
int pthread_mutex_destroy (pthread_mutex_t *__mutex);
int pthread_mutex_trylock (pthread_mutex_t *mutex);
int pthread_mutex_lock (pthread_mutex_t *mutex) ;
int pthread_mutex_timedlock (pthread_mutex_t *__restrict __mutex, __const struct timespec *__restrict __abstime);
int pthread_mutex_unlock (pthread_mutex_t *mutex);
int pthread_mutexattr_init (pthread_mutexattr_t *__attr) ;
int pthread_mutexattr_destroy (pthread_mutexattr_t *__attr);
int pthread_mutexattr_getpshared (__const pthread_mutexattr_t * __restrict __attr, int *__restrict __pshared);
int pthread_mutexattr_setpshared (pthread_mutexattr_t *__attr, int __pshared);
int pthread_cond_init (pthread_cond_t *__restrict cond, __const pthread_condattr_t *__restrict __cond_attr);
int pthread_cond_destroy (pthread_cond_t *__cond);
int pthread_cond_signal (pthread_cond_t *cond);
int pthread_cond_broadcast (pthread_cond_t *cond);
int pthread_cond_wait (pthread_cond_t *__restrict cond, pthread_mutex_t *__restrict mutex);
int pthread_cond_timedwait (pthread_cond_t *__restrict __cond, pthread_mutex_t *__restrict __mutex, __const struct timespec *__restrict __abstime);
int pthread_condattr_init (pthread_condattr_t *__attr);
int pthread_condattr_destroy (pthread_condattr_t *__attr);
int pthread_condattr_getpshared (__const pthread_condattr_t * __restrict __attr, int *__restrict __pshared);
int pthread_condattr_setpshared (pthread_condattr_t *__attr, int __pshared);
int pthread_spin_init (pthread_spinlock_t *__lock, int __pshared);
int pthread_spin_destroy (pthread_spinlock_t *__lock);
int pthread_spin_lock (pthread_spinlock_t *__lock);
int pthread_spin_trylock (pthread_spinlock_t *__lock);
int pthread_spin_unlock (pthread_spinlock_t *__lock);
int pthread_barrier_init (pthread_barrier_t *__restrict barrier, __const pthread_barrierattr_t *__restrict __attr, unsigned int count);
int pthread_barrier_destroy (pthread_barrier_t *__barrier);
int pthread_barrierattr_init (pthread_barrierattr_t *__attr);
int pthread_barrierattr_destroy (pthread_barrierattr_t *__attr);
int pthread_barrierattr_getpshared (__const pthread_barrierattr_t * __restrict __attr, int *__restrict __pshared);
int pthread_barrierattr_setpshared (pthread_barrierattr_t *__attr, int __pshared);
int pthread_barrier_wait (pthread_barrier_t *barrier);
//////////////////////////////////////////////////
int socket(int family, int type, int protocol);
int bind(int sockfd, struct sockaddr *myaddr, socklen_t addrlen);
int recv(int s, void *buf, size_t len, int flags);
int recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
int send(int s, const void *buf, size_t len, int flags);
int sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
int connect(int s, const struct sockaddr *serv_addr, socklen_t addrlen);
int accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int listen(int s, int backlog);
int kill(int pid);
int getpids(int pid[], int n);
int getprocinfo(int pid, struct uprocinfo *pinf);
int close_socket(int s);

int sem_init(sem_t *sem, int pshared, unsigned int val);
int sem_unlink(const char *name);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
int sem_post(sem_t *sem);
int sem_getvalue(sem_t *sem, int *val);
int brk(void *enddataaddr);

int primalloc(unsigned int size);
int prifree(void *addr, unsigned int size);


pthread_t pthread_self(void);
int gettsc(unsigned int *hi, unsigned int *lo, unsigned int *msecs);


int change_datasegment_size(unsigned int oldenddataaddr, unsigned int newenddataaddr, int param);
int procrelease(int pid);
int pageinfo(int pid, unsigned int pageno);
void machid(void);

int getcharf(int fd);
int ungetcharf(int c,int fd);

#endif


