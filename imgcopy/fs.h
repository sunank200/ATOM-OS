#ifndef __FS_H
#define __FS_H

typedef unsigned short int mode_t;
//typedef int	off_t;
typedef	char	dev_t;
typedef	long	ino_t;
typedef	unsigned short int nlink_t;
typedef	unsigned short int uid_t;
typedef	unsigned short int gid_t;
typedef	unsigned short int blksize_t;
typedef	unsigned long	blkcnt_t;
typedef	unsigned long time_t;

#define NAME_MAX	13

#define CLUSTER_BAD	0xFF7
#define FTABLE_SIZE	100

#define FTYPE_READONLY	0x01
#define FTYPE_HIDDEN	0x02
#define FTYPE_SYSTEM	0x04
#define FTYPE_VOLUME	0x08
#define FTYPE_DIR	0x10
#define FTYPE_ARCHIVE	0x20

struct dir_entry {
	char name[8];
	char ext[3];
	char attrib;
	char res[10];
	short int time;
	short int date;
	short int start_cluster;
	unsigned int fsize;
};

#define FTABLE_ENTRY_UNUSED	0x00
#define FTABLE_ENTRY_USED	0x01
#define FTABLE_FLAGS_DIR	0x02
#define FTABLE_FLAGS_FILE	0x04
#define FTABLE_FLAGS_READ	0x08
#define FTABLE_FLAGS_WRITE	0x10
#define FTABLE_FLAGS_APPEND	0x20
#define FTABLE_FLAGS_CHANGED	0x40

struct DIR_FILE {
	struct dir_entry dent;		/* This dir/file entry itself */
	int parent_index;
	unsigned int current_pos;
	unsigned int current_cluster;
	int ref_count;
	char device;
	char flags;

};


struct dirent
{
	long d_ino;                 /* inode number */
	off_t d_off;                /* offset to this dirent */
	unsigned short d_reclen;    /* length of this d_name */
	char d_name [NAME_MAX+1];   /* file name (null-terminated) */
        char attrib;
        short int time;
        short int date;
        short int start_cluster;
        unsigned int fsize;
};

struct dir_opaque {
	int dir_index;
	int current_pos;
	int current_cluster;
	int current_blockno;
	struct dirent dent;
};

#define S_IFMT		0x01d0
#define S_IFDIR		0x0010
#define S_IFCHR		0x0040
#define S_IFBLK		0x0080
#define S_IFREG		0x0000	/* Not used */
#define S_IFIFO		0x0100

#ifndef _SYS_STAT_H
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
#endif

struct fsinfo {
	char device;
	int spc;
	int total_space;
	int reserved_space;
	int free_space;
	int bad_space;
	int used_space;
};

#define O_RDONLY	0x01
#define O_WRONLY	0x02
#define O_RDWR		0x04
#define O_APPEND	0x08
#define O_TRUNC		0x10
#define O_CREAT		0x20

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

#define ENO_FREE_FILE_TABLE_SLOT	-1
#define EINVALID_DIR_INDEX	-2
#define EDIR_ENTRY_NOT_FOUND	-3
#define EDIR_FULL		-4
#define EDUPLICATE_ENTRY	-5
#define EDIR_NOT_EMPTY		-6
#define ENO_DISK_SPACE		-7
#define ENOT_A_DIRECTORY	-8
#define EFILE_CREATE_ERROR	-9
#define EDIR_OPEN_AS_A_FILE	-10
#define EVOLUME_ENTRY		-11
#define EFILE_NOT_FOUND		-12
#define ENOT_READABLE		-13
#define ENOT_WRITEABLE		-14
#define EACCESS			-15
#define EINVALIDNAME		-16
#define EINVALID_FILE_HANDLE	-17
#define EINVALID_ARGUMENT	-18
#define ELONGPATH		-19
#define EINVALID_DRIVE		-20
#define EINVALID_SEEK		-21
#define EDEVICE_DIFFERENT	-22
#define ETARGET_EXISTS		-24
#define EPATH_NOT_EXISTS	-25
#define	EREADONLY		-26
#define EOLDPATH_PARENT_OF_NEWPATH	-27
#define	ESRC_DEST_NOT_SAME_TYPE	-28

/*
 * Functions provided by fs.c
 */

int find_entry(int dir_index, char name[], struct dir_entry *dent);
int read_first_entry(int dir_index, struct dir_opaque *dop, struct dir_entry *dent);
int read_next_entry(struct dir_opaque *dop, struct dir_entry *dent);
int read_block(char device, unsigned int blk, char *data);
int write_block(char device, unsigned int blk, char *data);
int get_fs_info(struct fsinfo *inf);
int alloc_ftab_slot(void);
int open_dir(int dir_index, char name[11]);
void close_dir(int dir_index);
int make_dir(int dir_index, char name[]);
int rm_dir(int dir_index, char name[]);
int create_file(int dir_handle, char name[], int attrib);
int open_file(int dir_handle, char name[], int type, int mode);
void close_file(int handle);
int read_file(int handle, char *buf, int nbytes);
int write_file(int handle, char *buf, int nbytes);

int mount_floppy(void);
void unmount_floppy(int handle);
void write_fat(void);
void increment_ref_count(int handle);
void decrement_ref_count(int handle);
int add_dir_entry(int dir_index, struct dir_entry *dent);
int delete_dir_entry(int dir_index, char name[]);
int update_dir_entry(int dir_index, struct dir_entry *dent);
int seek_file(int fhandle, int pos, int whence);

void free_cluster_chain(unsigned short int clno);
int empty_dir(int dir_handle, struct dir_entry *dent);
unsigned short int next_cluster_12(int clno);

#endif
