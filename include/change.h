
#ifndef __CHANGE_H__

#define __CHANGE_H__

#define fprintf(stderr, ...)	printf(__VA_ARGS__)
#define fflush(stream)		;

#define stringof(arg)		"arg"
#define assert(arg)	{ if (!(arg)) { printf(stringof(Assertion ## arg ## failed\n )); exit(0); } }

#define fputs(str, stream)	puts(str)
#define fputc(c, stream) putchar(c)

#define strerror(err) stringof(Error ## No ## err ## xx\n)
#define NULL	0x0
#define EOF	-1


typedef int FILE;

extern FILE stdstreams[3];

#define stdin	&(stdstreams[0])
#define stdout	&(stdstreams[1])
#define stderr	&(stdstreams[2])

struct utsname {
	char sysname[20];
	char nodename[20];
	char release[20];
	char version[20];
	char machine[20];
	char domainname[20];
};
#endif

