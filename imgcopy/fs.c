#include<string.h>
#include "buffers.h"
#include "fscalls.h"
#include "timer.h"

extern char bss_start, bss_end;
extern unsigned int secs;
int bps=512,spc=1,resect=1,nfat=2;
int spt=18,heads=2,no_tracks=80;
int total_sect=2 * 18 * 80;
int no_rootdir_ent = 224;
int no_sect_fat=9;
unsigned char fat[9 * 512];
struct DIR_FILE dir_file_list[FTABLE_SIZE];

void read_fat(void);
void cluster2param(int clno, int *head, int *track, int *sector);
unsigned short int next_cluster_12(int clno);
void set_clval(int clno, unsigned short int val);
unsigned short int get_clval(int clno);
unsigned int alloc_block(void);
void free_block(unsigned int blk);
int open_dir(int dir_index, char name[11]);
int find_entry(int dir_index, char name[], struct dir_entry *dent);
int read_first_entry(int dir_index, struct dir_opaque *dop, struct dir_entry *dent);
int read_next_entry(struct dir_opaque *dop, struct dir_entry *dent);
int add_dir_entry(int dir_index, struct dir_entry *dent);
int delete_dir_entry(int dir_index, char name[]);
void delete_cluster_from_chain(int dir_index, int cl);
int read_block(char device, unsigned int blk, char *data);
int write_block(char device, unsigned int blk, char *data);
int get_fs_info(struct fsinfo *inf);
void mark_time_date(struct dir_entry *dent);
struct date_time secs_to_date(unsigned int secs);
void increment_ref_count(int dir_handle);
void decrement_ref_count(int dir_handle);

// temporary test variables
extern int heads,spt;
extern struct lock_t write_queue_lock;
extern struct buffer_header *delayed_write_q_head,*delayed_write_q_tail;
extern FILE *fp;
struct disk_request drbuf;

void sync_floppy(void)
{
	int i;
	extern struct buffer_header buffers[];
	/* Flushing of modified buffers */
	for (i=0; i<MAX_BUFFERS; i++)
	{
		if (buffers[i].flags & BUFF_WRITEDELAY)
		{
			fseek(fp, buffers[i].blockno*512,SEEK_SET);
			fwrite(buffers[i].buff,512,1,fp);
		}
	}
}

struct disk_request * enqueue_disk_request(char device, 
			unsigned long blockno, char *buff, int op)
{
	struct disk_request *dreq = &drbuf;

	dreq->sector = (blockno %spt) + 1;
	dreq->track = (blockno / (spt * heads));
	dreq->head = (blockno / spt) % heads;
	if (op == DISK_READ)
	{
		fseek(fp, blockno*512,SEEK_SET);
		fread(buff,512,1,fp);
	}
	else if (op == DISK_WRITE)
	{
		fseek(fp, blockno*512,SEEK_SET);
		fwrite(buff,512,1,fp);
	}
	dreq->status = DISK_OPERATION_SUCCESS;
	return dreq;
}
void dequeue_disk_request(struct disk_request * dr)
{
	return;
}

/*
 * Root dir handl is returned
 */

extern int dummy;
int mount_floppy()
{
	struct DIR_FILE *dirp;
	int handle;

	init_buffers();
	read_fat();
	// temporary statements end
	
	//printf("read fat\n");
	/* Create root directory handle */
	handle = alloc_ftab_slot();
	dirp = &dir_file_list[handle];
	strcpy(dirp->dent.name,"ROOT");
	
	dirp->dent.attrib = FTYPE_DIR;
	dirp->dent.start_cluster = -12;
	dirp->dent.fsize = 0;	/* Root dir size */
	dirp->parent_index = -1;
	dirp->current_pos = 0;
	dirp->current_cluster = dirp->dent.start_cluster;
	dirp->flags = dirp->flags | FTABLE_FLAGS_DIR | FTABLE_FLAGS_READ;
	mark_time_date(&(dirp->dent));
	dirp->ref_count = 1;
	
	//printf("Mounting completed\n");

	return handle;
	
}

void write_fat()
{
	int lsect=1;
	int fat_index=0;
	int i;
	for (i=0; i<no_sect_fat; i++)
	{
		if (write_block(0,lsect,(char *)&fat[fat_index]) != 1)
			printf("Error: Writing FAT\n");
		lsect++;
		fat_index += 512;
	}
	return;
}

void unmount_floppy(int handle)
{
	write_fat();
	dir_file_list[handle].flags = FTABLE_ENTRY_UNUSED;
	sync_floppy();
}

void read_fat(void )
{
	int lsect=1;
	int fat_index=0;
	int i;
	for (i=0; i<no_sect_fat; i++)
	{
		if (read_block(0,lsect,(char *)&fat[fat_index]) != 1)
			printf("Error: Reading FAT\n");
		lsect++;
		fat_index += 512;
	}
	return;
}

void print_fat(void )
{
	int i;
	unsigned int val;

	for (i=0; i<no_sect_fat*512; i++)
	{
		if (i % 10 == 0) putchar('\r');
		val = (unsigned int) fat[i];
		printf("%04d %02x ",i,val);
		if(i%150==0)  getchar();
	}
	putchar('\r');
}

void cluster2param(int clno, int *head, int *track, int *sector)
{
	int lsect = clno - 2 + (no_sect_fat * nfat ) + (no_rootdir_ent / 16 ) + 1; 
	*sector = (lsect % spt) + 1;
	*track = (lsect / (spt * heads));
	*head = (lsect / spt) % heads;
}

unsigned short int next_cluster_12(int clno)
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
		nclno = get_clval(clno);
	else	nclno = 0;

	return nclno;
}

void set_clval(int clno, unsigned short int val)
{
	int fat_index = (clno * 3) / 2;

	if (clno % 2 == 0)
	{
		fat[fat_index] = (val & 0xff);
		fat[fat_index+1] = (fat[fat_index+1] & 0xf0) | ((val >> 8) & 0x000f);
	}
	else
	{
		fat[fat_index] = (fat[fat_index] & 0x0f) | ((val << 4) & 0x00f0);
		fat[fat_index+1] = ((val >> 4) & 0x00ff);
	}
}

unsigned short int get_clval(int clno)
{
	int fat_index = (clno * 3) / 2;
	unsigned short val1 = (unsigned short)fat[fat_index];
       	unsigned short val2 = (unsigned short)fat[fat_index+1];
       	unsigned short val = val1 | (val2 << 8);
	if (clno % 2 == 0)
		val = val & 0x0fff;
	else
		val = (val >> 4) & 0x0fff;
	return val;
}
unsigned int alloc_block(void)
{
	unsigned int blk = 0;
	int i=2;
	unsigned short int clval;

	while (i < total_sect / spc )
	{
		clval = get_clval(i);
		if (clval == 0)
		{
			blk = i;
			set_clval(i,0x0fff);
			break;
		}
		else i++;
	}

	return blk;	
}
void free_block(unsigned int blk)
{
	set_clval(blk,0x0);
	return;
}

int open_dir(int dir_index, char name[11])
{	
	int ftab_slot;
	struct DIR_FILE *dirp;
	struct dir_entry dent;

	/* If the given dir index is valid opened directory */
	if ((dir_file_list[dir_index].flags & FTABLE_FLAGS_DIR) != 0)
	{
		/* Find the named entry in the given directory */
		if ((find_entry(dir_index, name, &dent) != EDIR_ENTRY_NOT_FOUND)
			&& ((dent.attrib & FTYPE_DIR) != 0))
		{

			/* Allocate file/dir table slot */
			ftab_slot = alloc_ftab_slot();
			if (ftab_slot < 0)
				return (ENO_FREE_FILE_TABLE_SLOT);

			/* File/dir table slot found, initialize that structure */
			dirp = &dir_file_list[ftab_slot];
			dirp->device = 0;
			dirp->dent = dent;
			dirp->parent_index = dir_index;
			dirp->current_pos = 0;
			dirp->current_cluster = dent.start_cluster;
			dirp->flags = dirp->flags | FTABLE_FLAGS_DIR | FTABLE_FLAGS_READ;
			dirp->ref_count = 1;
			increment_ref_count(dir_index);

			/* The directory's slot number is returned */
			return ftab_slot;
		}
		else return (EDIR_ENTRY_NOT_FOUND);
	}
	else return (EINVALID_DIR_INDEX);
}

/* dir_index : Valid opened directory slot number.
 * name : Char array having 11 characters (name + extension), 
 * without dot (.) and may be having null character at the end.
 * dent : directory entry structure pointer.
 */
int find_entry(int dir_index, char name[], struct dir_entry *dent)
{
	struct dir_opaque dop;
	int entry_found;

	/* Read the first entry */
	entry_found = read_first_entry(dir_index, &dop, dent);

	while (entry_found == 1)
	{
		/* Find if the entry is matching. */
		if (strncmp(name,dent->name,11) == 0)
			return 1;

		/* Find the next entry. */
		entry_found = read_next_entry(&dop,dent);
	}
	/* Entry not found */
	return (EDIR_ENTRY_NOT_FOUND);
}

int read_first_entry(int dir_index, struct dir_opaque *dop, struct dir_entry *dent)
{
	struct DIR_FILE *dirf = &dir_file_list[dir_index];
	struct buffer_header *bhead;
	int cur_cluster = dirf->dent.start_cluster;
	int pos,lblkno;

	/* If the given dir index is valid opened directory */
	if ((dirf->flags & FTABLE_FLAGS_DIR) != 0)
	{
		while (1)
		{
			/* 31 should be added to make starting cluster
			 * no 2 to be equal to its logical sector no
			 * 33. ( 0 - boot, 18-fat, 14-root dir)
			 */
			lblkno = cur_cluster + 31;
			bhead = bread(dirf->device, lblkno);
			if (bhead->io_status != 0)
			{
				brelease(bhead);
				return EDISK_READ;
			}

			/* Skip unused and deleted first entries  and long file name entries in the directory */
			pos=0;
			while ((pos < 512) && ((bhead->buff[pos] == 0) || ((unsigned char)bhead->buff[pos] == 0xE5) || (bhead->buff[pos+11] == 0x0f)))
				pos = pos + 32;

			/* If an entry found 
			 * Copy this first entry into the structure 
			 */
			if (pos < 512) 
			{
				bcopy(&(bhead->buff[pos]),(char *)dent,32);
				/* Initialize the dir_opaque structure */
				dop->dir_index = dir_index;
				dop->current_pos = pos + 32 ;
				dop->current_cluster = cur_cluster;
				if (dent->name[0] == 0x05)
					dent->name[0] = 0xE5;
				brelease(bhead);
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
	else cur_cluster = next_cluster_12(cur_cluster);
				/* If no more clusters */
				if (cur_cluster >= 0xff8)
					return EDIR_ENTRY_NOT_FOUND;

				dop->current_blockno++;
			}
		}
	}
	else	/* Not valid opened directory */
		return EINVALID_DIR_INDEX;	
}


int read_next_entry(struct dir_opaque *dop, struct dir_entry *dent)
{
	int dir_index = dop->dir_index;
	int cur_cluster = dop->current_cluster;
	int pos = dop->current_pos;
	struct DIR_FILE *dirf = &dir_file_list[dir_index];
	struct buffer_header *bhead;
	int lblkno;
	/* If the given dir index is valid opened directory */
	if ((dirf->flags & FTABLE_FLAGS_DIR) != 0)
	{
		while (1)
		{
			/* 31 should be added to make starting cluster
			 * no 2 to be equal to its logical sector no
			 * 33. ( 0 - boot, 18-fat, 14-root dir)
			 */
			lblkno = cur_cluster + 31;
			bhead = bread(dirf->device, lblkno);
			if (bhead->io_status != 0)
			{
				brelease(bhead);
				return EDISK_READ;
			}


			/* Skip unused and deleted entries in the directory or long file name*/
			while ((pos < 512) && ((bhead->buff[pos] == 0) || ((unsigned char)bhead->buff[pos] == 0xE5) || (bhead->buff[pos+11] == 0x0f)))
				pos = pos + 32;

			/* If an entry found 
			 * Copy this entry into the structure 
			 */
			if (pos < 512) 
			{
				bcopy(&(bhead->buff[pos]),(char *)dent,32);
				/* Initialize the dir_opaque structure */
				dop->dir_index = dir_index;
				dop->current_pos = pos + 32 ;
				dop->current_cluster = cur_cluster;
				if (dent->name[0] == 0x05)
					dent->name[0] = 0xE5;
				brelease(bhead);
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
	else	cur_cluster = next_cluster_12(cur_cluster);
				/* If no more clusters */
				if (cur_cluster >= 0xff8)
					return EDIR_ENTRY_NOT_FOUND;

				dop->current_blockno++;
				pos=0;
			}
		}
	}
	else	/* Not valid opened directory */
		return EINVALID_DIR_INDEX;	
}
int add_dir_entry(int dir_index, struct dir_entry *dent)
{
	struct DIR_FILE *dirf = &dir_file_list[dir_index];
	struct buffer_header *bhead;
	struct dir_entry temp;
	int cur_cluster = dirf->dent.start_cluster,next_cl;
	int pos = 0;
	int lblkno;
	char datablock[512];

	/* If the given dir index is valid opened directory */
	if ((dirf->flags & FTABLE_FLAGS_DIR) != 0)
	{
		if (find_entry(dir_index,dent->name,&temp) == 1)
			return EDUPLICATE_ENTRY;
		while (1)
		{
			lblkno = cur_cluster + 31;
			bhead = bread(dirf->device, lblkno);
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
				mark_time_date(&(dirf->dent));
				dirf->flags = dirf->flags | FTABLE_FLAGS_CHANGED;

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
	else	next_cl = next_cluster_12(cur_cluster);

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
						next_cl = alloc_block();
						if (next_cl == 0)
							return ENO_DISK_SPACE;
						set_clval(cur_cluster, next_cl);
						bzero(datablock,512);
						bcopy((char *)dent,datablock,32);
						lblkno = next_cl + 31;
						write_block(0,lblkno,datablock);
						/* Update this directory entry
						 */
						mark_time_date(&(dirf->dent));
						dirf->flags = dirf->flags | FTABLE_FLAGS_CHANGED;
						return 1;

					}
					else return EDIR_FULL;
				}
				cur_cluster = next_cl;
				pos=0;
			}
		}
	}
	else	/* Not valid opened directory */
		return EINVALID_DIR_INDEX;	
}

/*
 * Assumes that the named entry is not having any blocks allotted
 * for it (it may be file or directory).
 */
int delete_dir_entry(int dir_index, char name[])
{
	struct DIR_FILE *dirf = &dir_file_list[dir_index];
	struct buffer_header *bhead;
	struct dir_entry *temp;
	int cur_cluster = dirf->dent.start_cluster;
	int pos = 0;
	int lblkno;

	/* If the given dir index is valid opened directory */
	if (dirf->flags & FTABLE_FLAGS_DIR)
	{
		while (1)
		{
			lblkno = cur_cluster + 31;
			bhead = bread(dirf->device, lblkno);
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
				temp = (struct dir_entry *) &bhead->buff[pos];
				temp->name[0] = 0xE5;

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
					delete_cluster_from_chain(dir_index, cur_cluster);
					/* Mark the block invalid */
					bhead->flags = bhead->flags & (~(BUFF_DATAVALID | BUFF_WRITEDELAY));
				}
				}
				brelease(bhead);
				/* Update this directory entry
				 */
				mark_time_date(&(dirf->dent));
				dirf->flags = dirf->flags | FTABLE_FLAGS_CHANGED;
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
	else	cur_cluster = next_cluster_12(cur_cluster);

				/* If no more clusters */
				if (cur_cluster >= 0xff8)
					return EDIR_ENTRY_NOT_FOUND;

				pos=0;
			}
		}
	}
	else	/* Not valid opened directory */
		return EINVALID_DIR_INDEX;	
}

/* This is called normally from the deletion of dir entry */
void delete_cluster_from_chain(int dir_index, int cl)
{
	unsigned short int cur_cl, prev_cl;

	if (cl < 2) /* It is part of root directory */
		return;
	else
	{
		/*
		 * First cluster will never be removed from
		 * a directory, unless the directory is removed.
		 */
		prev_cl = cur_cl = dir_file_list[dir_index].dent.start_cluster;
		cur_cl = next_cluster_12(prev_cl);
		while (cur_cl != cl)
		{
			prev_cl = cur_cl;
			cur_cl = next_cluster_12(prev_cl);
		}
		set_clval(prev_cl,next_cluster_12(cl));

	       /* Mark cl as unused.  */
		set_clval(cl,0x0);
	}
}
int read_block(char device, unsigned int blk, char *data)
{
	struct buffer_header *bhead;
	bhead = bread(device,blk);
	if (bhead->io_status != 0)
	{
		brelease(bhead);
		return EDISK_READ;
	}
	bcopy(bhead->buff,data,512);
	brelease(bhead);
	return 1;
}
int write_block(char device, unsigned int blk, char *data)
{
	struct buffer_header *bhead;

	bhead = getblk(device, blk);
	bcopy(data,bhead->buff,512);
	bhead->flags = bhead->flags | BUFF_WRITEDELAY | BUFF_DATAVALID;
	brelease(bhead);
	return 1;
}

int get_fs_info(struct fsinfo *inf)
{
	int free=0,bad=0,used=0;
	unsigned short int clval;
	int i;

	inf->device = 0;
	inf->spc = 1;
	inf->total_space = total_sect;
	inf->reserved_space = 1 + nfat * no_sect_fat + (no_rootdir_ent / 16);
	for (i=2; i<(total_sect - 33 + 2); i++)
	{
		clval = get_clval(i);
		if (clval == 0x0) free++;
		else if (clval == 0xff7) bad++;
		else used++;
	}

	inf->free_space = free;
	inf->bad_space = bad;
	inf->used_space = used;

	return 1;
}

void free_cluster_chain(unsigned short int clno)
{
	int next_clno;

	while (!(clno >= 0xff8))
	{
		next_clno = next_cluster_12(clno);
		set_clval(clno, 0x0);
		clno = next_clno;
	}
	return;
}

/* This called during the updating of some
 * sub directory or a file.
 */
int update_dir_entry(int dir_index, struct dir_entry *dent)
{
	struct DIR_FILE *dirf = &dir_file_list[dir_index];
	struct buffer_header *bhead;
	int cur_cluster = dirf->dent.start_cluster;
	int pos = 0;
	int lblkno;

	/* If the given dir index is valid opened directory */
	if (dirf->flags & FTABLE_FLAGS_DIR)
	{
		while (1)
		{
			lblkno = cur_cluster + 31;
			bhead = bread(dirf->device, lblkno);
			if (bhead->io_status != 0)
			{
				brelease(bhead);
				return EDISK_READ;
			}

			/* Search for the entry with the same name */
			while ((pos < 512) && (strncmp(&bhead->buff[pos],dent->name,11) != 0))
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
	else	cur_cluster = next_cluster_12(cur_cluster);
				if (cur_cluster >= 0xff8)
					return EDIR_ENTRY_NOT_FOUND;
				pos = 0;
			}
		}
	}
	else return EINVALID_DIR_INDEX;
}

int alloc_ftab_slot()
{
	int slot;
	for (slot=0; slot < FTABLE_SIZE; slot++)
		if (dir_file_list[slot].flags == FTABLE_ENTRY_UNUSED)
		{
			dir_file_list[slot].flags = FTABLE_ENTRY_USED;
			return slot;
		}

	return ENO_FREE_FILE_TABLE_SLOT;
}

void close_dir(int dir_index)
{
	decrement_ref_count(dir_index);
}


/*
 * Name shuld have exactly 11 charaaters (8 + 3)
 * No (.) should be there. If the name is short remaining characters
 * should have space characters. 
 */
int make_dir(int dir_index, char name[])
{
	struct dir_entry dent;
	int i,cl,err;
	char data[512];

	/* Allocate first block of the directory */
	cl = alloc_block();
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
	err =  add_dir_entry(dir_index, &dent);
	if (err == 1) /* Success */
	{
		bzero(data,512);
		/* Initialize that directory with ., .. entries */
		dent.name[0]='.';
		for (i=1; i<11; i++)
			dent.name[i] = ' ';
		bcopy((char *)&dent,data,32);
		dent = dir_file_list[dir_index].dent;
		dent.name[0]=dent.name[1]='.';
		for (i=2; i<11; i++)
			dent.name[i] = ' ';
		bcopy((char *)&dent,data+32,32);
		write_block(0,cl+31,data);
	}
	else /* Release the previously allocated block */
		set_clval(cl,0x0);	

	return err;
}
int rm_dir(int dir_index, char name[])
{
	struct dir_entry dent;

	if (find_entry(dir_index, name, &dent)==1)
	{
		if (dent.attrib & FTYPE_DIR)
		{
			if (empty_dir(dir_index,&dent))
			{
				free_block(dent.start_cluster);
				delete_dir_entry(dir_index, name);
			}
			else 	return EDIR_NOT_EMPTY;
		}
		else return ENOT_A_DIRECTORY;
	}
	else return EDIR_ENTRY_NOT_FOUND;

	return 1;
}

int empty_dir(int dir_handle, struct dir_entry *dent)
{
	int pos, lblkno;
	struct buffer_header *bhead;
	struct DIR_FILE *dirf = &dir_file_list[dir_handle];

	/* If more than one cluster means some entries exist */
	if (next_cluster_12(dent->start_cluster) < 0xff8)
		return 0; /* Not empty */
	else 
	{
		/* Check that single first block for any valid entry */
		lblkno = dent->start_cluster + 31;
		bhead = bread(dirf->device, lblkno);
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

int create_file(int dir_handle, char name[], int attrib)
{
	struct DIR_FILE *dirf;
	struct dir_entry dent;
	int i,err;
	int handle;

	if (find_entry(dir_handle,name,&dent)==1)
	{
		/* entry already exists */
		if (((dent.attrib & FTYPE_READONLY) == 0) && ((dent.attrib & FTYPE_DIR) == 0) && ((dent.attrib & FTYPE_VOLUME) == 0))
		{
			/* It is a regular file.
			 * Truncate it and change its
			 * entry
			 */
			if (dent.fsize != 0)
				free_cluster_chain(dent.start_cluster);
			mark_time_date(&dent);
			dent.start_cluster = 0x0;
			dent.fsize = 0;
		}
		else return EFILE_CREATE_ERROR;
	}
	else
	{
		/* Prepare new entry */
		for (i=0; i<11; i++) 
			dent.name[i] = name[i];

		dent.attrib = attrib;
		mark_time_date(&dent);
		dent.start_cluster = 0x0;
		dent.fsize = 0;
		/* Add this entry to the given directory */
		if ((err = add_dir_entry(dir_handle, &dent)) != 1)
			return err;
	}

	/* Create a handle for it return that */
	if ((handle = alloc_ftab_slot()) >= 0)
	{
		/* Initialize file table entry */
		dirf = &dir_file_list[handle];
		dirf->dent = dent;
		dirf->parent_index = dir_handle;
		dirf->device = 0;
		dirf->flags = dirf->flags | FTABLE_FLAGS_FILE | FTABLE_FLAGS_WRITE | FTABLE_FLAGS_CHANGED;
		/* Change may come through truncation */
		dirf->current_pos = 0;
		dirf->current_cluster = 0x0;
		dirf->ref_count = 1;
		increment_ref_count(dir_handle);
		return handle;
	}
	else return ENO_FREE_FILE_TABLE_SLOT;
}
int open_file(int dir_handle, char name[], int type, int mode)
{
	struct DIR_FILE *dirf;
	struct dir_entry dent;
	int i,next_cl;
	int handle;

	if (find_entry(dir_handle,name,&dent)==1)
	{
		/* Check whether it is a file */
		if ((dent.attrib & FTYPE_DIR) != 0)
			return EDIR_OPEN_AS_A_FILE;
		else if ((dent.attrib & FTYPE_VOLUME) != 0)
			return EVOLUME_ENTRY;

		/* Check the open type */
		if (type & O_WRONLY || type & O_RDWR || type & O_APPEND)
		{
			if (dent.attrib & FTYPE_READONLY)
				return EACCESS;
		}
	
		/* Otherwise it can be opened */
		if ((handle = alloc_ftab_slot()) >= 0)
		{
			/* Initialize file table entry */
			dirf = &dir_file_list[handle];
			dirf->dent = dent;
			dirf->parent_index = dir_handle;
			dirf->device = 0;

			if (type & O_TRUNC || type & O_WRONLY)
			{
				if (dent.fsize != 0)
					free_cluster_chain(dent.start_cluster);
				dirf->dent.start_cluster = 0x0;
				dirf->dent.fsize = 0;
				dirf->current_cluster =0x0;
				dirf->current_pos =0;


				dirf->flags |= FTABLE_FLAGS_CHANGED | FTABLE_FLAGS_WRITE;
			}
			else if (type == O_APPEND)
			{
				/* Go to the last cluster */
				i = dirf->dent.start_cluster;
				if (i != 0x0)
				{
					next_cl = next_cluster_12(i);
					while (!(next_cl >= 0xff8))
					{
						i = next_cl;
						next_cl = next_cluster_12(i);
					}
				}
				dirf->current_cluster = i;
				dirf->current_pos = dirf->dent.fsize;
				dirf->flags |= FTABLE_FLAGS_APPEND;
			}
			else 
			{
				/* Set the currnt position to normal values */
				dirf->current_pos = 0;
				dirf->current_cluster = dirf->dent.start_cluster;
			}

			/* Set the read/write flags  properly*/
			if (type & O_RDONLY || type & O_RDWR)
				dirf->flags |= FTABLE_FLAGS_READ;
			if (type & O_WRONLY || type & O_RDWR)
				dirf->flags |= FTABLE_FLAGS_WRITE;
/*
 * We are not checking for consistency of
 * combinations of flags 
 */
			dirf->flags = dirf->flags | FTABLE_FLAGS_FILE;
			dirf->ref_count = 1;
			increment_ref_count(dir_handle);
			return handle;
		}
		else return ENO_FREE_FILE_TABLE_SLOT;
	}
	else if (type & O_CREAT || type & O_WRONLY || type & O_RDWR)
	{
		/* Entry not found so create it */
		handle = create_file(dir_handle, name, mode);

		if (handle >= 0)
		{
			if (type & O_RDWR)
				dir_file_list[handle].flags |= FTABLE_FLAGS_READ;
		}
		return handle;
	}
	else return EFILE_NOT_FOUND;
}

void close_file(int handle)
{
	decrement_ref_count(handle);
}

int read_file(int handle, char *buf, int nbytes)
{
	struct DIR_FILE *dirf = &dir_file_list[handle];
	struct buffer_header *bhead;
	int j,n,ret_bytes,nb_to_copy;
	int lblkno;

	if (!(dirf->flags & FTABLE_FLAGS_READ))
		return ENOT_READABLE;

	/*
	 * Maximum number of bytes to be read...
	 */
	ret_bytes = n = ((dirf->current_pos + nbytes) > dirf->dent.fsize) ? (dirf->dent.fsize - dirf->current_pos) : nbytes;

	while (n > 0)
	{
		/* Read the current cluster.  */
		lblkno = dirf->current_cluster + 31;
		bhead = bread(dirf->device, lblkno);
		if (bhead->io_status != 0)
		{
			brelease(bhead);
			return EDISK_READ;
		}
		/* Bytes remaining in the current buffer */
		j = 512 - dirf->current_pos % 512;
		nb_to_copy = (j > n) ? n : j;
		bcopy(&bhead->buff[dirf->current_pos % 512],buf,nb_to_copy);
		brelease(bhead);
		/* Update current position ... */
		n = n - nb_to_copy;
		dirf->current_pos += nb_to_copy;
		buf = buf + nb_to_copy;
		/* If cluster boundary is reached */
		if (dirf->current_pos % 512 == 0)
			dirf->current_cluster = next_cluster_12(dirf->current_cluster);
	}
	return ret_bytes;
}

int write_file(int handle, char *buf, int nbytes)
{
	struct DIR_FILE *dirf = &dir_file_list[handle];
	struct buffer_header *bhead;
	int j,n,nb_to_copy;
	unsigned short int next_cl;
	int lblkno;

	if ((dirf->flags & FTABLE_FLAGS_WRITE) == 0 &&
            (dirf->flags & FTABLE_FLAGS_APPEND) == 0)
		return ENOT_WRITEABLE;

	n = nbytes;

	dirf->flags = dirf->flags | FTABLE_FLAGS_CHANGED;
	while (n > 0)
	{
		/* Current cluster is not full */
		if (dirf->current_pos % 512 != 0)
		{
			lblkno = dirf->current_cluster + 31;
			bhead = bread(dirf->device, lblkno);
			if (bhead->io_status != 0)
			{
				brelease(bhead);
				return EDISK_READ;
			}
			/* Fill the remaining block
			 * Bytes remaining in the 
			 * current block 
			 */
			j = 512 - (dirf->current_pos % 512);
			nb_to_copy = (j > n) ? n : j;
		//printing cluster
			bcopy(buf,&bhead->buff[dirf->current_pos % 512],nb_to_copy);
			bhead->flags = bhead->flags | BUFF_WRITEDELAY;
			brelease(bhead);
			/* Update current position ... */
			n = n - nb_to_copy;
			dirf->current_pos += nb_to_copy;
			buf = buf + nb_to_copy;

			if (dirf->current_pos > dirf->dent.fsize)
				dirf->dent.fsize = dirf->current_pos;
		}
		else
		{
	/*
	 * Currently if the file is not
	 * having any clusters allocate first cluster
	 */
	if (dirf->dent.fsize == 0)
	{
		next_cl = alloc_block();
		if (next_cl == 0)
			return ENO_DISK_SPACE;
		dirf->dent.start_cluster = next_cl;
		dirf->current_cluster = next_cl;
	}
	else
	{
		 next_cl = next_cluster_12(dirf->current_cluster);
		/* If the current one is the 
		 * last cluster then allocate
		 * new cluster
		 */ 
		if (next_cl >= 0xff8)
		{
			next_cl = alloc_block();
			if (next_cl == 0)
				return ENO_DISK_SPACE;
			set_clval(dirf->current_cluster, next_cl);

		}
		dirf->current_cluster = next_cl;
	}
			lblkno = dirf->current_cluster + 31;
			if (n >= 512) /* Full block */
			{
				write_block(dirf->device,lblkno, buf);
				n -= 512;
				dirf->current_pos += 512;
				buf += 512;
				if (dirf->current_pos > dirf->dent.fsize)
					dirf->dent.fsize = dirf->current_pos;
			}
			else /* Partial block */
			{
				bhead = bread(dirf->device, lblkno);
				if (bhead->io_status != 0)
				{
					brelease(bhead);
					return EDISK_READ;
				}
				bcopy(buf,bhead->buff,n);
				bhead->flags = bhead->flags | BUFF_WRITEDELAY;
				brelease(bhead);
				dirf->current_pos += n;
				if (dirf->current_pos > dirf->dent.fsize)
					dirf->dent.fsize = dirf->current_pos;
				n = 0;
			}
		}
	}
	return nbytes;
}

int seek_file(int fhandle, int pos, int whence)
{
	struct DIR_FILE *dirf = &dir_file_list[fhandle];
	unsigned short int cl1;
	long size;

	if ((dirf->flags & FTABLE_FLAGS_FILE) != 0)
	{
		if (whence == SEEK_SET)
		{
			if (pos >= 0 && pos <= dirf->dent.fsize)
				dirf->current_pos = pos;
		}
		else if (whence == SEEK_CUR)
		{
			if ((dirf->current_pos + pos >= 0) && (dirf->current_pos + pos <= dirf->dent.fsize))
				dirf->current_pos += pos;
		}
		else if (whence == SEEK_END)
		{
			if ((pos < 0) && (dirf->dent.fsize + pos >= 0))
				dirf->current_pos = dirf->dent.fsize + pos;
		}

		cl1 = dirf->dent.start_cluster;
		size = 512;
		while (size < dirf->current_pos)
		{
			cl1 = next_cluster_12(cl1);
			size += 512;
		}
		dirf->current_cluster = cl1;
		return dirf->current_pos;
	}

	return EINVALID_FILE_HANDLE;
}

void mark_time_date(struct dir_entry *dent)
{
	struct date_time dt;
	time_t time(time_t *t);
	
	dt = secs_to_date(time(NULL));
	dent->time = (dt.hours << 11) | (dt.minutes << 5) | (dt.seconds/2);
	dent->date = ((dt.year - 1980) << 9) | (dt.month << 5) | (dt.date);
	return;
}
void increment_ref_count(int dir_handle)
{
	int i;

	for (i=dir_handle; i >= 0; i = dir_file_list[i].parent_index)
		dir_file_list[i].ref_count++;
}

void decrement_ref_count(int handle)
{
	int i;
	struct DIR_FILE *dirf;

	for (i=handle; i >= 0; i = dir_file_list[i].parent_index)
	{
		 dirf = &dir_file_list[i];

		/* Update the entry in the parent directory 
		 * if this directory is modified 
		 */
		if (dirf->flags & FTABLE_FLAGS_CHANGED )
			update_dir_entry(dirf->parent_index,&dirf->dent);
	
		/* Release the file table slot */
		dirf->ref_count--;
		if (dirf->ref_count == 0)
			dirf->flags = FTABLE_ENTRY_UNUSED;
	}

	return;
}

