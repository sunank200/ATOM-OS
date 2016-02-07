// HAS TO BE MODIFIED LATER

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
 * System wide used, all error codes.
 */
#ifndef	__ERRNO_H__
#define __ERRNO_H__


#define NOERR				0
#define SUCCESS				0
#define ERR				-1
#define ENOMEM			-2
#define ERESOURCE			-3
#define EMEMCORRUPT			-4
#define EFORMAT			-5
#define ENOTFOUND			-6
#define ETYPE				-7
#define ETIMEOUT			-8
#define ENOTSUPPORTED			-9
#define EOUTOFRANGE			-10
#define EPRIV				-11
#define ENOTREADY			-12
#define ENONEFREE			-13
#define EARG				-14
#define EINVALID			-15
#define ENOTOPEN			-16
#define EALREADYDONE			-17
#define EVER				-18
#define EOVERFLOW			-19
#define EINUSE			-20
#define ETOOBIG			-21
#define ENULL				-22
#define ECACHEMISS			-23
#define ENOTINIT			-24

#define ENODOCK			-26
#define EDEST_NOTEXIST			-27
#define EMEMSEGMENT_NOTAVAILABLE	-28
#define EPROCESS_ENTRYNOTAVAILABLE	-29
// FDC error codes
#define FDC_OK 				0 	/* successful completion */
#define FDC_ERROR 			-30    /* abnormal termination */
#define FDC_NODISK 			-31  /* no disk in drive */
#define FDC_TIMEOUT 			-32  /* operation timed out */

#define EDISK_READ      		-33
#define EDISK_WRITE     		-34
#define EDISK_SUCCESS   		0
#define EDISK_OPERATION_NOT_COMPLETE    1

#define ENO_FREE_FILE_TABLE_SLOT        -35
#define EINVALID_DIR_INDEX      	-36
#define EDIR_ENTRY_NOT_FOUND    	-37
#define EDIR_FULL               	-38
#define EDUPLICATE_ENTRY        	-39
#define EDIR_NOT_EMPTY          	-40
#define ENO_DISK_SPACE          	-41
#define ENOT_A_DIRECTORY        	-42
#define EFILE_CREATE_ERROR      	-43
#define EDIR_OPEN_AS_A_FILE     	-44
#define EVOLUME_ENTRY           	-45
#define EFILE_NOT_FOUND         	-46
#define ENOT_READABLE           	-47
#define ENOT_WRITEABLE          	-48
#define EACCESS                 	-49
#define EINVALIDNAME            	-50
#define EINVALID_HANDLE    	-51
#define EINVALID_ARGUMENT       	-52
#define ELONGPATH               	-53
#define EINVALID_DRIVE          	-54
#define EINVALID_SEEK           	-55
#define EDEVICE_DIFFERENT       	-56
#define ETARGET_EXISTS          	-57
#define EPATH_NOT_EXISTS        	-58
#define EREADONLY               	-59
#define EOLDPATH_PARENT_OF_NEWPATH      -60
#define ESRC_DEST_NOT_SAME_TYPE 	-61

#define ELOCK_SUCCESS           	0
#define ELOCK_FAILURE           	-62
#define ELOCK_NOTOWNED          	-63

#define ENICNOTFOUND    		-64
#define EPKTTOOBIG      		-65
#define EBUSY           		-66
#define ETRANSMISSION   		-67

#define EMAXRCVPKTS     		-68
#define EPKTSIZE        		-69
#define EINVALIDSOCKET  		-70

#define EINVALID_EXECSIZE       	-71
#define EINVALID_SEGMENTTYPESIZE        -72
#define EFILE_READ              	-73
#define EEXECSIGNATURE_NOTFOUND 	-74
#define EARGSLENGTH             	-75
#define EINVALID_PDE            	-76
#define EEXECFILE_SIZE          	-77
#define EMEMCOPY                	-78

#define ENOSOCKET       		-79
#define EBADF           		-80
#define EINVALIDFUNCTION        	-81
#define ENOTSOCK        		-82
#define ENOTCONNECTED   		-83
#define EPORTALREADYEXISTS      	-84
#define ENOTBOUND               	-85
#define ECONNECTED              	-86
#define ESOCKTYPE               	-87
#define EPEERNOTEXISTS          	-88
#define EARGS				-89
#define ENOTHREADSLOT			-90
#define ETXMTTIMEDOUT			-91
#define ECONNECTIONCLOSED		-92


#define EINVALIDPID			-93
#define EALREADYTERMINATED		-94
#define ETIMEDOUT			-95
#define ESEMNOTFOUND			-96
#define EINVALIDADDRESS			-97
#define EINVALIDPTR			-98
#define EINTR				-99
#define EILSEQ				-100
#define EALREADYEXISTS			-101
#define EAGAIN				-102
#define ENORESOURCE			-103
#define EMAXTHREADS			-104
#define EPARALLELPROCESSNOTREGISTERED	-105
#define EREMOTEFAILURE			-106
#define ERANGE				-107
#define EMAXPROCESSES			-108
#define ENOCHILD			-109
#define EMAXFILES			-110
#define ECREATE				-111
#define EINVALIDOFFSET			-112
#define EDIR				-113
#define EXIT_FAILURE			-114

extern int errno;
#endif

