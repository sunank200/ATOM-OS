
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

#include "mklib.h"
#include "lock.h"
#include "fs.h"
#include "fat12.h"
#include "errno.h"
#include "buffers.h"
#include "process.h"
#include "vmmap.h"
#include "timer.h"
#include "alfunc.h"

extern volatile struct process *current_process;
extern volatile struct kthread *current_thread;
extern volatile unsigned int secs, millesecs;
extern struct lock_t vnlist_lock;
extern volatile struct vnode *rootvnode;

extern struct FileTableEntry openFileTable[FTABLE_SIZE];
static unsigned short int get_clval12(struct vfs *fs, int clno);
static void delete_cluster_from_chain(struct vnode *dvn, int cl);
static int update_dir_entry(struct vnode *dvn, struct fat_dirent *dent);
static void free_cluster_chain(struct vfs *fs, unsigned short int clno);
static void mark_time_date(struct fat_dirent *dent);
static int empty_dir12(struct vnode *dvn, struct fat_dirent *dent);

int fat12_vop_destroy(struct vnode *vn)
{
	struct fat_dirent *dent;

	dent = (struct fat_dirent *)vn->v_data;

	if ((vn->v_flag & FTFLAGS_CHANGED) && vn->v_count >= 0)
	{
		update_dir_entry(vn->v_parent, dent);
	}
	if (vn->v_count < 0) // Directory entry removed
	{
		if (dent->fsize != 0)
		{
			free_cluster_chain(vn->v_vfsp, dent->start_cluster);
		}
		// Free memory of vnode
		kfree(vn);
	}
	return 0;
}

static int convert_name(const char *source, char dest[11])
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
		if (source[8] != '.' && source[8] != 0) return -1;
	}

	if (source[i] == '\0') // not having extension
	{
		// fill all remaining (11) characters with spaces.
		for (j=i; j<11; j++) dest[j] = ' ';
		return 1;
	}

	if (source[i] == '.')
	{
		// fill with spaces
		for (j = i; j<8; j++) dest[j] = ' ';
		i++;
	}
	j = 8;	
	for ( ; j<11; j++)
	{
		if (source[i] != '\0') dest[j] = toupper(source[i++]);
		else	dest[j] = ' ';
	}

	if (source[i] != '\0') return -1; // invalid name (extension > 3 chars)

	return 1;
}

void read_fat12(struct vfs *fs, int dev)
{
	struct fat12_fsdata *fat12 = (struct fat12_fsdata *)fs->vfs_data;
	syscall_readblocks(dev, 1, fat12->no_sect_fat, (char *)fat12->fat);
	return;
}

static unsigned short int next_cluster_12(struct vfs *fs, int clno)
{
	unsigned short int nclno;
	/*
	 * Special handling for root directory. Blocks of
	 * root directory is assumed to have cluser numbers
	 * -12, -11, -10, ... 0, 1 (14 blocks).
	 */
	if (clno < 1) 		/* part of root directory */
		nclno = clno+1;
	else if (clno == 1)	/* Last cluster of root directory */
		nclno = 0xfff;
	else if (clno >= 2 && clno <= 0xff6)	/* Part of fat */
		nclno = get_clval12(fs, clno);
	else	nclno = 0;

	return nclno;
}

static void set_clval12(struct vfs *fs, int clno, unsigned short int val)
{
	struct fat12_fsdata *fat12 = (struct fat12_fsdata *)fs->vfs_data;
	int fat_index = (clno * 3) / 2;
	
	if (clno % 2 == 0)
	{
		fat12->fat[fat_index] = (val & 0xff);
		fat12->fat[fat_index+1] = (fat12->fat[fat_index+1] & 0xf0) | ((val >> 8) & 0x000f);
	}
	else
	{
		fat12->fat[fat_index] = (fat12->fat[fat_index] & 0x0f) | ((val << 4) & 0x00f0);
		fat12->fat[fat_index+1] = ((val >> 4) & 0x00ff);
	}
}

static unsigned short int get_clval12(struct vfs *fs, int clno)
{
	struct fat12_fsdata *fat12 = (struct fat12_fsdata *)fs->vfs_data;
	int fat_index = (clno * 3) / 2;
	unsigned short val1, val2, val;

	val1 = (unsigned short)fat12->fat[fat_index];
       	val2 = (unsigned short)fat12->fat[fat_index+1];
       	val = val1 | (val2 << 8);
	if (clno % 2 == 0) val = val & 0x0fff;
	else val = (val >> 4) & 0x0fff;
	return val;
}

static unsigned int alloc_block12(struct vfs *fs)
{
	struct fat12_fsdata *fat12 = (struct fat12_fsdata *)fs->vfs_data;
	unsigned int blk = 0;
	int i=2;
	unsigned short int clval;

	while (i < (fat12->total_sect / fat12->spc ))
	{
		clval = get_clval12(fs, i);
		if (clval == 0)
		{
			blk = i;
			set_clval12(fs, i,0x0fff);
			break;
		}
		else i++;
	}

	return blk;	
}

static void free_block12(struct vfs *fs, unsigned int blk)
{
	set_clval12(fs, blk,0x0);
	return;
}

static int add_dir_entry(struct vnode *dvn, struct fat_dirent *dent)
{
	struct buffer_header *bhead;
	int cur_cluster, next_cl;
	int pos = 0;
	int lblkno;
	char datablock[512];
	struct fat_dirent *pardent;

	pardent = (struct fat_dirent *)dvn->v_data;
	cur_cluster = pardent->start_cluster;
	/* If the given dir index is valid opened directory */
	while (1)
	{
		lblkno = cur_cluster + 31;
		bhead = bread(dvn->v_dev, lblkno);
		if (bhead->io_status != 0)
		{
			brelease(bhead);
			return EDISK_READ;
		}
		/* Search for unused or deleted entries in the directory */
		while ((pos < 512) && ((bhead->buff[pos] != 0) && ((unsigned char)bhead->buff[pos] != 0xE5)))
			pos = pos + 32;

		/* If an empty entry found 
		 * Copy directory structure into that entry.
		 */
		if (pos < 512) 
		{
			bcopy((char *)dent,&(bhead->buff[pos]),32);
			/* Mark that buffer for delayed write
			 * and release it.
			 */ 
			bhead->flags = bhead->flags | BUFF_WRITEDELAY;
			brelease(bhead);

			/* Update this directory entry
			 */
			mark_time_date((struct fat_dirent *)(dvn->v_data));
			dvn->v_flag |= FTFLAGS_CHANGED;

			return 1;
		}
		else 
		{	
			/* Release the buffer block */
			brelease(bhead);

			/* Find next cluster */
	if (cur_cluster < 1) 		/* part of root directory */
		next_cl = cur_cluster+1;
	else if (cur_cluster == 1)	/* Last cluster of root directory */
		next_cl = 0xfff;
	else	next_cl = next_cluster_12(dvn->v_vfsp, cur_cluster);

			/* If no more clusters */
			if (next_cl >= 0xff8)
			{
				/* Is it a subdirectory */
				if (cur_cluster >= 2)
				{
					/* Allocate a new
					 * cluster for more
					 * entries.
					 */
					next_cl = alloc_block12(dvn->v_vfsp);
					if (next_cl == 0)
						return ENO_DISK_SPACE;
					set_clval12(dvn->v_vfsp, cur_cluster, next_cl);
					bzero(datablock,512);
					bcopy((char *)dent,datablock,32);
					lblkno = next_cl + 31;
					write_block(0,lblkno,datablock);
					/* Update this directory entry
					 */
					mark_time_date((struct fat_dirent *)(dvn->v_data));
					dvn->v_flag |= FTFLAGS_CHANGED;
					return 1;
				}
				else return EDIR_FULL;
			}
			cur_cluster = next_cl;
			pos=0;
		}
	}
}

/*
 * Assumes that the named entry is not having any blocks allotted
 * for it (it may be file or directory).
 */
static int delete_dir_entry(struct vnode *dvn, char name[])
{
	struct buffer_header *bhead;
	int cur_cluster;
	int pos = 0;
	int lblkno;
	struct fat_dirent *pardent;

	pardent = (struct fat_dirent *)dvn->v_data;
	cur_cluster = pardent->start_cluster;

	/* If the given dir index is valid opened directory */
	while (1)
	{
		lblkno = cur_cluster + 31;
		bhead = bread(dvn->v_dev, lblkno);

		if (bhead->io_status != 0)
		{
			brelease(bhead);
			return EDISK_READ;
		}

		/* Search for the named entry */
		while ((pos < 512) && strncmp(&(bhead->buff[pos]),name,11) != 0) 
			pos = pos + 32;

		/* If an entry found mark it as deleted.
		 */
		if (pos < 512) 
		{
			bhead->buff[pos] = 0xE5; // mark the first char

			bhead->flags = bhead->flags | BUFF_WRITEDELAY;
			/* Find whether that block contains
			 * no more valid entries.
			 */
			if (cur_cluster >= 2)
			{
				pos = 0;
				while ((pos < 512) && ((bhead->buff[pos] == 0) || ((unsigned char)bhead->buff[pos] == 0xE5))) 
					pos = pos + 32;
				if (pos >= 512)
				{
					delete_cluster_from_chain(dvn, cur_cluster);
					/* Mark the block invalid */
					bhead->flags = bhead->flags & (~(BUFF_DATAVALID | BUFF_WRITEDELAY));
				}
			}
			brelease(bhead);
			/* Update this directory entry
			 */
			mark_time_date((struct fat_dirent *)(dvn->v_data));
			dvn->v_flag |= FTFLAGS_CHANGED;
			return 1;
		}
		else 
		{	
			/* Release the buffer block */
			brelease(bhead);

			/* Find next cluster */
	if (cur_cluster < 1) 		/* part of root directory */
		cur_cluster = cur_cluster+1;
	else if (cur_cluster == 1)	/* Last cluster of root directory */
		cur_cluster = 0xfff;
	else	cur_cluster = next_cluster_12(dvn->v_vfsp, cur_cluster);

			/* If no more clusters */
			if (cur_cluster >= 0xff8)
				return EDIR_ENTRY_NOT_FOUND;

			pos=0;
		}
	}
}

/* This is called normally from the deletion of dir entry */
static void delete_cluster_from_chain(struct vnode * dvn, int cl)
{
	unsigned short int cur_cl, prev_cl;
	struct fat_dirent *pardent;

	if (cl < 2) /* It is part of root directory */
		return;
	else
	{
		/*
		 * First cluster will never be removed from
		 * a directory, unless the directory is removed.
		 * because of ., ..
		 */
		pardent = (struct fat_dirent *)dvn->v_data;
		prev_cl = pardent->start_cluster;
		cur_cl = next_cluster_12(dvn->v_vfsp, prev_cl);
		while (cur_cl != cl)
		{
			prev_cl = cur_cl;
			cur_cl = next_cluster_12(dvn->v_vfsp, prev_cl);
		}
		set_clval12(dvn->v_vfsp, prev_cl, next_cluster_12(dvn->v_vfsp,cl));

		/* Mark cl as unused.  */
		set_clval12(dvn->v_vfsp, cl, 0x0);
	}
}

static void free_cluster_chain(struct vfs *fs, unsigned short int clno)
{
	int next_clno;

	while (!(clno >= 0xff8))
	{
		next_clno = next_cluster_12(fs, clno);
		set_clval12(fs, clno, 0x0);
		clno = next_clno;
	}
	return;
}

/* This called during the updating of some
 * sub directory or a file.
 */
static int update_dir_entry(struct vnode *dvn, struct fat_dirent *dent)
{
	struct buffer_header *bhead;
	int cur_cluster;
	int pos = 0;
	int lblkno;
	struct fat_dirent *pardent = (struct fat_dirent *)dvn->v_data;
 	cur_cluster = pardent->start_cluster;

	/* If the given dir index is valid opened directory */
	while (1)
	{
		lblkno = cur_cluster + 31;
		bhead = bread(dvn->v_dev, lblkno);
		if (bhead->io_status != 0)
		{
			brelease(bhead);
			return EDISK_READ;
		}

		/* Search for the entry with the same name */
		while ((pos < 512) && (strncmp(&(bhead->buff[pos]),dent->name,11) != 0))
			pos = pos + 32;

		/* If an entry found 
		 * Copy modified entry into that old entry.
		 */
		if (pos < 512) 
		{

			bcopy((char *)dent,&(bhead->buff[pos]),32);
			/* Mark that buffer for delayed write
			 * and release it.
			 */ 
			bhead->flags = bhead->flags | BUFF_WRITEDELAY;
			brelease(bhead);
			/* For Update this directory entry 
			 * need not be modified.
			 */

			return 1;
		}
		else 
		{	
			/* Release the buffer block */
			brelease(bhead);

			/* Find next cluster */
	if (cur_cluster < 1) 		/* part of root directory */
		cur_cluster = cur_cluster+1;
	else if (cur_cluster == 1)	/* Last cluster of root directory */
		cur_cluster = 0xfff;
	else	cur_cluster = next_cluster_12(dvn->v_vfsp, cur_cluster);
			if (cur_cluster >= 0xff8)
				return EDIR_ENTRY_NOT_FOUND;
			pos = 0;
		}
	}
}

/*
 * Name shuld have exactly 11 charaaters (8 + 3)
 * No (.) should be there. If the name is short remaining characters
 * should have space characters. 
 */
static int make_dir(struct vnode *dvn, char name[], mode_t mode)
{
	struct fat_dirent dent;
	int i,cl,err;
	char data[512];

	/* Allocate first block of the directory */
	cl = alloc_block12(dvn->v_vfsp);
	if (cl == 0)
		return ENO_DISK_SPACE;
	/* Copy the name */
	for (i=0; i<11; i++) 
		dent.name[i] = name[i];

	/* initialize the remaining fields */
	dent.attrib = FTYPE_DIR;
	mark_time_date(&dent);

	dent.start_cluster = cl;
	dent.fsize = 0;		/* Dir size is always zero */

	/* Add this entry to the given directory */
	err =  add_dir_entry(dvn, &dent);
	if (err == 1) /* Success */
	{
		bzero(data,512);
		/* Initialize that directory with ., .. entries */
		dent.name[0]='.';
		for (i=1; i<11; i++)
			dent.name[i] = ' ';
		bcopy((char *)&dent,data,32);
		bcopy(dvn->v_data, (char *)&dent, 32);
		dent.name[0]=dent.name[1]='.';
		for (i=2; i<11; i++)
			dent.name[i] = ' ';
		bcopy((char *)&dent,data+32,32);
		write_block(0,cl+31,data);
	}
	else /* Release the previously allocated block */
		set_clval12(dvn->v_vfsp, cl,0x0);	

	return err;
}

static int rm_dir(struct vnode *dvn, char name[])
{
	struct fat_dirent *dent;

	dent = (struct fat_dirent *)dvn->v_data;
	if (dent->attrib & FTYPE_DIR)
	{
		if (empty_dir12(dvn,dent))
		{
			free_block12(dvn->v_vfsp, dent->start_cluster);
			delete_dir_entry(dvn, name);
		}
		else 	return EDIR_NOT_EMPTY;
	}
	else return ENOT_A_DIRECTORY;

	return 1;
}

// Return empty or not empty
static int empty_dir12(struct vnode *dvn, struct fat_dirent *dent)
{
	int pos, lblkno;
	struct buffer_header *bhead;

	/* If more than one cluster means some entries exist */
	if (next_cluster_12(dvn->v_vfsp, dent->start_cluster) < 0xff8)
		return 0; /* Not empty */
	else 
	{
		/* Check that single first block for any valid entry */
		lblkno = dent->start_cluster + 31;
		bhead = bread(dvn->v_dev, lblkno);

		if (bhead->io_status != 0)
		{
			/* Failed to read the dir so it can be 
			 * treated as empty 
			 */
			brelease(bhead);
			return 1; 
		}
		/* Search for any valid entry other than ., .. */
		pos = 64; 
		while (pos < 512)
		{
			if ((bhead->buff[pos] != 0) && ((unsigned char)bhead->buff[pos] != 0xE5))
				break;
			else pos = pos + 32;
		}
		brelease(bhead);

		if (pos < 512) return 0; /* Not empty */
		else return 1;	/* empty */
	}
}

int fat12_vop_open(struct vnode *vn, int flags, mode_t mode) { return 0; }

int fat12_vop_close(struct vnode *vn) { return 0; }

// Returns new offset position. nb is changed to indicate actual 
// no of bytes read.
int fat12_vop_read(struct vnode *vn, int offset, char *buf, int nb)
{
	struct buffer_header *bhead;
	int i,j,n,ret_bytes,nb_to_copy, nbytes = nb;
	int lblkno;
	struct fat_dirent *dent;
	int cur_cluster;

	/*
	 * Maximum number of bytes to be read...
	 */
	dent = (struct fat_dirent *)vn->v_data;
	ret_bytes = n = ((offset + nbytes) > dent->fsize) ? (dent->fsize - offset) : nbytes;

	while (n > 0)
	{
		// Go to the current cluster position
		i = offset / 512;
		cur_cluster = dent->start_cluster;
		while (i > 0)
		{
			if (cur_cluster < 1) 		/* part of root directory */
				cur_cluster = cur_cluster+1;
			else if (cur_cluster == 1)	/* Last cluster of root directory */
				cur_cluster = 0xfff;
			else cur_cluster = next_cluster_12(vn->v_vfsp, cur_cluster);
			i--;
		}
		/* Read the current cluster.  */
		lblkno = cur_cluster + 31;
		bhead = bread(vn->v_dev, lblkno);
		if (bhead->io_status != 0)
		{
			brelease(bhead);
			return EDISK_READ;
		}
		/* Bytes remaining in the current buffer */
		j = 512 - offset % 512;
		nb_to_copy = (j > n) ? n : j;
		bcopy(&bhead->buff[offset % 512], buf, nb_to_copy);

		brelease(bhead);
		/* Update current position ... */
		n = n - nb_to_copy;
		offset += nb_to_copy;
		buf = buf + nb_to_copy;
		/* If cluster boundary is reached */
	}
	return offset;	// New offset
}

int fat12_vop_write(struct vnode *vn, int offset, char *buf, int nb)
{
	struct buffer_header *bhead;
	int j,n,nb_to_copy, nbytes = nb;
	unsigned short int next_cl, cur_cluster;
	int lblkno, i;
	struct fat_dirent *dent;

	n = nbytes;
	dent = (struct fat_dirent *)vn->v_data;

	vn->v_flag |= FTFLAGS_CHANGED;
	while (n > 0)
	{
		// Go to the current cluster position
		i = offset / 512;
		if ((offset % 512) == 0) i--;
		cur_cluster = dent->start_cluster;
		while (i > 0)
		{
			if (cur_cluster < 1) 		/* part of root directory */
				cur_cluster = cur_cluster+1;
			else if (cur_cluster == 1)	/* Last cluster of root directory */
				cur_cluster = 0xfff;
			else cur_cluster = next_cluster_12(vn->v_vfsp, cur_cluster);
			i--;
		}
		/* Current cluster is not full */
		if (offset % 512 != 0)
		{
			lblkno = cur_cluster + 31;
			bhead = bread(vn->v_dev, lblkno);
			if (bhead->io_status != 0)
			{
				brelease(bhead);
				return EDISK_READ;
			}
			/* Fill the remaining block
			 * Bytes remaining in the 
			 * current block 
			 */
			j = 512 - (offset % 512);
			nb_to_copy = (j > n) ? n : j;
			bcopy(buf, &bhead->buff[offset % 512], nb_to_copy);
			bhead->flags = bhead->flags | BUFF_WRITEDELAY;
			brelease(bhead);
			/* Update current position ... */
			n = n - nb_to_copy;
			offset += nb_to_copy;
			buf = buf + nb_to_copy;

			if (offset > dent->fsize)
				dent->fsize = offset;
		}
		else
		{
	/*
	 * Currently if the file is not
	 * having any clusters allocate first cluster
	 */
	if (dent->fsize == 0)
	{
		next_cl = alloc_block12(vn->v_vfsp);
		if (next_cl == 0) return ENO_DISK_SPACE;
		dent->start_cluster = next_cl;
		cur_cluster = next_cl;
		// Re-hash the vnode
		_lock(&vnlist_lock);
		vnode_del_hashlist(vn);
		vn->v_no = cur_cluster;
		vnode_add_hashlist(vn);
		unlock(&vnlist_lock);
	}
	else
	{
		 next_cl = next_cluster_12(vn->v_vfsp, cur_cluster);
		/* If the current one is the 
		 * last cluster then allocate
		 * new cluster
		 */ 

		if (next_cl >= 0xff8)
		{
			next_cl = alloc_block12(vn->v_vfsp);
			if (next_cl == 0)
				return ENO_DISK_SPACE;

			set_clval12(vn->v_vfsp, cur_cluster, next_cl);
		}
		cur_cluster = next_cl;
	}
			lblkno = cur_cluster + 31;
			if (n >= 512) /* Full block */
			{
				write_block(vn->v_dev,lblkno, buf);
				n -= 512;
				offset += 512;
				buf += 512;
				if (offset > dent->fsize)
					dent->fsize = offset;
			}
			else /* Partial block */
			{
				bhead = bread(vn->v_dev, lblkno);
				if (bhead->io_status != 0)
				{
					brelease(bhead);
					return EDISK_READ;
				}
				bcopy(buf, bhead->buff, n);
				bhead->flags = bhead->flags | BUFF_WRITEDELAY;
				brelease(bhead);
				offset += n;
				if (offset > dent->fsize)
					dent->fsize = offset;
				n = 0;
			}
		}
	}
	return offset;
}

int fat12_vop_getattr(struct vnode *vn, struct stat *buf)
{
	struct date_time dt;
	struct fat_dirent *dent;

	dent = (struct fat_dirent *)vn->v_data;

	// Fill up the stat buf
	buf->st_dev = vn->v_dev;
	buf->st_ino = dent->start_cluster;
	buf->st_mode = 0;
	if ((dent->attrib & (FTYPE_READONLY | FTYPE_SYSTEM | FTYPE_VOLUME)) != 0)
		buf->st_mode |= 0555;
	else
		buf->st_mode |= 0777;
	if ((dent->attrib & FTYPE_DIR) != 0)
		buf->st_mode |= (S_IFDIR);
	buf->st_nlink = 1;
	buf->st_uid = buf->st_gid = 0;
	buf->st_rdev = 0;
	buf->st_size = dent->fsize;
	buf->st_blksize = 512;
	// Find time in seconds
	dt.seconds = (dent->time & 0x1f) * 2;
	dt.minutes = ((dent->time & 0x7e0) >> 5);
	dt.hours = (((unsigned short int)(dent->time & 0xf800)) >> 11);
	dt.date = dent->date & 0x1f;
	dt.month = ((dent->date & 0x1e0) >> 5);
	dt.year = (((unsigned short int)(dent->date & 0xfe00)) >> 9);
	dt.year = dt.year + 1980;

	buf->st_atime = buf->st_mtime = buf->st_ctime = date_to_secs(dt);
	buf->st_blocks = (buf->st_size + 511) >> 9;

	return 0;
}

int fat12_vop_setattr(struct vnode *vn, struct stat *stbuf)
{
	struct fat_dirent *dent;
	
	dent = (struct fat_dirent *)vn->v_data;
	if (stbuf->st_mode & 0222)
		dent->attrib &= (~FTYPE_READONLY);
	else
		dent->attrib |= FTYPE_READONLY;
	// Other attributes can also be changed.
	vn->v_flag |= FTFLAGS_CHANGED;
	return 0;
}

struct vnode * fat12_vop_lookup(struct vnode *dvn, const char *name)
{
	struct dirent entry;
	int offset=0;
	struct vnode *vn;
	struct fat_dirent fsdent; 
	struct fat_dirent *dent;
	char fname[11];

	if (strcmp(name, ".") == 0)
	{
		get_vnode(dvn);
		dvn->v_count++;
		return dvn;
	}
	if (strcmp(name, "..") == 0) return get_pdir(dvn);

	if (convert_name(name, fname) == -1) return NULL;
	while (fat12_vop_readdir(dvn, offset, &entry, (char *)&fsdent) == 0)
	{
		if (strncmp(fsdent.name, fname, 11) == 0)
		{	// Entry found, prepare vnode
			// Change of device and type of vnode data must be done 
			// at mount point. (Not completed)
			vn = alloc_vnode(dvn->v_dev, entry.d_ino, dvn, entry.d_off, sizeof(struct fat_dirent)); // Returns in locked mode
			if (vn == NULL) return NULL;
			
			// If this is a new vnode then intialize the fields.
			if (vn->v_count == 1)
			{
				dent = (struct fat_dirent *)&fsdent;
				if ((dent->attrib & FTYPE_DIR) != 0)
					vn->v_type = S_IFDIR;
				else	// Have to check for other types
					vn->v_type = S_IFREG;
				vn->v_flag = 0;
				vn->v_vfsmountedhere = NULL;
				vn->v_op = dvn->v_op;
				vn->v_vfsp = dvn->v_vfsp;
				
				bcopy((char *)&fsdent, vn->v_data, sizeof(struct fat_dirent));
			}

			// Every thing is ok return this vnode
			return vn;
		}
		else offset = entry.d_off + sizeof(struct fat_dirent);
	}

	// Not found
	return NULL;
}

// Creating the direntry(file) is done here. Other checkings are 
// already completed
struct vnode * fat12_vop_create(struct vnode *dvn, const char *fname, mode_t mode)
{
	struct fat_dirent dent; //, fsdent;
	struct vnode *vn;
	int offset;

	if (convert_name(fname, dent.name) < 0)	// Error must be given
		return NULL;

	/* Prepare new entry */
	mode = mode & 0x3f;
	if (mode == 0) mode = 0x20;
	else mode = mode & (~(FTYPE_DIR | FTYPE_VOLUME));
	dent.attrib = mode;
	bzero(dent.res, 10);
	mark_time_date(&dent);
		
	dent.start_cluster = 0x0;
	dent.fsize = 0;
	
	/* Add this entry to the given directory */
	if ((offset = add_dir_entry(dvn, &dent)) < 0)
		return NULL;

	vn = alloc_vnode(dvn->v_dev, 0, dvn, offset, sizeof(struct fat_dirent)); // Returns in locked mode
	if (vn == NULL) return NULL;
			
	// This is a new vnode, intialize the fields.
	if ((dent.attrib & FTYPE_DIR) != 0)
		vn->v_type = S_IFDIR;
	else	// Have to check for other types
		vn->v_type = S_IFREG;
	vn->v_flag = 0;
	vn->v_vfsmountedhere = NULL;
	vn->v_op = dvn->v_op;
	vn->v_vfsp = dvn->v_vfsp;
				
	bcopy((char *)&dent, vn->v_data, sizeof(struct fat_dirent));

	// Every thing is ok return this vnode
	return vn;
}

// Already entry is existing truncate it
int fat12_vop_trunc(struct vnode *vn) //, const char *fname, mode_t mode)
{
	struct fat_dirent *dentp;

	dentp = (struct fat_dirent *)vn->v_data;
	if (dentp->fsize != 0)
		free_cluster_chain(vn->v_vfsp, dentp->start_cluster);
	dentp->start_cluster = 0;
	dentp->fsize = 0;
	mark_time_date(dentp);

	return 0;
}

int fat12_vop_remove(struct vnode *dvn) //, const char *fname)
{
	struct fat_dirent *dentp;
	int err;

	dentp = (struct fat_dirent *)dvn->v_data;
	free_cluster_chain(dvn->v_vfsp, dentp->start_cluster);
	delete_dir_entry(dvn->v_parent, dentp->name);
	err = 0;
	return err;
}

// Lookup operation for the entire path except the last component might have 
// been done by the higer level module. This is called only if both names are 
// belonging to the same device or file system. If they are belonging to 
// two separate devices/file systems, then copying follwed by deletion is to 
// be done.
int fat12_vop_rename(struct vnode *sdvn, struct vnode *ddvn, const char *nname)
{
	struct fat_dirent ndent, *dent;
	int err;

	dent = (struct fat_dirent *)sdvn->v_data;
	bcopy(sdvn->v_data, (char *)&ndent, sizeof(struct fat_dirent));
	if (convert_name(nname, ndent.name) < 0) // Replace new name
		return EINVALIDNAME;

	if ((err = add_dir_entry(ddvn, &ndent)) < 0)
		return err;

	delete_dir_entry(sdvn->v_parent, dent->name);

	return 0;
}

int fat12_vop_mkdir(struct vnode *dvn, const char *dirname, mode_t mode)
{
	char name[11];
	int ret;

	ret = convert_name(dirname, name);
	if (ret < 0) return ret;
	return make_dir(dvn, name, mode);
}

int fat12_vop_rmdir(struct vnode *dvn, const char *dirname)
{
	char name[11];
	int ret;

	ret = convert_name(dirname, name);
	if (ret < 0) return ret;
	return rm_dir(dvn, name);
}

int fat12_vop_opendir(struct vnode *vn) { return 0; }
int fat12_vop_closedir(struct vnode *vn) { return 0; }

int fat12_vop_readdir(struct vnode *vn, int offset, struct dirent *ind_dirp, char *dep_dirp)
{
	int cur_cluster;
	struct buffer_header *bhead;
	int lblkno;
	int i, j, pos;
	struct fat_dirent *pardent = (struct fat_dirent *)vn->v_data;
	struct fat_dirent *dent;

	// Go to the current cluster position
	i = offset / 512;

	pos = offset % 512;
	cur_cluster = pardent->start_cluster;
	while (i > 0)
	{
		if (cur_cluster < 1) 		/* part of root directory */
			cur_cluster = cur_cluster+1;
		else if (cur_cluster == 1)	/* Last cluster of root directory */
			cur_cluster = 0xfff;
		else cur_cluster = next_cluster_12(vn->v_vfsp, cur_cluster);
		i--;
		if (cur_cluster >= 0xff8) break;
	}
	if (i > 0) return EINVALIDOFFSET;
	while (1)
	{
		/* 31 should be added to make starting cluster
		 * no 2 to be equal to its logical sector no
		 * 33. ( 0 - boot, 18-fat, 14-root dir)
		 */
		lblkno = cur_cluster + 31;
		bhead = bread(vn->v_dev, lblkno);
		if (bhead->io_status != 0)
		{
			brelease(bhead);
			return EDISK_READ;
		}
		/* Skip unused and deleted entries in the directory or long file name*/
		while ((pos < 512) && ((bhead->buff[pos] == 0) || ((unsigned char)bhead->buff[pos] == 0xE5) || (bhead->buff[pos+11] == 0x0f)))
		{
			pos = pos + 32;
			offset += 32;
		}

		/* If an entry found 
		 * Copy this entry into the structure 
		 */
		if (pos < 512) 
		{
			dent = (struct fat_dirent *)&bhead->buff[pos];
			if (dep_dirp != NULL)
			{
				bcopy(&(bhead->buff[pos]),(char *)dep_dirp,32);
				if (dep_dirp[0] == 0x05)
					dep_dirp[0] = 0xE5;
			}
			brelease(bhead);

			// Prepare fstype independent entry
			ind_dirp->d_ino = dent->start_cluster;
			ind_dirp->d_off = offset;
			ind_dirp->d_reclen = 32;
		
			j = 0;
			for (i=0; i<8 && dent->name[i] != ' '; i++)
				ind_dirp->d_name[j++] = dent->name[i];

			if (dent->name[8] != ' ')
			{
				ind_dirp->d_name[j++] = '.';

				for (i=8; i<11 && dent->name[i] != ' '; i++)
					ind_dirp->d_name[j++] = dent->name[i];
			}
			ind_dirp->d_name[j] = 0; // null termination
	
			return 0;
		}
		else 
		{	
			/* Release the buffer block */
			brelease(bhead);

			/* Find next cluster */
			if (cur_cluster < 1) 	/* part of root directory */
				cur_cluster = cur_cluster+1;
			else if (cur_cluster == 1) /* Last cluster of root directory */
				cur_cluster = 0xfff;
			else	cur_cluster = next_cluster_12(vn->v_vfsp, cur_cluster);
			/* If no more clusters */
			if (cur_cluster >= 0xff8)
				return EDIR_ENTRY_NOT_FOUND;

			pos=0;
		}
	}
	return EDIR_ENTRY_NOT_FOUND; // Never reaches this point.
}

off_t fat12_vop_telldir(struct vnode *vn)
{
	// Nothing here available. It is available only in
	// open file table entry.
	return 0;
}
	
// None of these are used
int fat12_vop_link(struct vnode *sdvn, const char *srcpathname, struct vnode *ddvn, const char *targetpathname) { return 0; }
int fat12_vop_symlink(struct vnode *sdvn, const char *srcpathname, struct vnode *ddvn, const char *targetpathname) { return 0; }
int fat12_vop_readlink(struct vnode *vn, char *buf, size_t bufsiz) { return 0; }
int fat12_vop_ioctl(struct vnode *vn, int request, ...) { return 0; }
int fat12_vop_access(void) { return 0; }
int fat12_vop_inactive(void) { return 0; }
int fat12_vop_rwlock(struct vnode *vn, int lcktype) { return 0; }
int fat12_vop_rwunlock(struct vnode *vn) { return 0; }
int fat12_vop_realvp(void) { return 0; }
int fat12_vop_getpage(void) { return 0; }
int fat12_vop_putpage(void) { return 0; }
int fat12_vop_map(void) { return 0; }
int fat12_vop_poll(void) { return 0; }


int fat12_vfs_mount(struct vnode *svn, struct vfs *dvfs, struct vnode *dvn) { return 0; }
int fat12_vfs_unmount(struct vnode *svn, struct vnode *dvn) { return 0; }
struct vnode * fat12_vfs_root(struct vnode *vn) { return NULL; }
int fat12_vfs_sync(struct vnode *vn) { return 0; }

int syscall_sync(int dev)
{
	flush_buffers();
	return 0;
}

void write_fat12(int dev)
{
	struct fat12_fsdata *fs = (struct fat12_fsdata *)(rootvnode->v_vfsp->vfs_data);
	syscall_writeblocks(dev, 1, fs->no_sect_fat, (char *)fs->fat);
	return;
}
int fat12_vfs_statvfs(struct vnode *vnroot, struct vfsinfo *inf)
{
	struct vfs *fs = vnroot->v_vfsp;
	struct fat12_fsdata *fat12 = (struct fat12_fsdata *)fs->vfs_data;
	int free=0,bad=0,used=0;
	unsigned short int clval;
	int i;

	inf->fs_device = 0;
	inf->fs_spc = 1;
	inf->fs_totalinodes = 0;
	inf->fs_freeinodes = 0;
	for (i=2; i<(fat12->total_sect - 33 + 2); i++)
	{
		clval = get_clval12(fs, i);
		if (clval == 0x0) free++;
		else if (clval == 0xff7) bad++;
		else used++;
	}

	inf->fs_totalspace = fat12->total_sect;
	inf->fs_resspace = 1 + fat12->nfat * fat12->no_sect_fat + (fat12->no_rootdir_ent / 16);
	inf->fs_badspace = bad;
	inf->fs_usedspace = used;

	return 0;
}

int fat12_vop_seekdir(struct vnode *vn, int offset) { return 0; }
int fat12_vop_seek(struct vnode *vn, int offset, int pos, int whence) { return 0; }

static void mark_time_date(struct fat_dirent *dent)
{
	struct date_time dt;

	dt = secs_to_date(secs);
	dent->time = (dt.hours << 11) | (dt.minutes << 5) | (dt.seconds/2);
	dent->date = ((dt.year - 1980) << 9) | (dt.month << 5) | (dt.date);
	return;
}

