#include <pthread.h>
#include "syscalls.h"

#define NN 	0x400000
#define SQRTN	2048
int P;
int N;
int args[64] = {0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10, 11, 12, 13, 14, 15, 
		16, 17, 18, 19 ,20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
		32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};

int prime;
pthread_barrier_t barr;

void * strike_factors(void *arg)
{
	int l=*(int *)arg;
	int start,last;
	int lprime, i, incr;
	char *flags;

	start = (l*N)/P + 1;
	last = ((l+1) * N) / P;
	printf("Thread id : %d - start : %d - last : %d\n",l,start, last);
	// Allocate memory
	flags = (char *)malloc(last-start+1);
	for (i=0; i<N/P; i++) flags[i] = 0;	
	do {
		// Choose the next prime number
		if (l == 0) // First processor
		{
			prime++;
			while (flags[prime - start] == 1 && prime < N) prime++;
		}
		//printf("pthread barrier 1: %d\n",pthread_barrier_wait(&barr));
		pthread_barrier_wait(&barr);
		lprime = prime;
		printf("%d -- prime : %d\n",l, lprime);

		// Mark the flags;
		i = (start / lprime) * lprime;
		if (i < start) i = i + lprime;
		if (i == lprime) i = i + lprime;

		//printf("%d -- Starting to mark i = %d - prime = %d last : %d\n", l, i, lprime, last);
		while (i <= last)
		{
			flags[i-start] = 1;
			i += lprime;
		}
		//printf("pthread barrier 2: %d\n",pthread_barrier_wait(&barr));
		pthread_barrier_wait(&barr);
	} while (lprime <= SQRTN); // This is to be verified
	
	return NULL;
}


int main(int argc,char *argv[])
{
	pthread_t thr[64];
	int i, mcmodel;
	int t2, t1;

	if(argc!=3)
	{
		printf("Usage: seive <NOPROC> <NELEMENTS>\n");
		exit(0);
	}

	P=atoi(argv[1]);
	N=atoi(argv[2]);
	prime = 1;
	pthread_barrier_init(&barr, NULL, P);
	for(i=1; i<P; i++)
		pthread_create(&thr[i], NULL, strike_factors, &args[i]);

	t1 = time(NULL);
	strike_factors(&args[0]);
	t2 = time(NULL);

	for (i=1; i<P; i++)
		pthread_join(thr[i],NULL);
	
	printf("\nTime taken : %d\n", t2-t1);
	return 0;
}
