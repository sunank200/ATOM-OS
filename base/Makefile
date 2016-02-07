# definitions
ASM:=nasm
CC:=gcc
KERNEL:=kernel

LDFLAGS:=-T mkernel.lds -e _main -o $(KERNEL) -Map mkern.map --cref -static
CFLAGSUSER:= -I ../include -Wall -Wstrict-prototypes -march=i586 -mpc32 -fomit-frame-pointer -fno-strict-aliasing -fno-builtin -ffreestanding -nostdlib -nostdinc 
CFLAGSMAIN:=-I ../include -Wall -Wstrict-prototypes -march=i586 -mpc32 -fomit-frame-pointer -fno-strict-aliasing -fno-builtin -ffreestanding -nostdlib -nostdinc 
CFLAGS:=-I ../include -O4 -Wall -Wstrict-prototypes -march=i586 -mpc32 -fomit-frame-pointer -fno-strict-aliasing -fno-builtin -ffreestanding -nostdlib -nostdinc
AFLAGS:=-f elf

# Rules to build
all:	imgcopy lib samples IMAGE shell 
	cp fimage.orig fimage
	./bootcp
	./cpfile IMAGE IMAGE
	./cpfile shell shell
	./cpfile ../samples/quiksort quiksort
	./cpfile ../samples/reduct reduct
	./cpfile ../samples/matmult matmult
	./cpfile ../samples/seive seive
	./cpfile ../samples/seive2 seive2
	./cpfile ../samples/merge smerg
	./cpfile ../samples/tsp tsp
	./cpfile ../samples/tsp2 tsp2
	mv fimage ../bochsimg

imgcopy:
	make -C ../imgcopy

lib:
	make -C ../lib 

samples:
	make -C ../samples

IMAGE:	$(KERNEL) pmswitch1k sysdata1k
	cat sysdata1k pmswitch1k $(KERNEL) > IMAGE

shell: shell.o ../lib/lib.a
	$(LD) -T userproc.lds -Map shell.map -o shell shell.o ../lib/lib.a

$(KERNEL):	initboot.o intr_entry.o timer.o kbd.o string.o \
	vdisplay.o klib.o fs.o fat12.o lock.o procintr.o syscalls.o main.o \
	process.o fdc.o nic.o vmmap.o semaphore.o socket.o pthread.o

	$(LD) $(LDFLAGS) intr_entry.o timer.o kbd.o string.o \
	vdisplay.o klib.o fs.o fat12.o lock.o procintr.o syscalls.o main.o \
	process.o fdc.o nic.o vmmap.o semaphore.o socket.o pthread.o
	sort -k 17 mkern.map > mkern.sort

pmswitch1k:	pmswitch.o
	dd if=pmswitch.o of=pmswitch1k bs=1024 conv=sync

sysdata1k:	sysdata.o
	dd if=sysdata.o of=sysdata1k bs=1024 conv=sync

semaphore.o:	semaphore.c ../include/mklib.h ../include/process.h ../include/semaphore.h ../include/lock.h ../include/fs.h 
	$(CC) $(CFLAGS) -c -o semaphore.o semaphore.c

socket.o:	socket.c ../include/mklib.h ../include/process.h ../include/lock.h ../include/packet.h ../include/socket.h ../include/ne2k.h ../include/timer.h ../include/vmmap.h
	$(CC) $(CFLAGS) -c -o socket.o socket.c

vmmap.o:	vmmap.c ../include/vmmap.h ../include/mklib.h ../include/process.h 
	$(CC) $(CFLAGS) -c -o vmmap.o vmmap.c

nic.o:	nic.c ../include/ne2k.h ../include/ne2000.h ../include/lock.h ../include/mklib.h  ../include/packet.h ../include/process.h ../include/vmmap.h ../include/timer.h
	$(CC) $(CFLAGS) -c -o nic.o nic.c

fdc.o:	fdc.c ../include/mklib.h ../include/timer.h ../include/fdc.h ../include/process.h ../include/vmmap.h
	$(CC) $(CFLAGS) -c -o fdc.o fdc.c

syscalls.o:	syscalls.c ../include/syscalls.h ../include/mklib.h ../include/mtype.h ../include/process.h ../include/fs.h ../include/ne2k.h ../include/socket.h ../include/semaphore.h 
	$(CC) $(CFLAGS) -c -o syscalls.o syscalls.c

sysdata.o:	sysdata.asm ../include/setup.inc ../include/mconst.inc
	$(ASM) -f bin -o sysdata.o sysdata.asm

pmswitch.o:	pmswitch.asm 
	$(ASM) -f bin -o pmswitch.o pmswitch.asm

initboot.o:	initboot.asm ../include/setup.inc ../include/mconst.inc
	$(ASM) $(AFLAGS) -o initboot.o initboot.asm

intr_entry.o:	intr_entry.asm ../include/mconst.inc
	$(ASM) $(AFLAGS) -o intr_entry.o intr_entry.asm

kbd.o:	kbd.c ../include/mklib.h ../include/lock.h ../include/process.h ../include/timer.h ../include/ne2k.h
	$(CC) $(CFLAGS) -c -o kbd.o kbd.c

main.o:	main.c 
	$(CC) $(CFLAGSMAIN) -c -o main.o main.c

procintr.o:	procintr.c ../include/process.h ../include/mklib.h ../include/vmmap.h 
	$(CC) $(CFLAGSMAIN) -c -o procintr.o procintr.c

process.o:	process.c ../include/process.h ../include/lock.h ../include/mklib.h ../include/segment.h ../include/vmmap.h ../include/elf.h ../include/fs.h ../include/timer.h ../include/syscalls.h
	$(CC) $(CFLAGS) -fno-builtin-bzero -fno-builtin-strncmp -c -o process.o process.c

pthread.o:	pthread.c ../include/process.h ../include/lock.h ../include/mklib.h ../include/timer.h ../include/syscalls.h ../include/pthread.h
	$(CC) $(CFLAGS) -fno-builtin-bzero -fno-builtin-strncmp -c -o pthread.o pthread.c

timer.o:	timer.c ../include/timer.h ../include/process.h ../include/lock.h ../include/mklib.h
	$(CC) $(CFLAGS) -c -o timer.o timer.c

vdisplay.o:	vdisplay.c ../include/vdisplay.h ../include/mklib.h ../include/process.h ../include/stdarg.h
	$(CC) $(CFLAGS) -c -o vdisplay.o vdisplay.c

klib.o:	klib.asm ../include/mconst.inc
	$(ASM) $(AFLAGS) -o klib.o klib.asm

lock.o:	lock.c ../include/process.h ../include/lock.h ../include/mklib.h ../include/timer.h 
	$(CC) $(CFLAGS) -c -o lock.o lock.c

string.o:	string.c 
	$(CC) $(CFLAGS) -O3 -c -o string.o string.c

fs.o: fs.c ../include/fs.h ../include/mklib.h ../include/buffers.h ../include/process.h ../include/lock.h ../include/vmmap.h ../include/fat12.h ../include/fdc.h
	$(CC) $(CFLAGS) -c -o fs.o fs.c
	
fat12.o: fat12.c ../include/fat12.h ../include/fs.h ../include/process.h ../include/mklib.h ../include/buffers.h ../include/vmmap.h ../include/timer.h
	$(CC) $(CFLAGS) -c -o fat12.o fat12.c
	
shell.o:	shell.c ../include/uprocess.h ../include/syscalls.h 
	$(CC) $(CFLAGSUSER) -c -o shell.o shell.c

clean:
	rm -f *.o 
	rm -f sysdata1k pmswitch1k
	rm -f kernel IMAGE
	rm -f lib.a shell
	make -C ../lib clean
	make -C ../samples clean
	make -C ../imgcopy clean

