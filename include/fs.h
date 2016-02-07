
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

// Common file system related structures are defined here
// FS system call prototypes are also listed.
#ifndef __FS_H__
#define __FS_H__
#include "mconst.h"
#include "mtype.h"
#include "lock.h"

// FS types
#define FSTYPE_FAT12		0


struct vfs {
	volatile struct vfs	*vfs_next;
	struct vfsops	*vfs_op;
	struct vnode	*vfs_vnodecovered;
	int		vfs_fstype;
	dev_t		vfs_dev;
	char 		vfs_data[1];
};

#define VNODE_ROOT	0x0001

struct vnode {
	unsigned short	v_type;
	unsigned short	v_flag;
	unsigned short	v_count;

	struct vfs	*v_vfsmountedhere;
	struct vnodeops *v_op;
	struct vfs	*v_vfsp;
	dev_t		v_dev;		// Identity of vnode (dev, no)
	int		v_offset;	// Within parent dir offset position
	int		v_no;		// Vnode no/Cluster number, if known
	struct rlock_t	v_vnodelock; //  Recursive lock for serialization
	volatile struct vnode 	*v_next;	// To add to hash list
	volatile struct vnode	*v_prev;
	struct vnode	*v_parent;
	char		v_data[1];	// vnode specific data
};

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

struct vfsinfo {		// Again should be checked for valid fields.
	char fs_device;
	int fs_spc;
	int fs_totalinodes;
	int fs_freeinodes;
	int fs_totalspace;
	int fs_resspace;
	int fs_badspace;
	int fs_usedspace;
};

// Paramters need to be identified
struct vnodeops {
	int (*vop_open)(struct vnode *vn, int flags, mode_t mode);
	int (*vop_close)(struct vnode *vn);
	int (*vop_read)(struct vnode *vn, int offset, char *buf, int nb);
	int (*vop_write)(struct vnode *vn, int offset, char *buf, int nb);
	int (*vop_ioctl)(struct vnode *vn, int request, ...);
	int (*vop_getattr)(struct vnode *vn, struct stat *stbuf);
	int (*vop_seek)(struct vnode *vn, int offset, int pos, int whence);
	int (*vop_setattr)(struct vnode *vn, struct stat *stbuf);
	int (*vop_access)(void);
	struct vnode * (*vop_lookup)(struct vnode *dvn, const char *pathname);
	struct vnode * (* vop_create)(struct vnode *dvn, const char *pathname, mode_t mode);
	int (* vop_trunc)(struct vnode *dvn); //, const char *pathname, mode_t mode);
	int (*vop_remove)(struct vnode *dvn); //, const char *pathname);
	int (*vop_link)(struct vnode *sdvn, const char *srcpathname, struct vnode *ddvn, const char *targetpathname);
	int (*vop_rename)(struct vnode *sdvn, struct vnode *ddvn, const char *newpathname);
	int (*vop_mkdir)(struct vnode *dvn, const char *dirname, mode_t mode);
	int (*vop_rmdir)(struct vnode *dvn, const char *dirname);
	int (*vop_opendir)(struct vnode *vn);
	int (*vop_seekdir)(struct vnode *vn, off_t offset);
	int (*vop_closedir)(struct vnode *vn);
	int (*vop_readdir)(struct vnode *vn, int offset, struct dirent *dirp, char *dep_dirp);
	off_t (*vop_telldir)(struct vnode *vn);
	int (*vop_destroy)(struct vnode *vn);
	
	int (*vop_symlink)(struct vnode *sdvn, const char *srcpathname, struct vnode *ddvn, const char *targetpathname);
	int (*vop_readlink)(struct vnode *vn, char *buf, size_t bufsiz);
	int (*vop_inactive)(void); // None of these are used
	int (*vop_rwlock)(struct vnode *vn, int lcktype);
	int (*vop_rwunlock)(struct vnode *vn);
	int (*vop_realvp)(void);	
	int (*vop_getpage)(void);
	int (*vop_putpage)(void);
	int (*vop_map)(void);
	int (*vop_poll)(void);
};

struct vfsops {
	int vfs_namemax;
	int (* vfs_mount)(struct vnode *svn, struct vfs *dvfs, struct vnode *dvn);
	int (* vfs_unmount)(struct vnode *svn, struct vnode *dvn);
	struct vnode * (* vfs_root)(struct vnode *vn);
	int (* vfs_statvfs)(struct vnode *vn, struct vfsinfo *vfsinf);
	int (* vfs_sync)(struct vnode *vn);
};

#define FTE_UNUSED	0x00
#define FTE_USED	0x01
#define FTFLAGS_DIR	0x02
#define FTFLAGS_FILE	0x04
#define FTFLAGS_READ	0x08
#define FTFLAGS_WRITE	0x10
#define FTFLAGS_APPEND	0x20
#define FTFLAGS_CHANGED	0x40

	
struct FileTableEntry {
	short fte_refcount;
	short fte_flags;	// USED, Unused, changed ...
	short fte_mode;		// read-write-exec permissions
	short fte_opmode;	// Open mode for reading, writing ...
	unsigned int fte_size;
	unsigned int fte_curposition;	// Current file/dir pointer position
	struct vnode *fte_vn; 	// Fs specific vnode is pointed here.
				// base fields are shown in the above struct.
};
	

#define S_IFMT		0170000
#define S_IFDIR		0040000
#define S_IFCHR		0020000
#define S_IFBLK		0060000
#define S_IFREG		0100000	/* Not used */
#define S_IFIFO		0010000
#define S_IFLNK		0120000
#define S_IFSOCK	0140000

#define O_RDONLY	0x01
#define O_WRONLY	0x02
#define O_RDWR		0x04
#define O_APPEND	0x08
#define O_TRUNC		0x10
#define O_CREAT		0x20
#define O_EXCL		0x40

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2


struct dir_opaque{
	int dir_handle;
	int dir_offset;
	struct dirent dir_dent;
};

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

void get_vnode(struct vnode *vn); 
void release_vnode(struct vnode *vn); 
struct vnode * get_curdir(void);
struct vnode * get_rootdir(void);
#endif
