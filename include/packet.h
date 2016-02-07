

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


#ifndef	__PACKET_H__

#define	__PACKET_H__

#define MAX_FRDATA      1500
#define MIN_FRDATA	46
#define MAX_PROTOCOL_FRSIZE	1438 // Should be changed whenever pkt header size changes
#define MIN_PROTOCOL_FRSIZE	4 // Should be changed whenever pkt header size changes
#define MAX_FRSIZE	1518
#define MIN_FRSIZE	64


#define PROTOTYPE_TEMP	0x1111
#define PKTTYPE_DATA    1
#define PKTTYPE_ACK 	2
#define PKTTYPE_ERROR	3

#define PKTTYPE_CONNECTREQ	5
#define PKTTYPE_CONNECTREP      6
#define PKTTYPE_CONNECTCLOSE	7
#define PKTTYPE_PKTSEQNOINIT	8
#define PKTTYPE_SYNCH		9
#define PKTTYPE_REXEC		10
#define PKTTYPE_MCCONSISTENCY	11

#define PKTSTATUS_RCVCOMPLETED	1
#define PKTSTATUS_RECEIVING	2
#define PKTSTATUS_DELIVERED	3
#define PKTSTATUS_CREATING	4
#define PKTSTATUS_READYTOTXMT	5
#define PKTSTATUS_WAITINGACK	6
#define PKTSTATUS_RESTARTTXMT	7
#define PKTSTATUS_CONTINUENEXTWINDOW	8
#define PKTSTATUS_TXCOMPLETE	9


struct procid	{
	unsigned short prid_taskno;
	short prid_procno;
};

struct isockaddr {
	int saddr_type;
	int saddr_host;
	int saddr_port;
};

struct eth_frame_header {
        unsigned char ethfrhdr_hwaddr_dest[6], ethfrhdr_hwaddr_src[6];
        unsigned short ethfrhdr_type;
	unsigned short ethfrhdr_seqno;
};

struct packet_header {
	unsigned short pkt_type;
	struct isockaddr pkt_dest, pkt_src; 
        unsigned short pkt_frame_length;
        unsigned int pkt_offset;
	unsigned int pkt_length;
	unsigned int pkt_id;
	unsigned int pkt_tag; // For MPI
	unsigned int pkt_groupid; // For MPI
	unsigned short pkt_winseq_no;
	unsigned int pkt_srcsockctime;
};

struct eth_frame_rcv {
//	struct eth_frame_header ethfr_header;
        unsigned char ethfrhdr_hwaddr_dest[6], ethfrhdr_hwaddr_src[6];
        unsigned short ethfrhdr_type;
	unsigned short ethfrhdr_seqno;
//	struct packet_header ethfr_pheader;
	unsigned short pkt_checksum;
	unsigned short pkt_type;
        unsigned short pkt_frame_length;
	unsigned short pkt_winseq_no;
//	struct isockaddr pkt_dest, pkt_src; 
	int	pkt_dest_saddr_type;
	int	pkt_dest_saddr_host;
	int 	pkt_dest_saddr_port;
	int	pkt_src_saddr_type;
	int	pkt_src_saddr_host;
	int 	pkt_src_saddr_port;
        unsigned int pkt_offset;
	unsigned int pkt_length;
	unsigned int pkt_id;
	unsigned int pkt_tag; // For MPI
	unsigned int pkt_groupid; // For MPI
	unsigned int pkt_srcsockctime;
        unsigned char ethfr_data[MAX_PROTOCOL_FRSIZE];
};

struct connect_request {
	int conreq_type;
	struct isockaddr conreq_dest;
	struct isockaddr conreq_src;
};
struct eth_frame_snd {
        unsigned char ethfrhdr_hwaddr_dest[6], ethfrhdr_hwaddr_src[6];
        unsigned short ethfrhdr_type;
	unsigned short ethfrhdr_seqno;
	unsigned short pkt_checksum;
	unsigned short pkt_type;
        unsigned short pkt_frame_length;
	unsigned short pkt_winseq_no;
	int	pkt_dest_saddr_type;
	int	pkt_dest_saddr_host;
	int 	pkt_dest_saddr_port;
	int	pkt_src_saddr_type;
	int	pkt_src_saddr_host;
	int 	pkt_src_saddr_port;
        unsigned int pkt_offset;
	unsigned int pkt_length;
	unsigned int pkt_id;
	unsigned int pkt_tag; // For MPI
	unsigned int pkt_groupid; // For MPI
	unsigned int pkt_srcsockctime;
};

#define FRAMEDATA_OFFSET sizeof(struct eth_frame_snd)
struct ack_frame {
//	struct eth_frame_header ethfr_header;
        unsigned char ethfrhdr_hwaddr_dest[6], ethfrhdr_hwaddr_src[6];
        unsigned short ethfrhdr_type;
	unsigned short ethfrhdr_seqno;
//	struct packet_header ethfr_pheader;
	unsigned short pkt_checksum;
	unsigned short pkt_type;
        unsigned short pkt_frame_length;
	unsigned short pkt_winseq_no;
	int 	pkt_dest_saddr_type;
	int	pkt_dest_saddr_host;
	int 	pkt_dest_saddr_port;
	int	pkt_src_saddr_type;
	int	pkt_src_saddr_host;
	int 	pkt_src_saddr_port;
        unsigned int pkt_offset;
	unsigned int pkt_length;
	unsigned int pkt_id;
	unsigned int pkt_tag; // For MPI
	unsigned int pkt_groupid; // For MPI
	unsigned int pkt_srcsockctime;
	char padding[10];	// To make it minimum size frame
};

#ifndef TIMEROBJECT
#define TIMEROBJECT
struct timerobject {
	volatile long timer_count;
	int (* timer_handler)(void *);
	void *timer_handlerparam;
	struct kthread *timer_ownerthread;         
	volatile struct timerobject *timer_next, *timer_prev;
	struct event_t timer_event;
};
#endif

struct packet {
	struct packet_header pkt_header;
	char *pkt_data;
	volatile struct frame_buffer *pkt_rcvframe_head;
	volatile struct frame_buffer *pkt_rcvframe_tail;
	volatile unsigned short pkt_expc_seqno;
	volatile unsigned int pkt_rcv_offset;
	volatile unsigned int pkt_rcvddata;
	volatile unsigned int pkt_ackdata;
	volatile short pkt_status;
	volatile short pkt_retries;
	volatile short pkt_swinstatus;
	short pkt_nonblocking;
	volatile unsigned int pkt_wincur_pos;
	volatile unsigned int pkt_cur_pos;
	volatile unsigned short pkt_window_size;
	volatile unsigned short pkt_current_frame; 
	volatile unsigned short pkt_start_frame;
	struct timerobject pkt_tobj;
	struct lock_t pkt_lock;
	struct event_t pkt_windowcomplete;
	struct event_t pkt_sendcomplete;
	volatile struct packet *pkt_next; // To be used in the process's packet queue.
};

#endif

