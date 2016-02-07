#ifndef __SETUP_H__
#define __SETUP_H__

#define CONFIG_DATA_SEGMENT	0x60
#define SYSDATA_SIGNATURE	0x00
#define	E820_FLAG		SYSDATA_SIGNATURE + 2
#define MEM_SEGMENT_DESC	E820_FLAG + 1

#define FD0_DATA_ADDR	(MEM_SEGMENT_DESC + 10 * 20) 
		/* 10 descriptors plus initial one byte code. */
#define FD1_DATA_ADDR	FD0_DATA_ADDR + 12
#define HD0_DATA_ADDR	FD1_DATA_ADDR + 12
#define HD1_DATA_ADDR	HD0_DATA_ADDR + 16
#define SECONDS		HD1_DATA_ADDR + 16
#define MINUTES		SECONDS + 1
#define HOURS		MINUTES + 1
#define DATE		HOURS + 1
#define MONTH		DATE + 1
#define YEAR		MONTH + 1
#define MEMSIZE_ADDR	YEAR + 1

#endif

