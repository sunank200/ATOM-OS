
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


/* Driver program for the floppy disk drive
 * There is only one floppy drive, and it handles only 3.5" HD diskettes
 * The driver has been written accordingly */

#include "errno.h"
#include "mklib.h"	/* for port communication functions */
#include "fdc.h"
#include "timer.h"
#include "alfunc.h"

struct core_header dma_ch;
DWORD        dma_addr;         /* Physical address of DMA buf  */
FDC          pri_fdc;          /* Status of the primary FDC    */
int          drivePresent;     /* Whether the disk drive is present */
volatile int motor_on_flag = 0;
struct event_t fdc_waitevent;
struct lock_t fdc_lck;
struct event_t floppy_event;

// floppy information
FDD fdd;

static volatile short int fdcwait_flag;
struct floppy_request drbuf[MAX_FLOPPY_REQUESTS];

struct event_t floppy_req_entry_free_event;
struct lock_t free_drbuf_lock;
volatile struct floppy_request *free_drbuf_head = NULL;
struct lock_t req_drbuf_lock;
volatile struct floppy_request *req_drbuf_head = NULL;
struct kthread * fdc_thread = NULL;

extern volatile struct kthread *current_thread;
extern volatile struct process *current_process;
extern struct process system_process;
extern struct segdesc gdt_table[];


void irq_entry_0_6(void);
void set_irq_gate(int irqno, void (*handler)(int irq));
//------------------------------------------------------------------------
// Floppy driver functions

// Send a byte to the controller FIFO
static int send_byte(unsigned byte) {
        unsigned msr; /* main status register */
        unsigned tries;
        
	for(tries = 0; tries < 128; tries++) {
                msr = in_byte(MAIN_STATUS_REGISTER);   /* Get contents for MSR */
                if((msr & 0xc0) == 0x80) {
                        /* This means data register is ready,
                           and the controller expects data from the CPU */
                        out_byte(DATA_FIFO, byte);
                        return FDC_OK;
                }
                in_byte(0x80);       /* just for delay */
        }
        return FDC_TIMEOUT;    /* write timeout */
}

// Receive a byte from the controller FIFO
static int get_byte(void) {
        unsigned msr;
        unsigned tries;

        for(tries = 0; tries < 512; tries++) {
                msr = in_byte(MAIN_STATUS_REGISTER);

                if((msr & 0xd0) == 0xd0)
		{
                        /* This means three things -
                                data register is ready,
                                controller is ready to send to CPU, and
                                device is active - currently executing a command
                         */

                        return (int) in_byte(DATA_FIFO);
		}
                in_byte(0x80);       /* just for delay */
        }

        return FDC_TIMEOUT;    /* read timeout */
}

// Enable drive and motor via DOR
// Introduce spin-up delay explicitly if required
void motor_on(FDD *fdd) {
	unsigned long oflags;

	CLI;
	fdd->fdc->dor = 0x1c;
	out_byte(DIGITAL_OUTPUT_REGISTER, fdd->fdc->dor);
	motor_on_flag = 1;
	STI;
}

// Turn the motor off, else the green light will keep glowing
// Introduce spin-down delay explicitly if required
void motor_off(FDD *fdd) {
	unsigned long oflags;

	CLI;
	fdd->fdc->dor = 0x0c;
	out_byte(DIGITAL_OUTPUT_REGISTER, fdd->fdc->dor);
	motor_on_flag = 0;
	STI;
}

void fdc_intr_handler(int n) {
	struct kthread *ithr;

	// Remember the previous interrupted thread and clear its task busy bit
	ithr = (struct kthread *)current_thread;
	// Change the high level task related pointers to point to 
	// this kbd task.
	current_process = &system_process;
	current_thread = fdc_thread;
	current_thread->kt_schedinf.sc_state = THREAD_STATE_RUNNING;
	current_thread->kt_schedinf.sc_tqleft = TIME_QUANTUM;
	ithr->kt_schedinf.sc_state = THREAD_STATE_READY;
	ithr->kt_cr0 = get_cr0();
	gdt_table[ithr->kt_tssdesc >> 3].b &= 0xfffffdff;
	math_state_save(&ithr->kt_fps);
	math_state_restore((struct fpu_state *)&fdc_thread->kt_fps);
	add_readyq(ithr);
	// Enable interrupts now
	sti_intr();

	_lock(&fdc_lck);
	fdcwait_flag = 0;	// Expected interrupt completed
	event_wakeup(&fdc_waitevent);
	unlock(&fdc_lck);

	// schedule the interrupted thread at this point
	cli_intr();
	current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
	enable_irq(6);
	scheduler();
}

// Waits for FDC command to complete. Returns FDC_TIMEOUT on time out.
static int fdc_wait(FDC *fdc, unsigned int msec) {
	int err = 0;

	/* Wait for the interrupt */
	_lock(&fdc_lck);
	if (fdcwait_flag != 0) err = event_timed_sleep(&fdc_waitevent, &fdc_lck, msec);
	unlock(&fdc_lck);

	/* Read in command result bytes while controller is busy */
	fdc->resultSize = 0;
	while ((fdc->resultSize < 7) && (in_byte(MAIN_STATUS_REGISTER) & 0x10))
		fdc->result[(int) fdc->resultSize++] = get_byte();

	if(err != 0)  return FDC_TIMEOUT;
		/* Thus, return timeout here */

	fdcwait_flag = 0;
	/* Reaching here means the command executed properly */
	return FDC_OK;
}

// DMA transfer. If 'read' is set, transfer is from memory to device, 
// otherwise the opposite. Page size is 64k
static void dma_transfer(DWORD phy_addr, unsigned length, int op) {
	DWORD page, offset;
	BYTE mode;

	/* Calculate the variables */
	page = phy_addr >> 16;
	offset = phy_addr & 0xffff;
	length -= 1; 		/* in DMA, if you need k bytes, request k-1 */
	mode = (op == FDC_READ ? 0x44 : 0x48) + DMA_CHANNEL;

	/* Now use port addressing to perform the DMA transfer */

	out_byte(0x0a, DMA_CHANNEL | 4);	/* Set channel mask bit */
	out_byte(0x0c, 0);			/* Clear flip-flop */
	out_byte(0x0b, mode);		/* set mode register */
	out_byte(DMA_PAGE, page);
	out_byte(DMA_OFFSET, offset & 0xff); /* send lower byte first, then higher */
	out_byte(DMA_OFFSET, offset >> 8);
	out_byte(DMA_LENGTH, length & 0xff); /* similarly, length */
	out_byte(DMA_LENGTH, length >> 8);
	out_byte(0x0a, DMA_CHANNEL);		/* clear DMA mask bit */
}

// Recalibrates a drive (seek head to track 0).
// Since the RECALIBRATE command sends up to 79 pulses to the head,
// this function issues as many RECALIBRATE as needed.
// The drive is supposed to be selected (motor on).
static void recalibrate(FDD *fdd) {
	int k;

	for (k = 0; k < 13; k++) {
		fdcwait_flag = 1;
		send_byte(CMD_RECAL);
		send_byte(0x00);
		fdc_wait(fdd->fdc, 200);
		/* Send a "sense interrupt status" command */
		send_byte(CMD_SENSEINS);
		fdd->fdc->sr0 = get_byte();
		fdd->track    = get_byte();
		if (!(fdd->fdc->sr0 & 0x10))
			break; /* Exit if Unit Check is not set */
	}
}

// Specify command
// To program data rate and specify drive timings
static void specify(void) {

	send_byte(CMD_SPECIFY);
	send_byte(fmt_srHut);
	send_byte(dp_hlt << 1);	// To ensure that DMA mode is enabled
}

// Seeks a drive to the specified track.
// The drive is supposed to be selected (motor on).
static int fdc_seek(FDD *fdd, unsigned track) {

	int oldtrack, diff_track;

	oldtrack = fdd->track;
	if (fdd->track == track) return FDC_OK;		/* Already there */
	if (track >= fmt_tracks) return FDC_ERROR;	/* Invalid track */
	

	if (track > fdd->track) diff_track = track - fdd->track;
	else diff_track = fdd->track - track;
	if (diff_track < 5) diff_track = 5;

	fdcwait_flag = 1;
	send_byte(CMD_SEEK);
	in_byte(0x80);
	send_byte(0x00);
	in_byte(0x80);
	send_byte(track);

	if (fdc_wait(fdd->fdc, 100+15*diff_track) != FDC_OK) return FDC_TIMEOUT;	/* Timeout */

	/* Send a "sense interrupt status" command */
	send_byte(CMD_SENSEINS);
	in_byte(0x80);
	fdd->fdc->sr0	= get_byte();
	in_byte(0x80);
	fdd->track	= get_byte();

	/* Check that seek worked */
	if ((fdd->fdc->sr0 != 0x20) || (fdd->track != track)) {
		printk("[FDC:fdc_seek] Seek error on drive 0: SR0=%x, Track=%u Expected=%u oldtrack : %u \n", fdd->fdc->sr0, fdd->track, track, oldtrack);
		return FDC_ERROR;
	}

	return FDC_OK;
}

// Resets the FDC to a known state
// Performs the initialization too
void fdc_reset(FDC *fdc) {
	int err;

	fdcwait_flag = 1;
	out_byte(DIGITAL_OUTPUT_REGISTER, 0);	/* Stop the motor and disable IRQ/DMA  */

	/* Add a small delay (20 us) to make older controllers more happy */

	in_byte(0x80);       /* just for delay */
	out_byte(DIGITAL_OUTPUT_REGISTER, 0x0C); /* Re-enable IRQ/DMA and release reset */
	fdc->dor = 0x0C;

	/* Set the data transfer rate */

	out_byte(CONFIGURATION_CONTROL_REGISTER, fmt_rate);

	err = fdc_wait(fdc, 200);
	if (err != 0)
		printk("[FDC:fdc_reset] Timed out while waiting for FDC after reset\n");

	send_byte(CMD_SENSEINS);
	fdc->sr0	= (BYTE) get_byte();
	get_byte();

	specify();
}

// Initializes a drive to a known state
int setup_drive(FDD *fdd) {
	fdd->track = 0;
	fdd->fdc = &pri_fdc;

	return FDC_OK;
}

// Initialize the driver
/*
 * Memory at 0x0000 0010 is reserved for DMA with floopy controller
 * size of that area is enough to hold (one or four ?) track at a time.
 * (512 * 18 * 2 * 2) bytes
 */
int fdc_setup(FDD *fdd) {
	unsigned		k  = 0;
	unsigned		cmos_drive;
	int			res = FDC_OK;

	/* Here I'll write the code for binding the IRQ 6 to the interrupt handler */
	lockobj_init(&fdc_lck);
	eventobj_init(&fdc_waitevent);
	lockobj_init(&free_drbuf_lock);
	lockobj_init(&req_drbuf_lock);
	eventobj_init(&floppy_event);
	eventobj_init(&floppy_req_entry_free_event);

	fdc_thread = initialize_irqtask(6, "FDC", irq_entry_0_6, IT_PRIORITY_MIN + 5);
	/* To find a decent size for the DMA buffer */
	/* This is just like reserving some memory in the RAM */
	dma_ch.phys_addr = 0x0010;
	dma_ch.no_blocks = (512 * 18 * 2 * 2) / (4 * 1024);

	dma_addr = dma_ch.phys_addr;

	/* Reset primary controller */
	fdc_reset(&pri_fdc);

	/* Read floppy drives types from CMOS memory (up to two drives). */
	/* They are supposed to belong to the primary FDC.*/
	out_byte(0x70, 0x10);
	k = in_byte(0x71);
	cmos_drive = k >> 4;

	/* check if the drive installed is the one for which the driver is written */
	if(!(k & 0x04)) {
		/* This means the floppy drive 0 is 1.44MB - so we can load the driver */
		drivePresent = 1;
		setup_drive(fdd);
	}
	else {
		drivePresent = 0;
		res = FDC_ERROR;
	}
       
	return res;
}

void reset_drive(FDD *fdd)
{
	if (drivePresent == 0) return;
	fdd->track   = 0;
	specify();
	motor_on(fdd);
	recalibrate(fdd);
	motor_off(fdd);
	
	syscall_tsleep(dp_spinDown);

}

// Transfers sectors of data between disk and DMA buffer
static int fdc_transfer(FDD *fdd, const Chs *chs, DWORD dma_addr, unsigned num_sectors, int op) {
	char command = (op == FDC_READ ? CMD_READ : CMD_WRITE);

	/* Send the read/write command */
	fdcwait_flag = 1;
	dma_transfer(dma_addr, 512 * num_sectors, op);

	send_byte(command);
	send_byte(chs->h << 2);
	send_byte(chs->c);
	send_byte(chs->h);
	send_byte(chs->s);
	send_byte(2);	/* 512 bytes per sector */
	send_byte(fmt_sectorsPerTrack);
	send_byte(fmt_gap3);
	send_byte(0xff);	/* DTL (bytes to transfer) = unused */

	/* Wait for completion and produce exit code */
	if(fdc_wait(fdd->fdc, 200) != FDC_OK) {
		fdc_reset(fdd->fdc);
		reset_drive(fdd);
		return FDC_TIMEOUT;
	}

	if( (fdd->fdc->result[0] & 0xc0) == 0)
		return FDC_OK;

	return FDC_ERROR;
}

// Reads sector from the floppy, up to the end of the cylinder
// If the buffer is NULL, read data is discarded
int fdc_read(FDD *fdd, const Chs *chs, DWORD buffer, unsigned num_sectors) {
	unsigned tries;
	int res = FDC_ERROR;

	if(!drivePresent) return FDC_ERROR;	/* Drive not available */

	specify();

	for(tries = 0; tries < 3; tries++) {
		/* Move the head to the right track */
		if(fdc_seek(fdd, chs->c) == FDC_OK) {
	
			/* If changeline is active, no disk in the drive */
			if(in_byte(DIGITAL_INPUT_REGISTER) & 0x80) {
				res = FDC_NODISK;
				break;
			}
			res = fdc_transfer(fdd, chs, dma_addr, num_sectors, FDC_READ);

			if(res == FDC_OK || res == FDC_TIMEOUT) break;
		}
		else res = FDC_ERROR;
		if(tries == 2) recalibrate(fdd);

	}

	/* Copy data from DMA buffer to the caller's buffer */
	if((res == FDC_OK) && buffer)
		bcopy((char *)dma_addr, (char *)buffer, num_sectors * 512);

	return res;
}

// Writes sectors to the floppy, up to the end of the cylinder
int fdc_write(FDD *fdd, const Chs *chs, const DWORD buffer, unsigned num_sectors) {
	unsigned tries;
	int res = FDC_ERROR;

	if(!drivePresent || !buffer) return FDC_ERROR;	/* Drive not available */

	specify();

	bcopy((char *)buffer, (char *)dma_addr, num_sectors * 512);

	for(tries = 0; tries < 3; tries++) {
		/* Move the head to the right track */
		if(fdc_seek(fdd, chs->c) == FDC_OK) {
			/* If changeline is active, no disk in the drive */
			if(in_byte(DIGITAL_INPUT_REGISTER) & 0x80) {
//			
				res = FDC_NODISK;
				break;
			}
			res = fdc_transfer(fdd, chs, dma_addr, num_sectors, FDC_WRITE);
		
			if(res == FDC_OK || res == FDC_TIMEOUT) break;
		}
		else
			res = FDC_ERROR;
		if(tries == 2) recalibrate(fdd);
	
	}

	return res;
}

// Issues the format command and waits for its completion
// Assumes that seek has been performed and
//  the head is positioned onto the required track
static int issue_format(FDD *fdd, unsigned head, DWORD dma_addr[]) {

	fdcwait_flag = 1;
	dma_transfer((DWORD)dma_addr, fmt_sectorsPerTrack * 4, FDC_WRITE);
	
	send_byte(CMD_FORMAT);
	send_byte(head << 2);
	send_byte(2);
	send_byte(fmt_sectorsPerTrack);
	send_byte(fmt_gap3Format);
	send_byte(0x00);

	/* Wait for completion and produce exit code */
	if(fdc_wait(fdd->fdc, 200) != FDC_OK) {
		fdc_reset(fdd->fdc);
		reset_drive(fdd);
		return FDC_TIMEOUT;
	}

	if( (fdd->fdc->result[0] & 0xc0) == 0) return FDC_OK;

	return FDC_ERROR;
}

static void init_format_buffer(DWORD format_buffer[], unsigned track, unsigned head) {
	int i;
	char *ptr = (char *) format_buffer;

	for(i = 0; i < fmt_sectorsPerTrack; i++, ptr += 4) {
		ptr[0] = track;
		ptr[1] = head;
		ptr[2] = i+1;
		ptr[3] = 2;
	}
}

// Frees low-level driver resources
// Low-level formatting of the diskette
// Implements the flormatting of the floppy disk using
//  the floppy disk controller's FORMAT command
int fdc_format(FDD *fdd) {
	unsigned track = 0, head = 0;
	int res = FDC_ERROR;
	DWORD format_buffer[fmt_sectorsPerTrack]; 

	if(!drivePresent) 
		return FDC_ERROR;	/* Drive not available */

	recalibrate(fdd);
	specify();

	for(track = 0; track < fmt_tracks; track++) {
		/* Move the head to the right track */
		if(fdc_seek(fdd, track) == FDC_OK) {
			/* If changeline is active, no disk in the drive */
			if(in_byte(DIGITAL_INPUT_REGISTER) & 0x80) {
				res = FDC_NODISK;
				break;
			}
			// Issue the format command for both the floppy heads
			for(head = 0; head < fmt_heads && res != FDC_TIMEOUT; head++) {
				init_format_buffer(format_buffer, track, head);
				res = issue_format(fdd, head, format_buffer);
			}

			if(res == FDC_TIMEOUT) break;
		}
		else res = FDC_ERROR;
	}

	return res;
}

// Higher level floppy disk functions - to be used by the kernel

/* Make the disk request buffers into a linked list */
void initialize_floppy_request_bufs(void) {
	int i;
	struct floppy_request *p;

	req_drbuf_head = NULL;
	p = &drbuf[0];
	eventobj_init(&p->complete);
	free_drbuf_head = p;
	for (i=1; i<MAX_FLOPPY_REQUESTS; i++) {
		p->next = &drbuf[i];
		p = p->next;
		eventobj_init(&p->complete);
	}
	p->next = NULL;
}

#ifdef DEBUG
void print_readyq(void)
{
	int i;
	struct kthread *t;
	struct sleep_q *slq;
	extern volatile struct kthread *ready_qhead[];

	printk("\nready threads :\n");
	for (i=0; i<MAXREADYQ; i++)
	{
		t = (struct kthread *)ready_qhead[i];
		while (t != NULL)
		{
			if (t->kt_type != PROC_TYPE_SYSTEM || t->kt_threadid >= 0)
				printk("TASK [%s]->%d, %x, %x, %x eip : %x, pri : %d, %d q: %d",t->kt_name, t->kt_schedinf.sc_state, t->kt_slpchannel, t->synchtry, t->synchobtained, t->kt_tss->eip, t->kt_schedinf.sc_usrpri, t->kt_schedinf.sc_cpupri, i);
			slq = (struct sleep_q *)t->kt_synchobjlist;
			while (slq != NULL)
			{
				printk("%x-%d,",slq,slq->sl_type);
				slq = (struct sleep_q *)slq->sl_next;
			}
			t = (struct kthread *)t->kt_qnext;
		}
	}
}
#endif

struct floppy_request *enqueue_floppy_request(char device, unsigned long lba, unsigned blocks, void *buffer, int op) {
	struct floppy_request *dreq, *p; //, *q;

	while (1) {	/* Repeat until request gets queued */
		_lock(&free_drbuf_lock);	
enq1:
		if (free_drbuf_head == NULL) {
			/* No free buffer */
			event_sleep(&floppy_req_entry_free_event, &free_drbuf_lock);
			goto enq1;
		}
		else {
			/* Request queue slot available */
			dreq = (struct floppy_request *)free_drbuf_head;
			free_drbuf_head = free_drbuf_head->next;
			unlock(&free_drbuf_lock);

			/* Initialize the buffer */
			dreq->lba_start = lba;
			dreq->blocks = blocks;
			dreq->buffer = buffer;
			dreq->operation = op;

			/* Insert this into req queue in 
			 * the order of logical block address (LBA). */
			_lock(&req_drbuf_lock);
			if (req_drbuf_head == NULL) {
				req_drbuf_head = dreq;
				dreq->next = NULL;
			}
			else {
				p = (struct floppy_request *)req_drbuf_head;
				if (p->lba_start >= dreq->lba_start) {
					// Inserted at the beginning
					dreq->next = p;
					req_drbuf_head = dreq;
				}
				else {
					while ((p->next !=  NULL) && ((p->next)->lba_start < dreq->lba_start)) 
						p = p->next;

					dreq->next = p->next;
					p->next = dreq;
				}
			}

			if (floppy_event.e_slq.sl_head != NULL)
				event_wakeup(&floppy_event);



			event_sleep(&(dreq->complete), &req_drbuf_lock);
			unlock(&req_drbuf_lock);
			return (struct floppy_request *)dreq;
		}
	}
}

/* Assuming that it is already removed from request queue
 * it is to be added to free disk request buffer list. */
void free_floppy_request(struct floppy_request *dr) {
	_lock(&free_drbuf_lock);
	dr->next = (struct floppy_request *)free_drbuf_head;
	free_drbuf_head = dr;
	if (floppy_req_entry_free_event.e_slq.sl_head != NULL)
		event_wakeup(&floppy_req_entry_free_event);
	unlock(&free_drbuf_lock);
}

/* Functions implementing floppy disk operations */

/* Converts an absolute sector number (LBA) to CHS using disk geometry.
 * Returns 0 on success, or a negative value meaning invalid sector number.
 * There are a total of 2880 logical blocks in the diskette */
static int lba_to_chs(unsigned lba, Chs *chs)
{
	unsigned temp;
	unsigned int cyl_bytes = fmt_heads * fmt_sectorsPerTrack;

	/* Perform simple bound check */ 
	if (lba >= cyl_bytes * fmt_tracks) return -1;	
	
	/* The product turns out to be 2880 */
	chs->c = lba / cyl_bytes;
	temp   = lba % cyl_bytes;
	chs->h = temp / fmt_sectorsPerTrack;
	chs->s = temp % fmt_sectorsPerTrack + 1;
	return 0;
}

/* Reads sectors from a disk into a user buffer.
 * Returns 0 on success, or a negative value on failure. */
int floppy_read(FDD *fdd, unsigned start, unsigned count, void *buffer)
{
	int res;
	unsigned n,m;
	Chs chs;
	unsigned char * b = (unsigned char *) buffer;

	m = count;
	while (m > 0)
	{
		if (lba_to_chs(start, &chs)) return -1;

		n = (fmt_sectorsPerTrack - chs.s + 1); // No of sectors in current track.
		n = (n < m) ? n : m;

		res = fdc_read(fdd, &chs, (unsigned long)b, n);
		if (res < 0) return res;

		start = start + n;
		m -= n;
		b = b + (n *  512);
	}

	return count;
}

/* Writes to a logical block - purpose is the same as read sector */

/* Writes sectors from a user buffer into disk
 * Returns 0 on success, or a negative value on failure. */
int floppy_write(FDD *fdd, unsigned start, unsigned count, const void *buffer)
{
	int res;
	unsigned n,m;
	Chs chs;
	unsigned char * b = (unsigned char *) buffer;

	m = count;
	while (m > 0)
	{
		if (lba_to_chs(start, &chs)) return -1;
		n = (fmt_sectorsPerTrack - chs.s + 1); // No of sectors in current track.
		n = (n < m) ? n : m;
		res = fdc_write(fdd, &chs, (unsigned long)b, n);

		if (res < 0) return res;
		start = start + n;
		m -= n;
		b = b + (n *  512);
	}

	return count;
}

/* Formats a floppy with low-level formatting */
int floppy_format(FDD *fdd) {
	if(fdc_format(fdd) == FDC_OK)
		return 0; /* That's all actually, the controller takes care */

	return -1;
}

/* Sets up the floppy controller for use by initializations */
int floppy_setup(FDD *fdd) {
	if(fdc_setup(fdd) == FDC_OK)
		return 0;	/* Controller routine for initialization */

	return -1;
}

// new fun code here
void floppy_thread(void)
{
	int i;	
	struct floppy_request *freq;	

	for(;;)
	{
		_lock(&req_drbuf_lock);

floppy_thread_1:
		if(req_drbuf_head==NULL)
		{
			if (motor_on_flag == 1) motor_off(&fdd);
			event_sleep(&floppy_event, &req_drbuf_lock);
			goto floppy_thread_1;
		}

		if (motor_on_flag == 0)
		{
			motor_on(&fdd);
			syscall_tsleep(dp_spinDown);
		}

		freq = (struct floppy_request *)req_drbuf_head;
		req_drbuf_head = freq->next;
		freq->next = NULL;
		unlock(&req_drbuf_lock);

		switch(freq->operation)
		{
			case FLOPPY_READ :
				for (i=0; i<10; i++)
				{
					freq->status = floppy_read(&fdd,freq->lba_start,freq->blocks,(char *)freq->buffer);
					if (freq->status >= 0) break;
					if (motor_on_flag == 0)
					{
						motor_on(&fdd);
						syscall_tsleep(dp_spinDown);
					}
				}
				break;
				
			case FLOPPY_WRITE:
				for (i=0; i<10; i++)
				{
					freq->status = floppy_write(&fdd,freq->lba_start,freq->blocks,(char *)freq->buffer);
					if (freq->status >= 0) break;
					if (motor_on_flag == 0)
					{
						motor_on(&fdd);
						syscall_tsleep(dp_spinDown);
					}
				}
				break;
			case FLOPPY_FORMAT:
				for (i=0; i<10; i++)
				{
					freq->status = floppy_format(&fdd);
					if (freq->status == 0) break;
					if (motor_on_flag == 0)
					{
						motor_on(&fdd);
						syscall_tsleep(dp_spinDown);
					}
				}
				break;
		} 
		// Wake up the requesting thread
		_lock(&req_drbuf_lock);
		event_wakeup(&(freq->complete));
		unlock(&req_drbuf_lock);
	}
} 

