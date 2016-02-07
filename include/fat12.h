
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


#ifndef __FAT12_H
#define __FAT12_H

#include "lock.h"

#define CLUSTER_BAD	0xFF7

#define FTYPE_READONLY	0x01
#define FTYPE_HIDDEN	0x02
#define FTYPE_SYSTEM	0x04
#define FTYPE_VOLUME	0x08
#define FTYPE_DIR	0x10
#define FTYPE_ARCHIVE	0x20

struct fat_dirent {
	char name[11];

	char attrib;
	char res[10];
	short int time;
	short int date;
	short int start_cluster;
	unsigned int fsize;
};

// FAT12 specific data
struct fat12_fsdata {
	int bps, spc, resect, nfat, spt;
	int heads, no_tracks, total_sect;
	int no_rootdir_ent, no_sect_fat;
	struct lock_t fatlock;
	unsigned char fat[9 * 512];
};

#endif
