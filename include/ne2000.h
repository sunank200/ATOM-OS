
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


#ifndef __NE2000_H__
#define __NE2000_H__
#include "ne2k.h"
/* internal implementation procedures */
static int nic_probe(int addr);
static int nic_dump_prom(snic *nic, unsigned char *prom);
static void nic_rx(snic *nic);
static void nic_get_header(snic *nic, unsigned int page, nicframe_header *header);
static int nic_send(snic *nic);
static void nic_block_input(snic *nic, unsigned char *buf, unsigned int len, unsigned int offset);
static void nic_block_output(snic *nic, frame_buffer *pkt);

#define PSTART			0x20	/* if NE2000 is byte length */
#define PSTOP			0x40
#define PSTARTW			0x40	/* if NE2000 is wordlength */
#define PSTOPW			0xC0
#define MAX_LOAD		12	/* maximum services per IRQ request*/
#define MAX_RX			10	/* maximum packets recieve per call*/
#define TRANSMITBUFFER		0x40

/* DP8390 NIC Registers*/
#define COMMAND			0x00
#define STATUS			COMMAND+0
#define PHYSICAL		COMMAND+1	/* page 1 */
#define MULTICAST		COMMAND+8	/* page 1 */
#define	PAGESTART		COMMAND+1	/* page 0 */
#define PAGESTOP		COMMAND+2
#define BOUNDARY		COMMAND+3
#define TRANSMITSTATUS		COMMAND+4
#define TRANSMITPAGE		COMMAND+4
#define TRANSMITBYTECOUNT0	COMMAND+5
#define NCR			COMMAND+5
#define TRANSMITBYTECOUNT1	COMMAND+6
#define INTERRUPTSTATUS		COMMAND+7
#define CURRENT			COMMAND+7	/* page 1 */
#define REMOTESTARTADDRESS0	COMMAND+8
#define CRDMA0			COMMAND+8
#define REMOTESTARTADDRESS1	COMMAND+9
#define CRDMA1			COMMAND+9
#define REMOTEBYTECOUNT0	COMMAND+10	/* how many bytes we will */
#define REMOTEBYTECOUNT1	COMMAND+11	/* read through remote DMA->IO */
#define RECEIVESTATUS		COMMAND+12
#define RECEIVECONFIGURATION	COMMAND+12
#define TRANSMITCONFIGURATION	COMMAND+13
#define FAE_TALLY		COMMAND+13	/* page 0 */
#define DATACONFIGURATION	COMMAND+14
#define CRC_TALLY		COMMAND+14
#define INTERRUPTMASK		COMMAND+15
#define MISS_PKT_TALLY		COMMAND+15

/* NE2000 specific implementation registers */
#define NE_RESET	0x1f	/* Reset */
#define NE_DATA		0x10	/* Data port (use for PROM) */

#define PAR0			COMMAND+1
#define PAR1			COMMAND+2
#define PAR2			COMMAND+3
#define PAR3			COMMAND+4
#define PAR4			COMMAND+5
#define PAR5			COMMAND+6

/* NIC Commands */
#define NIC_STOP	0x01	/* STOP */
#define NIC_START	0x02	/* START */
#define NIC_PAGE0	0x00
#define	NIC_PAGE1	0x40
#define NIC_PAGE2	0x80
#define NIC_TRANSMIT	0x04	/* Transmit a frame */
#define NIC_REM_READ	0x08	/* Remote Read */
#define NIC_REM_WRITE	0x10	/* Remote Write */
#define NIC_DMA_DISABLE	0x20	/* Disable DMA */

/* Data Configuration Register */
#define DCR_WTS		0x01	/* Word Transfer Select (0=byte, 1=word) */
#define DCR_BOS		0x02	/* Byte Order Select (0=big-endian) */
#define DCR_LAS		0x04	/* Long Address Select (0=dual 16-bit DMA) */
#define	DCR_LS		0x08	/* Loopback Select (0=loopback) */
#define DCR_AR		0x10	/* Auto Initialize Remote */
#define DCR_FT		0x60	/* (FT0 & FT1) FIFO Threshhold (see datasheet) */
//#define DCR_DEFAULT	0x58	/* Standard value for the DCR register */
#define DCR_DEFAULT	0x48	/* don't use Automatic send packet */
#define DCR_DEFAULT_WORD 0x49	/* defuault with wold length transfer */

/* Recieve Configure Register */
#define RCR_SEP		0x01	/* Save Errored Packets */
#define RCR_AR		0x02	/* Accept Runt Packets */
#define RCR_AB		0x04	/* Accept Broadcast */
#define RCR_AM		0x08	/* Accept Multicast */
#define RCR_PRO		0x10	/* Promiscuous Physical */
#define RCR_MON		0x20	/* Monitor Mode */
//#define RCR_DEFAULT	0x00	/* Standard value for the RCR register */
#define RCR_DEFAULT	0x0c	/* Accept Broadcast/Multicast Packets */

/* Recieve Status Register */
/* note, this is also stored in the status byte in the buffer header. */
/* That's the 4 byte entry in the local buffer, not the packet header. */
#define RSR_PRX		0x01	/* Pakcet Received Intact */
#define RSR_CRC		0x02	/* CRC Error */
#define RSR_FAE		0x04	/* Frame Alignment Error */
#define RSR_FO		0x08	/* FIFO Overrun */
#define RSR_MPA		0x10	/* Missed Packet */
#define RSR_PHY		0x20	/* Physical/Multicast Address (0=physical) */
#define RSR_DIS		0x40	/* Receiver Disabled */
#define RSR_DFR		0x80	/* Deferring */

/* Transmit Configure Register */
#define TCR_CRC		0x01	/* Inhibit CRC (0=CRC active) */
#define TCR_LB		0x06	/* (LB0 & LB1) Encoded Loopback Control */
#define TCR_ATD		0x08	/* Auto Transmit Disable (0=normal) */
#define TCR_OFST	0x10	/* Collision Offset Enable (1=low priority) */
#define TCR_DEFAULT	0x00	/* Standard value for the TCR register */
#define TCR_INTERNAL_LOOPBACK 0x02 /* Internal loopback configuration */

/* Transmit Status Register */
#define TSR_PTX		0x01	/* Packet Transmitted */
#define TSR_ND		0x02	/* Non-Deferral (Documented???) */
#define TSR_COL		0x04	/* Transmit Collided */
#define TSR_ABT		0x08	/* Transmit Aborted */
#define TSR_CRS		0x10	/* Carrier Sense Lost */
#define TSR_FU		0x20	/* FIFO Underrun */
#define TSR_CDH		0x40	/* CD Heartbeat */
#define TSR_OWC		0x80	/* Oout of Window Collision */

/* Interrupt Status Register */
#define ISR_PRX		0x01	/* Packet Received */
#define ISR_PTX		0x02	/* Packet Transmitted */
#define ISR_RXE		0x04	/* Receive Error */
#define ISR_TXE		0x08	/* Transmit Error */
#define ISR_OVW		0x10	/* Overwrite Warning */
#define ISR_CNT		0x20	/* Counter Overflow */
#define ISR_RDC		0x40	/* Remote DMA Complete */
#define ISR_RST		0x80	/* Reset Status */
#define ISR_DEFAULT	0x00	/* Standard value for the ISR register */
#define ISR_ALL		0x3f	/* The services that we handle in the isr */

/* Interrupt Mask Register */
#define IMR_PRXE	0x01	/* Packet Received Interrupt Enable */
#define IMR_PTXE	0x02	/* Packet Transmitted Interrupt Enable */
#define IMR_RXEE	0x04	/* Receive Error Interrupt Enable */
#define IMR_TXEE	0x08	/* Transmit Error Interrupt Enable */
#define IMR_OVWE	0x10	/* Overwrite Warning Interrupt Enable */
#define IMR_CNTE	0x20	/* Counter Overflow Interrupt Enable */
#define IMR_RDCE	0x40	/* DMA Complete Interrupt Enable */
#define IMR_DEFAULT	0x3f	/* CNTE | OVWE | TXEE | RXEE | PTXE | PRXE */

#endif

