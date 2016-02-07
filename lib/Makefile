# definitions
ASM:=nasm
CC:=gcc

CFLAGSMAIN:=-I ../include -Wall -Wstrict-prototypes -march=i586 -mpc32 -fomit-frame-pointer -fno-strict-aliasing -fno-builtin -ffreestanding -nostdlib -nostdinc 
CFLAGS:=-I ../include -Wall -Wstrict-prototypes -march=i586 -mpc32 -fomit-frame-pointer -fno-strict-aliasing -fno-builtin -ffreestanding -nostdlib -nostdinc
AFLAGS:=-f elf

# Rules to build
all:	lib.a 

lib.a:	datetime.o drand48.o getopt.o input.o klib.o library.o malloc.o scanf1.o string.o strtod.o syscallwrap.o syscallib.o vsprintf.o pow.o mathfunc.o strtol.o 

	ar cr lib.a datetime.o drand48.o getopt.o input.o klib.o library.o malloc.o scanf1.o string.o strtod.o syscallwrap.o syscallib.o vsprintf.o pow.o mathfunc.o strtol.o 

klib.o:	klib.asm
	$(ASM) $(AFLAGS) -o klib.o klib.asm

pow.o:	pow.asm
	$(ASM) $(AFLAGS) -o pow.o pow.asm

mathfunc.o:	mathfunc.c
	$(CC) $(CFLAGSMAIN) -c -o mathfunc.o mathfunc.c
	
strtol.o:	strtol.c
	$(CC) $(CFLAGSMAIN) -c -o strtol.o strtol.c
	
string.o:	string.c 
	$(CC) $(CFLAGSMAIN) -c -o string.o string.c

getopt.o: getopt.c 
	$(CC) $(CFLAGSMAIN) -c -o getopt.o getopt.c

syscallwrap.o:	syscallwrap.c ../include/semaphore.h ../include/syscalls.h ../include/fs.h 
	$(CC) $(CFLAGSMAIN) -c -o syscallwrap.o syscallwrap.c

malloc.o:	malloc.c ../include/malloc.h 
	$(CC) $(CFLAGSMAIN) -c -o malloc.o malloc.c

drand48.o:	drand48.c 
	$(CC) $(CFLAGSMAIN) -c -o drand48.o drand48.c

syscallib.o:	syscallib.asm
	$(ASM) $(AFLAGS) -o syscallib.o syscallib.asm

datetime.o:	datetime.c 
	$(CC) $(CFLAGSMAIN) -c -o datetime.o datetime.c

input.o:	input.c 
	$(CC) $(CFLAGSMAIN) -c -o input.o input.c

library.o:	library.c 
	$(CC) $(CFLAGSMAIN) -c -o library.o library.c

readline.o:	readline.c 
	$(CC) $(CFLAGSMAIN) -c -o readline.o readline.c

scanf1.o:	scanf1.c 
	$(CC) $(CFLAGSMAIN) -c -o scanf1.o scanf1.c

strtod.o:	strtod.c 
	$(CC) $(CFLAGSMAIN) -c -o strtod.o strtod.c

vsprintf.o:	vsprintf.c 
	$(CC) $(CFLAGSMAIN) -c -o vsprintf.o vsprintf.c

clean:
	rm -f *.o lib.a *.map *.lst


