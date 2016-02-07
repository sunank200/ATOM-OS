#ifndef __FSCALLS_H__
#define __FSCALLS_H__

#include "fs.h"

typedef struct dir_opaque DIR;

int open(const char *pathname, int flags, mode_t mode);
int creat(const char *pathname, mode_t mode);

int write(int fd, void *buf, size_t count);
int read(int fd, void *buf, size_t count);
int close(int fd);

off_t lseek(int fildes, off_t offset, int whence);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
DIR *opendir(const char *name);       int closedir(DIR *dir);
struct dirent * readdir(DIR *dir);
void seekdir(DIR *dir, off_t offset);
off_t telldir(DIR *dir);
void rewinddir(DIR *dir);
int scandir(const char *dir, struct dirent ***namelist,
	int(*filter)(const struct dirent *),
	int(*compar)(const struct dirent **, const struct dirent **));

int mkdir(const char *pathname, mode_t mode);
int rmdir(const char *pathname);
int chdir(const char *path);
int fchdir(int fd);
int unlink(const char *pathname);
int rename(const char *oldpath, const char *newpath);
int chattrib(const char *pathname, unsigned char attr);
int stat(const char *pathname, struct stat *statbuf);
 
#endif
