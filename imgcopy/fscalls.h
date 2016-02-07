#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__
#include<stdio.h>
#include "fs.h"

int syscall_open(const char *pathname, int flags, mode_t mode);
int syscall_creat(const char *pathname, mode_t mode);

int syscall_write(int fd, void *buf, size_t count);
int syscall_read(int fd, void *buf, size_t count);
int syscall_close(int fd);

off_t syscall_lseek(int fildes, off_t offset, int whence);
int syscall_mkdir(const char *pathname, mode_t mode);
int syscall_rmdir(const char *pathname);
int syscall_chdir(const char *path);
int syscall_unlink(const char *pathname);
int syscall_rename(const char *oldpath, const char *newpath);
int syscall_chattrib(const char *pathname, unsigned char attr);
int syscall_stat(const char *pathname, struct stat *statbuf);
#endif

