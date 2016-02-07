
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


#include "ne2k.h"
#include "ne2000.h"
#include "packet.h"
#include "process.h"
#include "mklib.h"
#include "vmmap.h"
#include "timer.h"
#include "errno.h"
#include "alfunc.h"

#define outportg(a, b)	out_byte(b, a)
#define outportgw(a, b)	out_word(b, a)
#define inportb(a)	in_byte(a)
#define inport(a)	in_word(a)

#define RUNNING		THREAD_STATE_RUNNING
#define SLEEP		THREAD_STATE_SLEEP

unsigned int default_ports[] = { 0x220, 0x240, 0x260, 0x280, 0x300, 0x320, 0x340, 0x360, 0}; 
snic nic;
struct lock_t rxq_lock, txq_lock;
volatile frame_buffer *rx_q_head = NULL, *rx_q_tail = NULL;
volatile frame_buffer *tx_q_head = NULL, *tx_q_tail = NULL;
struct event_t nic_recv_event, nic_send_event, rx_q_empty;
volatile frame_buffer *tx_wait_pkt = NULL;
frame_buffer dummypkt;
unsigned short int this_hostid, no_hosts = 4; // = 32;
unsigned char *this_hwaddr = NULL;
struct kthread *nic_intr_thread, *nicthr;
unsigned short __ethseqno__ = 0;
unsigned short ethseqno_list[MAXHOSTS];
extern unsigned char host_hwaddr_table[MAXHOSTS][6];
extern volatile struct kthread *current_thread;
extern volatile struct process *current_process;
extern struct process system_process;
extern struct lock_t socktable_lock;
extern struct lock_t socklist_lock;
extern struct event_t pkt_send_event;
extern struct segdesc gdt_table[];

static int ring_ovfl(snic *nic);
void nic_isr(int isr);

void set_irq_gate(int irqno, void (* handler)(int irq));
int syscall_puts(char *str);
void irq_entry_1_1(void);

static int nic_probe(int addr)
{
	unsigned int regd;
	unsigned int state=inportb(addr);

	if(inportb(addr)==0xff) return ENOTFOUND;

	outportg(NIC_DMA_DISABLE | NIC_PAGE1 | NIC_STOP, addr);
	regd=inportb(addr + 0x0d);
	outportg(0xff, addr + 0x0d);
	outportg(NIC_DMA_DISABLE | NIC_PAGE0, addr);
	inportb(addr + FAE_TALLY);
	if(inportb(addr + FAE_TALLY)) 
	{
		outportg(state,addr);
		outportg(regd,addr + 0x0d);
		return ENOTFOUND; 
	}

	return addr;
}

static int nic_detect(int given)
{
	int found; int f;
	if(given) if((found=nic_probe(given))!=ENOTFOUND) 
	{
		return found; 
	}

	for(f=0; default_ports[f]!=0; f++) 
	{
		if((found=nic_probe(default_ports[f]))!=ENOTFOUND) 
		{
			return found; 
		}
	}
	return ENOTFOUND;
}

static int nic_setup(snic* nic, int addr, unsigned char *prom, unsigned char *manual)
{
	unsigned int f;

	if(!nic->iobase) {
		nic->iobase=addr;
		nic->pstart=0; nic->pstop=0; nic->wordlength=0;
		nic->current_page=0;
		nic->tx_frame= NULL;
		lockobj_init(&nic->busy);
	}
	else if(!nic->iobase || nic->iobase!=addr) return ERR;

	outportg(inportb(addr + NE_RESET), addr + NE_RESET);
	outportg(0xff,addr + INTERRUPTSTATUS);
	outportg(NIC_DMA_DISABLE | NIC_PAGE0 | NIC_STOP, addr);
	outportg(DCR_DEFAULT, addr + DATACONFIGURATION);
	outportg(0x00, addr + REMOTEBYTECOUNT0);
	outportg(0x00, addr + REMOTEBYTECOUNT1);
	outportg(0x00, addr + INTERRUPTMASK);
	outportg(0xff, addr + INTERRUPTSTATUS);
	outportg(RCR_MON, addr+RECEIVECONFIGURATION);
	outportg(TCR_INTERNAL_LOOPBACK, addr+TRANSMITCONFIGURATION);


	nic->wordlength=nic_dump_prom(nic,prom);
	if(prom[14]!=0x57 || prom[15]!=0x57) {
		printk("NE2000: PROM signature does not match NE2000 0x57.\n");
		return ENOTFOUND;
	}

	if(nic->wordlength==2)
	{
		outportg(DCR_DEFAULT_WORD, addr + DATACONFIGURATION);
	}
	nic->pstart=(nic->wordlength==2) ? PSTARTW : PSTART;
	nic->pstop=(nic->wordlength==2) ? PSTOPW : PSTOP;

	outportg(NIC_DMA_DISABLE | NIC_PAGE0 | NIC_STOP, addr);
	outportg(nic->pstart, addr + TRANSMITPAGE);
	outportg(nic->pstart + TXPAGES, addr + PAGESTART);
	outportg(nic->pstop - 1, addr + BOUNDARY);
	outportg(nic->pstop, addr + PAGESTOP);
	outportg(0x00, addr + INTERRUPTMASK);
	outportg(0xff, addr + INTERRUPTSTATUS);
	nic->current_page=nic->pstart + TXPAGES;
	outportg(NIC_DMA_DISABLE | NIC_PAGE1 | NIC_STOP, addr);
	if(manual) for(f=0;f<6;f++) outportg(manual[f], addr + PHYSICAL + f);
	else for(f=0;f<LEN_ADDR;f++) outportg(prom[f], addr + PHYSICAL + f);

	this_hostid = (inportb(addr+PAR5)) - 1; 
	this_hwaddr = host_hwaddr_table[this_hostid];
	printk(" [[ This host id : %d ]] ", this_hostid); 

	for(f=0;f<8;f++) 
		outportg(0xFF, addr + MULTICAST + f); 
	outportg(nic->pstart+TXPAGES, addr + CURRENT); 
	outportg(NIC_DMA_DISABLE | NIC_PAGE0 | NIC_STOP, addr); 
	return NOERR; 
} 
static void nic_start(snic *nic, int promiscuous) 
{ 
	int iobase; 
	if(!nic || !nic->iobase) 
	{ 
		printk("NE2000: can't start a non-initialized card.\n"); 
		return; 
	} 
	iobase=nic->iobase; 
	outportg(0xff, iobase + INTERRUPTSTATUS); 
	outportg(IMR_DEFAULT, iobase + INTERRUPTMASK); 
	outportg(NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START, iobase); 
	outportg(TCR_DEFAULT, iobase + TRANSMITCONFIGURATION); 
	if(promiscuous) 
		outportg(RCR_PRO | RCR_AM, iobase + RECEIVECONFIGURATION); 
	else 
		outportg(RCR_DEFAULT, iobase + RECEIVECONFIGURATION); 
} 

// Main interrupt handling task.
void nic_isr(int irq) 
{ 
	unsigned int isr; 
	int iobase = nic.iobase; 
	int status = 0;
	struct kthread *ithr;

	// Remember the previous interrupted thread and clear its task busy bit
	ithr = (struct kthread *)current_thread;

	// Change the high level task related pointers to point to 
	// this nic thread.
	current_process = &system_process;
	current_thread = nic_intr_thread;
	current_thread->kt_schedinf.sc_state = THREAD_STATE_RUNNING;
	current_thread->kt_schedinf.sc_tqleft = TIME_QUANTUM;

	ithr->kt_schedinf.sc_state = THREAD_STATE_READY;
	ithr->kt_cr0 = get_cr0();
	gdt_table[ithr->kt_tssdesc >> 3].b &= 0xfffffdff;
	math_state_save(&ithr->kt_fps);
	math_state_restore(&nic_intr_thread->kt_fps);
	add_readyq(ithr);

	// Enable interrupts now
	sti_intr();

	_lock(&nic.busy);
	outportg(NIC_DMA_DISABLE | NIC_PAGE0, iobase); 

	while ((isr=inportb(iobase+INTERRUPTSTATUS)))
	{
		if (isr & (ISR_CNT | ISR_OVW | ISR_PTX | ISR_TXE))
		{
			if (isr & (ISR_CNT | ISR_OVW))
			{
				ring_ovfl(&nic);
				status = ETRANSMISSION;
			}
			if(isr & ISR_PTX) 
			{
				out_byte(iobase+INTERRUPTSTATUS, 0x0a);
				status = 1;
			}
			else if (isr & ISR_TXE)
			{
				out_byte(iobase+INTERRUPTSTATUS, 0x0a);
				status = ETRANSMISSION;
			}

			if (tx_wait_pkt != NULL)
			{
				tx_wait_pkt->status = status;
				event_wakeup((struct event_t *)&tx_wait_pkt->threadwait);
				tx_wait_pkt = NULL;
			}
		}
		if(isr & (ISR_PRX | ISR_RXE))
		{
			nic_rx(&nic);
		}
	}
	out_byte(iobase+INTERRUPTSTATUS, 0xff);
	outportg(IMR_DEFAULT, iobase +INTERRUPTMASK);
	unlock(&nic.busy);

	// Schedule the interrupted thread at this point.
	cli_intr();
	current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
	//current_thread->kt_schedinf.sc_reccpuusage = 0;
	
	enable_irq(9); // Enable the same irq again.
	scheduler();

	return;
}

static int ring_ovfl(snic *nic)
{
	unsigned char isr;
	int iobase = nic->iobase;
	int i;

	outportg(0x21, iobase);
	outportg(0x0, iobase + REMOTEBYTECOUNT0);
	outportg(0x0, iobase + REMOTEBYTECOUNT1);
	
	for (i=0; i<0x7fff; i++)
	{
		isr=inportb(iobase+INTERRUPTSTATUS);

		if ((isr & 0x80) == 0) break;
	}
	
	outportg(TCR_INTERNAL_LOOPBACK, iobase + TRANSMITCONFIGURATION);
	outportg(0x22, iobase);  //?
	nic_rx(nic);
	outportg(0x10, iobase+INTERRUPTSTATUS);

	outportg(TCR_DEFAULT, iobase + TRANSMITCONFIGURATION);

	return 0;
}

static int nic_send_frame(snic *nic, frame_buffer *buffer) 
{
	unsigned int iobase = nic->iobase;

	if (buffer->len>MAX_FRSIZE) return ETOOBIG;

	outportg(0x00, iobase + INTERRUPTMASK); // Disable interrupts
		
	nic->tx_frame = tx_wait_pkt = buffer;
	nic->tx_frame->page = nic->pstart ;
	nic_block_output(nic,(struct frame_buffer *)nic->tx_frame);

	if(nic_send(nic)<0) 
	{
		outportg(IMR_DEFAULT, iobase + INTERRUPTMASK);
		nic->tx_frame = NULL;

		return ERR;
	}

	outportg(IMR_DEFAULT, iobase + INTERRUPTMASK);
	return NOERR;
}

static int nic_dump_prom(snic *nic, unsigned char *prom)
{
	unsigned int f;
	int iobase=nic->iobase;
	char wordlength=2;
	unsigned char dump[32];
	outportg(32, iobase + REMOTEBYTECOUNT0);
	outportg(0x00, iobase + REMOTEBYTECOUNT1);
	outportg(0x00, iobase + REMOTESTARTADDRESS0);
	outportg(0x00, iobase + REMOTESTARTADDRESS1);
	outportg(NIC_REM_READ | NIC_START, iobase);
	for(f=0;f<32;f+=2) {
		dump[f]=inportb(iobase + NE_DATA);
		dump[f+1]=inportb(iobase + NE_DATA);
		if(dump[f]!=dump[f+1]) wordlength=1;
	}
	for(f=0;f<LEN_PROM;f++) prom[f]=dump[f+((wordlength==2)?f:0)];
	return wordlength;
}

static frame_buffer *alloc_frame_buffer(unsigned int size)
{
	frame_buffer *newframe=(frame_buffer *)kmalloc(sizeof(frame_buffer));
	if (newframe != NULL)
	{
		bzero((char *)newframe, sizeof(frame_buffer));
		lockobj_init(&newframe->frbuflock);
		eventobj_init(&newframe->threadwait);
		newframe->len1 = newframe->len=size;
		newframe->page=0;
		newframe->buf1=(char*)kmalloc(size);
		if (newframe->buf1 == NULL)
		{
			printk("Malloc failure in alloc_frame_buffer\n");
			kfree(newframe);
			newframe = NULL;
		}
	}
	else printk("Malloc failure in alloc_frame_buffer\n");
	return newframe;
}

void free_frame_buffer(frame_buffer *newframe)
{
	if (newframe->buf2 != NULL) kfree(newframe->buf2);
	if (newframe->buf3 != NULL) kfree(newframe->buf3);
	kfree(newframe->buf1);
	kfree(newframe);
}

static void nic_rx(snic *nic)
{
	unsigned int frame;	unsigned int rx_page;	unsigned int rx_offset;
	unsigned int len;	unsigned int next_pkt;	unsigned int numpages;
	int iobase=nic->iobase;
	nicframe_header header;
	frame_buffer *newframe;
	struct ack_frame *fr;

	while(1)
	{
		outportg(NIC_DMA_DISABLE | NIC_PAGE1, iobase);
		rx_page=inportb(iobase + CURRENT);
		outportg(NIC_DMA_DISABLE | NIC_PAGE0, iobase);
		frame=inportb(iobase + BOUNDARY)+1;
		if(frame>=nic->pstop) frame=nic->pstart+TXPAGES;
		if(frame != nic->current_page)
		{
			printk("NE2000: ERROR: mismatched read page pointers!\n");
			printk("NE2000: NIC-Boundary:%x  dev-current_page:%x\n",
				frame, nic->current_page);
		}

		if(frame==rx_page) break;	/* all frames read */
		rx_offset=frame << 8;

		nic_get_header(nic,frame,&header);
		len=header.count - sizeof(nicframe_header);
		next_pkt=frame + 1 + ((len+4)>>8);
		numpages=nic->pstop-(nic->pstart+TXPAGES);
		if ((header.next!=next_pkt)
		     && (header.next!=next_pkt + 1)
		     && (header.next!=next_pkt - numpages)
		     && (header.next != next_pkt +1 - numpages)){
			printk("NE2000: ERROR: Index mismatch.   header.next:%X  next_pkt:%X frame:%X\n", header.next,next_pkt,frame);
			nic->current_page=frame;
			outportg(nic->current_page-1, iobase + BOUNDARY);
			continue;
		}

		if(len<60 || len>1518) printk("NE2000: invalid frame size:%d\n",len);

		else if((header.status & 0x0f) == RSR_PRX)
		{
			// We have a good frame, so let's recieve it!
			newframe=alloc_frame_buffer(len);  
				
			if (newframe == NULL) // No memory
			{
				printk("NE2000 : No memory for new frame received\n");
				nic_block_input(nic, (unsigned char *)dummypkt.buf1, len, rx_offset+sizeof(nicframe_header));
				// Discarded
			}
			else
			{
				nic_block_input(nic,(unsigned char *)newframe->buf1,newframe->len, rx_offset+sizeof(nicframe_header));

				// Check for duplicate frame
				newframe->next = NULL;
				fr = (struct ack_frame *)newframe->buf1;
				if (fr->ethfrhdr_seqno != (ethseqno_list[fr->pkt_src_saddr_host])) // Other than previous one.
				{
					ethseqno_list[fr->pkt_src_saddr_host]  = fr->ethfrhdr_seqno;
					fr->ethfrhdr_seqno = 0; // Required to verify the checksum
					// Insert this frame into received frames q
					_lock(&rxq_lock);
					if (rx_q_tail == NULL) rx_q_head = rx_q_tail = newframe;
					else
					{
						rx_q_tail->next = newframe;
						rx_q_tail = newframe;
					}
					event_wakeup(&nic_recv_event);
					unlock(&rxq_lock);
				}
				else free_frame_buffer(newframe); // Duplicate
			}
		}
		else
		{
			printk("NE2000: ERROR: bad frame.  header-> status:%X next:%X len:%x.\n",
			header.status,header.next,header.count);
		}

		next_pkt=header.next;
		if(next_pkt >= nic->pstop)
		{
			printk("NE2000: ERROR: next frame beyond local buffer!  next:%x.\n", next_pkt);
			next_pkt=nic->pstart+TXPAGES;
		}

		nic->current_page=next_pkt;
		outportg(next_pkt-1, iobase + BOUNDARY);
	}
	outportg(ISR_PRX | ISR_RXE, iobase + INTERRUPTSTATUS);
}

static void nic_get_header(snic *nic, unsigned int page, nicframe_header *header)
{
	int iobase=nic->iobase;	unsigned int f;
	outportg(NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START, iobase);
	outportg(sizeof(nicframe_header), iobase + REMOTEBYTECOUNT0);
	outportg(0x00, iobase + REMOTEBYTECOUNT1);
	outportg(0x00, iobase + REMOTESTARTADDRESS0);
	outportg(page, iobase + REMOTESTARTADDRESS1);
	outportg(NIC_REM_READ | NIC_START, iobase);

	if(nic->wordlength==2) for(f=0;f<(sizeof(nicframe_header)>>1);f++)
		((unsigned short *)header)[f]=inport(iobase+NE_DATA);
	else for(f=0;f<sizeof(nicframe_header);f++)
		((unsigned char *)header)[f]=inportb(iobase+NE_DATA);

	outportg(ISR_RDC, iobase + INTERRUPTSTATUS);
}

static int nic_send(snic *nic)
{
	 unsigned int len;


	len = (nic->tx_frame->len<MIN_FRSIZE) ?  MIN_FRSIZE : nic->tx_frame->len;

	outportg(NIC_DMA_DISABLE |  NIC_PAGE0, nic->iobase);
	if(inportb(nic->iobase + STATUS) & NIC_TRANSMIT)
	{
		printk("NE2000: ERROR: Transmitor busy.\n");
		nic->tx_frame= NULL;
		printk("nic_send: returning with %d\n", ETIMEOUT);
		return ETIMEOUT;
	}

	outportg(len & 0xff,nic->iobase+TRANSMITBYTECOUNT0);
	outportg(len >> 8,nic->iobase+TRANSMITBYTECOUNT1);
	outportg(nic->tx_frame->page,nic->iobase+TRANSMITPAGE);
	outportg(NIC_DMA_DISABLE | NIC_TRANSMIT | NIC_START,nic->iobase);

	return NOERR;
}

static void nic_block_input(snic *nic, unsigned char *buf, unsigned int len, unsigned int offset)
{
	int iobase=nic->iobase;	unsigned int f;
	unsigned int xfers=len;
	
	outportg(NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START, iobase);
	outportg(len & 0xff, iobase + REMOTEBYTECOUNT0);
	outportg(len >> 8, iobase + REMOTEBYTECOUNT1);
	outportg(offset & 0xff, iobase + REMOTESTARTADDRESS0);
	outportg(offset >> 8, iobase + REMOTESTARTADDRESS1);
	outportg(NIC_REM_READ | NIC_START, iobase);

	if(nic->wordlength==2) {
		for(f=0;f<(len>>1);f++)
			((unsigned short *)buf)[f]=inport(iobase+NE_DATA);
		if(len&0x01) {
			((unsigned char *)buf)[len-1]=inportb(iobase+NE_DATA);
			xfers++;
		}
	} else for(f=0;f<len;f++)
		((unsigned char *)buf)[f]=inportb(iobase+NE_DATA);
	outportg(ISR_RDC, iobase + INTERRUPTSTATUS);
}


static void nic_block_output(snic *nic, frame_buffer *pkt)
{
	int iobase=nic->iobase;
	int f;	int tmplen;
	int  buflen;
	void *bufptr;
	char odd;

	outportg(NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START, iobase);

	if(pkt->len > MAX_FRSIZE) return;

	// this next part is to supposedly fix a "read-before-write" bug...
	outportg(0x42, iobase + REMOTEBYTECOUNT0);
	outportg(0x00, iobase + REMOTEBYTECOUNT1);
	outportg(0x42, iobase + REMOTESTARTADDRESS0);
	outportg(0x00, iobase + REMOTESTARTADDRESS1);
	outportg(NIC_REM_READ | NIC_START, iobase);

	outportg(ISR_RDC, iobase + INTERRUPTSTATUS);
	tmplen=(pkt->len < MIN_FRSIZE) ? MIN_FRSIZE : pkt->len;
	outportg(tmplen & 0xff, iobase + REMOTEBYTECOUNT0);
	outportg(tmplen >> 8, iobase + REMOTEBYTECOUNT1);
	outportg(0x00, iobase + REMOTESTARTADDRESS0);
	outportg(pkt->page, iobase + REMOTESTARTADDRESS1);
	outportg(NIC_REM_WRITE | NIC_START, iobase);

	odd = pkt->len & 0x01;
	if(nic->wordlength==2)
	{

		buflen=pkt->len1;
		bufptr=pkt->buf1;
		for(f=0;f<buflen>>1;f++)
			outportgw(((unsigned short *)bufptr)[f],iobase+NE_DATA);
		buflen=pkt->len2;
		bufptr=pkt->buf2;
		for(f=0;f<buflen>>1;f++)
			outportgw(((unsigned short *)bufptr)[f],iobase+NE_DATA);
		if (odd && buflen != 0)
		{
			short tmp=((unsigned char *)bufptr)[buflen-1];
			outportgw(tmp,iobase + NE_DATA);
		}
		buflen=pkt->len3;
		bufptr=pkt->buf3;
		for(f=0;f<buflen>>1;f++)
			outportgw(((unsigned short *)bufptr)[f],iobase+NE_DATA);

		if (odd && buflen != 0)
		{
			short tmp=((unsigned char *)bufptr)[buflen-1];
			outportgw(tmp,iobase + NE_DATA);
		}
		
	}
	else
	{
		buflen=pkt->len1;
		bufptr=pkt->buf1;
		for(f=0;f<buflen;f++)
			outportg(((unsigned char *)bufptr)[f],iobase+NE_DATA);
		buflen=pkt->len2;
		bufptr=pkt->buf2;
		for(f=0;f<buflen;f++)
			outportg(((unsigned char *)bufptr)[f],iobase+NE_DATA);
		buflen=pkt->len3;
		bufptr=pkt->buf3;
		for(f=0;f<buflen;f++)
			outportg(((unsigned char *)bufptr)[f],iobase+NE_DATA);
	}
	if(pkt->len < MIN_FRSIZE)
	{
		if(nic->wordlength==2)
		{
			for(f=pkt->len;f<MIN_FRSIZE;f+=2)
				outportgw(0x0000,iobase + NE_DATA);
		}
		else
			for(f=pkt->len;f<MIN_FRSIZE;f++)
				outportg(0x0000,iobase + NE_DATA);
	}
	outportg(ISR_RDC, iobase + INTERRUPTSTATUS);
}

extern struct kthread *intr_threads[];
void initialize_nic(void)
{
	lockobj_init(&rxq_lock);
	lockobj_init(&txq_lock);
	lockobj_init(&socktable_lock);
	lockobj_init(&socklist_lock);
	eventobj_init(&nic_recv_event);
	eventobj_init(&rx_q_empty);
	eventobj_init(&nic_send_event);
	eventobj_init(&pkt_send_event);

	intr_threads[9] = nic_intr_thread = initialize_irqtask(9, "NICTHR", irq_entry_1_1, IT_PRIORITY_MIN + 15);
	dummypkt.buf1 = (char *)kmalloc(1600);
	nic.iobase = nic_detect(0x0);
	nic_setup(&nic,nic.iobase,(unsigned char *)nic.prom,NULL);
	nic_start(&nic,0);
}

void syscall_machid(void)
{
	printk("Machine ID : %d (%02x:%02x:%02x:%02x:%02x:%02x)\n",this_hostid,this_hwaddr[0],this_hwaddr[1],this_hwaddr[2],this_hwaddr[3],this_hwaddr[4],this_hwaddr[5]);
	return;
}

extern volatile unsigned int secs;
volatile unsigned int nicwait_start=0;
void nicthread(void)
{
	int status;
	frame_buffer *sreq;
	struct ack_frame *fr;

	nicthr = (struct kthread *)current_thread;

	nicthr->kt_schedinf.sc_cid = SCHED_CLASS_INTERRUPT; // To raise its priority, even though not an interrupt task
	nicthr->kt_schedinf.sc_cpupri = IT_PRIORITY_MIN;
	
	// Watch transmit queue and send the frames
	for ( ; ; )
	{
		_lock(&txq_lock);
//nicthread_1:
		while (tx_q_head == NULL) // No request pending
			event_sleep(&nic_send_event, &txq_lock);

		sreq = (struct frame_buffer *)tx_q_head;
		tx_q_head = sreq->next;
		if (tx_q_head == NULL) tx_q_tail = NULL;
		sreq->next = NULL;
		unlock(&txq_lock);

		// Set frame sequence number
		fr = (struct ack_frame *)sreq->buf1;
		fr->ethfrhdr_seqno = ++__ethseqno__;

nicthread_2:
		_lock(&nic.busy);
		sreq->status = 0;
		status = nic_send_frame(&nic,sreq);

		if (status < 0)
		{
			unlock(&nic.busy);
			syscall_tsleep(50);
			goto nicthread_2;
		}

		event_timed_sleep(&sreq->threadwait, &nic.busy, 50);
		unlock(&nic.busy);

		// Only one frame at a time is sent.		
		sreq->status = status;
		sreq->completed = 1;
		if (sreq->freethis == 1)
			free_frame_buffer(sreq);
	}
}


