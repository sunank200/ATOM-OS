# CFLAGS:= -I ../include/ -c -O3
CFLAGS:= -I ../include -I /root/mpichinstall/include -c -march=i586 -mpc32

all:	seive reduct merge quiksort tsp tsp2 matmult seive2 

seive:	seive.o ../lib/lib.a
	ld -T userproc.lds -Map seive.map -o seive seive.o ../lib/lib.a

seive2:	seive2.o ../lib/lib.a
	ld -T userproc.lds -Map seive2.map -o seive2 seive2.o ../lib/lib.a

reduct:	reduct.o ../lib/lib.a
	ld -T userproc.lds -Map reduct.map -o reduct reduct.o ../lib/lib.a

merge:	merge.o ../lib/lib.a
	ld -T userproc.lds -Map merge.map -o merge merge.o ../lib/lib.a

quiksort:	quiksort.o ../lib/lib.a
	ld -T userproc.lds -Map quiksort.map -o quiksort quiksort.o ../lib/lib.a

tsp:	tsp.o ../lib/lib.a
	ld -T userproc.lds -Map tsp.map -o tsp tsp.o ../lib/lib.a

tsp2:	tsp2.o ../lib/lib.a
	ld -T userproc.lds -Map tsp2.map -o tsp2 tsp2.o ../lib/lib.a

matmult:	matmult.o ../lib/lib.a
	ld -T userproc.lds -Map matmult.map -o matmult matmult.o ../lib/lib.a

../lib/lib.a:
	make -C ../lib lib.a

seive.o:	seive.c
	gcc $(CFLAGS) seive.c

reduct.o:	reduct.c
	gcc $(CFLAGS) reduct.c

merge.o:	merge.c
	gcc $(CFLAGS) merge.c

quiksort.o:	quiksort.c
	gcc $(CFLAGS) quiksort.c

tsp.o:	tsp.c
	gcc $(CFLAGS) tsp.c

tsp2.o:	tsp2.c
	gcc $(CFLAGS) tsp2.c

matmult.o:	matmult.c
	gcc $(CFLAGS) matmult.c

clean:
	rm -f *.map *.o seive reduct merge quiksort tsp tsp2 matmult seive2 
