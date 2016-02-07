

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


#ifndef	__SOCKET_H__
#define __SOCKET_H__

//#include "err.h"
#include "packet.h"
#include "process.h"

#define PF_INET		1
#define AF_INET		1
#define PF_UNIX		2
#define INADDR_ANY	0L

#define SOCK_UNUSED	0
#define SOCK_STREAM	1
#define SOCK_DGRAM	2

#define __SOCK_SIZE__	16

#define PROCSOCK_DEFAULT        0x1
#define PROCSOCK_USER           0x2

struct proc_sock {
	unsigned short psk_type;
	int psk_port;
	volatile short psk_refcount;

	volatile unsigned int psk_sndseqid;
	volatile unsigned int psk_rcvseqid;
	struct lock_t psk_rcvpktlock;
	volatile struct packet *psk_rcvpkt;
	volatile struct packet *psk_sndpkt;
	struct isockaddr psk_peer; // For stream sockets

	short int  psk_transmitterusing;
	struct lock_t psk_busy;	// Protects psk_transmitterusing, psk_sndpkt, 
				// psk_delrequest and pkt_sendcomplete event
	int psk_delrequest;
	struct event_t psk_rcvqempty;
	struct event_t psk_rcvdevent;
	volatile struct proc_sock *psk_globalnext;
};

struct in_addr {
	unsigned int s_addr;
};

struct sockaddr_in {
        short int     		sin_family;
        unsigned short int      sin_port;
        struct in_addr          sin_addr;
        unsigned char 		__pad[__SOCK_SIZE__ - sizeof(short int) - sizeof(unsigned short int) - sizeof(struct in_addr)];
};
                                                                                
struct sockaddr {
        unsigned short int	sa_family;
        unsigned char		sa_data[14];
};
	
int bind(int sockfd, struct sockaddr *myaddr, socklen_t addrlen);
int socket(int family, int type, int protocol);
int recv(int s, void *buf, size_t len, int flags);
int recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
int send(int s, const void *buf, size_t len, int flags);
int sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);

void initialize_hash_table(void);
void cleanHashTable(void);
void add_hash_node(int host,int port, int type, int packet_id);
void seqno_init(int host, int port, int socktype, int pkt_id);
int remove_pkt(void *arg);
int deliver_packet(int ind);
int pktindex(struct eth_frame_rcv *ef);
void wakeup_transmitter(void *arg);
void delete_socket(struct proc_sock *psock);

unsigned short checksum(char *buf1, int len1, char *buf2, int len2, char *buf3, int len3);
void free_packet(struct packet *pkt);
struct proc_sock * locate_socket(int portno, int type);
void purge_seqno_hashtable(int before);
void seqno_init(int host, int port, int type, int pkt_id);
void init_socktable(void);

int syscall_broadcast(int portno, char *buf, int len);
int syscall_tsendto(short dest_threadid, char *buf, unsigned int len, int groupid, int tag, short nb);
int local_broadcast(struct isockaddr * destsock, struct isockaddr *srcsock, char *buf, unsigned int len, int pkt_seqno, int groupid, int tag);
int internal_tsend(struct isockaddr * destsock, struct isockaddr *srcsock, char *buf, unsigned int len, int groupid, int tag, short nb);
int sock_broadcast(char *buf, int len, short nb);
void pkttransmitter(void);
void delete_socket(struct proc_sock *psock);
void send_ack(int type,struct eth_frame_rcv *ef, unsigned short expect_winseqno, unsigned int offset);
int remove_frames_fromtxq(int pkt_id, int offset, int srcport, int srctype, int pkttype);
void process_ack(struct packet *pkt, struct eth_frame_rcv *rcvd_eth_frame);
void disp_debuginfo(struct process *proc);
void packet_type_data(struct frame_buffer *rcvd_frame_buf);
void assemble(void);
int deliver_packet(int ind);
int trecvfrom_internal(char buf[], int maxlen, short *from, int nb, int pkttype, int groupid, int *tag, int wmsec);
int syscall_trecvfrom(char buf[], int maxlen, short *from, int nb);
int syscall_socket(int family, int type, int protocol);
int syscall_bind(int sockfd, struct sockaddr *myaddr, socklen_t addrlen);
int recv_fromsock_stream(struct proc_sock *psk, char *buf, int maxlen, int flags);
int recv_fromsock_dgram(struct proc_sock *psk, struct isockaddr *from, char *buf, int maxlen, int flags);
int syscall_recv(int s, void *buf, size_t len, int flags);
int syscall_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
int sock_send(struct proc_sock *psock, struct isockaddr *dest, const char *buf, unsigned int len, int pkttype, int groupid, int tag, int nb);
int syscall_send(int s, const void *buf, size_t len, int flags);
int syscall_sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
int syscall_connect(int s, const struct sockaddr *serv_addr, socklen_t addrlen);
int syscall_accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int syscall_listen(int s, int backlog);
int syscall_close_socket(int s);

#define MSG_DONTWAIT	0x0001
#define MSG_TRUNC	0x0002
#define MSG_PEEK	0x0004

#endif
