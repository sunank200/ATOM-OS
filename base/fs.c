
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

#include "fs.h"
#include "mklib.h"
#include "errno.h"
#include "buffers.h"
#include "process.h"
#include "lock.h"
#include "vmmap.h"
#include "fat12.h"
#include "fdc.h"
#include "alfunc.h"

#define DRIVE_MAX	1
#define VNODEHASHMAX	19

struct vnodeops fat12_vnops = {
	fat12_vop_open, fat12_vop_close, fat12_vop_read, fat12_vop_write,
	fat12_vop_ioctl, fat12_vop_getattr, fat12_vop_seek, fat12_vop_setattr,
	fat12_vop_access, fat12_vop_lookup, fat12_vop_create, fat12_vop_trunc,
	fat12_vop_remove, fat12_vop_link, fat12_vop_rename, fat12_vop_mkdir,
	fat12_vop_rmdir, fat12_vop_opendir, fat12_vop_seekdir, fat12_vop_closedir,
	fat12_vop_readdir, fat12_vop_telldir, fat12_vop_destroy, fat12_vop_symlink,
	fat12_vop_readlink, fat12_vop_inactive, fat12_vop_rwlock, fat12_vop_rwunlock,
	fat12_vop_realvp, fat12_vop_getpage, fat12_vop_putpage, fat12_vop_map,
	fat12_vop_poll
};

struct vfsops fat12_vfsops = {
	11, fat12_vfs_mount, fat12_vfs_unmount, fat12_vfs_root,
	fat12_vfs_statvfs, fat12_vfs_sync
};

// System information
char floppy_info[12];
char harddisk_info[16];

struct lock_t ftlck;
struct FileTableEntry openFileTable[FTABLE_SIZE];
volatile struct vfs *vfs_list = NULL;
struct vnode *vnlist_hdr[VNODEHASHMAX];
struct lock_t vnlist_lock;
volatile struct vnode *rootvnode = NULL;

extern volatile unsigned short ready_qlock;
volatile unsigned short vnhashlist_lock=0;
volatile struct kthread * vnhashlist_lock_owner = NULL;
struct lock_t  buffer_cache_lock;
struct lock_t  write_queue_lock;
struct event_t	brelse_event;
extern volatile struct kthread *current_thread;
extern volatile struct process *current_process;

struct buffer_header buffers[MAX_BUFFERS];
struct buffer_header * hash_queue_buffers[BUFF_HASHNUMBER];
volatile struct buffer_header * free_buff_list_head = NULL;
volatile struct buffer_header * free_buff_list_tail = NULL;
volatile struct buffer_header * delayed_write_q_head = NULL;
volatile struct buffer_header * delayed_write_q_tail = NULL;

static void enqueue_to_hashlist(int hash,struct buffer_header * b);
static void delete_from_hashlist(int hash,struct buffer_header * b);
static void delete_from_freelist(struct buffer_header * b);
static void append_to_freelist(struct buffer_header * b);
static void prepend_to_freelist(struct buffer_header * b);
static void append_to_write_queue(struct buffer_header * b);
extern void free_floppy_request(struct floppy_request *dr);

int alloc_ftab_slot(void)
{
	int i, slot = ENO_FREE_FILE_TABLE_SLOT;
	_lock(&ftlck);
	for (i=0; i < FTABLE_SIZE; i++)
		if (openFileTable[i].fte_flags == FTE_UNUSED)
		{
			openFileTable[i].fte_flags = FTE_USED;
			slot = i;
			break;
		}

	unlock(&ftlck);
	return slot;
}

int alloc_process_file_handle(int ft_handle)
{
	int i;

	_lock(&(current_process->proc_fileinf->fhandles_lock));
	// Store this into task file handle table and return its index
	for (i=0; i<MAXPROCFILES; i++)
		if (current_process->proc_fileinf->fhandles[i] == -4)
			break;

	if (i < MAXPROCFILES) 
		current_process->proc_fileinf->fhandles[i] = ft_handle;
	else i = ENO_FREE_FILE_TABLE_SLOT;

	unlock(&(current_process->proc_fileinf->fhandles_lock));
	return i;
}

// Usually (dev, vno) is used for identifying a vnode uniquely. Under FAT vno
// is taken as starting cluster no of the file/dir. If the file size is zero 
// bytes then starting cluster is 0. So in that specific case, 
// (dev, pvno, offset) parent dir vnode no and its entry offset position is 
// taken as unique value.
struct vnode * alloc_vnode(dev_t dev, int vno, struct vnode * pvn, int offset, size_t size);

void get_vnode(struct vnode *vn)
{
	rlock(&vn->v_vnodelock);
	vn->v_count++;
}

void release_vnode(struct vnode *vn)
{
	struct vnode *vnp;
	vn->v_count--;
	if (vn->v_count <= 0)
	{
		vnp = vn->v_parent;
		runlock(&vn->v_vnodelock);
		delete_vnode(vn);

		if (vnp != NULL) // Reduce parent ref count
		{
			get_vnode(vnp);
			vnp->v_count--;
			release_vnode(vnp);
		}
	}
	else runlock(&vn->v_vnodelock);
}

void delete_vnode(struct vnode *vn) { vn->v_op->vop_destroy(vn); }

int read_block(char device, unsigned int blk, char *data)
{
	struct buffer_header *bhead;
	
	bhead = bread(device,blk);

	if (bhead->io_status != 0)
	{
		brelease(bhead);
		return EDISK_READ;
	}
	bcopy(bhead->buff, data, 512);
	brelease(bhead);

	return 1;
}

int syscall_writeblocks(char device, unsigned int startblk, int count, char *buf)
{
	int i;

	for (i=0; i<count; i++)
		write_block(device, i + startblk, &buf[i*512]);

	return 0;
}

int syscall_readblocks(char device, unsigned int startblk, int count, char *buf)
{
	int i;

	for (i=0; i<count; i++)
		read_block(device, i + startblk, &buf[i*512]);

	return 0;
}

int write_block(char device, unsigned int blk, char *data)
{
	struct buffer_header *bhead;

	bhead = getblk(device, blk);
	bcopy(data, bhead->buff, 512);
	bhead->flags = bhead->flags | BUFF_WRITEDELAY | BUFF_DATAVALID;
	brelease(bhead);

	return 1;
}

void increment_ref_count(int dir_handle)
{
	void increment_sockref_count(int sockds);

	if (dir_handle >= 0 && dir_handle < FTABLE_SIZE)
	{
		get_vnode(openFileTable[dir_handle].fte_vn);
		openFileTable[dir_handle].fte_vn->v_count++;
		openFileTable[dir_handle].fte_refcount++;
		release_vnode(openFileTable[dir_handle].fte_vn);
	}
	else 	increment_sockref_count(dir_handle);
}

void decrement_ref_count(int handle)
{
	struct FileTableEntry *dirf;
	void decrement_sockref_count(int sockds);

	if (handle >=0 && handle < FTABLE_SIZE)
	{
		dirf = &openFileTable[handle];

		/* Update the entry in the parent directory 
		 * if this directory is modified 
		 */

		get_vnode(dirf->fte_vn);
		dirf->fte_vn->v_count--;
		dirf->fte_refcount--;
		release_vnode(dirf->fte_vn);
			
		/* Release the file table slot */
		if (dirf->fte_refcount == 0)
			dirf->fte_flags = FTE_UNUSED;
	}
	else decrement_sockref_count(handle);

	return;
}

struct vnode * lookup_path(const char *path) // Opening the directory(s)...
{
	int i, j;
	struct vnode *curdir_handle, *new_handle;
	char name_comp[NAME_MAX]; //, conv_name[11];

	// Open the directory component
	i =0;
	if (path[0] == '/' || path[0] == '\\')	// Absolute path
	{
		curdir_handle = get_rootdir();
		i = 1;
	}
	else curdir_handle = get_curdir();

	curdir_handle->v_count++;
	while (path[i] != 0)
	{
		j = 0;
		
		while (j < NAME_MAX && path[i] != 0 && path[i] != '/' && path[i] != '\\')
		{
			name_comp[j] = path[i];
			j++; i++;
		}

		if (path[i] != 0) i++; // skip '/' or '\'

		name_comp[j] = 0;

		if (strcmp(name_comp,".") == 0) continue;
		else if (strcmp(name_comp,"..") == 0)
		{
			// IS this correct?
			new_handle = get_pdir(curdir_handle);
		}
		else
			new_handle = curdir_handle->v_op->vop_lookup(curdir_handle, name_comp);

		release_vnode(curdir_handle);
		if (new_handle == NULL) return NULL;

		curdir_handle = new_handle;
		curdir_handle->v_count++;
	}

	curdir_handle->v_count--;
	// Successfully opened the path
	return curdir_handle;
}

void parse_path(const char *path, char *dir_path, char *name_comp)
{
	int j,i=0;

	// Extract name component
	j = strlen(path)-1;

	while (j >= 0 && (path[j] != '/' && path[j] != '\\')) j--;
	strncpy(name_comp, &path[j+1], NAME_MAX);
	// remaining is dir_path.
	for (i=0; i<=j; i++)
		dir_path[i] = path[i];
	dir_path[i] = 0;
}

struct vnode * get_rootdir(void)
{
	get_vnode((struct vnode *)current_process->proc_fileinf->root_dir);
	return (struct vnode *)current_process->proc_fileinf->root_dir;
}

struct vnode * get_curdir(void)
{
	get_vnode((struct vnode *)current_process->proc_fileinf->current_dir);
	return (struct vnode *)current_process->proc_fileinf->current_dir;
}

struct vnode * get_pdir(struct vnode *vn)
{
	get_vnode(vn->v_parent);
	return vn->v_parent;
}

int valid_handle(int handle, int flags)
{
	if (handle >= 0 && handle < FTABLE_SIZE)
	{
		struct FileTableEntry *dirf = &openFileTable[handle];
		return (dirf->fte_flags & flags);
	}
	else if ((handle == -1 && (flags & FTFLAGS_READ)) ||
		 ((handle == -2 || handle == -3) && (flags & FTFLAGS_WRITE)) ||
		 (handle >= SOCKHANDLEBEGIN && handle < SOCKHANDLEEND))
		return 1; // stdin or stdout or stderr special handle nos or a
			  // socket handle.

	return EINVALID_HANDLE;
}

int syscall_open(const char *path, int type, mode_t mode)
{
	int err;
	char name_comp[NAME_MAX], dir_path[PATH_MAX+1];
	struct vnode *vn, *fvn;
	struct stat stbuf;
	int fhandle = EMAXFILES, handle;
	struct FileTableEntry *dirf;

	if (strlen(path) > PATH_MAX) return ELONGPATH;

	parse_path(path, dir_path, name_comp);

	if (dir_path[0] != 0)
	{
		vn = lookup_path(dir_path);
		if (vn == NULL) return EPATH_NOT_EXISTS;	// Error
	}
	else vn = get_curdir();

	// Last file name component.
	fvn = vn->v_op->vop_lookup(vn, name_comp);
	err = EPATH_NOT_EXISTS; // May be, used next fvn == null check 
	if ((fvn == NULL) && ((type & (O_CREAT | O_WRONLY | O_RDWR)) != 0)) // Not exists, creation can be done.
	{
		fvn = vn->v_op->vop_create(vn, name_comp, mode);
		if (fvn == NULL) err = EFILE_CREATE_ERROR;
	}

	if (fvn == NULL) 
	{
		release_vnode(vn);
		return err; // Error
	}

	// Check for open permissions
	fvn->v_op->vop_getattr(fvn, &stbuf);	
	if (stbuf.st_mode & S_IFDIR)
	{
		release_vnode(vn);
		release_vnode(fvn);
		return EDIR_OPEN_AS_A_FILE;	
	}
	
	if (type == 0) type = O_RDONLY;
	else type = type & (O_RDONLY | O_WRONLY | O_CREAT | O_RDWR | O_APPEND | O_TRUNC); // Clear invalid bits

	// Check permissions, simple check only
	if ((type & O_WRONLY || type & O_RDWR || type & O_APPEND) && ((stbuf.st_mode & 0222) == 0))
	{
		release_vnode(vn);
		release_vnode(fvn);
		return EACCESS;
	}

	// Allocate & initialize open file structure.
	if ((handle = alloc_ftab_slot()) >= 0)
	{
		/* Initialize file table entry */
		dirf = &openFileTable[handle];

		dirf->fte_mode = stbuf.st_mode; // All permissions on dir
		dirf->fte_opmode = mode;
		dirf->fte_vn = fvn;
		fvn->v_count++;
		dirf->fte_size = stbuf.st_size;

		if (type & O_TRUNC || type & O_WRONLY)
		{
			if (stbuf.st_size != 0)
				fvn->v_op->vop_trunc(fvn); 
			dirf->fte_curposition = 0;
			dirf->fte_size = 0;
			dirf->fte_flags |= FTFLAGS_CHANGED | FTFLAGS_WRITE;
		}
		else if (type == O_APPEND)
		{
			dirf->fte_curposition = stbuf.st_size;
			dirf->fte_flags |= FTFLAGS_APPEND;
		}
		else 
		{
			/* Set the current position to normal values */
			dirf->fte_curposition = 0;
		}

		/* Set the read/write flags  properly*/
		if (type & O_RDONLY || type & O_RDWR)
			dirf->fte_flags |= FTFLAGS_READ;
		if (type & O_WRONLY || type & O_RDWR)
			dirf->fte_flags |= FTFLAGS_WRITE;

		dirf->fte_flags = dirf->fte_flags | FTFLAGS_FILE;
	}
	else handle = ENO_FREE_FILE_TABLE_SLOT;

	release_vnode(vn);
	release_vnode(fvn);
	
	if (handle >= 0)
	{
		fhandle = alloc_process_file_handle(handle);
		if (fhandle < 0) decrement_ref_count(handle);
	}
	
	return fhandle; // may be failure or success
}

int syscall_close_socket(int sd);
int syscall_close(int fhandle)
{
	if (fhandle >= 0 && fhandle < MAXPROCFILES && current_process->proc_fileinf->fhandles[fhandle] >= 0)
	{
		// Check if it is a file descriptor or socket descriptor?

		if (current_process->proc_fileinf->fhandles[fhandle] >= 0 && current_process->proc_fileinf->fhandles[fhandle] < FTABLE_SIZE)
		{
			decrement_ref_count(current_process->proc_fileinf->fhandles[fhandle]);
			current_process->proc_fileinf->fhandles[fhandle]=-4; // unused
			return 0;
		}
		else if (current_process->proc_fileinf->fhandles[fhandle] >= SOCKHANDLEBEGIN && current_process->proc_fileinf->fhandles[fhandle] < SOCKHANDLEEND)
		{
			// It must be a socket descriptor
			return syscall_close_socket(fhandle);
		}
	}
	return EINVALID_HANDLE;
}

int syscall_read(int handle, void *buf, size_t nbytes)
{
	int retval, oldoffset;
	extern int kbd_read(char *buf, int len);
	// Do some checking
	if ((handle >= 0 && handle < MAXPROCFILES) && valid_handle(current_process->proc_fileinf->fhandles[handle], FTFLAGS_READ))
	{
		// If a file handle?
		if ((current_process->proc_fileinf->fhandles[handle] >= 0) && (current_process->proc_fileinf->fhandles[handle] < FTABLE_SIZE))
		{
			handle=current_process->proc_fileinf->fhandles[handle];

			get_vnode(openFileTable[handle].fte_vn); 
			retval = openFileTable[handle].fte_vn->v_op->vop_read(openFileTable[handle].fte_vn, openFileTable[handle].fte_curposition, (char *)buf, nbytes); 
			if (retval > 0) 
			{
				oldoffset = openFileTable[handle].fte_curposition;
				openFileTable[handle].fte_curposition = retval;
				retval =  (retval - oldoffset);
			}
			release_vnode(openFileTable[handle].fte_vn); 
			return retval;
		}
		// If it is stdin
		if (current_process->proc_fileinf->fhandles[handle] == -1)
			return kbd_read(buf, nbytes);

		// If it is a socket..
		if ((current_process->proc_fileinf->fhandles[handle] >= SOCKHANDLEBEGIN) && (current_process->proc_fileinf->fhandles[handle] < SOCKHANDLEEND)) 
			return syscall_recv(handle, buf, nbytes, 0);
	}

	return EINVALID_HANDLE;
}

int syscall_write(int handle, void *buf, size_t nbytes)
{
	int retval, oldoffset;

	// Do some checking

	if ((handle >= 0 && handle < MAXPROCFILES) && valid_handle(current_process->proc_fileinf->fhandles[handle], FTFLAGS_WRITE))
	{
		// If a file handle?
		if ((current_process->proc_fileinf->fhandles[handle] >= 0) && (current_process->proc_fileinf->fhandles[handle] < FTABLE_SIZE))
		{
			handle=current_process->proc_fileinf->fhandles[handle];
			get_vnode(openFileTable[handle].fte_vn); 
			retval = openFileTable[handle].fte_vn->v_op->vop_write(openFileTable[handle].fte_vn, openFileTable[handle].fte_curposition, (char *)buf, nbytes); 
			if (retval > 0) 
			{
				oldoffset = openFileTable[handle].fte_curposition;
				openFileTable[handle].fte_curposition = retval;
				retval =  (retval - oldoffset);
			}
			release_vnode(openFileTable[handle].fte_vn); 
			return retval;
		}

		// If it is stdout
		if (current_process->proc_fileinf->fhandles[handle] == -2)
		{
			output_proc(buf, nbytes);
			return nbytes;
		}

		// If it is a socket..
		if ((current_process->proc_fileinf->fhandles[handle] >= SOCKHANDLEBEGIN) && (current_process->proc_fileinf->fhandles[handle] < SOCKHANDLEEND)) 
			return syscall_send(handle, buf, nbytes, 0);
	}

	return EINVALID_HANDLE;
}

int syscall_creat(const char *path, mode_t mode)
{
	char name_comp[NAME_MAX], dir_path[PATH_MAX+1];
	int fhandle = EMAXFILES, handle;
	struct vnode *vn, *fvn;
	struct FileTableEntry *dirf;
	struct stat stbuf;


	if (strlen(path) > PATH_MAX) return ELONGPATH;

	parse_path(path, dir_path, name_comp);

	if (dir_path[0] != 0)
	{
		vn = lookup_path(dir_path);

		if (vn == NULL) return EPATH_NOT_EXISTS;	// Error
	}
	else vn = get_curdir();

	// Last file name component.
	fvn = vn->v_op->vop_lookup(vn, name_comp);

	// If existing check for permissions
	if (fvn != NULL)
	{
		fvn->v_op->vop_getattr(fvn, &stbuf);	
		if ((stbuf.st_mode & S_IFDIR) || ((stbuf.st_mode & 0222) == 0))
		{
			release_vnode(fvn);
			release_vnode(vn);
			return EACCESS;	
		}
		else // Truncate the existing file
			fvn->v_op->vop_trunc(fvn); //, name_comp, mode);
	}
	else // create new entry
		fvn = vn->v_op->vop_create(vn, name_comp, mode);

	if (fvn == NULL)
	{
		release_vnode(vn);
		return EFILE_CREATE_ERROR; // Error
	}

	// Allocate & initialize open file structure.
	if ((handle = alloc_ftab_slot()) >= 0)
	{
		/* Initialize file table entry */
		dirf = &openFileTable[handle];

		dirf->fte_mode = stbuf.st_mode; // All permissions on dir
		dirf->fte_opmode = mode;
		dirf->fte_vn = fvn;
		fvn->v_count++;
		dirf->fte_size = 0;
		dirf->fte_curposition = 0;
		dirf->fte_flags |= FTFLAGS_CHANGED | FTFLAGS_WRITE;

		dirf->fte_flags = dirf->fte_flags | FTFLAGS_FILE;
	}
	else handle = ENO_FREE_FILE_TABLE_SLOT;
	release_vnode(fvn);
	release_vnode(vn);

	if (handle >= 0)
	{
		fhandle = alloc_process_file_handle(handle);
		if (fhandle < 0)
			decrement_ref_count(handle);
	}
	return fhandle; // may be failure or success
}

off_t syscall_lseek(int fildes, off_t offset, int whence)
{
	struct FileTableEntry *dirf;
	int pos, res;

	if (fildes >= 0 && fildes < MAXPROCFILES && current_process->proc_fileinf->fhandles[fildes] >= 0 && (whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END))
	{
		dirf = &openFileTable[current_process->proc_fileinf->fhandles[fildes]];
		get_vnode(dirf->fte_vn);
		pos = dirf->fte_curposition; // old position

		if (whence == SEEK_SET)
		{
			if (offset >= 0 && offset <= dirf->fte_size)
				dirf->fte_curposition = offset;
		}
		else if (whence == SEEK_CUR)
		{
			if ((dirf->fte_curposition + offset >= 0) && (dirf->fte_curposition + offset <= dirf->fte_size))
				dirf->fte_curposition += offset;
		}
		else if (whence == SEEK_END)
		{
			//  Must be changed to accept even positive pos value
			//  so that sparse file can be created
			if ((offset < 0) && (dirf->fte_size + offset >= 0))
				dirf->fte_curposition = dirf->fte_size + offset;
		}
		res = dirf->fte_vn->v_op->vop_seek(dirf->fte_vn, dirf->fte_curposition, offset, whence);
		release_vnode(dirf->fte_vn);
		return res;
		
	}
	else	return EINVALID_SEEK;
}

int syscall_dup(int oldfd)
{	
	int i;
	struct files_info *finf;

	if (!(oldfd >= 0 && oldfd < MAXPROCFILES))
		return EINVALID_HANDLE;

	finf = current_process->proc_fileinf;
	_lock(&finf->fhandles_lock);	
	if (finf->fhandles[oldfd] <= -4) 
	{
		unlock(&finf->fhandles_lock);	
		return EINVALID_HANDLE;
	}
	// Store this into task file handle table and return its index
	for (i=0; i<MAXPROCFILES; i++)
	{
		if (finf->fhandles[i] == -4)
		break;
	}	
	if (i < MAXPROCFILES) 
	{
		finf->fhandles[i] = finf->fhandles[oldfd];
		increment_ref_count(finf->fhandles[oldfd]);
	}
	else i = ENO_FREE_FILE_TABLE_SLOT;
	unlock(&finf->fhandles_lock);	
	
	return i;
}

int syscall_dup2(int oldfd, int newfd)
{
	struct files_info *finf;

	if (!(oldfd >= 0 && oldfd < MAXPROCFILES)) return EINVALID_HANDLE;

	if (!(newfd >= 0 && newfd < MAXPROCFILES)) return EINVALID_HANDLE;

	finf = current_process->proc_fileinf;
	
	_lock(&finf->fhandles_lock);	
	if (finf->fhandles[oldfd] <= -4) 
	{
		unlock(&finf->fhandles_lock);	
		return EINVALID_HANDLE;
	}

	if (finf->fhandles[newfd] >= 0) // Already opened file handle, close it
		syscall_close(newfd);
	// Store this into task file handle table and return its index
	finf->fhandles[newfd] = finf->fhandles[oldfd];
	increment_ref_count(finf->fhandles[oldfd]);
	unlock(&finf->fhandles_lock);	
	
	return newfd;
}

DIR * syscall_opendir(const char *path, DIR *dir)
{
	char name_comp[NAME_MAX], dir_path[PATH_MAX+1];
	struct vnode *vn, *fvn;
	struct stat stbuf;
	int fhandle = EMAXFILES, handle;
	struct FileTableEntry *dirf;

	if (strlen(path) > PATH_MAX) return NULL;

	parse_path(path, dir_path, name_comp);

	if (dir_path[0] != 0)
	{
		vn = lookup_path(dir_path);

		if (vn == NULL) return NULL; // EPATH_NOT_EXISTS;	// Error
	}
	else vn = get_curdir();

	// Last name component.
	fvn = vn->v_op->vop_lookup(vn, name_comp);
	if (fvn == NULL)
	{
		release_vnode(vn);
		return NULL; // EPATH_NOT_EXISTS; 
	}

	// Check for open permissions, simple check only
	fvn->v_op->vop_getattr(fvn, &stbuf);	
	if ((!(stbuf.st_mode & S_IFDIR)) || ((stbuf.st_mode & 0111) == 0))
	{
		release_vnode(fvn);
		release_vnode(vn);
		return NULL; // EACCESS;
	}
	
	// Allocate & initialize open dir structure.
	if ((handle = alloc_ftab_slot()) >= 0)
	{
		/* Initialize file table entry */
		dirf = &openFileTable[handle];

		dirf->fte_mode = stbuf.st_mode; // All permissions on dir
		dirf->fte_opmode = O_RDONLY;
		dirf->fte_vn = fvn;
		fvn->v_count++;
		dirf->fte_size = stbuf.st_size;
		dirf->fte_curposition = 0;
		dirf->fte_flags |= FTFLAGS_DIR;
	}
	else handle = ENO_FREE_FILE_TABLE_SLOT;
	release_vnode(fvn);
	release_vnode(vn);
	
	if (handle >= 0)
	{
		fhandle = alloc_process_file_handle(handle);
		if (fhandle < 0)
			decrement_ref_count(handle);
	}
	
	dir->dir_handle = fhandle;// may be failure or success
	dir->dir_offset = 0;

	return dir;
}

int syscall_closedir(DIR *dir)
{
	if (dir != NULL) return syscall_close(dir->dir_handle);
	else return ENOT_A_DIRECTORY;
}

struct dirent * syscall_readdir(DIR *dir)
{
	struct FileTableEntry *dirf;
	int handle;
	struct vnode *vn;
	struct dirent *de;

	if (dir == NULL) return NULL; //EARG;
	handle = dir->dir_handle;

	if (handle >= 0 && handle < MAXPROCFILES  && current_process->proc_fileinf->fhandles[handle] >= 0)
	{
		dirf = &openFileTable[current_process->proc_fileinf->fhandles[handle]];
		if ((dirf->fte_flags & FTFLAGS_DIR) == 0) return NULL; //EINVALIDHANDLE;// Not a dir
		vn = dirf->fte_vn;
		get_vnode(vn);
		if (vn->v_op->vop_readdir(vn, dirf->fte_curposition, &dir->dir_dent, NULL) == 0) // Found
		{
			dirf->fte_curposition = dir->dir_dent.d_off + dir->dir_dent.d_reclen;
			de = &(dir->dir_dent);
		}
		else	de = NULL;
		
		release_vnode(vn);

		return de;
	}

	return NULL;
}

void syscall_seekdir(DIR *dir, off_t offset)
{
	struct FileTableEntry *dirf;
	int handle;

	if (dir == NULL) return; // EARG;
	handle = dir->dir_handle;
	if (handle >= 0 && handle < MAXPROCFILES  && current_process->proc_fileinf->fhandles[handle] >= 0)
	{
		dirf = &openFileTable[handle];
		if ((dirf->fte_flags & FTFLAGS_DIR) == 0) return; //EINVALIDHANDLE;// Not a dir
		if (offset >= 0)	
		{
			dirf->fte_curposition = offset;
			return;
		}
		else return; // EINVALIDOFFSET;
	}
	else return; // EINVALIDHANDLE 
}

off_t syscall_telldir(DIR *dir)
{
	struct FileTableEntry *dirf;
	int handle;

	if (dir == NULL) return EARG;
	handle = dir->dir_handle;
	if (handle >= 0 && handle < MAXPROCFILES  && current_process->proc_fileinf->fhandles[handle] >= 0)
	{
		dirf = &openFileTable[handle];
		if ((dirf->fte_flags & FTFLAGS_DIR) == 0) return EINVALID_HANDLE;// Not a dir
		return dirf->fte_curposition;
	}
	else return EINVALID_ARGUMENT;
}
void syscall_rewinddir(DIR *dir)
{
	struct FileTableEntry *dirf;
	int handle;

	if (dir == NULL) return; // EARG;
	handle = dir->dir_handle;
	if (handle >= 0 && handle < MAXPROCFILES  && current_process->proc_fileinf->fhandles[handle] >= 0)
	{
		dirf = &openFileTable[handle];
		if ((dirf->fte_flags & FTFLAGS_DIR) == 0) return; // EINVALIDHANDLE;// Not a dir
		dirf->fte_curposition = 0;
	}

	return;
}
/*
int scandir(const char *dir, struct dirent ***namelist,
	int(*filter)(const struct dirent *),
	int(*compare)(const struct dirent **, const struct dirent **))
{
	int i, count=0, capacity = 50;
	struct dirent **t1, **t2;
	DIR *dirhandle;
	struct dirent *ent;

	if ((dirhandle = opendir(dir)) == NULL) return -1;

	t1 = (struct dirent **) malloc(sizeof(struct dirent *) * capacity);
	while ((ent = readdir(dirhandle)) != NULL)
	{
		if (filter(ent) == 0)
			continue;


		if (count >= capacity)
		{	// Increase capacity
			capacity += 20;
			t2 = (struct dirent **) malloc(sizeof(struct dirent *) * capacity);
			for (i=0; i<count; i++)
				* (t2 + i) = * (t1 + i);
			
			free(t1);
			t1 = t2;
		}

		* (t1 + count) = (struct dirent *) malloc(sizeof(struct dirent));
		bcopy(ent, *(t1+count), sizeof(struct dirent));
		count++;
	}
	
	qsort(t1, count, sizeof(struct dirent *), compare);

	*namelist = t1;
	return count;
}

int select(const struct dirent *dent)
{
	return 1;
}

int compare(const struct dirent *ent1, const struct dirent *ent2)
{
	struct dirent ** t;

	return strcmp((ent1)->d_name, (ent2)->d_name);
}
*/

int syscall_mkdir(const char *path, mode_t mode)
{
	char name_comp[NAME_MAX], dir_path[PATH_MAX+1];
	int err;
	struct vnode *vn, *fvn;


	if (strlen(path) > PATH_MAX) return ELONGPATH;

	parse_path(path, dir_path, name_comp);

	if (dir_path[0] != 0)
	{
		vn = lookup_path(dir_path);
		if (vn == NULL)
			return EPATH_NOT_EXISTS;	// Error
	}
	else vn = get_curdir();

	// Last dir name component.
	fvn = vn->v_op->vop_lookup(vn, name_comp);

	// If existing ...
	if (fvn != NULL) err = EDUPLICATE_ENTRY;	
	else // create new entry
		err = vn->v_op->vop_mkdir(vn, name_comp, mode);

	if (fvn != NULL) release_vnode(fvn);
	release_vnode(vn);
	return err;	
}

int syscall_rmdir(const char *path)
{
	char name_comp[NAME_MAX], dir_path[PATH_MAX+1];
	int err;
	struct stat stbuf;
	struct vnode *vn, *fvn;


	if (strlen(path) > PATH_MAX) return ELONGPATH;

	parse_path(path, dir_path, name_comp);

	if (dir_path[0] != 0)
	{
		vn = lookup_path(dir_path);
		if (vn == NULL)
			return EPATH_NOT_EXISTS;	// Error
	}
	else vn = get_curdir();

	fvn = vn->v_op->vop_lookup(vn, name_comp);
	// If not existing ...
	if (fvn == NULL) err = EPATH_NOT_EXISTS;	
	else // delete entry
	{
		// Check for permissions
		fvn->v_op->vop_getattr(fvn, &stbuf);
		if ((!(stbuf.st_mode & S_IFDIR)) || ((stbuf.st_mode & 0222) == 0))
			err = EACCESS;
		else // Delete dir name component.
			err = vn->v_op->vop_rmdir(vn, name_comp);
	}
	
	if (fvn != NULL) release_vnode(fvn);
	release_vnode(vn);

	return err;	
}

int syscall_chdir(const char *path)
{
	struct vnode *vn;

	if (strlen(path) > PATH_MAX) return ELONGPATH;

	_lock(&(current_process->proc_fileinf->fhandles_lock));
	vn = lookup_path(path);
	if (vn == NULL)
	{
		unlock(&(current_process->proc_fileinf->fhandles_lock));
		return EPATH_NOT_EXISTS;
	}
	else 
	{
		vn->v_count++;
		release_vnode(vn);
		get_vnode((struct vnode *)current_process->proc_fileinf->current_dir);
		current_process->proc_fileinf->current_dir->v_count--;
		release_vnode((struct vnode *)current_process->proc_fileinf->current_dir);
		current_process->proc_fileinf->current_dir = vn;
		unlock(&(current_process->proc_fileinf->fhandles_lock));
		return 0; // Success
	}
}

int syscall_fchdir(int fd)
{
	struct FileTableEntry * dirf;
	struct vnode *vn;
	int err;

	_lock(&current_process->proc_fileinf->fhandles_lock);
	if (fd >= 0 && fd < MAXPROCFILES  && current_process->proc_fileinf->fhandles[fd] >= 0)
	{
		dirf = &openFileTable[current_process->proc_fileinf->fhandles[fd]];
		if (dirf->fte_flags & FTFLAGS_DIR) 
		{
			vn = dirf->fte_vn;
			get_vnode(vn);
			vn->v_count++;
			release_vnode(vn);
			get_vnode((struct vnode *)current_process->proc_fileinf->current_dir);
			current_process->proc_fileinf->current_dir->v_count--;
			release_vnode((struct vnode *)current_process->proc_fileinf->current_dir);
			current_process->proc_fileinf->current_dir = vn;
			err = 0; // Success
		}
		else	err = EINVALID_ARGUMENT;
	}
	else	err = EINVALID_ARGUMENT;
	unlock(&current_process->proc_fileinf->fhandles_lock);
	return err;
}

int syscall_unlink(const char *path)
{
	char name_comp[NAME_MAX], dir_path[PATH_MAX+1];
	int err;
	struct stat stbuf;
	struct vnode *vn, *fvn;


	if (strlen(path) > PATH_MAX) return ELONGPATH;

	parse_path(path, dir_path, name_comp);

	if (dir_path[0] != 0)
	{
		vn = lookup_path(dir_path);
		if (vn == NULL)
			return EPATH_NOT_EXISTS;	// Error
	}
	else vn = get_curdir();

	fvn = vn->v_op->vop_lookup(vn, name_comp);
	// If not existing ...
	if (fvn == NULL) err = EPATH_NOT_EXISTS;	
	else // delete entry
	{
		// Check for permissions
		fvn->v_op->vop_getattr(fvn, &stbuf);
		if ((stbuf.st_mode & 0222) == 0)
			err = EACCESS;
		else if ((stbuf.st_mode & S_IFDIR) != 0)
			err = EDIR;
		else // Delete dir name component.
			err = fvn->v_op->vop_remove(fvn);
		release_vnode(fvn);
	}
	
	release_vnode(vn);

	return err;	
}

int syscall_rename(const char *a_oldpath, const char *a_newpath)
{
	char dir_path1[512], dir_path2[512];
	char name_comp1[NAME_MAX], name_comp2[NAME_MAX];
	int err;
	struct vnode *svn, *dvn, *fsvn, *fdvn;

	if (strlen(a_oldpath) > PATH_MAX || strlen(a_newpath) > PATH_MAX) return ELONGPATH;

	parse_path(a_oldpath, dir_path1, name_comp1);
	parse_path(a_newpath, dir_path2, name_comp2);

	if (dir_path1[0] != 0)
	{
		svn = lookup_path(dir_path1);
		if (svn == NULL) return EPATH_NOT_EXISTS;	// Error
	}
	else svn = get_curdir();

	if (dir_path2[0] != 0)
	{
		dvn = lookup_path(dir_path2);
		if (dvn == NULL)
		{
			release_vnode(svn);
			return EPATH_NOT_EXISTS;	// Error
		}
	}
	else dvn = get_curdir();
		

	// Check if source exists
	fsvn = svn->v_op->vop_lookup(svn, name_comp1);
	if (fsvn == NULL)
	{
		release_vnode(svn);
		release_vnode(dvn);
		return EPATH_NOT_EXISTS;	// Error
	}
	fdvn = dvn->v_op->vop_lookup(dvn, name_comp2);
	if (fdvn != NULL)
	{
		release_vnode(fdvn);
		release_vnode(fsvn);
		release_vnode(svn);
		release_vnode(dvn);
		return ETARGET_EXISTS;	// Error
	}
	// Check if devices are same or not	
	if (fdvn->v_dev != fsvn->v_dev)
	{
		printk("At preset this can only rename within the same fs.\n");
		release_vnode(fsvn);
		release_vnode(svn);
		release_vnode(dvn);
		return 0;
	}	

	// Otherwise rename
	err = fsvn->v_op->vop_rename(fsvn, dvn, name_comp2);	
	release_vnode(fsvn);
	release_vnode(svn);
	release_vnode(dvn);
	return err;
}

int syscall_stat(const char *path, struct stat *buf)
{
	char name_comp[NAME_MAX], dir_path[PATH_MAX+1];
	int err;
	struct vnode *vn, *fvn;


	if (strlen(path) > PATH_MAX) return ELONGPATH;

	parse_path(path, dir_path, name_comp);

	if (dir_path[0] != 0)
	{
		vn = lookup_path(dir_path);
		if (vn == NULL) return EPATH_NOT_EXISTS;	// Error
	}
	else vn = get_curdir();

	fvn = vn->v_op->vop_lookup(vn, name_comp);
	// If not existing ...
	if (fvn == NULL) err = EPATH_NOT_EXISTS;	
	else
	{
		err = fvn->v_op->vop_getattr(fvn, buf);
		release_vnode(fvn);
	}
		
	release_vnode(vn);
	return err;
}
	
	
int syscall_chmod(const char *pathname, mode_t mode)
{
	int len;
	char dir_path[512];
	struct stat stbuf;
	struct vnode *vn;
	
	len = strlen(pathname);
	if (len > 512) return ELONGPATH;

	stbuf.st_mode = mode;

	vn = lookup_path(dir_path);
	if (vn == NULL) return EPATH_NOT_EXISTS;	// Error

	vn->v_op->vop_setattr(vn, &stbuf);
	release_vnode(vn);

	return 0;
}

int syscall_getchar(void);
int syscall_getcharf(int fd)
{
	int syshandle;
	unsigned char c;
	int retval;

	if (fd >= 0 && fd < MAXPROCFILES)
	{
		syshandle = current_process->proc_fileinf->fhandles[fd];
		if (syshandle == -1) return syscall_getchar();
		else if (syshandle >= 0)
		{
			retval = syscall_read(fd, &c, 1);
			if (retval > 0) return c;
			else return retval;
		}
	}
	return EINVALID_HANDLE;
}

int syscall_ungetcharf(int c, int fd)
{
	int syshandle;
	int retval;
	extern int insertkb_char(int c);

	if (fd >= 0 && fd < 20)
	{
		syshandle = current_process->proc_fileinf->fhandles[fd];
		if (syshandle == -1) return insertkb_char(c);
		else if (syshandle > 0)
		{
			retval = syscall_lseek(fd, -1, SEEK_CUR);
			return retval;
		}
	}
	return EINVALID_HANDLE;
}

// Initialize first vfs structure. Create root vnode.
struct vnode * mountroot(void)
{
	struct vnode *rootvn;
	struct vfs *fatvfs;
	struct fat_dirent *dent;
	struct fat12_fsdata *fat12fsdat;

	lockobj_init(&vnlist_lock);	
	lockobj_init(&ftlck);	
	lockobj_init(&buffer_cache_lock);
	lockobj_init(&write_queue_lock);
	eventobj_init(&brelse_event);

	rootvn = (struct vnode *)kmalloc(sizeof(struct vnode) + sizeof(struct fat_dirent)); 
	fatvfs = (struct vfs *)kmalloc(sizeof(struct vfs) + sizeof(struct fat12_fsdata));

	if (rootvn == NULL || fatvfs == NULL)
	{
		printk("Memory allocation failure in rootvnode and fatvfs creation!");
	}
	// Initialize vfs fields
	fatvfs->vfs_next = NULL;
	fatvfs->vfs_op = &fat12_vfsops;
	fatvfs->vfs_vnodecovered = rootvn;
	fatvfs->vfs_fstype = FSTYPE_FAT12;
	fatvfs->vfs_dev = 0;
	vfs_list = fatvfs;

	// Initialize fat12 fs details.
	fat12fsdat = (struct fat12_fsdata *)fatvfs->vfs_data;
	fat12fsdat->bps = 512;
	fat12fsdat->spc = 1;
	fat12fsdat->resect = 1;
	fat12fsdat->nfat = 2;
	fat12fsdat->spt = 18;
	fat12fsdat->heads = 2;
	fat12fsdat->no_tracks = 80;
	fat12fsdat->total_sect = 2*18*80;
	fat12fsdat->no_rootdir_ent = 224;
	fat12fsdat->no_sect_fat = 9;
	lockobj_init(&fat12fsdat->fatlock);
	read_fat12(fatvfs, 0);

	// Initialize fat12 dir entry
	dent = (struct fat_dirent *)rootvn->v_data;
	dent->name[0] = '/'; dent->name[1] = 0;
	dent->attrib = FTYPE_DIR;
	dent->res[0] = 0;
	dent->date = 0;
	dent->time = 0;
	dent->start_cluster = -12;
	dent->fsize = 0;

	// Initialize root vnode
	rootvn->v_type = S_IFDIR;
	rootvn->v_flag = VNODE_ROOT;
	rootvn->v_count = 1;
	rootvn->v_vfsmountedhere = NULL; // This is root
	rootvn->v_op = &fat12_vnops;
	rootvn->v_vfsp = fatvfs;
	rootvn->v_dev = 0;
	rootvn->v_offset = 0;
	rootvn->v_no = -12;
	rlockobj_init(&rootvn->v_vnodelock);
	rootvn->v_parent = NULL;
	vnode_add_hashlist(rootvn);

	// global root dir vnode
	rootvnode = rootvn;	

	return (struct vnode *)rootvnode;
}


int vnode_add_hashlist(struct vnode *vn)
{
	int i;
	unsigned long oflags;

	i = (vn->v_no >= 0) ? (vn->v_no % VNODEHASHMAX) : ((-vn->v_no) % VNODEHASHMAX);

	CLI;
	spinlock(vnhashlist_lock);
	vn->v_prev = NULL;	
	vn->v_next = vnlist_hdr[i];
	if (vnlist_hdr[i] != NULL) vnlist_hdr[i]->v_prev = vn;
	vnlist_hdr[i] = vn;
	spinunlock(vnhashlist_lock);
	STI;

	return 0;
}

int vnode_del_hashlist(struct vnode *vn)
{
	int i;
	unsigned long oflags;

	i = (vn->v_no >= 0) ? (vn->v_no % VNODEHASHMAX) : ((-vn->v_no) % VNODEHASHMAX);

	CLI;
	spinlock(vnhashlist_lock);
	if (vn->v_prev != NULL) vn->v_prev->v_next = vn->v_next;
	else vnlist_hdr[i] = (struct vnode *)vn->v_next;

	if (vn->v_next != NULL) vn->v_next->v_prev = vn->v_prev;
	spinunlock(vnhashlist_lock);
	STI;
	return 0;
}
	
struct vnode * alloc_vnode(dev_t dev, int vno, struct vnode * pvn, int offset, size_t size)
{
	int i;
	struct vnode *vn;
	unsigned long oflags;

	// First search for the vnode
	i = (vno >= 0) ? (vno % VNODEHASHMAX) : ((-vno) % VNODEHASHMAX);

	_lock(&vnlist_lock);
	CLI;
	spinlock(vnhashlist_lock);
	vn = vnlist_hdr[i];

	while ((vn != NULL) && (!(vn->v_dev == dev && vn->v_no == vno && vn->v_parent == pvn && vn->v_offset == offset)))
		vn = (struct vnode *)vn->v_next;
	spinunlock(vnhashlist_lock);
	STI;

	if (vn == NULL)
	{
		// Create a new vnode
		vn = (struct vnode *)kmalloc(sizeof(struct vnode) + size);
	
		if (vn != NULL)
		{
			vn->v_dev = dev;
			vn->v_no = vno;
			vn->v_parent = pvn;
			vn->v_offset = offset;
			vn->v_count = 0;
			rlockobj_init(&vn->v_vnodelock);
			vn->v_vnodelock.rl_slq.sl_type = SYNCH_TYPE_RLOCK;
			vnode_add_hashlist(vn);
		}
		else printk("Malloc failure in vnode_alloc\n");
	}

	// Lock and return
	if (vn != NULL) 
	{
		rlock(&vn->v_vnodelock);
		vn->v_count++;
	}

	unlock(&vnlist_lock);

	return vn;
}

/*
 * Later must be extended to support larger size buffers that span multiple
 * disk blocks.
 */
void initialize_buffer_cache(void)
{
	int i;
	/*
	 * Initialising the buffer headers, hash list, write_q and
	 * free list.
	 */
	for (i=0; i<BUFF_HASHNUMBER; i++)
	 	hash_queue_buffers[i] =  NULL;
	for (i=0; i<MAX_BUFFERS; i++)
	{
		buffers[i].hash_q_prev = NULL;
		buffers[i].hash_q_next = NULL;
		if (i < MAX_BUFFERS-1)
			buffers[i].free_list_next = &buffers[i+1];
		if (i > 0)
			buffers[i].free_list_prev = &buffers[i-1];
		buffers[i].write_q_next = NULL;

		/*
		 * Flags and block numbers are ok.
		 */
		buffers[i].flags = 0;
		eventobj_init(&buffers[i].release_event);
	}
	buffers[0].free_list_prev = NULL;
	buffers[MAX_BUFFERS-1].free_list_next = NULL;
	free_buff_list_head = &buffers[0];
	free_buff_list_tail = &buffers[MAX_BUFFERS-1];
	delayed_write_q_head = NULL;
	delayed_write_q_tail = NULL;

	
}

static void enqueue_to_hashlist(int hash, struct buffer_header *b)
{
	b->hash_q_next = hash_queue_buffers[hash];
	b->hash_q_prev = NULL;
	if (hash_queue_buffers[hash] != NULL)
		hash_queue_buffers[hash]->hash_q_prev = b;
	hash_queue_buffers[hash] = (struct buffer_header *)b;
	return;
}


static void delete_from_hashlist(int hash, struct buffer_header * b)
{
	struct buffer_header * n,* p;

	n = (struct buffer_header *)b->hash_q_next;
	p = (struct buffer_header *)b->hash_q_prev;

	if (p == NULL && n == NULL) return; // Not in hash list

	if (p != NULL) p->hash_q_next = n;
	else hash_queue_buffers[hash] = (struct buffer_header *)n;
	if (n != NULL) n->hash_q_prev = p;
	return;
}


static void delete_from_freelist( struct buffer_header * b)
{
	struct buffer_header * n, * p;

	n = (struct buffer_header *)b->free_list_next;
	p = (struct buffer_header *)b->free_list_prev;
	if (free_buff_list_head != b) p->free_list_next = n;
	else free_buff_list_head =  n;

	if (free_buff_list_tail != b)	n->free_list_prev = p;
	else free_buff_list_tail = p;
	return;
}

static void append_to_freelist( struct buffer_header * b)
{
	b->free_list_next = NULL;
	b->free_list_prev = free_buff_list_tail;
	if (free_buff_list_tail != NULL) free_buff_list_tail->free_list_next = b;
	else free_buff_list_head = b;
		
	free_buff_list_tail = b;
	return;
}
	
static void prepend_to_freelist( struct buffer_header * b)
{
	b->free_list_next = free_buff_list_head;
	b->free_list_prev = NULL;
	if (free_buff_list_head != NULL) free_buff_list_head->free_list_prev = b;
	free_buff_list_head = b;
	return;
}

static void append_to_write_queue( struct buffer_header * b)
{
	_lock(&write_queue_lock);
	b->write_q_next = NULL;
	if (delayed_write_q_tail != NULL) delayed_write_q_tail->write_q_next = b;
	else /* No block is in the write queue already */
	{
		delayed_write_q_head = b;
		delayed_write_q_tail = b;
	}
	unlock(&write_queue_lock);
}

/*
 * Allocates a buffer block.
 */
struct buffer_header * getblk(char device,unsigned long blockno)
{
	struct buffer_header * b;
	char found_in_hashq;
	int hash_index = blockno % BUFF_HASHNUMBER;
	extern struct event_t sync_event;

	while (1)
	{
		_lock(&buffer_cache_lock);
		/* 
		 * Search buffer cache for this block.
		 */
		b = hash_queue_buffers[hash_index];
		found_in_hashq=0;

		while (b != NULL && !found_in_hashq)
		{
			if (b->device == device && b->blockno == blockno)
				found_in_hashq = 1;
			else
				b = (struct buffer_header *)b->hash_q_next;
		}

		if (found_in_hashq)
		{
			if (b->flags & BUFF_BUSY)
			{
				event_sleep(&(b->release_event), &buffer_cache_lock);
				unlock(&buffer_cache_lock);
				continue;
			}

			/*
			 * block found and not busy so allocate it.
			 * Remove from list of free buffers.
			 */
			b->flags |= BUFF_BUSY;
			delete_from_freelist(b);
			unlock(&buffer_cache_lock);

			return ((struct buffer_header *)b);
		}
		else
		{
			/*
			 * Remove a buffer from the free buffers list.
			 */
			if (free_buff_list_head == NULL)
			{
				event_wakeup(&sync_event);
				event_sleep(&brelse_event, &buffer_cache_lock);
				unlock(&buffer_cache_lock);
				continue;
			}
		
			b = (struct buffer_header *)free_buff_list_head;
			delete_from_freelist(b);

			/*
			 * Buffer contains delayed write data.
			 */
			if (b->flags & BUFF_WRITEDELAY)
			{
				append_to_write_queue(b);
				unlock(&buffer_cache_lock);
				continue;
			}

			/*
			 * Remove from old hash and add to new hash.
			 */
			delete_from_hashlist(b->blockno % BUFF_HASHNUMBER,b);
			b->blockno = blockno;
			b->device = device;
			b->flags = (b->flags | BUFF_BUSY) & (~BUFF_DATAVALID);
			enqueue_to_hashlist(hash_index,b);
			unlock(&buffer_cache_lock);
			return((struct buffer_header *)b);
		}
	}
}

void brelease( struct buffer_header * b)
{
	/* Add this buffer to free list */
	_lock(&buffer_cache_lock);
	if (b->flags & BUFF_DATAVALID) append_to_freelist(b);
	else prepend_to_freelist(b);

	b->flags = b->flags & (~BUFF_BUSY);

	/* Wakeup the waiting processes */
	event_wakeup(&brelse_event);	
	event_wakeup(&b->release_event);

	unlock(&buffer_cache_lock);

	return;
}

struct buffer_header* bread(char device,unsigned long blockno)
{
	struct buffer_header *b;
	struct floppy_request *dreq;

	/* Get a buffer block and see if data valid */
	b = getblk(device,blockno);
	if (b->flags & BUFF_DATAVALID) return b;

	/* Make a disk read request and wait for completion */
	dreq = enqueue_floppy_request(device, blockno, 1, (void *)b->buff, FLOPPY_READ);

	if (dreq->status >= 0)
	{
		b->flags = b->flags | BUFF_DATAVALID;
		b->io_status = 0;
	}
	else 
	{
		b->io_status = dreq->status;
		printk("Error bread : %d -- %d\n",blockno, b->io_status);
	}

	free_floppy_request(dreq);
	return b;
}

void write_fat(void);

void flush_buffers(void)
{
	struct buffer_header *b1,*b2, *b3;
	struct floppy_request *dr[20];
	int i, count = 0;

	_lock(&write_queue_lock);
	b1 = (struct buffer_header *)delayed_write_q_head;
	delayed_write_q_head = delayed_write_q_tail = NULL;
	unlock(&write_queue_lock);
	
	while (b1 != NULL)
	{
		b3 = getblk(b1->device, b1->blockno);
		dr[0] = enqueue_floppy_request(b3->device, b3->blockno, 1, b3->buff, FLOPPY_WRITE);
		b3->flags = b3->flags & (~BUFF_WRITEDELAY);
		free_floppy_request(dr[0]);
		brelease(b3);
		b2 = (struct buffer_header *)b1->write_q_next;
		b1->write_q_next = NULL;
		b1 = b2;
	}


	count = 0;
	for (i=0; i<MAX_BUFFERS; i++)
	{
		if ((buffers[i].flags & BUFF_WRITEDELAY) != 0)
		{
			b3 = getblk(buffers[i].device, buffers[i].blockno);
			dr[0] = enqueue_floppy_request(b3->device, b3->blockno, 1, b3->buff, FLOPPY_WRITE);
			b3->flags = b3->flags & (~BUFF_WRITEDELAY);
			free_floppy_request(dr[0]);
			brelease(b3);
		}
	}
}

