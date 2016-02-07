
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

#include "mconst.h"
#include "mklib.h"
#include "errno.h"
#include "lock.h"
#include "packet.h"
#include "socket.h"
#include "ne2k.h"
#include "process.h"
#include "vmmap.h"
#include "timer.h"
#include "alfunc.h"

#define MPI_ANY_SOURCE	-2
#define MPI_ANY_TAG	-1

#define RUNNING	THREAD_STATE_RUNNING
#define SLEEP	THREAD_STATE_SLEEP
#define MaxIndex 	50	// Hash table size for received packets info
extern volatile unsigned short ready_qlock;
int syscall_puts(char *str);
typedef struct nodetag
{
	int type;
	int host;
	int port;
	int next_seqno;
	int msec;
	struct nodetag *prev;
	struct nodetag *next;

} node;

extern struct lock_t rxq_lock, txq_lock;
extern volatile frame_buffer *rx_q_head, *rx_q_tail;
extern volatile frame_buffer *tx_q_head, *tx_q_tail;
extern struct event_t nic_recv_event, nic_send_event, rx_q_empty;
struct event_t pkt_send_event;
extern volatile unsigned int secs, millesecs;
extern unsigned short no_hosts;
extern unsigned short this_hostid;
extern unsigned char *this_hwaddr;

extern struct lock_t timerq_lock;
extern volatile struct timerobject *timers;

struct packet *rcvpkts[MAXACTIVEPACKETS];
struct lock_t socklist_lock;
volatile struct proc_sock *socklist_hdr = NULL;
unsigned char host_hwaddr_table[MAXHOSTS][6] = {
{0x70, 0x00, 0x00, 0x00, 0x00, 0x01}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x02},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x03}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x04},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x05}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x06},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x07}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x08},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x09}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x0a},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x0b}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x0c},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x0d}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x0e},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x0f}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x10},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x11}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x12},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x13}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x14},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x15}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x16},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x17}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x18},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x19}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x1a},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x1b}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x1c},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x1d}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x1e},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x1f}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x20},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x21}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x22},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x23}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x24},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x25}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x26},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x27}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x28},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x29}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x2a},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x2b}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x2c},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x2d}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x2e},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x2f}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x30},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x31}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x32},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x33}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x34},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x35}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x36},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x37}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x38},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x39}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x3a},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x3b}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x3c},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x3d}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x3e},
{0x70, 0x00, 0x00, 0x00, 0x00, 0x3f}, {0x70, 0x00, 0x00, 0x00, 0x00, 0x40},
};

unsigned int temp_proc_map[MAXHOSTS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};

extern volatile struct process *current_process;
extern volatile struct kthread *current_thread;
extern struct lock_t used_proclock;
extern struct lock_t suspended_proclock;
extern volatile struct process * used_processes;
extern volatile struct process * suspended_processes;
extern void *kmalloc(unsigned int size);
extern void kfree(void *ptr);
extern void free_frame_buffer(frame_buffer *fr);

node *hash_table[MaxIndex];
struct lock_t socktable_lock;
struct proc_sock socktable[MAXSOCKETS];
volatile unsigned short int next_internal_port_no = 0x004000; // = 0x004000;
struct kthread * assembthread;
struct kthread * trans;
unsigned char assembler_active = 0;

int get_processid(void) { return current_process->proc_id; }
int get_threadid(void) { return current_thread->kt_threadid; }

// Works correctly  only if len1, len2 are even 
unsigned short checksum(char *buf1, int len1, char *buf2, int len2, char *buf3, int len3)
{
	unsigned short check_sum = 0;
	unsigned short *si_ptr=(unsigned short *)buf1;
	int i;

	for(i=0 ; i<len1/2 ; i++)
	{
		check_sum ^= (unsigned short)((*si_ptr));
		si_ptr++;
	}
	if (len1 % 2 == 1)
	{
		check_sum ^= (((unsigned short)(*si_ptr)) & 0x00ff );
	}

	if (buf2 != NULL)
	{
		si_ptr = (unsigned short *)buf2;
		for(i=0 ; i<len2/2 ; i++)
		{
			check_sum ^= (unsigned short)((*si_ptr));
			si_ptr++;
		}
		if (len2 % 2 == 1)
		{
			check_sum ^= (((unsigned short)(*si_ptr)) & 0x00ff );
		}
	}
	
	if (buf3 != NULL)
	{
		si_ptr = (unsigned short *)buf3;
		for(i=0 ; i<len3/2 ; i++)
		{
			check_sum ^= (unsigned short)((*si_ptr));
			si_ptr++;
		}
		if (len3 % 2 == 1)
		{
			check_sum ^= (((unsigned short)(*si_ptr)) & 0x00ff );
		}
	}
	return check_sum;
}

void initialize_recvpkts(void)
{
	int i;
	for (i=0; i<MAXACTIVEPACKETS; i++) rcvpkts[i] = NULL;
}

int alloc_pktindex(struct eth_frame_rcv *ef)
{
	int i;

	// Only single thread uses this so, no need of locking.
	for (i=0; i<MAXACTIVEPACKETS; i++)
	{
		if (rcvpkts[i] == NULL) // Free packet slot found
		{
			rcvpkts[i] = kmalloc( sizeof(struct packet));

			if (rcvpkts[i] == NULL)
			{
				printk("Error : Alloc pkt index : kmalloc\n");
				return ENOMEM;
			}

			rcvpkts[i]->pkt_header.pkt_type = ef->pkt_type;
			rcvpkts[i]->pkt_header.pkt_dest.saddr_type = ef->pkt_dest_saddr_type;
			rcvpkts[i]->pkt_header.pkt_dest.saddr_host = ef->pkt_dest_saddr_host;
			rcvpkts[i]->pkt_header.pkt_dest.saddr_port = ef->pkt_dest_saddr_port;
			rcvpkts[i]->pkt_header.pkt_src.saddr_type = ef->pkt_src_saddr_type;
			rcvpkts[i]->pkt_header.pkt_src.saddr_host = ef->pkt_src_saddr_host;
			rcvpkts[i]->pkt_header.pkt_src.saddr_port = ef->pkt_src_saddr_port;
			rcvpkts[i]->pkt_header.pkt_offset = 0;
			rcvpkts[i]->pkt_header.pkt_length = ef->pkt_length;
			rcvpkts[i]->pkt_header.pkt_id = ef->pkt_id;
			rcvpkts[i]->pkt_header.pkt_tag = ef->pkt_tag;
			rcvpkts[i]->pkt_header.pkt_groupid = ef->pkt_groupid;
			rcvpkts[i]->pkt_header.pkt_winseq_no = 0;
			rcvpkts[i]->pkt_header.pkt_srcsockctime = ef->pkt_srcsockctime;

			rcvpkts[i]->pkt_data = NULL;
		
			rcvpkts[i]->pkt_rcvframe_head = NULL;
			rcvpkts[i]->pkt_rcvframe_tail = NULL;

			rcvpkts[i]->pkt_expc_seqno = 0;
			rcvpkts[i]->pkt_rcv_offset = 0;
			rcvpkts[i]->pkt_status = PKTSTATUS_RECEIVING;
			rcvpkts[i]->pkt_rcvddata = 0;
			rcvpkts[i]->pkt_ackdata = 0;
			
			timerobj_init(&rcvpkts[i]->pkt_tobj);
			lockobj_init(&rcvpkts[i]->pkt_lock);
			eventobj_init(&rcvpkts[i]->pkt_windowcomplete);
			eventobj_init(&rcvpkts[i]->pkt_sendcomplete);

			rcvpkts[i]->pkt_next = NULL;
			return i;
		}
	}
	return EMAXRCVPKTS;
}

void free_packet(struct packet *pkt)
{
	if (pkt->pkt_data != NULL) kfree( pkt->pkt_data);
	kfree( pkt);
}

// Returns the socket in psk_busy locked state
// This expects the socklist_lock to be locked before calling.
struct proc_sock * locate_socket(int portno, int type)
{
	struct proc_sock *psock;

	psock = (struct proc_sock *)socklist_hdr;
	while (psock != NULL && (psock->psk_port != portno || psock->psk_type != type))
		psock = (struct proc_sock *)psock->psk_globalnext;
	if (psock != NULL) 
	{
		_lock(&psock->psk_busy);
		// Check whether still it is working with the same port number
		if (psock->psk_port != portno || psock->psk_type != type)
		{
			// Changed, may be getting deleted
			unlock(&psock->psk_busy);
			psock = NULL;
		}
	}

	return psock;
}

int remove_pkt(void *arg)
{
	struct packet **pkt = (struct packet **) arg;

	// This may interfere with pkt assembler, but it will not
	// cause any race problem.
	if ((*pkt)->pkt_data != NULL) kfree((*pkt)->pkt_data);
	kfree(*pkt);
	*pkt = NULL;
	return 0;
}

int pktindex(struct eth_frame_rcv *ef)
{
	int i;   

	for(i=0;i<MAXACTIVEPACKETS;i++)
	{
		if ((rcvpkts[i] != NULL) 
		     && (rcvpkts[i]->pkt_header.pkt_id==ef->pkt_id 
		         && rcvpkts[i]->pkt_header.pkt_src.saddr_host==ef->pkt_src_saddr_host 
		         && rcvpkts[i]->pkt_header.pkt_src.saddr_type==ef->pkt_src_saddr_type 
		         && rcvpkts[i]->pkt_header.pkt_src.saddr_port==ef->pkt_src_saddr_port))
			return i;	
	}
	return -1;
}

void initialize_hash_table()
{
	int i;
	for(i=0;i<MaxIndex;i++) hash_table[i]=NULL;
}
node * locate_hash_node(int host, int port, int type)
{
	node *n;
	int index;

	index = ((host*100+port) % MaxIndex);
	n=hash_table[index];

	while(n != NULL)
	{
  		if(n->host == host && n->port == port && n->type == type) break;
		n=n->next;
	}
	return n;
}

void add_hash_node(int host,int port, int type, int packet_id)
{
	node *newnode;
	node *t1;
	int index;

	newnode=(node*)kmalloc( sizeof(node));
	if(newnode == NULL)
	{
      		printk("Not Enough memory...\n");
		return;
	}
	newnode->host=host;
	newnode->port=port;
	newnode->type=type;
	newnode->next_seqno = packet_id;
	newnode->msec = secs;
	newnode->prev=newnode->next=NULL;

	index =  ((host*100+port)%MaxIndex);
	t1=hash_table[index];
	newnode->next = t1;
	if (t1 != NULL) t1->prev = newnode;
	hash_table[index] = newnode;
	return;
}

void purge_seqno_hashtable(int before)
{
	int i;
	node *t1, *t2;

	for(i=0; i<MaxIndex; i++)
	{
		if (hash_table[i] != NULL)
		{
			t1 = hash_table[i];
			while (t1 != NULL)
			{
				if  (t1->msec < before) // Is it old?
				{ 	// Remove it
					if (t1->prev != NULL)
						(t1->prev)->next = t1->next;
					else hash_table[i] = t1->next;
					if (t1->next != NULL)
						(t1->next)->prev = t1->prev;
				}
				t2 = t1;
				t1 = t1->next;
				kfree(t2);
			}
		}
	}
}
				

void initialize_seqno(int host, int port, int type, int pkt_id)
{
	int i;
	node *t1, *t2;

	// No need of syncronization locks, only assembler manipulates 
	// hash table of received packet information.
	for(i=0;i<MaxIndex;i++)
	{
		if (hash_table[i]!=NULL)
		{
			t1 = hash_table[i];
			while (t1 != NULL)
			{
				if  (t1->host == host) // Is it from the given host?
				{ 	
					if (port == -1 || (t1->port == port && t1->type == type)) 
					{
						// Remove it
						if (t1->prev != NULL)
							(t1->prev)->next = t1->next;
						else hash_table[i] = t1->next;
						if (t1->next != NULL)
							(t1->next)->prev = t1->prev;
						t2 = t1;
						t1 = t1->next;
						kfree(t2);
					}
					else t1 = t1->next;
				}
				else t1 = t1->next;
			}
		}
	}
}

void initialize_socktable(void)
{
	int i;
	
	for (i=0; i<MAXSOCKETS; i++) socktable[i].psk_type = SOCK_UNUSED;
}


void increment_sockref_count(int handle) 
{
	// Lower and Higher end of the tables
	if (handle >= SOCKHANDLEBEGIN && handle < SOCKHANDLEEND)
	{
		_lock(&socktable[handle - SOCKHANDLEBEGIN].psk_busy);
		socktable[handle - SOCKHANDLEBEGIN].psk_refcount++;;
		unlock(&socktable[handle - SOCKHANDLEBEGIN].psk_busy);
	}
}

void decrement_sockref_count(int handle)
{
	struct proc_sock *psock;
	struct connect_request msg;
	struct isockaddr dest;
	int err;

	if (handle >= SOCKHANDLEBEGIN && handle < SOCKHANDLEEND)
	{
		handle -= SOCKHANDLEBEGIN;
		if (handle >= 0 && handle < MAXSOCKETS)
		{
			_lock(&socklist_lock);
			psock = &socktable[handle];

			_lock(&psock->psk_busy);
			psock->psk_refcount--;
	
			if (psock->psk_refcount > 0)
			{
				unlock(&psock->psk_busy);
				unlock(&socklist_lock);
				return; // Nothing more
			}

			// Otherwise take necessary actions for deleting the socket.
			if (psock->psk_type == SOCK_STREAM && psock->psk_peer.saddr_port != -1 && psock->psk_peer.saddr_port != 0) // Connected
			{
				msg.conreq_type = PKTTYPE_CONNECTCLOSE;
				msg.conreq_dest.saddr_host = psock->psk_peer.saddr_host;
				msg.conreq_dest.saddr_type = SOCK_STREAM;
				msg.conreq_dest.saddr_port = psock->psk_peer.saddr_port;
				dest.saddr_host = psock->psk_peer.saddr_host;
				dest.saddr_type = SOCK_STREAM;
				dest.saddr_port = psock->psk_peer.saddr_port;

				msg.conreq_src.saddr_host = this_hostid;
				msg.conreq_src.saddr_type = SOCK_STREAM;
				msg.conreq_src.saddr_port = psock->psk_port;

				// Send connection closing message.
				err = sock_send(psock, &dest, (char *)&msg, sizeof(msg), PKTTYPE_CONNECTCLOSE, 0, -1, 1);
				unlock(&socklist_lock);
				// After delivery of this message then delete the socket.
				if (psock->psk_peer.saddr_host != this_hostid)
				{
					psock->psk_delrequest = 1;
					unlock(&psock->psk_busy);
				}
				else // Local only, so delete now itself.
					delete_socket(psock);
			}
			else if (psock->psk_transmitterusing == 0) // Nobody is using
				delete_socket(psock);
			else
			{
				psock->psk_delrequest = 1;
				unlock(&psock->psk_busy);
			}
		}
	}
	return;
}

// Determines whether the given thread belongs to the specified group in the
// process or not (MPI)
int isThreadBelongs(int threadid, int groupid, struct process *p)
{
	//int i, j;

	return 0;
}

// Process address based interface function for sending
// Destination default port number = procno << 16 + threadno
// Internal representation of sockaddr contains 4-byte integer port no,
// whereas user program level sockaddr contains 2-byte short intger port no.
// So all port numbers > 65535 are default port numbers

int syscall_broadcast(int portno, char *buf, int len)
{
	int threadno, processno;
	struct isockaddr src, dest;

	// Check for maximum packet size
	if (len > MAX_PKTSIZE) return EPKTSIZE;

	threadno = get_threadid();
	processno = get_processid();

	dest.saddr_type = SOCK_DGRAM;
	dest.saddr_host = -1;	// All hosts
	dest.saddr_port = portno; // Process of the same type

	src.saddr_type = SOCK_DGRAM;
	src.saddr_host = this_hostid;
	src.saddr_port = (processno << 16) + threadno;

	return internal_tsend(&dest, &src, buf, len, 0, -1, 0);
}

int syscall_tsendto(short dest_threadid, char *buf, unsigned int len, int groupid, int tag, short nb)
{
	int processno;
	struct isockaddr src, dest;

	// Check for maximum packet size
	if (len > MAX_PKTSIZE) return EPKTSIZE;

	processno = get_processid();

	dest.saddr_type = SOCK_DGRAM;
	src.saddr_type = SOCK_DGRAM;	
	if (dest_threadid == -1)
	{
		dest.saddr_host = -1;
		dest.saddr_port = - (processno << 16);
	}
	else
	{	
		// Find the default process port number on the destination host.
		if (current_process->proc_type == PROC_TYPE_PARALLEL || current_process->proc_type == PROC_TYPE_MPIPARALLEL)
			dest.saddr_host = temp_proc_map[dest_threadid];
		else dest.saddr_host = this_hostid;
		dest.saddr_port = (processno << 16) + dest_threadid;
	}
	src.saddr_host = this_hostid;
	src.saddr_port = (processno << 16) + get_threadid();

	return internal_tsend(&dest, &src, buf, len, groupid, tag, nb);
}

int local_broadcast(struct isockaddr * destsock, struct isockaddr *srcsock, char *buf, unsigned int len, int pkt_seqno, int groupid, int tag)
{
	int deliver_count = 0;
	struct packet *pkt, *pkt1;
	struct proc_sock *psock;
	unsigned long old_cr0;
	struct kthread *t1;
	struct process *p1;

	// Search for the process
	_lock(&used_proclock);
	_lock(&suspended_proclock);
	p1 = (struct process *)used_processes;
	while (p1 != NULL && p1->proc_id != (((-destsock->saddr_port) >> 16) & 0xffff)) 
		p1 = (struct process *)p1->proc_next;

	if (p1 == NULL)
	{
		// Search suspended processes
		p1 = (struct process *)suspended_processes;
		while (p1 != NULL && p1->proc_id != (((-destsock->saddr_port) >> 16) & 0xffff)) 
			p1 = (struct process *)p1->proc_next;
	}

	if (p1 != NULL) _lock(&p1->proc_exitlock); // Should not exit in between
	unlock(&suspended_proclock);
	unlock(&used_proclock);

	//printk("local_broadcast : step 1 : p1 : %x\n", p1);
	if (p1 == NULL)
	{
		printk("Local broadcast cannot locate a process\n");
		return 0;
	}

	// Look for all threads under this process
	_lock(&p1->proc_thrlock);
	t1 = (struct kthread *)p1->proc_threadptr;
	while (t1 != NULL) 
	{
		if ((t1->kt_defsock->psk_port != srcsock->saddr_port) &&  (p1->proc_type != PROC_TYPE_MPIPARALLEL || isThreadBelongs(t1->kt_threadid, groupid, p1)))
		{	// Not the source itself
	
			// This process in the group of parallel task
			psock = (t1->kt_defsock);
			// Allocate packet buffer for holding the message.
			old_cr0 = clear_paging();
			pkt = kmalloc( sizeof(struct packet));
			if (pkt == NULL)
			{
				printk("Malloc failure in local_broadcast\n");
				restore_paging(old_cr0, current_thread->kt_tss->cr3);
				unlock(&p1->proc_thrlock);
				unlock(&p1->proc_exitlock);
				return ENOMEM;
			}
			pkt->pkt_data = kmalloc(len);
			if (pkt->pkt_data == NULL && len > 0)
			{
				printk("Malloc afilure in local_broadcast\n");
				kfree( (void *)pkt);
				restore_paging(old_cr0, current_thread->kt_tss->cr3);
				unlock(&p1->proc_thrlock);
				unlock(&p1->proc_exitlock);
				return ENOMEM;
			}
			restore_paging(old_cr0, current_thread->kt_tss->cr3);
			// Initialize the pkt to deliver to other processes of the smae task
			pkt->pkt_header.pkt_type = PKTTYPE_DATA;
			pkt->pkt_header.pkt_dest.saddr_type = SOCK_DGRAM;
			pkt->pkt_header.pkt_dest.saddr_host = this_hostid;
			pkt->pkt_header.pkt_dest.saddr_port = psock->psk_port;
			pkt->pkt_header.pkt_src = *srcsock;
			pkt->pkt_header.pkt_length = len;
			pkt->pkt_header.pkt_id = pkt_seqno;
			pkt->pkt_header.pkt_tag = tag;
			pkt->pkt_header.pkt_groupid = groupid;
			pkt->pkt_rcvframe_head = pkt->pkt_rcvframe_tail = NULL; 
			pkt->pkt_rcv_offset = pkt->pkt_expc_seqno = pkt->pkt_rcvddata = pkt->pkt_ackdata = 0;
			timerobj_init(&pkt->pkt_tobj);
			lockobj_init(&pkt->pkt_lock);
			eventobj_init(&pkt->pkt_windowcomplete);
			eventobj_init(&pkt->pkt_sendcomplete);
			pkt->pkt_next = NULL;

			// Copy the data of the packet
			old_cr0 = clear_paging();
			vm2physcopy(current_process->proc_vm, buf, pkt->pkt_data, len);
			restore_paging(old_cr0, current_thread->kt_tss->cr3);

			_lock(&psock->psk_rcvpktlock);
			// Queue the packet at the socket
			if (psock->psk_rcvpkt == NULL) psock->psk_rcvpkt = pkt;
			else // Insert at the end.
			{
				pkt1 = (struct packet *)psock->psk_rcvpkt;
				while (pkt1->pkt_next != NULL) 
					pkt1 = (struct packet *)pkt1->pkt_next;
				pkt1->pkt_next = pkt;
			}
			pkt->pkt_status = PKTSTATUS_READYTOTXMT;
			event_wakeup(&(psock->psk_rcvdevent));
			unlock(&psock->psk_rcvpktlock);

			deliver_count++;
		}
		t1 = (struct kthread *)t1->kt_sib;
	}
	unlock(&p1->proc_thrlock);
	unlock(&p1->proc_exitlock);
	return deliver_count;
}

// Generalized send function for supporting socket type of address 
// Queue it at the respective socket
int internal_tsend(struct isockaddr * destsock, struct isockaddr *srcsock, char *buf, unsigned int len, int groupid, int tag, short nb)
{
	struct proc_sock *psock;
	struct packet *pkt, *pkt1;
	unsigned long old_cr0;
	unsigned int status;
	int retval;
	int seqno;

	_lock(&socklist_lock);
	psock = (struct proc_sock *)socklist_hdr;

	// Find the required source socket
	while (psock != NULL && (psock->psk_port != srcsock->saddr_port || psock->psk_type != srcsock->saddr_type))
		psock = (struct proc_sock *)psock->psk_globalnext;

	if (psock == NULL)
	{
		unlock(&socklist_lock);
		return EINVALIDSOCKET;
	}
	_lock(&psock->psk_busy);

	// When a normal point-to-point message use sock_send
	if (destsock->saddr_host >= 0)
	{
		retval = sock_send(psock, destsock, buf, len, PKTTYPE_DATA, groupid, tag, nb);
		unlock(&psock->psk_busy);
		unlock(&socklist_lock); // Must be released early, because in sock_send this may sleep
		return retval;
	}

	seqno = psock->psk_sndseqid + 1;

	if (destsock->saddr_host < 0  && destsock-> saddr_port < -65535) // Broadcast message of a parallel process
	{
		local_broadcast(destsock, srcsock, buf, len, seqno, groupid, tag);
		psock->psk_sndseqid++;
	}
	// Allocate packet buffer for holding the message.
	old_cr0 = clear_paging();
	pkt = kmalloc( sizeof(struct packet));
	if (pkt == NULL)
	{
		printk("Malloc afilure in internal_tsend\n");
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
		unlock(&psock->psk_busy);
		unlock(&socklist_lock);
		return ENOMEM;
	}
	pkt->pkt_data = kmalloc(len);
	if (pkt->pkt_data == NULL && len > 0)
	{
		printk("Malloc afilure in internal_tsend\n");
		kfree( (void *)pkt);
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
		unlock(&psock->psk_busy);
		unlock(&socklist_lock);
		return ENOMEM;
	}
	// Copy the data of the packet
	vm2physcopy(current_process->proc_vm, buf, pkt->pkt_data, len);
	restore_paging(old_cr0, current_thread->kt_tss->cr3);
	timerobj_init(&pkt->pkt_tobj);
	lockobj_init(&pkt->pkt_lock);
	eventobj_init(&pkt->pkt_windowcomplete);
	eventobj_init(&pkt->pkt_sendcomplete);
	
	pkt->pkt_rcvframe_head = pkt->pkt_rcvframe_tail = NULL;

	pkt->pkt_header.pkt_type = PKTTYPE_DATA;
	pkt->pkt_header.pkt_dest = *destsock;
	pkt->pkt_header.pkt_src = *srcsock;
	pkt->pkt_header.pkt_length = len;
	pkt->pkt_header.pkt_id = seqno;//++packet_seq_no;
	pkt->pkt_header.pkt_tag = tag;
	pkt->pkt_header.pkt_groupid = groupid;
	pkt->pkt_next = NULL;
	pkt->pkt_rcv_offset = pkt->pkt_expc_seqno = pkt->pkt_rcvddata = pkt->pkt_ackdata = 0;

	// Initialize sliding window protocol variables.
	pkt->pkt_retries = 1;
	pkt->pkt_swinstatus = 0;
	pkt->pkt_nonblocking = nb;
	pkt->pkt_wincur_pos = 0;
	pkt->pkt_cur_pos = 0;
	pkt->pkt_window_size = 0;
	pkt->pkt_current_frame = 0;
	pkt->pkt_start_frame = 0;

	pkt->pkt_status = PKTSTATUS_READYTOTXMT;

	// Queue it and change the state of the packet
	if (psock->psk_sndpkt == NULL) psock->psk_sndpkt = pkt;
	else
	{
		pkt1 = (struct packet *)psock->psk_sndpkt;
		while (pkt1->pkt_next != NULL) pkt1 = (struct packet *)pkt1->pkt_next;
		pkt1->pkt_next = pkt;
	}
	// Wakeup pkttransmitter task if necessary
	event_wakeup(&pkt_send_event);
	unlock(&socklist_lock); // No longer this lock is required


	// If blocking call then wait on txmt complete
	if (nb == 0)
	{
		event_sleep(&(pkt->pkt_sendcomplete), &psock->psk_busy); 
		unlock(&psock->psk_busy);
		status = pkt->pkt_swinstatus;
		old_cr0 = clear_paging();
		kfree( (void *)pkt->pkt_data);
		kfree( (void *)pkt);
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
		printk("Tsend to : coming outof sleep\n");
	}
	else
	{
		unlock(&psock->psk_busy);
		status = 0;
	}

	// If this is port broadcast, then this must be delivered 
	// on the local host also.
	if  (destsock->saddr_host < 0 && destsock->saddr_port > 0 && destsock->saddr_port != srcsock->saddr_port) 
	{
		_lock(&socklist_lock);
		_lock(&psock->psk_busy);
		destsock->saddr_host = this_hostid;
		sock_send(psock, destsock, buf, len, PKTTYPE_DATA, groupid, tag, 0);
		unlock(&psock->psk_busy);
		unlock(&socklist_lock);
	}
	return status;
}

int sock_broadcast(char *buf, int len, short nb)
{
	int threadno, processno;
	struct isockaddr src, dest;

	threadno = get_threadid();
	processno = get_processid();

	// Check for maximum packet size
	if (len > MAX_PKTSIZE) return EPKTSIZE;

	dest.saddr_type = SOCK_DGRAM;
	dest.saddr_host = -1;
	dest.saddr_port = (processno << 16) + threadno; // Process of the same type

	src.saddr_type = SOCK_DGRAM;
	src.saddr_host = this_hostid;
	src.saddr_port = (processno << 16) + threadno;

	return internal_tsend(&dest, &src, buf, len, 0, -1, nb);
}

// Transmit the first packet of Socket.
// returns zero if socket not deleted, otherwise -1
int psock_send_pkt(struct proc_sock *psock, struct frame_buffer *swin_frame_buf, struct eth_frame_snd *swin_frame)
{
	int nframes;
	int frsize;
	unsigned char broadcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff ,0xff };
	int i, j, k, l;
	int ret = 0, slpretval;
	struct packet *pkt;
	unsigned long oflags;
	int queuedframes;

	pkt = (struct packet *)psock->psk_sndpkt; // Do not remove from socket until transmission is completed.

	// Prepare frame buffers	
	nframes = (pkt->pkt_header.pkt_length + MAX_PROTOCOL_FRSIZE - 1) / MAX_PROTOCOL_FRSIZE;
	if (nframes > MAXWINDOWSIZE) nframes = MAXWINDOWSIZE;


	while (pkt->pkt_cur_pos < pkt->pkt_header.pkt_length)
	{
		// Construct frames
		pkt->pkt_window_size=0;
		pkt->pkt_current_frame=0;

		// pkttransmitter_nextwindow:
		pkt->pkt_wincur_pos = pkt->pkt_cur_pos;
		while (pkt->pkt_cur_pos < pkt->pkt_header.pkt_length && pkt->pkt_window_size < MAXWINDOWSIZE)
		{
			frsize = (((pkt->pkt_header.pkt_length - pkt->pkt_cur_pos) > MAX_PROTOCOL_FRSIZE) ? MAX_PROTOCOL_FRSIZE : (pkt->pkt_header.pkt_length - pkt->pkt_cur_pos));

			j = pkt->pkt_current_frame;
			k = pkt->pkt_cur_pos;
			if (pkt->pkt_header.pkt_dest.saddr_host != -1)
				bcopy((char *)host_hwaddr_table[pkt->pkt_header.pkt_dest.saddr_host], (char *)swin_frame[j].ethfrhdr_hwaddr_dest, 6);
			else
				bcopy((char *)broadcast_addr, (char *)swin_frame[j].ethfrhdr_hwaddr_dest, 6);
			bcopy((char *)this_hwaddr, (char *)swin_frame[j].ethfrhdr_hwaddr_src, 6);
			swin_frame[j].ethfrhdr_type = PROTOTYPE_TEMP;
			swin_frame[j].ethfrhdr_seqno = 0;

			swin_frame_buf[j].buf2 = pkt->pkt_data + k;
			swin_frame_buf[j].len2 = frsize;

			swin_frame[j].pkt_type = pkt->pkt_header.pkt_type;

			swin_frame[j].pkt_dest_saddr_type=pkt->pkt_header.pkt_dest.saddr_type;
			swin_frame[j].pkt_dest_saddr_host=pkt->pkt_header.pkt_dest.saddr_host;
			swin_frame[j].pkt_dest_saddr_port=pkt->pkt_header.pkt_dest.saddr_port;
		
			swin_frame[j].pkt_src_saddr_type=pkt->pkt_header.pkt_src.saddr_type;
			swin_frame[j].pkt_src_saddr_host=pkt->pkt_header.pkt_src.saddr_host;
			swin_frame[j].pkt_src_saddr_port=pkt->pkt_header.pkt_src.saddr_port;

			swin_frame[j].pkt_frame_length = frsize;

			swin_frame[j].pkt_offset =  k;
			swin_frame[j].pkt_length = pkt->pkt_header.pkt_length;

			swin_frame[j].pkt_id = pkt->pkt_header.pkt_id;
			swin_frame[j].pkt_tag = pkt->pkt_header.pkt_tag;
			swin_frame[j].pkt_groupid = pkt->pkt_header.pkt_groupid;
			swin_frame[j].pkt_winseq_no = pkt->pkt_window_size++;
			swin_frame_buf[j].len = sizeof(struct eth_frame_snd) + frsize;
			swin_frame_buf[j].completed = 1;

			swin_frame[j].pkt_checksum = 0;
			swin_frame[j].pkt_checksum = checksum(swin_frame_buf[j].buf1, swin_frame_buf[j].len1, swin_frame_buf[j].buf2, swin_frame_buf[j].len2, NULL, 0);

			pkt->pkt_cur_pos += frsize;
			pkt->pkt_current_frame = (j + 1) % MAXWINDOWSIZE;
		}

		// Frame buffers construction completed.
		// Transmit them by queuing at nic transmit queue.
		pkt->pkt_start_frame = 0;
	
		l = 0;
		while (l < 2) // No of retries
		{
			// pkttransmitter_restarttransmission:
			// See if any frame is still in the txmt q
			for (i=pkt->pkt_start_frame; i<pkt->pkt_window_size; i++)
				while (swin_frame_buf[i].completed == 0)
				{
					CLI;
					scheduler();
					STI;
				}

			// Queue these frames into transmit queue.
			queuedframes = 0;
			_lock(&pkt->pkt_lock);
			_lock(&txq_lock);
			for (i=pkt->pkt_start_frame; i<pkt->pkt_window_size; i++)
			{
				queuedframes = 1;
				swin_frame_buf[i].completed = 0; 
				swin_frame_buf[i].next = NULL;

				if (tx_q_tail == NULL)
					tx_q_tail = tx_q_head = &swin_frame_buf[i];
				else
				{
					tx_q_tail->next = &swin_frame_buf[i];
					tx_q_tail = &swin_frame_buf[i];
				}
			}
			// Wakeup NIC driver
			event_wakeup(&nic_send_event);
			unlock(&txq_lock);
			if (queuedframes == 1)
				slpretval = event_timed_sleep(&pkt->pkt_windowcomplete, &pkt->pkt_lock, 5000);
			else
				slpretval = 0;
			unlock(&pkt->pkt_lock);
			if (slpretval != 0) l++;
			else
			{
				// Check the pkt status
				if (pkt->pkt_status == PKTSTATUS_RESTARTTXMT) l++;
				else break; // Window completed successfully
			}
		}

		if (l >= 2)
		{
			pkt->pkt_status = PKTSTATUS_TXCOMPLETE;
			pkt->pkt_swinstatus = ETXMTTIMEDOUT;
			printk("ERROR MAX RETRIES IN SENDING THE PACKET\n");
		}
		// Check if total packet is transmitted
		if ((pkt->pkt_cur_pos == pkt->pkt_header.pkt_length) || (pkt->pkt_status == PKTSTATUS_TXCOMPLETE))
		{
			// Packet delivered completely. 
			_lock(&psock->psk_busy);
			psock->psk_sndpkt = pkt->pkt_next;
			psock->psk_transmitterusing = 0;

			if (pkt->pkt_nonblocking)
			{
				// relase pkt memory
				kfree( pkt->pkt_data);
				kfree( pkt);
			}
			else // Wakeup the sender
				event_wakeup(&(pkt->pkt_sendcomplete));
			// If delete request is pending then delete this socket
			if (psock->psk_delrequest == 1)
			{
				delete_socket((struct proc_sock *)psock);
				ret = -1;
			}
			else unlock(&psock->psk_busy);
		}
	}
	return ret;
}

void pkttransmitter(void)
{
	struct proc_sock *psock;
	int ret;
	int workdone; 
	struct frame_buffer *swin_frame_buf;
	struct eth_frame_snd *swin_frame;
	int i; 

	trans = (struct kthread *)current_thread;
	// Preallocate maximum frames and frame buffers for each transmitter.
	swin_frame_buf = kmalloc( MAXWINDOWSIZE * sizeof(struct frame_buffer));
	swin_frame = kmalloc( MAXWINDOWSIZE * sizeof(struct eth_frame_snd));

	if (swin_frame_buf == NULL || swin_frame == NULL)
	{
		printk("Malloc failure in pkttransmitter\n");
	}
	for (i=0; i<MAXWINDOWSIZE; i++)
	{
		lockobj_init(&swin_frame_buf[i].frbuflock);
		eventobj_init(&swin_frame_buf[i].threadwait);
		swin_frame_buf[i].buf1 = (char *) &swin_frame[i];
		swin_frame_buf[i].len1 = sizeof(struct eth_frame_snd);
		swin_frame_buf[i].freethis = 0;
		swin_frame_buf[i].next = NULL;
	}
	while (1) 
	{
		workdone = 0;
		_lock(&socklist_lock);
		psock = (struct proc_sock *)socklist_hdr;
		while (psock != NULL)
		{
			while (psock != NULL)
			{
				_lock(&psock->psk_busy);
				if (psock->psk_sndpkt != NULL && psock->psk_transmitterusing == 0) // Send queue is nonempty,  Check if already another transmitter is servicing this?
				{
					// Found, go for servicing this
					break;
				}

				unlock(&psock->psk_busy);
				psock = (struct proc_sock *)psock->psk_globalnext;
			}

			if (psock == NULL) 
			{
				// No packet is found, if all sockets are examined, sleep
				if (workdone == 0)
					event_sleep(&pkt_send_event, &socklist_lock);
				break;
			}
			else // packet found
			{
				workdone = 1;
				psock->psk_transmitterusing = 1;
				unlock(&psock->psk_busy);
				unlock(&socklist_lock);
			}

			// Service the packet
			ret = psock_send_pkt(psock, swin_frame_buf, swin_frame);
			_lock(&socklist_lock);
			if (ret == 0) psock = (struct proc_sock *)psock->psk_globalnext;
			else break; 
			// Come out of the loop and again start from the beginning
		}
		unlock(&socklist_lock);
	}
}

// Remove all the packets (both sending and receved) queued with 
// this socket, then remove it from global queue, and thread queue
// (Assumes socklist_lock and psk_busy are already obtained)
int sys_unlock(struct lock_t *lck);
void delete_socket(struct proc_sock *psock)
{
	struct packet *pkt1, *pkt2;
	struct proc_sock *temp_hdr;
	struct frame_buffer *frbuf1, *frbuf2;
	unsigned long oflags;

	// Mark it unused by setting the port number to zero
	psock->psk_port = 0;
	psock->psk_transmitterusing = 1; // So that no transmitter selects this
	// Unlocking of psk_busy must be done as a second party, 
	// because terminater may delete the socket of a terminated thread
	CLI;
	spinlock(psock->psk_busy.l_slq.sl_bitval);
	sys_unlock(&psock->psk_busy);
	spinunlock(psock->psk_busy.l_slq.sl_bitval);
	STI;
	// Now this will not be matched by any locate socket 


	// Acquire socklist_lock, again psk_busy, psk_rcvpktlock in order
	_lock(&socklist_lock);
	_lock(&psock->psk_busy); // First party locking only.
	_lock(&psock->psk_rcvpktlock);

	// Remove it from the global list
	if (psock == socklist_hdr)
		socklist_hdr = (struct proc_sock *)psock->psk_globalnext;
	else
	{
		temp_hdr = (struct proc_sock *)socklist_hdr;
		while (temp_hdr != NULL && temp_hdr->psk_globalnext != psock)
			temp_hdr = (struct proc_sock *)temp_hdr->psk_globalnext;
		if (temp_hdr != NULL)
		{
			temp_hdr->psk_globalnext = psock->psk_globalnext;
		}
		else
		{
		}
	}
	psock->psk_globalnext = NULL;
	unlock(&socklist_lock);	// No longer socklist lock is required

	// removing receive queue
	pkt1 = (struct packet *)psock->psk_rcvpkt;
	while (pkt1 != NULL)
	{
		frbuf1 = (struct frame_buffer *)pkt1->pkt_rcvframe_head;
		while (frbuf1 != NULL)
		{
			frbuf2 = frbuf1;
			frbuf1 = (struct frame_buffer *)frbuf1->next;
			free_frame_buffer(frbuf2);
		}
		pkt2 = (struct packet *)pkt1->pkt_next;
		if (pkt1->pkt_data != NULL)
			kfree( (void *)pkt1->pkt_data);
		kfree( (void *)pkt1);
		pkt1 = pkt2;
	}
	psock->psk_rcvpkt = NULL;
	event_wakeup(&(psock->psk_rcvqempty));

	// removing send queue
	pkt1 = (struct packet *)psock->psk_sndpkt;
	while (pkt1 != NULL)
	{
		pkt2 = (struct packet *)pkt1->pkt_next;
		kfree( (void *)pkt1->pkt_data);
		kfree( (void *)pkt1);
		pkt1 = pkt2;
	}
	psock->psk_sndpkt = NULL;
	unlock(&psock->psk_rcvpktlock);
	unlock(&psock->psk_busy);	// Normal unlocking only.
	psock->psk_type = SOCK_UNUSED; // Finally mark it as unused
}

// If port number is -1 then this is for the entire machine 
// May be just now started or rebooted.
// For every socket that is created explicitly, it should initialize 
// its sequence number at all machines.
int send_seqnumber_mesg(int sourcemachine, int sourceport, int socktype)
{
	unsigned char broadcast_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	struct frame_buffer *seq_frbufs;
	struct ack_frame *seqno_frame; // ack_frame is OK for sending sequence init message.

	// Allocate frame memory
	seq_frbufs = (struct frame_buffer *)kmalloc(sizeof(struct frame_buffer));
	seqno_frame = (struct ack_frame *)kmalloc(sizeof(struct ack_frame));

	if (seq_frbufs == NULL || seqno_frame == NULL)
	{
		printk("Malloc failure in init_sequence number\n");
		if (seq_frbufs != NULL) kfree(seq_frbufs);
		if (seqno_frame != NULL) kfree(seqno_frame);
		return -1;
	}
	
	lockobj_init(&(seq_frbufs->frbuflock));
	eventobj_init(&(seq_frbufs->threadwait));
	seq_frbufs->status = 0; // in use or transmission not completed.
	seqno_frame->pkt_type = PKTTYPE_PKTSEQNOINIT;
	seqno_frame->pkt_id=1;
	seqno_frame->pkt_tag= -1;
	seqno_frame->pkt_groupid=0;
	bcopy((char *)this_hwaddr, (char *)seqno_frame->ethfrhdr_hwaddr_src, 6);
	bcopy((char *)broadcast_addr, (char *)seqno_frame->ethfrhdr_hwaddr_dest, 6);
	seqno_frame->ethfrhdr_type = PROTOTYPE_TEMP;
	seqno_frame->ethfrhdr_seqno = 0;
	seqno_frame->pkt_src_saddr_type=socktype;
	seqno_frame->pkt_src_saddr_host=sourcemachine;
	seqno_frame->pkt_src_saddr_port=sourceport;
	seqno_frame->pkt_dest_saddr_type=0;
	seqno_frame->pkt_dest_saddr_host=-1;
	seqno_frame->pkt_dest_saddr_port=0;
	seqno_frame->pkt_winseq_no = 0;
	seqno_frame->pkt_length = 0;
	seqno_frame->pkt_offset=0;
	seqno_frame->pkt_frame_length = 0;
	seqno_frame->pkt_checksum = 0;
	seqno_frame->pkt_checksum = checksum((char *)seqno_frame, sizeof(struct ack_frame), NULL, 0, NULL, 0);
	seq_frbufs->len1 = seq_frbufs->len=sizeof(struct ack_frame);
	seq_frbufs->buf1 = (char *) seqno_frame;
	seq_frbufs->buf2 = seq_frbufs->buf3 = NULL;
	seq_frbufs->len2 = seq_frbufs->len3 = 0;
	seq_frbufs->next = NULL;
	seq_frbufs->freethis = 1; // Should be freed by NIC
	seq_frbufs->next = NULL;
	
	// Queue this sequence init frame
	_lock(&txq_lock);
	if (tx_q_tail != NULL)
	{
		tx_q_tail->next = seq_frbufs;
		tx_q_tail = seq_frbufs;
	}
	else tx_q_tail = tx_q_head = seq_frbufs;
	
	event_wakeup(&nic_send_event);
	unlock(&txq_lock);
	return 0;
}

void send_ack(int type,struct eth_frame_rcv *ef, unsigned short expect_winseqno, unsigned int offset)
{
	struct frame_buffer *ack_frbufs;
	struct ack_frame *ack;
	struct frame_buffer *f1;

	// Should it be sent or not?
	_lock(&txq_lock);
	f1 = (struct frame_buffer *)tx_q_head;
	while (f1 != NULL)
	{
		ack = (struct ack_frame *)f1->buf1;
		if (ack->pkt_type == type && ack->pkt_id == ef->pkt_id && ack->pkt_dest_saddr_host==ef->pkt_src_saddr_host && ack->pkt_dest_saddr_port == ef->pkt_src_saddr_port && ack->pkt_dest_saddr_type == ef->pkt_src_saddr_type)
		{
			if (ack->pkt_offset == offset)
			{ // Same ack already in the tx q
				unlock(&txq_lock);
				return;
			}
			else
			{
				// Offset can be special case 0 (restart 
				// from the beginning of the packet or higher 
				// numbered offset.
				ack->pkt_offset = offset;
				ack->pkt_winseq_no = expect_winseqno;
				unlock(&txq_lock);
				return;
			}
		}
		f1 = (struct frame_buffer *)f1->next;
	}
	unlock(&txq_lock);

	// Not present in the tx q, so send new ack
	ack_frbufs = (struct frame_buffer *)kmalloc(sizeof(struct frame_buffer));
	ack = (struct ack_frame *)kmalloc(sizeof(struct ack_frame));

	if (ack_frbufs == NULL || ack == NULL)
	{
		if (ack_frbufs != NULL) kfree(ack_frbufs);
		if (ack != NULL) kfree(ack);
		printk("Error : send ack : kmalloc\n");
		return;
	}
	lockobj_init(&(ack_frbufs->frbuflock));
	eventobj_init(&(ack_frbufs->threadwait));
	ack_frbufs->status = 0; // in use or transmission not completed.
	ack->pkt_type= type;
	ack->pkt_id=ef->pkt_id;
	ack->pkt_tag=ef->pkt_tag;
	ack->pkt_groupid=ef->pkt_groupid;
	bcopy((char *)host_hwaddr_table[this_hostid], (char *)ack->ethfrhdr_hwaddr_src, 6);
	bcopy((char *)host_hwaddr_table[ef->pkt_src_saddr_host], (char *)ack->ethfrhdr_hwaddr_dest, 6);
	ack->ethfrhdr_type = PROTOTYPE_TEMP;
	ack->ethfrhdr_seqno = 0;
	ack->pkt_src_saddr_type=ef->pkt_dest_saddr_type;
	ack->pkt_src_saddr_host=ef->pkt_dest_saddr_host;
	ack->pkt_src_saddr_port=ef->pkt_dest_saddr_port;
	ack->pkt_dest_saddr_type=ef->pkt_src_saddr_type;
	ack->pkt_dest_saddr_host=ef->pkt_src_saddr_host;
	ack->pkt_dest_saddr_port=ef->pkt_src_saddr_port;
	ack->pkt_winseq_no = expect_winseqno;
	ack->pkt_length = 0;
	ack->pkt_offset=offset;
	ack->pkt_frame_length = 0;
	ack->pkt_checksum = 0;
	ack->pkt_checksum = checksum((char *)ack, sizeof(struct ack_frame), NULL, 0, NULL, 0);
	ack_frbufs->len1 = ack_frbufs->len=sizeof(struct ack_frame);
	ack_frbufs->buf1 = (char *) ack;
	ack_frbufs->buf2 = ack_frbufs->buf3 = NULL;
	ack_frbufs->len2 = ack_frbufs->len3 = 0;
	ack_frbufs->next = NULL;
	ack_frbufs->freethis = 1; // Should be freed by NIC

	// Queue ack, high priority (at the beginning)
	_lock(&txq_lock);
	ack_frbufs->next = tx_q_head;
	tx_q_head = ack_frbufs;
	if (tx_q_tail == NULL) tx_q_tail = ack_frbufs;

	event_wakeup(&nic_send_event); 		
	unlock(&txq_lock);
}	

int remove_frames_fromtxq(int pkt_id, int offset, int srcport, int srctype, int pkttype)
{
	frame_buffer *f1, *f2;
	struct eth_frame_snd *ethfr;
	int count=0;
	

	// Remove all the frames of specified packet id, 
	// that have offset value below the given offset from tx_q
	_lock(&txq_lock);
	f2 = NULL;
	f1 = (struct frame_buffer *)tx_q_head;
	while (f1 != NULL)
	{
		ethfr = (struct eth_frame_snd *)f1->buf1;
		if ((ethfr->pkt_type == pkttype) && (ethfr->pkt_id == pkt_id) && (ethfr->pkt_offset < offset) && (ethfr->pkt_src_saddr_port==srcport) && (ethfr->pkt_src_saddr_type==srctype))
		{ // Should be deleted
			if (f1 == tx_q_head)
			{ // First node
				tx_q_head = f1->next;
				f1->next = NULL; // Removed from the queue
				f1->status = 1;
				f1->completed = 1;
				if (tx_q_head == NULL) // is it last node?
					tx_q_tail = NULL;
				f1 = (struct frame_buffer *)tx_q_head;
			}
			else
			{
				f2->next = f1->next;
				f1->next = NULL; // Removed from the queue
				f1->status = 1;
				f1->completed = 1;
				if (f1 == tx_q_tail) // is it last node?
					tx_q_tail = f2;
				f1 = (struct frame_buffer *)f2->next;
			}
			count++;
		}
		else
		{ // Advance f1 normally
			f2 = f1;
			f1 = (struct frame_buffer *)f1->next;
		}
	}
	unlock(&txq_lock);
	return count;
}

void process_ack(struct packet *pkt, struct eth_frame_rcv *rcvd_eth_frame)
{
	// Acknowledgement packet received, process it.
	// corresponding packet is already in locked state.
	// Locate packet (first packet only)

	if (pkt->pkt_header.pkt_id == rcvd_eth_frame->pkt_id) // packet matching
	{
		// Find whether it is duplicate ACK (offset == 0 may not be 
		// duplicate, but asking for retransmit from the beginning)
		if (pkt->pkt_ackdata > rcvd_eth_frame->pkt_offset && rcvd_eth_frame->pkt_offset != 0) return;
	
		remove_frames_fromtxq(rcvd_eth_frame->pkt_id, rcvd_eth_frame->pkt_offset, rcvd_eth_frame->pkt_dest_saddr_port, rcvd_eth_frame->pkt_dest_saddr_type, pkt->pkt_header.pkt_type);

		if (rcvd_eth_frame->pkt_offset == 0)
		{
			// restarting from the beginning
			pkt->pkt_retries++;
			if (pkt->pkt_retries > 3)
			{
				pkt->pkt_status = PKTSTATUS_TXCOMPLETE;
				pkt->pkt_swinstatus = ETXMTTIMEDOUT;
				printk("ERROR MAX RETRIES IN SENDING THE PACKET\n");
			}
			else
			{
				printk("RESTARTING TRANSMISSION : %d\n",pkt->pkt_header.pkt_id);
				pkt->pkt_wincur_pos = 0;
				pkt->pkt_ackdata = 0;
				pkt->pkt_cur_pos=0;
				pkt->pkt_status = PKTSTATUS_CONTINUENEXTWINDOW;
			}
		}
		else 
		{
			pkt->pkt_ackdata = rcvd_eth_frame->pkt_offset;
			if (rcvd_eth_frame->pkt_offset > pkt->pkt_wincur_pos && rcvd_eth_frame->pkt_offset < pkt->pkt_cur_pos)
			{

				pkt->pkt_status = PKTSTATUS_RESTARTTXMT;
				pkt->pkt_start_frame = rcvd_eth_frame->pkt_winseq_no;
			}
			else if (rcvd_eth_frame->pkt_offset == pkt->pkt_cur_pos)
			{
				if (rcvd_eth_frame->pkt_offset == pkt->pkt_header.pkt_length)
					pkt->pkt_status = PKTSTATUS_TXCOMPLETE;
				else
					pkt->pkt_status = PKTSTATUS_CONTINUENEXTWINDOW;
			}
		}
	}
}

void disp_debuginfo(struct process *proc);
void packet_type_data(struct frame_buffer *rcvd_frame_buf)
{
	node *hnode;	
	int next_seqno;
	int ind;
	struct eth_frame_rcv *rcvd_eth_frame;
	struct mc_message *mcmesg;

	rcvd_eth_frame = (struct eth_frame_rcv *)rcvd_frame_buf->buf1;

	if((ind = pktindex(rcvd_eth_frame)) == -1)//if not exists
	{
		// Determine by looking into the history of received 
		// packets whether it is duplicate / new.
		hnode = locate_hash_node(rcvd_eth_frame->pkt_src_saddr_host, rcvd_eth_frame->pkt_src_saddr_port, rcvd_eth_frame->pkt_src_saddr_type);
		if (hnode != NULL)
			next_seqno = hnode->next_seqno;
		else next_seqno = 0;

		// Is it old/dumplicate?
		if (next_seqno >= rcvd_eth_frame->pkt_id)
		{
			if ((rcvd_eth_frame->pkt_dest_saddr_host != -1) || (((rcvd_eth_frame->pkt_src_saddr_host + 1) % no_hosts) == this_hostid))
			{
				send_ack(PKTTYPE_ACK, rcvd_eth_frame, ((rcvd_eth_frame->pkt_length + MAX_PROTOCOL_FRSIZE - 1 ) / MAX_PROTOCOL_FRSIZE) % MAXWINDOWSIZE, rcvd_eth_frame->pkt_length);
				printk("sending ack for duplicate packet\n");
			}
			printk("Expected packet id : %d, but received : %d and dropped from : (%d, %d) to (%d, %d) \n",next_seqno, rcvd_eth_frame->pkt_id, rcvd_eth_frame->pkt_src_saddr_host, rcvd_eth_frame->pkt_src_saddr_port, rcvd_eth_frame->pkt_dest_saddr_host, rcvd_eth_frame->pkt_dest_saddr_port);

			mcmesg = (struct mc_message *)rcvd_eth_frame->ethfr_data;
			return;
		}
	
		ind = alloc_pktindex(rcvd_eth_frame);
		
		if (ind < 0) 
		{
			// Discard packet
			free_frame_buffer(rcvd_frame_buf);
			printk("Unable to allocate a pkt index : dropped\n");
			return;
		}
		else
		{
			rcvpkts[ind]->pkt_tobj.timer_count = -1; // initialize
		}
	}

	// Received frame. Is it expected frame in the sequence?
	if (rcvpkts[ind]->pkt_rcvddata == rcvd_eth_frame->pkt_offset)
	{	// Restart the timer
		if (rcvpkts[ind]->pkt_tobj.timer_count >= 0)
			stoptimer(&(rcvpkts[ind]->pkt_tobj));
		starttimer(&(rcvpkts[ind]->pkt_tobj), 5000, &remove_pkt, (void *)&rcvpkts[ind]);

		// Integrate into packet
	
		rcvd_frame_buf->next = NULL;
		// Timeout handler may interfere
		if (rcvpkts[ind]->pkt_rcvframe_tail == NULL)
			rcvpkts[ind]->pkt_rcvframe_tail = rcvpkts[ind]->pkt_rcvframe_head = rcvd_frame_buf;
		else
		{
			rcvpkts[ind]->pkt_rcvframe_tail->next = rcvd_frame_buf;
			rcvpkts[ind]->pkt_rcvframe_tail = rcvd_frame_buf;
		}
			
		rcvpkts[ind]->pkt_rcvddata += rcvd_eth_frame->pkt_frame_length;
		rcvpkts[ind]->pkt_expc_seqno = (rcvpkts[ind]->pkt_expc_seqno + 1) % MAXWINDOWSIZE;

		// Check whether acknowledgement should be sent.
		// packet completed, or one window completed
		if ((rcvd_eth_frame->pkt_length == rcvpkts[ind]->pkt_rcvddata) || (rcvpkts[ind]->pkt_expc_seqno == 0))
		{
			if (rcvpkts[ind]->pkt_header.pkt_dest.saddr_host != -1 || ((rcvpkts[ind]->pkt_header.pkt_src.saddr_host + 1) % no_hosts) == this_hostid)
			{
				send_ack(PKTTYPE_ACK, rcvd_eth_frame, rcvpkts[ind]->pkt_expc_seqno, rcvpkts[ind]->pkt_rcvddata);
			}
		}

		// If assembly completed then deliver to the
		// local process (destination)
		if (rcvd_eth_frame->pkt_length == rcvpkts[ind]->pkt_rcvddata)
		{
			// No need of timer further, so stop it.
			if (rcvpkts[ind]->pkt_tobj.timer_count >= 0)
				stoptimer(&(rcvpkts[ind]->pkt_tobj));

			// Add this packet sequence no information to the hash table.
			hnode = locate_hash_node(rcvd_eth_frame->pkt_src_saddr_host, rcvd_eth_frame->pkt_src_saddr_port, rcvd_eth_frame->pkt_src_saddr_type);
			if (hnode == NULL)
				add_hash_node(rcvd_eth_frame->pkt_src_saddr_host, rcvd_eth_frame->pkt_src_saddr_port, rcvd_eth_frame->pkt_src_saddr_type, rcvd_eth_frame->pkt_id);
			else
			{
				hnode->next_seqno = rcvd_eth_frame->pkt_id;
				hnode->msec = secs;
			}

			if (deliver_packet(ind) != 0 )
			{
				if ( rcvd_eth_frame->pkt_dest_saddr_host != -1)
					send_ack(PKTTYPE_ERROR, rcvd_eth_frame, EDEST_NOTEXIST, 0); 
				printk("dest does not exist (%d, %d)\n",rcvd_eth_frame->pkt_dest_saddr_host, rcvd_eth_frame->pkt_dest_saddr_port);
			}

		}
	}
	else // Out of order / send negative ack
	{
		if (rcvpkts[ind]->pkt_header.pkt_dest.saddr_host != -1)
		{
			send_ack(PKTTYPE_ACK, rcvd_eth_frame, rcvpkts[ind]->pkt_expc_seqno, rcvpkts[ind]->pkt_rcvddata);
		}
		else
		{
			printk("Broadcast message received out of order frame : pktid : %d, from host %d, port : %d\n",rcvd_eth_frame->pkt_id, rcvd_eth_frame->pkt_src_saddr_host, rcvd_eth_frame->pkt_src_saddr_port);
		}
		// Discard the frame received.
	}
}

void assemble(void)
{
	struct eth_frame_rcv *rcvd_eth_frame;
	struct frame_buffer *rcvd_frame_buf;
	struct proc_sock *psock;
	struct packet *pkt = NULL;

	assembthread = (struct kthread *)current_thread;

	for( ; ; )
	{
		assembler_active = 0;
		// lock and check for any received frames ...
		_lock(&rxq_lock);
assembler_1:
		if (rx_q_head == NULL)
		{
			event_wakeup(&rx_q_empty);
			event_sleep(&nic_recv_event, &rxq_lock);
			goto assembler_1;
		}

		assembler_active = 1;
		// printk("Assembler : waken up\n");
		// This is the only other thread (another is nic). So continue...
		rcvd_frame_buf = (struct frame_buffer *)rx_q_head;
		rx_q_head = rx_q_head->next;
		if (rx_q_head == NULL) rx_q_tail = NULL;
		rcvd_frame_buf->next = NULL;
		unlock(&rxq_lock);

		rcvd_eth_frame=(struct eth_frame_rcv *)rcvd_frame_buf->buf1;

		if ((checksum((char *)rcvd_eth_frame, rcvd_frame_buf->len, NULL, 0, NULL, 0) != 0) ||  (rcvd_eth_frame->ethfrhdr_type != PROTOTYPE_TEMP))
		{	
			// Bad chechsum, descard it.
			// Not belonging to this protocol, descard it.
			free_frame_buffer((struct frame_buffer *)rcvd_frame_buf);
			continue;
		}

		if (rcvd_eth_frame->pkt_type == PKTTYPE_PKTSEQNOINIT)
		{
			initialize_seqno(rcvd_eth_frame->pkt_src_saddr_host, rcvd_eth_frame->pkt_src_saddr_port, rcvd_eth_frame->pkt_src_saddr_type, rcvd_eth_frame->pkt_id);
			free_frame_buffer((struct frame_buffer *)rcvd_frame_buf);
			continue;
		}
		_lock(&socklist_lock);
		psock = locate_socket(rcvd_eth_frame->pkt_dest_saddr_port, rcvd_eth_frame->pkt_dest_saddr_type);
		unlock(&socklist_lock);
		
		if (psock == NULL)
		{
			if (rcvd_eth_frame->pkt_dest_saddr_port > 0) // Not a parallel process broadcast message
			{
				printk("Socket not found : (%d, %d) <-- (%d, %d) \n", rcvd_eth_frame->pkt_dest_saddr_host, rcvd_eth_frame->pkt_dest_saddr_port,rcvd_eth_frame->pkt_src_saddr_host,rcvd_eth_frame->pkt_src_saddr_port);
				if (rcvd_eth_frame->pkt_dest_saddr_host != -1)
					send_ack(PKTTYPE_ERROR, rcvd_eth_frame, EDEST_NOTEXIST, 0); 

				free_frame_buffer((struct frame_buffer *)rcvd_frame_buf);

				continue;
			}
			else pkt = NULL;
		}
		else pkt = (struct packet *)psock->psk_sndpkt;

		if (pkt != NULL) _lock(&pkt->pkt_lock);
		// Otherwise handle it here
		switch (rcvd_eth_frame->pkt_type)
		{
		case PKTTYPE_ERROR:
			if (pkt != NULL && pkt->pkt_header.pkt_id == rcvd_eth_frame->pkt_id)
			{
				pkt->pkt_status = PKTSTATUS_TXCOMPLETE;
				pkt->pkt_swinstatus = rcvd_eth_frame->pkt_winseq_no; // Error code.
				event_wakeup(&pkt->pkt_windowcomplete); 
				unlock(&pkt->pkt_lock);
				free_frame_buffer((struct frame_buffer *)rcvd_frame_buf);
			}
			if (psock != NULL) unlock(&psock->psk_busy);
			break;

		case PKTTYPE_ACK:
			
			if (pkt != NULL)
			{
				process_ack(pkt, rcvd_eth_frame);
				event_wakeup(&pkt->pkt_windowcomplete); 
				unlock(&pkt->pkt_lock);
			}
			if (psock != NULL) unlock(&psock->psk_busy);
			free_frame_buffer((struct frame_buffer *)rcvd_frame_buf);
			break;
		case PKTTYPE_CONNECTCLOSE:
			// Ctrl packet such as connnection closing request, ...
			// process connection closing request
	
			send_ack(PKTTYPE_ACK, rcvd_eth_frame, 1, rcvd_eth_frame->pkt_length);
			if (psock != NULL)
			{
				psock->psk_peer.saddr_host = 0; // No longer connected
				psock->psk_peer.saddr_port = 0;
				if (pkt != NULL)
				{
					pkt->pkt_status = PKTSTATUS_TXCOMPLETE;
					pkt->pkt_cur_pos = pkt->pkt_header.pkt_length;
					pkt->pkt_swinstatus = ECONNECTIONCLOSED; // Error code.
					event_wakeup(&pkt->pkt_windowcomplete);
					unlock(&pkt->pkt_lock);
				}
				_lock(&psock->psk_rcvpktlock);
				event_wakeup(&(psock->psk_rcvdevent));
				unlock(&psock->psk_rcvpktlock);
				unlock(&psock->psk_busy);
			}
			free_frame_buffer((struct frame_buffer *)rcvd_frame_buf);

			break;

		case PKTTYPE_MCCONSISTENCY:
		case PKTTYPE_REXEC:
		case PKTTYPE_SYNCH:
		case PKTTYPE_DATA:	
			// packet_type_data(rcvd_eth_frame);
			// Locks must be released in advance because 
			// in delivery function these may be required again
			if (pkt != NULL) unlock(&pkt->pkt_lock);
			if (psock != NULL) unlock(&psock->psk_busy);
			packet_type_data(rcvd_frame_buf);
			
			break;
		default: 
			if (pkt != NULL) unlock(&pkt->pkt_lock);
			if (psock != NULL) unlock(&psock->psk_busy);
			printk("Some unknown frame received\n");
		}
	}
}

int deliver_packet(int ind)
{
	struct proc_sock *psock = NULL;
	struct packet *pkt;
	struct frame_buffer *frbuf1;
	char *data;
	int offset;

	// Queue it at the destination socket. 
	if (rcvpkts[ind] != NULL)
	{
		rcvpkts[ind]->pkt_next = NULL;
		rcvpkts[ind]->pkt_rcv_offset = 0;

		// Is it broadcast packet?
		if (rcvpkts[ind]->pkt_header.pkt_dest.saddr_host == -1)
		{
			// Is it for parallel processes's threads?
			if (rcvpkts[ind]->pkt_header.pkt_dest.saddr_port < -65535)
			{
				// Deliver this by local broadcasting to all threads of the specified process
				// Prepare the data
				data = (char *)kmalloc(rcvpkts[ind]->pkt_header.pkt_length);
				if (data != NULL)
				{
					offset = 0;
					while (rcvpkts[ind]->pkt_rcvframe_head != NULL)
					{
						frbuf1 = (struct frame_buffer *)rcvpkts[ind]->pkt_rcvframe_head; 
						bcopy(frbuf1->buf1 + FRAMEDATA_OFFSET, data+offset, frbuf1->len1 - FRAMEDATA_OFFSET);
						rcvpkts[ind]->pkt_rcvframe_head = (struct frame_buffer *)frbuf1->next;
						offset += frbuf1->len1 - FRAMEDATA_OFFSET;
						free_frame_buffer(frbuf1);
					}
					rcvpkts[ind]->pkt_rcvframe_tail = NULL;
					local_broadcast(&(rcvpkts[ind]->pkt_header.pkt_dest), &(rcvpkts[ind]->pkt_header.pkt_src), data, rcvpkts[ind]->pkt_header.pkt_length, rcvpkts[ind]->pkt_header.pkt_id, rcvpkts[ind]->pkt_header.pkt_groupid, rcvpkts[ind]->pkt_header.pkt_tag);
					kfree(data);
				}
				else printk("Error : deliver packet : kmalloc\n");
				free_packet(rcvpkts[ind]);
				rcvpkts[ind] = NULL;
				return 0;
			}
		}
		_lock(&socklist_lock);
		psock = locate_socket(rcvpkts[ind]->pkt_header.pkt_dest.saddr_port, rcvpkts[ind]->pkt_header.pkt_dest.saddr_type);
		unlock(&socklist_lock);
				
		if (psock != NULL) // Socket located 
		{
			_lock(&psock->psk_rcvpktlock);
			if (psock->psk_rcvpkt == NULL)
				psock->psk_rcvpkt = rcvpkts[ind];
			else // Insert at the end.
			{
				pkt = (struct packet *)psock->psk_rcvpkt;
				while (pkt->pkt_next != NULL) pkt = (struct packet *)pkt->pkt_next;
				pkt->pkt_next = rcvpkts[ind];
			}
			event_wakeup(&(psock->psk_rcvdevent));
			unlock(&psock->psk_rcvpktlock);
			unlock(&psock->psk_busy);
			rcvpkts[ind] = NULL; // Mark slot as empty
			return 0;
		}
		// Else error handling must be done?
		else
		{ 
			free_packet(rcvpkts[ind]);
			rcvpkts[ind] = NULL; // Mark slot as empty
			return -1;
		}
		return 0; // Nothing
	}
	return 0; // Nothing
}

int trecvfrom_internal(char buf[], int maxlen, short *from, int nb, int pkttype, int groupid, int *tagptr, int wmsec);
int syscall_trecvfrom(char buf[], int maxlen, short *from, int nb)
{
	int tag = -1;
	int wmsec = 0;
	
	return trecvfrom_internal(buf, maxlen, from, nb, PKTTYPE_DATA, 0, &tag, wmsec);
}

int trecvfrom_internal(char buf[], int maxlen, short *from, int nb, int pkttype, int groupid, int *tagptr, int wmsec)
{
	struct packet *pkt, *prev = NULL;
	int actlen;
	unsigned long old_cr0;
	int offset;
	int fromaddr;
	struct frame_buffer *frbuf1, *frbuf2;
	//int k;
	int tag = *tagptr;
	int st_sec, st_msec;

	if (from != NULL) fromaddr  = *from;
	else fromaddr = -2;

	st_sec = secs;
	st_msec = millesecs;
	while (1)
	{
		_lock(&current_thread->kt_defsock->psk_rcvpktlock);
trecvfrom_internal_2:
		
		if (current_thread->kt_defsock->psk_rcvpkt == NULL)
		{
trecvfrom_internal_1:	if (nb == 0)
			{
				if (wmsec <= 0) // Indefinite sleeping
					event_sleep(&(current_thread->kt_defsock->psk_rcvdevent), &current_thread->kt_defsock->psk_rcvpktlock);
				else
				{
					event_timed_sleep(&(current_thread->kt_defsock->psk_rcvdevent), &current_thread->kt_defsock->psk_rcvpktlock, wmsec);
					if (((secs - st_sec)*1000 + (millesecs - st_msec)) >= wmsec) // Maximum wiating time is over, then non-blocking
						nb = 1;
					else wmsec = wmsec - ((secs - st_sec)*1000 + (millesecs - st_msec));
				}
				goto trecvfrom_internal_2;
			}
			else 
			{
				unlock(&current_thread->kt_defsock->psk_rcvpktlock);
				return 0;
			}
		}
		else
		{
			old_cr0 = clear_paging();
			pkt = (struct packet *)current_thread->kt_defsock->psk_rcvpkt;
			prev = NULL;
			while (pkt != NULL) 
			{
				if ((pkt->pkt_header.pkt_type == pkttype) &&
				    ((fromaddr == MPI_ANY_SOURCE) || ((pkt->pkt_header.pkt_src.saddr_port & 0x0000ffff) == fromaddr)) && 
				    ((groupid == 0) || (pkt->pkt_header.pkt_groupid == groupid)) &&
				    ((tag == MPI_ANY_TAG) || (pkt->pkt_header.pkt_tag == tag)))	// Expected type of packet is found
					break;
				else // Otherwise skip this
				{
					prev = pkt;
					pkt = (struct packet *)pkt->pkt_next;
				}
			}
			if (pkt == NULL) // Expected type of packet not found
			{
				restore_paging(old_cr0, current_thread->kt_tss->cr3);
				goto trecvfrom_internal_1;
			}
			actlen = pkt->pkt_header.pkt_length;
			if (actlen <= maxlen)
			{
				// copy it.
				if (pkt->pkt_data != NULL)
				{
					phys2vmcopy((char *)pkt->pkt_data, current_process->proc_vm, buf, actlen);
					kfree(pkt->pkt_data);
				}
				else
				{
					offset = 0;
					frbuf1 = (struct frame_buffer *)pkt->pkt_rcvframe_head;
					while (frbuf1 != NULL)
					{
						phys2vmcopy((char *)frbuf1->buf1 + FRAMEDATA_OFFSET, current_process->proc_vm, buf+offset, frbuf1->len1 - FRAMEDATA_OFFSET);
						offset += (frbuf1->len1 - FRAMEDATA_OFFSET);
						frbuf2 = frbuf1;
						frbuf1 = (struct frame_buffer *)frbuf1->next;
						free_frame_buffer(frbuf2);
					}
				}
				
				if (from != NULL)
				{
					// Translate the from address into group member rank
						fromaddr = pkt->pkt_header.pkt_src.saddr_port;
					phys2vmcopy((char *)&(fromaddr), current_process->proc_vm, (char *)from, 2);
				}
				if (tagptr != NULL)
					phys2vmcopy((char *)&(pkt->pkt_header.pkt_tag), current_process->proc_vm, (char *)tagptr, 4);

				// Remove and free it.
				if (prev == NULL)
					current_thread->kt_defsock->psk_rcvpkt = pkt->pkt_next;
				else
					prev->pkt_next = pkt->pkt_next;
				kfree((void *)pkt);
			} 
			else 
			{
				actlen = -actlen;
				if (from != NULL)
				{
					// Translate the from address into group member rank
						fromaddr = pkt->pkt_header.pkt_src.saddr_port;
					phys2vmcopy((char *)&(fromaddr), current_process->proc_vm, (char *)from, 2);
				}
				if (tagptr != NULL)
					phys2vmcopy((char *)&(pkt->pkt_header.pkt_tag), current_process->proc_vm, (char *)tagptr, 4);

			}

			restore_paging(old_cr0, current_thread->kt_tss->cr3);
			if (current_thread->kt_defsock->psk_rcvpkt == NULL)
				event_wakeup(&(current_thread->kt_defsock->psk_rcvqempty));
			unlock(&current_thread->kt_defsock->psk_rcvpktlock);
			return actlen;
		}
	}
}

struct proc_sock * create_defsock(int portno)
{
	int i;
	struct proc_sock *psock;

	// Allocate proc socket and attach to the list of socket
	psock = NULL;
	_lock(&socktable_lock);
	for (i=0; i<MAXSOCKETS; i++)
	{
		if (socktable[i].psk_type == SOCK_UNUSED)
		{
			psock = &socktable[i];
			psock->psk_type = SOCK_DGRAM; // PROCSOCK_DEFAULT;
			psock->psk_port = portno;
			psock->psk_refcount = 1;
			psock->psk_sndseqid = 0;
			psock->psk_rcvseqid = 0;
			psock->psk_rcvpkt = NULL;
			psock->psk_sndpkt = NULL;

			psock->psk_transmitterusing = 0;
			psock->psk_delrequest = 0;
			lockobj_init(&psock->psk_rcvpktlock);
			lockobj_init(&psock->psk_busy);
			eventobj_init(&psock->psk_rcvqempty);
			eventobj_init(&psock->psk_rcvdevent);
			break;
		}
	}
	unlock(&socktable_lock);

	if (psock == NULL) return NULL;
	else
	{
		// Add to the global sockets
		_lock(&socklist_lock);
		psock->psk_globalnext = socklist_hdr;
		socklist_hdr = psock;
		unlock(&socklist_lock);
		return psock;
	}
}
// Eventhough these parameters are written, actually doesnot support 
// Any address family, or protocols (standard).
// Other than default sockets ( that are used with threading interface 
// without creating sockets explicitly ) sockets created explicitly using 
// socket call will be treated as file descriptors and will be shown under 
// the file table of the process. In side the file table socket table entry 
// will be stored. As usual -1 is invalid open file and invalid socket address

int syscall_socket(int family, int type, int protocol)
{
	int i, fhand = -1;
	struct proc_sock *psock;

	// Find a free file table slot
	for (i=0; i<MAXPROCFILES; i++)
	{	
		// handle = -1 stands for stdin, stdout, stderror 
		// (later this has to be modified.). Other unused handles are 
		// initialized with -3.
		if (current_process->proc_fileinf->fhandles[i] < -2)
		{
			fhand = i;
			current_process->proc_fileinf->fhandles[i] = 0; // In use
			break;
		}
	}
	if (fhand < 0) return ENO_FREE_FILE_TABLE_SLOT;

	// Allocate proc socket and attach to the list of socket
	psock = NULL;
	_lock(&socktable_lock);
	for (i=0; i<MAXSOCKETS; i++)
		if (socktable[i].psk_type == SOCK_UNUSED)
		{
			psock = &socktable[i];
			psock->psk_type = -1; // Just to mark it as under use
			break;
		}
	
	unlock(&socktable_lock);

	if (psock == NULL)
	{
		current_process->proc_fileinf->fhandles[fhand] = -3; // release
		return ENOSOCKET;
	}
	else // Store this address in the file table slot
		current_process->proc_fileinf->fhandles[fhand] = i + SOCKHANDLEBEGIN;
	if (type == SOCK_STREAM)
		psock->psk_type = SOCK_STREAM; // Other types not considered
	else
		psock->psk_type = SOCK_DGRAM;

	psock->psk_port = 0; // Not yet known.
	psock->psk_refcount = 1;
	psock->psk_sndseqid = 1;
	psock->psk_rcvseqid = 1;
	psock->psk_rcvpkt = NULL;
	psock->psk_sndpkt = NULL;
	psock->psk_peer.saddr_type = psock->psk_type;
	psock->psk_peer.saddr_host = 0;
	psock->psk_peer.saddr_port = -1; // Initial value for work around 
					 // recv function.
	psock->psk_transmitterusing = 0;
	psock->psk_delrequest = 0;
	lockobj_init(&psock->psk_rcvpktlock);
	lockobj_init(&psock->psk_busy);
	eventobj_init(&psock->psk_rcvqempty);
	eventobj_init(&psock->psk_rcvdevent);

	// Add to the global sockets
	_lock(&socklist_lock);
	psock->psk_globalnext = socklist_hdr;
	socklist_hdr = psock;
	unlock(&socklist_lock);

	return fhand;
}

// This doesnot honour any of the internet addresses
// This takes only the port number.
int syscall_bind(int sockfd, struct sockaddr *myaddr, socklen_t addrlen)
{
	struct proc_sock *psock, *ps1;
	struct sockaddr_in *sin;
	int port;

	if (sockfd < 0 || sockfd >= MAXPROCFILES) return -1; // Invalid descriptor

	if (current_process->proc_fileinf->fhandles[sockfd] < SOCKHANDLEBEGIN || current_process->proc_fileinf->fhandles[sockfd] > SOCKHANDLEEND)
		return -1; // Invalid address?

	psock = &socktable[current_process->proc_fileinf->fhandles[sockfd] - SOCKHANDLEBEGIN];

	// Still may be incorrect. (But not verified)
	// Only port number part of myaddr is taken and 
	// verified for duplicate usage.
	sin = (struct sockaddr_in *)myaddr;
	if (sin->sin_port > 0) port = sin->sin_port;
	else port = next_internal_port_no++;

	do
	{
		_lock(&socklist_lock);
		ps1 = (struct proc_sock *)socklist_hdr;
		while (ps1 != NULL)
		{
			if (ps1->psk_port == port && ps1->psk_type == psock->psk_type)
				break;
			ps1 = (struct proc_sock *)ps1->psk_globalnext;
		}

		if (ps1 != NULL)
		{
			// Already in use
			unlock(&socklist_lock);
			if (sin->sin_port == 0) port = next_internal_port_no++;
			else return EPORTALREADYEXISTS;
		}
		else
		{
			psock->psk_port = port;
			unlock(&socklist_lock);
			if (sin->sin_addr.s_addr == INADDR_ANY)
			{
				// At present multiple host addresses are not 
				// supported, so only one host address is 
				// taken into consideration.
				//psock->psk_host = ...
			}
			send_seqnumber_mesg(this_hostid, port, psock->psk_type);
			return 0;
		}
	} while (1);
}

// generic interface for receiving from a specified socket
// It is used by both default socket interface and explicit 
// socket interface
// MSG_PEEK, MSG_TRUNC, MSG_DONTWAIT, 
int recv_fromsock_stream(struct proc_sock *psk, char *buf, int maxlen, int flags)
{
	int nbytes = 0;
	unsigned long old_cr0; 
	struct packet *pkt;
	int actlen, len, len2, froffset;
	struct frame_buffer *frbuf1;

	
	while (1)
	{
		_lock(&psk->psk_rcvpktlock);
recv_fromsock_stream_1:

		if (psk->psk_peer.saddr_port != 0 && psk->psk_rcvpkt == NULL) // No packets pending
		{
			if ((flags & MSG_DONTWAIT) == 0)
			{
				event_sleep(&(psk->psk_rcvdevent), &psk->psk_rcvpktlock);
				goto recv_fromsock_stream_1;
			}
			else 
			{
				unlock(&psk->psk_rcvpktlock);
				return 0;
			}
		}
		else if (psk->psk_peer.saddr_port == 0)
		{
			unlock(&psk->psk_rcvpktlock);
			// Peer has closed the connection.
			return ECONNECTIONCLOSED;
		}
		else
		{
			old_cr0 = clear_paging();
			while (psk->psk_rcvpkt != NULL && nbytes < maxlen)
			{
				pkt = (struct packet *)psk->psk_rcvpkt;
				actlen = pkt->pkt_header.pkt_length - pkt->pkt_rcv_offset;
				len = (actlen < (maxlen-nbytes)) ? actlen : (maxlen - nbytes);
				// copy it.
				if (pkt->pkt_data != NULL)
				{
					phys2vmcopy(pkt->pkt_data+pkt->pkt_rcv_offset, current_process->proc_vm, buf, len);
					buf += len;
					nbytes += len;
					pkt->pkt_rcv_offset += len;
				}
				else
				{	
					while (len > 0)
					{
						froffset = (pkt->pkt_rcv_offset % MAX_PROTOCOL_FRSIZE);
						frbuf1 = (struct frame_buffer *)pkt->pkt_rcvframe_head;
						len2 = ((frbuf1->len1 - froffset - FRAMEDATA_OFFSET) < len) ? (frbuf1->len1 - froffset - FRAMEDATA_OFFSET) : len;
						phys2vmcopy(frbuf1->buf1+froffset + FRAMEDATA_OFFSET, current_process->proc_vm, buf, len2);
						// Determie whether frame should be cleared
						if (froffset + len2 + FRAMEDATA_OFFSET == frbuf1->len1)
						{
							pkt->pkt_rcvframe_head = (struct frame_buffer *)frbuf1->next;
							free_frame_buffer(frbuf1);
						}
						len -= len2;
						buf += len2;
						nbytes += len2;
						pkt->pkt_rcv_offset += len2;
					}
				}
				// Is packet emptied?
				if (pkt->pkt_rcv_offset == pkt->pkt_header.pkt_length)
				{
					// Remove and free it.
					psk->psk_rcvpkt = pkt->pkt_next;
					if (pkt->pkt_data != NULL)
						kfree((void *)pkt->pkt_data);
					kfree((void *)pkt);
				}
			}
			restore_paging(old_cr0, current_thread->kt_tss->cr3);

			if (psk->psk_rcvpkt == NULL)
				event_wakeup(&(psk->psk_rcvqempty));
			unlock(&psk->psk_rcvpktlock);
			return nbytes;
		}
	}
}

// generic interface for receiving from a specified socket
// It is used by both default socket interface and explicit 
// socket interface
// MSG_PEEK, MSG_TRUNC, MSG_DONTWAIT, 
void disp_debuginfo(struct process *proc);
int recv_fromsock_dgram(struct proc_sock *psk, struct isockaddr *from, char *buf, int maxlen, int flags)
{
	struct isockaddr isk;
	unsigned long old_cr0;
	struct packet *pkt;
	int len, len1, len2, actlen; 
	struct frame_buffer *frbuf1;
	
	while (1)
	{
		_lock(&psk->psk_rcvpktlock);
recv_fromsock_dgram_1:
		if (psk->psk_rcvpkt == NULL) // No packets pending
		{
			if ((flags & MSG_DONTWAIT) == 0)
			{
				event_wakeup(&(psk->psk_rcvqempty));
				event_sleep(&(psk->psk_rcvdevent), &psk->psk_rcvpktlock);
				goto recv_fromsock_dgram_1;
			}
			else 
			{
				event_wakeup(&(psk->psk_rcvqempty));
				unlock(&psk->psk_rcvpktlock);
				return 0;
			}
		}
		else
		{
			old_cr0 = clear_paging();
			pkt = (struct packet *)psk->psk_rcvpkt;
			actlen = pkt->pkt_header.pkt_length;
			if (actlen <= maxlen || ((flags & MSG_TRUNC) != 0))
			{
				len1 = len = (actlen < maxlen) ? actlen : maxlen;
				// copy it.
				if (pkt->pkt_data != NULL)
					phys2vmcopy(pkt->pkt_data, current_process->proc_vm, buf, len);
				else
				{
					frbuf1 = (struct frame_buffer *)pkt->pkt_rcvframe_head;
					while (len1 > 0)
					{
						len2 = (frbuf1->len1 - FRAMEDATA_OFFSET < len1) ? (frbuf1->len1 - FRAMEDATA_OFFSET) : len1;
						phys2vmcopy(frbuf1->buf1 + FRAMEDATA_OFFSET, current_process->proc_vm, buf, len2);
						frbuf1 = (struct frame_buffer *)frbuf1->next;
						len1 -= len2;
						buf += len2;
					}
				}
				phys2vmcopy((char *)&(pkt->pkt_header.pkt_src), current_process->proc_vm, (char *)&isk, sizeof(struct isockaddr));
       				if ((flags & MSG_PEEK) == 0)
				{
					// Remove and free it.
					while (pkt->pkt_rcvframe_head != NULL)
					{
						frbuf1 = (struct frame_buffer *)pkt->pkt_rcvframe_head;
						pkt->pkt_rcvframe_head = (struct frame_buffer *)frbuf1->next;
						free_frame_buffer(frbuf1);
					}
					psk->psk_rcvpkt = pkt->pkt_next;
					if (pkt->pkt_data != NULL)
						kfree((void *)pkt->pkt_data);
					kfree((void *)pkt);
				}
				actlen = len;
			}
			else actlen = -actlen;

			restore_paging(old_cr0, current_thread->kt_tss->cr3);
			if (from != NULL) *from = isk;

			if (psk->psk_rcvpkt == NULL)
				event_wakeup(&(psk->psk_rcvqempty));
			unlock(&psk->psk_rcvpktlock);
			return actlen;
		}
	}
}

int syscall_recv(int s, void *buf, size_t len, int flags)
{
	struct proc_sock *psk;

	// Is valid socket? simple test
	if (s < 0 || s >= MAXPROCFILES) return EBADF;
	if (current_process->proc_fileinf->fhandles[s] >= SOCKHANDLEBEGIN && current_process->proc_fileinf->fhandles[s] < SOCKHANDLEEND) // Not a file handle) // Not a file handle
	{
		psk = &socktable[current_process->proc_fileinf->fhandles[s] - SOCKHANDLEBEGIN];
		if (psk->psk_type != SOCK_STREAM) return EINVALIDFUNCTION;
		return recv_fromsock_stream(psk, buf, len, flags);
	}
	else return ENOTSOCK;
}

int syscall_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	struct proc_sock *psk;
	struct sockaddr_in *from_in;
	struct isockaddr ifrom;
	int res;

	// Is valid socket? simple test
	if (s < 0 || s >= MAXPROCFILES) return EBADF;
	if (current_process->proc_fileinf->fhandles[s] >= SOCKHANDLEBEGIN && current_process->proc_fileinf->fhandles[s] < SOCKHANDLEEND) // Not a file handle) // Not a file handle
	{
		psk = &socktable[current_process->proc_fileinf->fhandles[s] - SOCKHANDLEBEGIN];
		if (psk->psk_type != SOCK_DGRAM) return EINVALIDFUNCTION;
		
		res = recv_fromsock_dgram(psk, &ifrom, buf, len, flags);

		if (from != NULL)
		{
			from_in = (struct sockaddr_in *)from;	
			from_in->sin_family = PF_INET;
			from_in->sin_port = (0x0000ffff & ifrom.saddr_port);
			from_in->sin_addr.s_addr = ifrom.saddr_host;
		}
		if (fromlen != NULL) *fromlen = sizeof(struct sockaddr_in);
		return res;
	}
	else return ENOTSOCK;
}

// Assumes socklist_lock and psock - psk_busy are already locked.
int sock_send(struct proc_sock *psock, struct isockaddr *dest, const char *buf, unsigned int len, int pkttype, int groupid, int tag, int nb)
{
	struct packet *pkt, *p1;
	unsigned long old_cr0;
	struct proc_sock *s1;
	extern void free_packet(struct packet *pkt);
	int status;


	// Allocate packet buffer for holding the message.
	old_cr0 = clear_paging();
	pkt = kmalloc( sizeof(struct packet));
	if (pkt == NULL)
	{
		printk("Malloc failure in sock_send\n");
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
		return ENOMEM;
	}

	pkt->pkt_data = kmalloc( len);
	if (pkt->pkt_data == NULL)
	{
		printk("Malloc failure in sock_send\n");
		kfree( (void *)pkt);
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
		return ENOMEM;
	}
	// Copy the data of the packet
	vm2physcopy(current_process->proc_vm, (char *)dest, (char *)&(pkt->pkt_header.pkt_dest), sizeof(struct isockaddr));
	vm2physcopy(current_process->proc_vm, (char *)buf, pkt->pkt_data, len);

	timerobj_init(&(pkt->pkt_tobj));
	lockobj_init(&pkt->pkt_lock);
	eventobj_init(&pkt->pkt_windowcomplete);
	eventobj_init(&pkt->pkt_sendcomplete);

	pkt->pkt_rcvframe_head = pkt->pkt_rcvframe_tail = NULL;

	// Initialize the fields of packet
	pkt->pkt_header.pkt_type = pkttype;
	pkt->pkt_header.pkt_src.saddr_type = psock->psk_type;
	pkt->pkt_header.pkt_src.saddr_host = this_hostid;
	pkt->pkt_header.pkt_src.saddr_port = psock->psk_port;
	pkt->pkt_header.pkt_length = len;
	pkt->pkt_header.pkt_id = ++psock->psk_sndseqid;//++packet_seq_no;
	pkt->pkt_header.pkt_tag = tag;
	pkt->pkt_header.pkt_groupid = groupid;
	pkt->pkt_next = NULL;
        pkt->pkt_rcv_offset = pkt->pkt_expc_seqno = pkt->pkt_rcvddata = pkt->pkt_ackdata = 0;

	if (pkt->pkt_header.pkt_dest.saddr_host == this_hostid) // Local delivery only
	{
		s1 = locate_socket(pkt->pkt_header.pkt_dest.saddr_port, pkt->pkt_header.pkt_dest.saddr_type);

		if (s1 != NULL) // Found
		{
			_lock(&s1->psk_rcvpktlock);
			// Check if it is connect closing message
			if (pkttype == PKTTYPE_CONNECTCLOSE)
			{
				s1->psk_peer.saddr_host = 0;
				s1->psk_peer.saddr_port = 0;
				free_packet((struct packet *)pkt);
			}
			else if (s1->psk_rcvpkt == NULL) // Otherwise insert
			{
				s1->psk_rcvpkt = pkt;
			}
			else // Insert at the end.
			{
				p1 = (struct packet *)s1->psk_rcvpkt;
				while (p1->pkt_next != NULL) 
					p1 = (struct packet *)p1->pkt_next;
				p1->pkt_next = pkt;
			}
			event_wakeup(&(s1->psk_rcvdevent));
			unlock(&s1->psk_rcvpktlock);
			unlock(&s1->psk_busy); // Previous locate has locked it
			
			restore_paging(old_cr0, current_thread->kt_tss->cr3);
			return len;
		}
		else
		{
			free_packet((struct packet *)pkt);
			restore_paging(old_cr0, current_thread->kt_tss->cr3);
			return EPEERNOTEXISTS;
		}
	}

	// Otherwise this must be sent on the wire	
	// Initialize sliding window protocol variables.
	pkt->pkt_retries = 1;
	pkt->pkt_swinstatus = 0;
	pkt->pkt_nonblocking = nb;
	pkt->pkt_wincur_pos = 0;
	pkt->pkt_cur_pos = 0;
	pkt->pkt_window_size = 0;
	pkt->pkt_current_frame = 0;
	pkt->pkt_start_frame = 0;

	// Unlonk and then lock socklist_lock and psk_busy in order
	// Queue it and change the state of the packet
	if (psock->psk_sndpkt == NULL)
		psock->psk_sndpkt = pkt;
	else
	{
		p1 = (struct packet *)psock->psk_sndpkt;
		while (p1->pkt_next != NULL) p1 = (struct packet *)p1->pkt_next;
		p1->pkt_next = pkt;
	}
	pkt->pkt_status = PKTSTATUS_READYTOTXMT;

	// Wakeup pkttransmitter task if necessary
	event_wakeup(&pkt_send_event);
	if (nb == 0)
	{
		unlock(&socklist_lock);
                event_sleep(&(pkt->pkt_sendcomplete), &psock->psk_busy);
		unlock(&psock->psk_busy);
		// Before returning get again the locks - socklist_lock and psk_busy lock
		_lock(&socklist_lock);
		_lock(&psock->psk_busy);
                status = pkt->pkt_swinstatus;
                kfree( (void *)pkt->pkt_data);
                kfree( (void *)pkt);
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
                return status;
	}
	else 
	{
		restore_paging(old_cr0, current_thread->kt_tss->cr3);
		return len; // Successfully queued for transmission
	}
}

int syscall_send(int s, const void *buf, size_t len, int flags)
{
	struct proc_sock *psk;
	int ret;

	// Is valid socket? simple test
	if (s < 0 || s >= MAXPROCFILES) return EBADF;
	if (current_process->proc_fileinf->fhandles[s] >= SOCKHANDLEBEGIN && current_process->proc_fileinf->fhandles[s] < SOCKHANDLEEND) // Not a file handle) // Not a file handle
	{
		psk = &socktable[current_process->proc_fileinf->fhandles[s] - SOCKHANDLEBEGIN];
		if (psk->psk_type != SOCK_STREAM) return EINVALIDFUNCTION;
		if (psk->psk_peer.saddr_port == 0) return ENOTCONNECTED;
		_lock(&socklist_lock);
		_lock(&psk->psk_busy);
		ret =  sock_send(psk, &(psk->psk_peer), buf, len, PKTTYPE_DATA, 0, -1, 1);
		unlock(&psk->psk_busy);
		unlock(&socklist_lock);
		
		return ret;
	}
	else return ENOTSOCK;
}

int syscall_sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	struct proc_sock *psk;
	struct isockaddr dest;
	struct sockaddr_in *sinaddr;
	int ret;

	// Is valid socket? simple test
	if (s < 0 || s >= MAXPROCFILES) return EBADF;
	if (current_process->proc_fileinf->fhandles[s] >= SOCKHANDLEBEGIN && current_process->proc_fileinf->fhandles[s] < SOCKHANDLEEND) // Not a file handle) // Not a file handle
	{
		psk = &socktable[current_process->proc_fileinf->fhandles[s] - SOCKHANDLEBEGIN];
		if (psk->psk_type != SOCK_DGRAM) return EINVALIDFUNCTION;
		// prepare destination address
		sinaddr = (struct sockaddr_in *)to;
		dest.saddr_host = sinaddr->sin_addr.s_addr;
		dest.saddr_type = SOCK_DGRAM;
		dest.saddr_port = sinaddr->sin_port;
		
		_lock(&socklist_lock);
		_lock(&psk->psk_busy);
		ret = sock_send(psk, &dest, buf, len, PKTTYPE_DATA, 0, -1, 1);
		unlock(&psk->psk_busy);
		unlock(&socklist_lock);
		return ret;
	}
	else return ENOTSOCK;
}

int syscall_connect(int s, const struct sockaddr *serv_addr, socklen_t addrlen)
{
	struct proc_sock *psk;
	struct isockaddr dest;
	struct sockaddr_in *sinaddr;
	struct connect_request msg;
	int err;

	// Is valid socket? simple test
	if (s < 0 || s >= MAXPROCFILES) return EBADF;
	if (current_process->proc_fileinf->fhandles[s] >= SOCKHANDLEBEGIN && current_process->proc_fileinf->fhandles[s] < SOCKHANDLEEND) // Not a file handle) // Not a file handle
	{
		// Is socket useable for connection?
		psk = &socktable[current_process->proc_fileinf->fhandles[s] - SOCKHANDLEBEGIN];
		if (psk->psk_type != SOCK_STREAM) return ESOCKTYPE;
		if (psk->psk_port == 0) return ENOTBOUND;
		if (psk->psk_peer.saddr_port != -1) return ECONNECTED;

		// Prepare connection request message
		sinaddr = (struct sockaddr_in *)serv_addr;
		msg.conreq_type = PKTTYPE_CONNECTREQ;
		msg.conreq_dest.saddr_host = sinaddr->sin_addr.s_addr;
		msg.conreq_dest.saddr_type = SOCK_STREAM;
		msg.conreq_dest.saddr_port = sinaddr->sin_port;
		dest.saddr_host = sinaddr->sin_addr.s_addr;
		dest.saddr_type = SOCK_STREAM;
		dest.saddr_port = sinaddr->sin_port;

		msg.conreq_src.saddr_host = this_hostid;
		msg.conreq_src.saddr_type = SOCK_STREAM;
		msg.conreq_src.saddr_port = psk->psk_port;
		// Send connection request message.
		_lock(&socklist_lock);
		_lock(&psk->psk_busy);
		err = sock_send(psk, &dest, (char *)&msg, sizeof(msg), PKTTYPE_DATA, 0, -1, 1);
		unlock(&psk->psk_busy);
		unlock(&socklist_lock);
		
		if (err != sizeof(msg)) return err;

		// receive reply
		err = recv_fromsock_stream(psk, (char *)&msg, sizeof(msg), 0);
	
		if (err != sizeof(msg)) return err;

		// Remember the peer point address
		psk->psk_peer = msg.conreq_dest;

		return 0;
	}
	else return ENOTSOCK;
}

int syscall_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
	struct proc_sock *psk, *psk1;
	struct sockaddr_in saddr;
	struct connect_request msg;
	int err, s1;

	// Is valid socket? simple test
	if (s < 0 || s >= MAXPROCFILES) return EBADF;
	if (current_process->proc_fileinf->fhandles[s] >= SOCKHANDLEBEGIN && current_process->proc_fileinf->fhandles[s] < SOCKHANDLEEND) // Not a file handle) // Not a file handle
	{
		// Is socket useable for connection?
		psk = &socktable[current_process->proc_fileinf->fhandles[s] - SOCKHANDLEBEGIN];
		if (psk->psk_type != SOCK_STREAM) return ESOCKTYPE;
		if (psk->psk_port == 0) return ENOTBOUND;

accept1:
		// Receive connection request message
		err = recv_fromsock_stream(psk, (char *)&msg, sizeof(msg), 0);
		if (err != sizeof(msg)) return err;

		if (msg.conreq_type == PKTTYPE_CONNECTREQ) // Connection req?
		{
			// Create another socket assign it this connection
			s1 = syscall_socket(PF_INET, psk->psk_type, 0);
			if (s1 < 0) return s1; // error.

			// bind with some unused port number.
			do {
				saddr.sin_family = PF_INET;
				saddr.sin_port = next_internal_port_no++;
				saddr.sin_addr.s_addr = this_hostid;
			} while (syscall_bind(s1, (struct sockaddr *)&saddr, sizeof(saddr)) != 0);
			// Successfully created new socket copy
			// remember peer source addr.
			psk1 = &socktable[current_process->proc_fileinf->fhandles[s1] - SOCKHANDLEBEGIN];
			psk1->psk_peer = msg.conreq_src;
			// Send connection reply message.
			msg.conreq_type = PKTTYPE_CONNECTREP;
			msg.conreq_dest.saddr_host = saddr.sin_addr.s_addr; // Change the dest addr
			msg.conreq_dest.saddr_type = SOCK_STREAM;
			msg.conreq_dest.saddr_port = saddr.sin_port; 
			_lock(&socklist_lock);
			_lock(&psk1->psk_busy);
			err = sock_send(psk1, &(msg.conreq_src), (char *)&msg, sizeof(msg), PKTTYPE_DATA, 0, -1, 1);
			unlock(&psk1->psk_busy);
			unlock(&socklist_lock);
			
			if (err != sizeof(msg)) 
			{
				syscall_close_socket(s1);
				return err;
			}
			// Every thing is OK. SUCCESS
			return s1;
		}
		goto accept1;
	}
	else return ENOTSOCK;
}
			
int syscall_listen(int s, int backlog)
{
	struct proc_sock *psk;

	// Is valid socket? simple test
	if (s < 0 || s >= MAXPROCFILES) return EBADF;
	if (current_process->proc_fileinf->fhandles[s] >= SOCKHANDLEBEGIN && current_process->proc_fileinf->fhandles[s] < SOCKHANDLEEND) // Not a file handle) // Not a file handle
	{
		// Is socket useable for connection?
		psk = &socktable[current_process->proc_fileinf->fhandles[s] - SOCKHANDLEBEGIN];
		if (psk->psk_type != SOCK_STREAM) return ESOCKTYPE;
		// Nothing more can be done now...
		return 0;
	}
	else return ENOTSOCK;
	
}

int syscall_close_socket(int s)
{
	// Is valid socket? simple test
	if (s < 0 || s >= MAXPROCFILES) return EBADF;
	if (current_process->proc_fileinf->fhandles[s] >= SOCKHANDLEBEGIN && current_process->proc_fileinf->fhandles[s] < SOCKHANDLEEND) 
	{
		decrement_sockref_count(current_process->proc_fileinf->fhandles[s]);
		return 0;
	}
	else return ENOTSOCK;
}

#ifdef DEBUG
void dump_frame(struct eth_frame_rcv *eframe)
{
	char *buf = (char *) eframe;
	unsigned char c;
	int i;

	for (i=0; i<6; i++)
	{
		c = eframe->ethfrhdr_hwaddr_dest[i];
		printk("%3x", c);
	}
	for (i=0; i<6; i++)
	{
		c = eframe->ethfrhdr_hwaddr_src[i];
		printk("%3x", c);
	}
	for (i=0; i<8; i++)
	{
		c = buf[i + 12];
		printk("%3x", c);
	}

	printk("\nType = %x\n", eframe->ethfrhdr_type);

	for (i=0; i<48; i++)
		printk("%x", eframe->ethfr_data[i]);
	printk("\n");

}

void dump_packet(struct packet *pkt)
{
	printk("PKT: packet_id = %u src = (%u, %u) dest = (%u, %u) size = %d\n Data : ", pkt->pkt_header.pkt_id, pkt->pkt_header.pkt_src.saddr_host, pkt->pkt_header.pkt_src.saddr_port, pkt->pkt_header.pkt_dest.saddr_host, pkt->pkt_header.pkt_dest.saddr_port, pkt->pkt_rcvddata);

	printk("Dumping of the message is not modified\n");
}

void display_sockets(void)
{
	struct proc_sock *psock;
	struct packet *pkt;
	struct timerobject *t;

	psock = (struct proc_sock *)socklist_hdr;

	while (psock != NULL)
	{
		if (psock->psk_port == 10 ||psock->psk_port == 12 ||psock->psk_port == 13 ||psock->psk_port == 14 ||psock->psk_port == 15 ||psock->psk_port >= 100) 
		{
		printk("[ port: %d, busy: (%d, %d), sndpkt : %x txusing : %d rxpkts : %x] - ",psock->psk_port, psock->psk_busy.l_slq.sl_bitval, psock->psk_busy.l_val, psock->psk_sndpkt, psock->psk_transmitterusing, psock->psk_rcvpkt);
		if (psock->psk_sndpkt != NULL)
		{
			pkt = (struct packet *)psock->psk_sndpkt;
			printk(", pkt_status : %d lock : (%d, %d) ", pkt->pkt_status, pkt->pkt_lock.l_slq.sl_bitval, pkt->pkt_lock.l_val);
		}
		else
		{
		}
		if (psock->psk_rcvpkt != NULL)
		{
		}
		}
		psock = (struct proc_sock *)psock->psk_globalnext;
	}

	t = (struct timerobject *)timers;

	while (t != NULL)
	{
		printk("<Timers [%14s]: count : %d, timer : %x next : %x, prev : %x >",t->timer_ownerthread->kt_name, t->timer_count, t, t->timer_next, t->timer_prev);
		t = (struct timerobject *)t->timer_next;
	}
}
#endif

