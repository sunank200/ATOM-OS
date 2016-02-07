# definitions
ASM:=nasm
CC:=gcc

LDFLAGS:=-T mkernel.lds -e _main -o $(KERNEL) -Map mkern.map 
CFLAGS:= -Wall -Wstrict-prototypes -march=i686 -fomit-frame-pointer -fno-strict-aliasing 
AFLAGS:=-f elf

# Rules to build
all:	cpfile 

cpfile:	main.o buffers.o datetime.o fs.o fscalls.o 
	$(CC) -o cpfile main.o buffers.o datetime.o fs.o fscalls.o 
	cp	cpfile	../base/

main.o:	main.c
	$(CC) $(CFLAGS) -c -o main.o main.c

fs.o: fs.c ../include/fs.h
	$(CC) $(CFLAGS) -c -o fs.o fs.c
	
datetime.o: datetime.c
	$(CC) $(CFLAGS) -c -o datetime.o datetime.c

fscalls.o: fscalls.c ../include/fscalls.h
	$(CC) $(CFLAGS) -c -o fscalls.o fscalls.c
	
buffers.o: buffers.c ../include/buffers.h
	$(CC) $(CFLAGS) -c -o buffers.o buffers.c
	

clean:
	rm -f *.o cpfile

