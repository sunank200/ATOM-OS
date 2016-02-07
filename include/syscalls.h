#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__
#include "mconst.h"
#include "mtype.h"
#include "mklib.h"
#include "process.h"
#include "fs.h"
#include "ne2k.h"
#include "socket.h"
#include "semaphore.h"
#include "vmmap.h"
#include "olib.h"

int syscall_getchar(void);
int syscall_getchare(void);
int syscall_getscanchar(void);
int syscall_putchar(char c);
int syscall_puts(char *str);
void syscall_setcursor(int x, int y);
int syscall_open(const char *pathname, int flags, mode_t mode);
int syscall_creat(const char *pathname, mode_t mode);

int syscall_write(int fd, void *buf, size_t count);
int syscall_read(int fd, void *buf, size_t count);
int syscall_close(int fd);

off_t syscall_lseek(int fildes, off_t offset, int whence);
int syscall_dup(int oldfd);
int syscall_dup2(int oldfd, int newfd);
DIR *syscall_opendir(const char *name, DIR *dir);       
int syscall_closedir(DIR *dir);
struct dirent * syscall_readdir(DIR *dir);
void syscall_seekdir(DIR *dir, off_t offset);
int syscall_closedir(DIR *dirptr);
off_t syscall_telldir(DIR *dir);
void syscall_rewinddir(DIR *dir);

int syscall_mkdir(const char *pathname, mode_t mode);
int syscall_rmdir(const char *pathname);
int syscall_chdir(const char *path);

int syscall_unlink(const char *pathname);
int syscall_rename(const char *oldpath, const char *newpath);
int syscall_chattrib(const char *pathname, unsigned char attr);
int syscall_chmod(const char *pathname, mode_t mode);
int syscall_sync(int dev);
int syscall_stat(const char *pathname, struct stat *statbuf);
void syscall_tsleep(unsigned int msec);
int syscall_txpkt(frame_buffer *pb);
int syscall_rxpkt(frame_buffer **pb);
int syscall_readblocks(char device, unsigned int stblk, int count, char *buf);
int syscall_writeblocks(char device, unsigned int stblk, int count, char *buf);
void syscall_syncfloppy(void);
int syscall_lock(volatile struct lock_t *lck, int type);
int syscall_unlock(volatile struct lock_t *lck);
int syscall_event_sleep(volatile struct event_t *e, struct lock_t *lck);
int syscall_event_wakeup(volatile struct event_t *e);
int syscall_tsendto(short dest_procno, char *buf, unsigned int len, int groupid, int tag, short nb);
int syscall_broadcast(int port, char *buf, int len);
int syscall_trecvfrom(char buf[], int maxlen, short *from, int nb);
int syscall_exectask(char pathname[], char *argv[], char *envp[], int type);
void syscall_exit(int status);
time_t syscall_time(time_t *t);

//////////////////////////////////////////////////
int syscall_socket(int family, int type, int protocol);
int syscall_bind(int sockfd, struct sockaddr *myaddr, socklen_t addrlen);
int syscall_recv(int s, void *buf, size_t len, int flags);
int syscall_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
int syscall_send(int s, const void *buf, size_t len, int flags);
int syscall_sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
int syscall_connect(int s, const struct sockaddr *serv_addr, socklen_t addrlen);
int syscall_accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int syscall_listen(int s, int backlog);
int syscall_kill(int pid);
int syscall_getpids(int pid[], int n);
int syscall_getprocinfo(int pid, struct uprocinfo *pinf);
int syscall_close_socket(int s);

int syscall_sem_init(sem_t *sem, int pshared, unsigned int val);
int syscall_sem_destroy(sem_t *sem);
sem_t * syscall_sem_open(const char *name, int oflag, mode_t mode, unsigned int val, sem_t *sem);
int syscall_sem_close(sem_t *sem);
int syscall_sem_unlink(const char *name);
int syscall_sem_wait(sem_t *sem);
int syscall_sem_trywait(sem_t *sem);
int syscall_sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
int syscall_sem_post(sem_t *sem);
int syscall_sem_getvalue(sem_t *sem, int *val);
int syscall_brk(void *enddataaddr);

int syscall_primalloc(unsigned int size);
int syscall_prifree(void *addr, unsigned int size);

int syscall_gettsc(unsigned int *hi, unsigned int *lo, unsigned int *msecs);


int syscall_change_datasegment_size(unsigned int oldenddataaddr, unsigned int newenddataaddr, int param);
int syscall_procrelease(int pid);
int syscall_pageinfo(int pid, unsigned int pageno);
void syscall_machid(void);

int syscall_getcharf(int fd);
int syscall_ungetcharf(int c,int fd);

void marktsc(unsigned int *hi, unsigned int *lo);



#include "olib.h"
#endif


