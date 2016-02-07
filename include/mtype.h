
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


/*
 * Some typedefines that are used in many places
 */
#ifndef __MTYPE_H
#define __MTYPE_H

typedef unsigned 		port_t;
typedef unsigned 		segm_t;
typedef unsigned 		reg_t;		/* machine register */
typedef unsigned long 		phys_bytes;
typedef unsigned long 		size_t;
typedef long 			ssize_t;

typedef unsigned short int 	mode_t;
typedef int     		off_t;
typedef char    		dev_t;
typedef long    		ino_t;
typedef unsigned short int 	nlink_t;
typedef unsigned short int 	uid_t;
typedef unsigned short int 	gid_t;
typedef unsigned short int 	blksize_t;
typedef unsigned long   	blkcnt_t;
typedef unsigned long 		time_t;
typedef unsigned short 		socklen_t;
typedef unsigned long		DWORD;
typedef	unsigned char		BYTE;
typedef unsigned int		uintmax_t;
typedef int			ptrdiff_t;
typedef int			mbstate_t;
typedef char			char_t;
typedef short 			wchar_t;
typedef unsigned short  	uint16_t;
typedef unsigned int    	uint32_t;
typedef int             	int32_t;
typedef short           	int16_t;
typedef unsigned long long int  uint64_t;
typedef long long int   	int64_t;
typedef int			bool;

#define true			1
#define false			0
#define UCHAR_MAX		0xff
#define INT_MAX			0x7fffffff
#define	NULL			0x0

#endif
