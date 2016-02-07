#include<ctype.h>
#include<string.h>
#include "fscalls.h"
#include "timer.h"

#define MAXDRIVE 1
struct files_info {
	int current_drive;
	int current_dir[MAXDRIVE];
	int root_dir[MAXDRIVE];
	int fhandles[20];
};


struct files_info *fileinf;
FILE *fp;

int initialize(void)
{
	int i;
	void *malloc(size_t size);

	fp = fopen("fimage","r+b");
	fileinf = (struct files_info *)malloc(sizeof(struct files_info));
	fileinf->current_drive = 0;
	for (i=3; i<20; i++)
		fileinf->fhandles[i] = -1;

	fileinf->root_dir[0] = mount_floppy();
	fileinf->current_dir[0] = fileinf->root_dir[0];
	return 0;
}

int closeall(void)
{
	unmount_floppy(fileinf->root_dir[0]);
	return 0;
}

#define DRIVE_MAX	1

extern struct DIR_FILE dir_file_list[];

int root_handle(int drive);
int get_curdir_handle(int drive);
int get_parent_handle(int handle);

int convert_name(char *source, char dest[11])
{
	int i, j;
	
	while (*source == ' ') source++; // skip spaces

	if (*source == 0) // Null string
		return -1;
	for (i=0; i<8; i++)
	{
		if (source[i] != '.' && source[i] != '\0')
			dest[i] = toupper(source[i]);
		else	break;
		
	}

	if (i == 8)	
	{
		// all 8 characters are copied, see the remaining
		if (source[8] != '.' && source[8] != 0)
			return -1;
	}

	if (source[i] == '\0') // not having extension
	{
		// fill all remaining (11) characters with spaces.
		for (j=i; j<11; j++)
			dest[j] = ' ';
		return 1;
	}

	if (source[i] == '.')
	{
		// fill with spaces
		for (j = i; j<8; j++)
			dest[j] = ' ';
		i++;
	}
	
	for ( ; j<11; j++)
	{
		if (source[i] != '\0')
			dest[j] = toupper(source[i++]);
		else dest[j] = ' ';
	}

	if (source[i] != '\0') return -1; // invalid name (extension > 3 chars)

	return 1;
}
	
int open_path(int drive, const char *path) // Opening the directory(s)...
{
	int i, j, curdir_handle, new_handle;
	char name_comp[13], conv_name[11];

	// Open the directory component
	i =0;
	if (path[0] == '/' || path[0] == '\\')	// Absolute path
	{
		curdir_handle = root_handle(drive);
		i = 1;
	}
	else curdir_handle = get_curdir_handle(drive);

	// Increment the reference count
	increment_ref_count(curdir_handle);

	while (path[i] != 0)
	{
		j = 0;
		
		while (j < 12 && path[i] != 0 && path[i] != '/' && path[i] != '\\')
		{
			name_comp[j] = path[i];
			j++; i++;
		}

		if (path[i] != 0) i++; 

		name_comp[j] = 0;

		if (strcmp(name_comp,".") == 0) continue;
		else if (strcmp(name_comp,"..") == 0)
		{
			new_handle = get_parent_handle(curdir_handle);
			if (new_handle >= 0)
				increment_ref_count(new_handle);
		}
		else
		{
			// Open that directory component
			if (convert_name(name_comp, conv_name) < 0)
			{
				close_dir(curdir_handle);
					return EINVALIDNAME; // Error
			}

			new_handle = open_dir(curdir_handle, conv_name);
		}

		if (new_handle < 0)
		{
			close_dir(curdir_handle);
			return new_handle; // Error
		}

		close_dir(curdir_handle); // Effectively decrease the
					  // reference count to 1 less.
		curdir_handle = new_handle;
	}

	// Successfully opened the path
	return curdir_handle;
}

void parse_path(const char *path, int *drive, char *dir_path, char *name_comp)
{
	int j,flag,i=0;

	strcpy(dir_path,"");
	strcpy(name_comp,"");
	flag=0;
	if (path[1] == ':')	// Drive name present
	{
		*drive = toupper(path[0]) - 'A';
		path = path + 2;
	}
	else *drive = fileinf->current_drive;
	
	if(path[0]=='.'&&path[1]=='.')
	{
		strcat(dir_path,"..");
		i=2;
		flag=2;
	}
	else if(path[0]=='.')
	{
		strcat(dir_path,".");
		i=1;
		if(path[1]==0) flag=2;
	}	

	// Check if path contains any dir names
	// No directory component
	for ( ; i<strlen(path); i++)
	{
		if (path[i] == '/' || path[i] == '\\')
		{
			flag = 1;
			break;
		}
	}

	if (flag == 1)
	{
		strcpy(dir_path, path);
		// remove the last component
		j = 13;
		for (i=strlen(dir_path); (j>=0 && i>=0 && dir_path[i] != '/' && dir_path[i] != '\\'); i--) 
			name_comp[--j] = dir_path[i];

		if (j<0) j = 0; // Long name. Incorrect results
		if (i == 0) // special case
			dir_path[1] = 0; // only root dir
		else	dir_path[i] = 0; // replace last / with null char

		for (i=0; i<=12; i++)
			name_comp[i] = name_comp[j++];
	}
	else if(flag==0)
	{
		strcpy(name_comp,path);
		dir_path[0] = 0;
	}
}

int root_handle(int drive)
{
	if (drive < DRIVE_MAX)
		return fileinf->root_dir[drive];
	else return EINVALID_DRIVE;
}

int get_curdir_handle(int drive)
{
	if (drive < DRIVE_MAX)
		return fileinf->current_dir[drive];
	else return EINVALID_DRIVE;
}
int get_parent_handle(int handle)
{
	if (handle >= 0)
	{
		struct DIR_FILE *dirf = &dir_file_list[handle];

		return dirf->parent_index;
	}
	else return EINVALID_FILE_HANDLE;
}
int valid_handle(int handle, int flags)
{
	if (handle >= 0)
	{
		struct DIR_FILE *dirf = &dir_file_list[handle];

		return (dirf->flags & flags);
	}
	else return EINVALID_FILE_HANDLE;
}

void set_drive_curdir(int drive, int handle)
{
	if (drive < DRIVE_MAX && handle >= 0)
	{
		fileinf->current_dir[drive] = handle;
	}
}

int syscall_open(const char *path, int type, mode_t mode)
{
	int i, drive;
	int curdir_handle, new_handle;
	char name_comp[13], conv_name[11], dir_path[501];

	if (strlen(path) > 500) return ELONGPATH;

	parse_path(path, &drive, dir_path, name_comp);

	if (dir_path[0] != 0)
	{
		curdir_handle = open_path(drive, dir_path);

		if (curdir_handle < 0)
			return curdir_handle;	// Error
	}
	else
	{
		curdir_handle = get_curdir_handle(drive);
		increment_ref_count(curdir_handle);
	}

	// Last file name component.
	if (convert_name(name_comp, conv_name) < 0)
	{
		close_dir(curdir_handle);
		return EINVALIDNAME; // Error
	}

	mode = mode & 0x3f; // Valid bits
	if (mode == 0) mode = 0x20;
	else	mode = mode & (~(FTYPE_DIR | FTYPE_VOLUME));

	if (type == 0) type = O_RDONLY;
	else type = type & (O_RDONLY | O_WRONLY | O_CREAT | O_RDWR | O_APPEND | O_TRUNC); // Clear invalid bits

	new_handle = open_file(curdir_handle, conv_name, type, mode);
	close_dir(curdir_handle);
	if (new_handle >= 0)
	{
		// Store this into task file handle table and return its index
		for (i=3; i<20; i++)
		{
			if (fileinf->fhandles[i] < 0)
			break;
		}	
		if (i < 20) 
		{
			fileinf->fhandles[i] = new_handle;
			new_handle = i;
		}
		else
		{
			close_file(new_handle);
			new_handle = ENO_FREE_FILE_TABLE_SLOT;
		}
	}
	return new_handle; // may be failure or success
}

int syscall_close(int fhandle)
{
	struct DIR_FILE *dirf; 

	if (fhandle >= 3 && fhandle < 20)
		dirf = &dir_file_list[fileinf->fhandles[fhandle]];
	else
		return EINVALID_FILE_HANDLE;

	if ((dirf->flags & FTABLE_FLAGS_FILE) != 0) 
	{
		close_file(fileinf->fhandles[fhandle]);
		fileinf->fhandles[fhandle]=-1;
		return 0;
	}
	else return EINVALID_FILE_HANDLE;
}

int syscall_read(int handle, void *buf, size_t nbytes)
{
	// Do some checking
	if ((handle >= 0 && handle < 20  && fileinf->fhandles[handle] >= 0) && valid_handle(fileinf->fhandles[handle], O_RDONLY | O_RDWR))
	{
		handle=fileinf->fhandles[handle];
		return read_file(handle, (char *)buf, nbytes);
	}
	else	return EINVALID_FILE_HANDLE;
}

int syscall_write(int handle, void *buf, size_t nbytes)
{
	// Do some checking

	if ((handle >= 0 && handle < 20  && fileinf->fhandles[handle] >= 0) && valid_handle(fileinf->fhandles[handle], O_WRONLY | O_RDWR | O_APPEND))
	{
		handle=fileinf->fhandles[handle];
		return write_file(handle, (char *)buf, nbytes);
	}
	else	return EINVALID_FILE_HANDLE;
}

int syscall_creat(const char *path, mode_t mode)
{
	int i, drive;
	int curdir_handle, new_handle;
	char name_comp[13], conv_name[11], dir_path[501];

	if (strlen(path) > 500) return ELONGPATH;

	parse_path(path, &drive, dir_path, name_comp);
	if (dir_path[0] != 0)
	{
		curdir_handle = open_path(drive, dir_path);

		if (curdir_handle < 0)
			return curdir_handle;	// Error
	}
	else
	{
		curdir_handle = get_curdir_handle(drive);
		increment_ref_count(curdir_handle);
	}

	// Last file name component.
	if (convert_name(name_comp, conv_name) < 0)
	{
		close_dir(curdir_handle);
		return EINVALIDNAME; // Error
	}

	mode = mode & 0x3f; // Valid bits
	if (mode == 0) mode = 0x20;
	else	mode = mode & (~(FTYPE_DIR | FTYPE_VOLUME));

	new_handle = create_file(curdir_handle, conv_name, mode);
	close_dir(curdir_handle);
	
        for (i=3; i<20; i++)
        {
                if (fileinf->fhandles[i] < 0)
                break;
        }
        if (i < 20)
        {
                fileinf->fhandles[i] = new_handle;
                new_handle = i;
        }
        else
        {
                close_file(new_handle);
                new_handle = ENO_FREE_FILE_TABLE_SLOT;
        }

	return new_handle; // may be failure or success
}


off_t syscall_lseek(int fildes, off_t offset, int whence)
{
	if (fildes >= 0 && fildes < 20 && fileinf->fhandles[fildes] >= 0 && (whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END))
		return seek_file(fileinf->fhandles[fildes], offset, whence);
	else	return EINVALID_SEEK;
}

int dup(int oldfd)
{	// Not yet ...
	return oldfd;
}

int dup2(int oldfd, int newfd)
{	// Not yet ...
	return oldfd;
}

int syscall_mkdir(const char *pathname, mode_t mode)
{
	int i, j, drive, res;
	int curdir_handle;
	char name_comp[13], conv_name[11], dir_path[501];

	if (strlen(pathname) > 500) return ELONGPATH;

	parse_path(pathname, &drive, dir_path, name_comp);

	if (name_comp[0] == 0 && strlen(dir_path) > 0)
	{
		// remove the last component
		if (dir_path[strlen(dir_path)-1] == '/' || dir_path[strlen(dir_path)-1] == '\\') dir_path[strlen(dir_path)-1] = 0;
		j = 13;
		for (i=strlen(dir_path); (j>=0 && i>=0 && dir_path[i] != '/' && dir_path[i] != '\\'); i--) 
			name_comp[--j] = dir_path[i];

		if (j<0) j = 0; // Long name. Incorrect results
		if (i == 0) // special case
			dir_path[1] = 0; // only root dir
		else	dir_path[i] = 0; // replace last / with null char

		for (i=0; i<=12; i++)
			name_comp[i] = name_comp[j++];
	}
	if (dir_path[0] != 0)
	{
		curdir_handle = open_path(drive, dir_path);

		if (curdir_handle < 0)
			return curdir_handle;	// Error
	}
	else
	{
		curdir_handle = get_curdir_handle(drive);
		increment_ref_count(curdir_handle);
	}

	// Last new dir name component.
	if (name_comp[0] == 0)
	{
		close_dir(curdir_handle);
		return EDUPLICATE_ENTRY;
	}

	if (convert_name(name_comp, conv_name) < 0)
	{
		close_dir(curdir_handle);
		return EINVALIDNAME; // Error
	}

	res = make_dir(curdir_handle, conv_name);
	close_dir(curdir_handle);

	if (res == 1) return 0; // Success
	else return res; // failure
}

int syscall_rmdir(const char *pathname)
{
	int i, j, drive, res;
	int curdir_handle;
	char name_comp[13], conv_name[11], dir_path[501];

	if (strlen(pathname) > 500) return ELONGPATH;

	parse_path(pathname, &drive, dir_path, name_comp);

	if (name_comp[0] == 0 && strlen(dir_path) > 0)
	{
		// remove the last component
		if (dir_path[strlen(dir_path)-1] == '/' || dir_path[strlen(dir_path)-1] == '\\') dir_path[strlen(dir_path)-1] = 0;
		j = 13;
		for (i=strlen(dir_path); (j>=0 && i>=0 && dir_path[i] != '/' && dir_path[i] != '\\'); i--) 
			name_comp[--j] = dir_path[i];

		if (j<0) j = 0; // Long name. Incorrect results
		if (i == 0) // special case
			dir_path[1] = 0; // only root dir
		else	dir_path[i] = 0; // replace last / with null char

		for (i=0; i<=12; i++)
			name_comp[i] = name_comp[j++];
	}
	if (dir_path[0] != 0)
	{
		curdir_handle = open_path(drive, dir_path);

		if (curdir_handle < 0)
			return curdir_handle;	// Error
	}
	else
	{
		curdir_handle = get_curdir_handle(drive);
		increment_ref_count(curdir_handle);
	}

	// Last new dir name component.
	if (convert_name(name_comp, conv_name) < 0)
	{
		close_dir(curdir_handle);
		return EINVALIDNAME; // Error
	}

	res = rm_dir(curdir_handle, conv_name);
	close_dir(curdir_handle);

	if (res == 1) return 0; // Success
	else return res; // failure
}

int syscall_chdir(const char *path)
{
	int drive;
	int curdir_handle, olddir_handle;

	if (strlen(path) > 500) return ELONGPATH;

	if (path[1] == ':')	// Drive name present
	{
		drive = toupper(path[0]) - 'A';
		path = path + 2;
	}
	else drive = fileinf->current_drive;

	if (drive !=fileinf->current_drive) return EINVALID_ARGUMENT;

	olddir_handle = get_curdir_handle(drive);
	curdir_handle = open_path(drive, path);

	if (curdir_handle < 0)
		return curdir_handle;	// Error

	set_drive_curdir(drive, curdir_handle);
	close_dir(olddir_handle);

	return 0; // Success
}

int fchdir(int fd)
{
	struct DIR_FILE * dirf = &dir_file_list[fd];
	int olddir_handle;

	if (dirf->flags & FTABLE_FLAGS_DIR) 
	{
		if (dirf->device == fileinf->current_drive)
		{
			olddir_handle = get_curdir_handle(fileinf->current_drive);
			increment_ref_count(fd);
			set_drive_curdir(dirf->device, fd);
			close_dir(olddir_handle);
			return 0;
		}
		else return EINVALID_ARGUMENT;
	}
	else return EINVALID_DIR_INDEX;
}

int syscall_unlink(const char *pathname)
{
	int drive;
	int curdir_handle;
	char name_comp[13], conv_name[11], dir_path[501];
	struct dir_entry dent;
	int err;

	if (strlen(pathname) > 500) return ELONGPATH;

	parse_path(pathname, &drive, dir_path, name_comp);

	if (dir_path[0] != 0)
	{
		curdir_handle = open_path(drive, dir_path);

		if (curdir_handle < 0)
			return curdir_handle;	// Error
	}
	else
	{
		curdir_handle = get_curdir_handle(drive);
		increment_ref_count(curdir_handle);
	}

	// Last file name component.
	if (convert_name(name_comp, conv_name) < 0)
	{
		err =  EINVALIDNAME; // Error
	}
	else if (find_entry(curdir_handle, conv_name, &dent) == 1)
	{	// Check whether it is a deletable file
		if ((dent.attrib & (FTYPE_READONLY | FTYPE_DIR | FTYPE_VOLUME)) == 0)
		{
			delete_dir_entry(curdir_handle, conv_name);
			free_cluster_chain(dent.start_cluster);
			err = 0;
		}
		else
		{
			err = EACCESS; // Error
		}
	}
	else 
	{
		err = EFILE_NOT_FOUND;
	}

	close_dir(curdir_handle);
	return err;
}

int syscall_rename(const char *a_oldpath, const char *a_newpath)
{
	int ohandle, nhandle;
	int drive1, drive2;
	char dir_path1[512], dir_path2[512];
	char name_comp1[13], name_comp2[13], conv_name1[11], conv_name2[11];
	char oldpath[512], newpath[512];
	struct dir_entry dent1, dent2;
	int exist1, exist2;
	struct DIR_FILE *dirf;
	int len1, len2;
	int i,t;

	len1 = strlen(a_oldpath);
	len2 = strlen(a_newpath);

	if (len1 > 512 || len2 > 512) return ELONGPATH;

	strcpy(oldpath,a_oldpath);
	strcpy(newpath,a_newpath);

	if (oldpath[len1-1] == '/' || oldpath[len1-1] == '\\') oldpath[len1-1] = '\0';
	if (newpath[len2-1] == '/' || newpath[len2-1] == '\\') newpath[len2-1] = '\0';
	parse_path(oldpath, &drive1, dir_path1, name_comp1);
	parse_path(newpath, &drive2, dir_path2, name_comp2);

	if (drive1 != drive2) return EDEVICE_DIFFERENT;

	nhandle = open_path(drive2, dir_path2);
	if (nhandle < 0) return nhandle;

	if (name_comp2[0] !='\0')
	{
		if (convert_name(name_comp2, conv_name2) < 0)
		{
			close_dir(nhandle);
			return EINVALIDNAME; // Error
		}

		exist2 = find_entry(nhandle, conv_name2, &dent2);
	}
	
	ohandle = open_path(drive1, dir_path1);
	if (ohandle < 0)
	{
		close_dir(nhandle);
		return ohandle;
	}
	if (name_comp1[0] != '\0')
	{
		if (convert_name(name_comp1, conv_name1) < 0)
		{
			close_dir(nhandle);
			close_dir(ohandle);
			return EINVALIDNAME; // Error
		}

		exist1 = find_entry(ohandle, conv_name1, &dent1);
	}

	// Check whether new path exists and is removable
	if ((exist2 == 1) && ((dent2.attrib & FTYPE_READONLY) || ((dent2.attrib & FTYPE_DIR) && (empty_dir(nhandle, &dent2) != 1))))
	{
		close_dir(nhandle);
		close_dir(ohandle);
		return ETARGET_EXISTS;
	}

	// Check if source exists and is movable
	if (exist1 != 1)
	{
		close_dir(nhandle);
		close_dir(ohandle);
		return EPATH_NOT_EXISTS;
	}
	if ((dent1.attrib & FTYPE_READONLY) != 0)
	{
		close_dir(nhandle);
		close_dir(ohandle);
		return EREADONLY;
	}
	// Check whether oldpath is not a subpath of newpath
	if ((dent1.attrib & FTYPE_DIR) && (ohandle != nhandle))
	{	
		t = nhandle;
		dirf = &dir_file_list[t];

		while (dirf->parent_index >= 0 && dirf->parent_index != ohandle)
		{
			t = dirf->parent_index;
			dirf = &dir_file_list[t];
		}
		
		if (dirf->parent_index == ohandle)
		{
			close_dir(nhandle);
			close_dir(ohandle);
			return EOLDPATH_PARENT_OF_NEWPATH;
		}
	}

	// Check if newpath already exists whether it is compatible or not
	if ((exist2 == 1) && (((dent1.attrib & FTYPE_DIR) != 0 && (dent2.attrib & FTYPE_DIR) == 0) || ((dent1.attrib & FTYPE_DIR) == 0 && (dent2.attrib & FTYPE_DIR) != 0))) 
	{
		close_dir(nhandle);
		close_dir(ohandle);
		return ESRC_DEST_NOT_SAME_TYPE;
	}

	// Remove destination entry if exists
	if (exist2 == 1)
	{
		if (dent2.attrib & FTYPE_DIR)
			syscall_rmdir(newpath);
		else	syscall_unlink(newpath);
	}

	// Add the source dir entry after changing the name
	// to destination directory
	bcopy( (char *)&dent1, (char *)&dent2, sizeof(struct dir_entry));
	for (i=0; i<11; i++)	// Both name and extension
		dent2.name[i] = conv_name2[i];

	t = add_dir_entry(nhandle, &dent2);
	if (t == 1)
	{
		delete_dir_entry(ohandle, dent1.name);
	}

	// Close the handles of parent directories
	close_dir(ohandle);
	close_dir(nhandle);
	
	if (t == 1) return 0;
	else	return t;

}

int syscall_stat(const char *file_name, struct stat *buf)
{
	int drive;
	int curdir_handle;
	char name_comp[13], conv_name[11], dir_path[501];
	struct dir_entry dent;
	int err, count;
	struct date_time dt;
	int clno;

	if (strlen(file_name) > 500) return ELONGPATH;

	parse_path(file_name, &drive, dir_path, name_comp);

	if (dir_path[0] != 0)
	{
		curdir_handle = open_path(drive, dir_path);

		if (curdir_handle < 0)
		{
			return curdir_handle;	// Error
		}
	}
	else
	{
		curdir_handle = get_curdir_handle(drive);
		increment_ref_count(curdir_handle);
	}

	// Last file name component.
	if (convert_name(name_comp, conv_name) < 0)
	{
		err =  EINVALIDNAME; // Error
	}
	else if (find_entry(curdir_handle, conv_name, &dent) == 1)
	{
		// Fill up the stat buf
		buf->st_dev = drive;
		buf->st_ino = 0;
		buf->st_mode = dent.attrib;
		buf->st_nlink = 1;
		buf->st_uid = buf->st_gid = 0;
		buf->st_rdev = 0;
		buf->st_size = dent.fsize;
		buf->st_blksize = 512;
		// Find time in seconds
		dt.seconds = (dent.time & 0x1f) * 2;
		dt.minutes = ((dent.time & 0x7e0) >> 5);
		dt.hours = (((unsigned short int)(dent.time & 0xf800)) >> 11);
		dt.date = dent.date & 0x1f;
		dt.month = ((dent.date & 0x1e0) >> 5);
		dt.year = (((unsigned short int)(dent.date & 0xfe00)) >> 9);

		buf->st_atime = buf->st_mtime = buf->st_ctime = date_to_secs(dt);

		// Find number of blocks
		count = 0;
		clno = dent.start_cluster;
		while (clno < 0xff8)
		{
			count++;
			clno = next_cluster_12(clno);
		}
		buf->st_blocks = count;
		err = 0;
	}
	else	err = EFILE_NOT_FOUND;
	
	close_dir(curdir_handle);
	return err;
}

int syscall_chattrib(const char *pathname, unsigned char attr_val)
{
	int nhandle,drive,len;
	char dir_path[512], oldpath[512];
	char name_comp[13],conv_name[11];
	struct dir_entry dent;
	int exist, res;

	len = strlen(pathname);

	if (len > 512) return ELONGPATH;

	strcpy(oldpath,pathname);

	if (oldpath[len-1] == '/' || oldpath[len-1] == '\\') oldpath[len-1] = '\0';
	parse_path(oldpath, &drive, dir_path, name_comp);


	nhandle = open_path(drive, dir_path);
	if (nhandle < 0) return nhandle;

	if (name_comp[0] !='\0')
	{
		if (convert_name(name_comp, conv_name) < 0)
		{
			close_dir(nhandle);
			return EINVALIDNAME; // Error
		}

		exist = find_entry(nhandle, conv_name, &dent);
	}
	
	// Check whether path exists 
	if (exist == 1)
	{
		dent.attrib = attr_val; // Change attribute
		res = update_dir_entry(nhandle, &dent);
	}
	else	res = exist; // Error code.

	close_dir(nhandle);
	return res;
}


