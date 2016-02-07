
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


/* Driver header for the floppy disk drive
 * There is only one floppy drive, and it handles only 3.5" HD diskettes
 * The driver has been written accordingly */

//------------------------------------------------------------------------
// Macro definitions for the driver

#ifndef __FDC_H
#define __FDC_H

#include "lock.h"
#include "process.h"
#include "vmmap.h"

#define SAFESEEK  5 /* Track to seek before issuing a recalibrate       */

// FDC operation codes - better documentation
#define FDC_READ	0
#define FDC_WRITE 	1

// The following are the #define for all the floppy controller registers
// We are only concerned with the primary controller
#define BASE_PORT			0x3f0	// primary controller
#define DIGITAL_OUTPUT_REGISTER		0x3f2
#define MAIN_STATUS_REGISTER		0x3f4	// read-only
#define DATA_FIFO			0x3f5
#define DIGITAL_INPUT_REGISTER		0x3f7	// read-only
#define CONFIGURATION_CONTROL_REGISTER	0x3f7	// write only

// Constants for DMA transfer
#define DMA_CHANNEL	2	/* Channel number of the DMA controller used by FDC */
#define DMA_PAGE	0x81	/* Page register for that particular channel */
#define DMA_OFFSET	0x04	/* Offset register for the channel */
#define DMA_LENGTH	0x05	/* Length register for the channel */
                                                                                                                             
// Controller commands
#define CMD_SPECIFY	0x03	// Specify drive timings
#define CMD_WRITE	0xc5	// write data (+MT, MFM)
#define CMD_READ	0xe6	// read data (+MT, MFM, SK)
#define CMD_SENSEINS	0x08	// sense interrupt status
#define CMD_READID	0x4a	// read sector ID (+MFM)
#define CMD_RECAL	0x07	// recalibrate
#define CMD_SEEK	0x0f	// seek track
#define CMD_VERSION	0x10	// get FDC version
#define CMD_FORMAT	0x4d	// format track
                                                                                                                             
/* Some legend for the above commands
 * MT = multi-track mode
 * MFM = for double density mode
 * SK = skip mode (read operation)
 */

/* Drive format specification - for 1.44MB 3.5" diskette */
#define fmt_name		"H1440"	/* name of the format */
#define fmt_size		2880	/* total number of sectors */
#define fmt_sectorsPerTrack	18	/* sectors per track */
#define fmt_heads		2	/* heads (sides) */
#define fmt_tracks		80	/* tracks */
#define fmt_stretch		0	/* tracks are not to be doubled (1 if they were to be) */
#define fmt_gap3		0x1b	/* GAP3 used for reading/writing */
#define fmt_rate		0x00	/* sector size and data transfer rate - 500 kbits/s */
#define fmt_srHut		0xcf	/* step rate (4 ms) and head unload time (240 ms) */
#define fmt_gap3Format		0x6c	/* GAP3 used for formatting */
                                                                                                                             
/* Parameters used to manage a floppy disk drive */
#define uS 1000 /* for conversion from milliseconds (ms) to microseconds (us) */
/* We have a fixed set, only for the 3.5" HD diskettes */
#define dp_name			"3.5\" HD, 1440 KB"	/* display name for drive type */
#define dp_cmosType		4			/* drive type read from CMOS */
#define dp_hlt			0x04			/* Head Load Time (8 ms) */
#define dp_spinUp		40			/* time for spinning up (ms) */
#define dp_spinDown		40			/* time for spinning down (ms) */
#define dp_selectDelay		200			/* select delay (ms) */
#define dp_interruptTimeout	200			/* interrupt timeout (ms) */
#define dp_nativeFormat		7			/* native format of disk for drive type */
// int dp_detectFormat[] = { 7, 4, 25, 22, 31, 21, 29, 11 } ; /* Autodetect formats */

//------------------------------------------------------------------------
// Data structures for the driver

// Sector address as Cylinder/Head/Sector
typedef struct {
        unsigned c, h, s ;
} Chs ;

// Status structure for floppy disk controller
typedef struct {
        /* We're using BYTE since we want to store in bytes */
        BYTE result[7] ;        /* 7 because it's the maximum number of result bytes for any command */
        BYTE resultSize ;       /* number of result bytes */
        BYTE sr0 ;              /* status register 0 after sense interrupt */
        BYTE dor ;              /* Data output register */
} FDC ;

// Status structure for the floppy disk drive
typedef struct {
        FDC *fdc ;
        unsigned track ;                /* current track sought by the drive */
} FDD ;


// Higher level floppy disk functions - to be used by the kernel
// Also included are floppy disk related information and disk request structures

#define MAX_FLOPPY_REQUESTS 50
#define FLOPPY_READ 1
#define FLOPPY_WRITE 2
#define FLOPPY_FORMAT 3

struct floppy_request {
	unsigned lba_start;
	unsigned blocks;
	void *buffer;
	struct event_t complete;
	struct floppy_request *next;
	char operation;
	char status;
};

void init_floppy_request_bufs(void);
struct floppy_request * enqueue_floppy_request(char device, unsigned long start, unsigned blocks, void *buffer, int op);
void dequeue_floppy_request(struct floppy_request *dr);

void fdc_reset( FDC *fdc ) ;
int setup_drive( FDD *fdd ) ;
int fdc_setup( FDD *fdd ) ;
void reset_drive( FDD *fdd ) ;
int fdc_read( FDD *fdd, const Chs *chs, DWORD buffer, unsigned num_sectors ) ;
int fdc_write( FDD *fdd, const Chs *chs, const DWORD buffer, unsigned num_sectors ) ;
int fdc_format( FDD *fdd ) ;
void fdcDispose(void) ;

#endif

