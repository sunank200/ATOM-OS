#include<pthread.h>
#include "syscalls.h"

#define NN		5 * 1024 * 1024
#define MINIMUMSIZE	100
#define MAXSUBLISTS	(NN / MINIMUMSIZE)

int A[NN] __attribute__ ((aligned(4096)));
int templist[NN]__attribute__ ((aligned(4096)));
int count __attribute__ ((aligned(4096)));
int sort_complete __attribute__ ((aligned(4096)));
int P __attribute__ ((aligned(4096)));
int N;

int args[64] = {0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10, 11, 12, 13, 14, 15,
                16, 17, 18, 19 ,20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
                48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};

pthread_mutex_t mut;
pthread_cond_t cond;
pthread_mutex_t mut1;
pthread_cond_t barr;
pthread_barrier_t b;
int jcount;

void insertionsort(int start, int end)
{
	int i, j, val;

	//printf("Insertion sort : %d, %d\n", start, end);
	for (i=start+1; i<=end; i++)
	{
		val = A[i];
		j = i-1;
		while (j >= start && A[j] > val)
		{
			A[j+1] = A[j];
			j--;
		}
		A[j+1] = val;
	}
}

int partition(int start, int end)
{
	int i, j, pivotpos;
	int pivot = A[end];
	int done = 0, temp;

	//printf("partitionl : %d, %d\n", start, end);
	i = start;
	j = end - 1;
	while (!done)
	{
		while (i < j && A[i] < pivot) i++;
		while (i < j && A[j] > pivot) j--;
		if (i < j)
		{
			temp = A[i];
			A[i] = A[j];
			A[j] = temp;
		}
		else
		{
			if (A[i] > pivot)
			{
				A[end] = A[i];
				A[i] = pivot;
				pivotpos = i;
			}
			else pivotpos = end;
			done = 1;
		}
	}

	return pivotpos;	
}

void quicksort(int start, int end)
{
	int i, n, pivot;
	int lo, hi;

	if (end <= start) return; // Invalid list or single element

	//printf("Quick sort called : %d, %d\n",start,end);
	if ((end - start + 1) < MINIMUMSIZE)
	{
		insertionsort(start, end);
	}
	else
	{
		pivot = partition(start, end);
		if (pivot-1 > start)
			quicksort(start, pivot-1); // This list can be sorted by another thread.
		if (pivot+1 < end)
			quicksort(pivot+1, end);
	}
}

void merge(int start, int end, int end2)
{
	int i,j,k;

	i = start; j=end+1;
	k = start;

	printf("merging : %d, %d, %d\n", start, end, end2);
	while (i <= end && j <= end2)
	{
		if (A[i] > A[j])
		{
			templist[k] = A[j];
			j++;
		}
		else 
		{
			templist[k] = A[i];
			i++;
		}
		k++;
	}

	while (i <= end) templist[k++] = A[i++];
	while (j <= end2) templist[k++] = A[j++];
	// Copy back to their original locations.
	for (i=start; i<=end2; i++)
		A[i] = templist[i];
	
}

void *thread_func(void *arg)
{
	int thrno =*(int *)arg;
	int done = 0;
	int start, end, end2;
	int two_powers = 2;
	int chunksize = (N + P - 1) / P;
	int nextjcount = P;

	pthread_barrier_wait(&b);
	// Sorting of sub list
	start = (thrno * chunksize);
	end = (thrno + 1) * chunksize-1;
	//printf("thr : %d (%d, %d)\n",thrno, start, end);
	if (end > N-1) end = N-1;
	quicksort(start, end);

	do {
		//printf("THRNO : %d PHASE NO : 2 powers : %d nextjcount : %d ........................\n",thrno, two_powers,nextjcount);
		// barrier
		pthread_mutex_lock(&mut1);
		jcount++;

		if (jcount == nextjcount)
		{
			jcount = 0;
			pthread_cond_broadcast(&barr);
		}
		else pthread_cond_wait(&barr, &mut1);
		pthread_mutex_unlock(&mut1);
		
		printf("THRNO : %d COME OUT THE BARRIER : next jcount : %d ....................\n",thrno, nextjcount);	
		// Merging phase
		if (thrno % two_powers == 0) // This Merges and otherone waits
		{
			end2 = end + chunksize;
			if (end2 > N-1) end2 = N-1;
			merge(start, end, end2);
			end = end2;
			chunksize *= 2;
			two_powers *= 2;
			nextjcount /= 2;
			if (start == 0 && end == N-1)
			{
				pthread_mutex_lock(&mut);
				sort_complete = 1;
				printf(" THR NO : %d  WAKEUP ALL ........................\n", thrno);
				pthread_cond_broadcast(&cond);
				pthread_mutex_unlock(&mut);
			}
		}
		else
		{
			pthread_mutex_lock(&mut);
			if (sort_complete == 0)
			{
				//printf("THRNO : %d GOING TO SLEEP .......................\n", thrno);
				pthread_cond_wait(&cond, &mut);
				//printf("THRNO : %d GOING TO SLEEP AFTER.......................\n", thrno);
			}
			pthread_mutex_unlock(&mut);
		}
		//printf("Sort completed : %d\n",sort_complete);
	} while (!sort_complete);
	
		
	//pthread_exit(NULL);
	//return NULL;
}

int main(int argc, char **argv)
{
	int i,j, mcmodel;
	int t1, t2, t3, t4;
	int t;
	pthread_t thr[64];

	if (argc != 3)
	{
		printf("Usage : merge <NPROC> <NELEMENTS>\n");
		exit(0);
	}

	P = atoi(argv[1]);
	N = atoi(argv[2]);
	pthread_barrier_init(&b, NULL, P);
	pthread_mutex_init(&mut, NULL);
	pthread_mutex_init(&mut1, NULL);
	pthread_cond_init(&cond, NULL);
	pthread_cond_init(&barr, NULL);

	// Initialize and Shuffle
	for (i=0; i<N; i++)
		A[i] = i;

	j = 1137;

	for (i=0; i<N; i = i+3)
	{
		t = A[j];
		A[j] = A[i];
		A[i] = t;
		j = (j + 337) % N;
	}
	//insertionsort(N-1, N-1);

	// Create threads.	
	t3 = time(NULL);
	for (i=1; i<P; i++)
		pthread_create(&thr[i], NULL, thread_func, &args[i]);
	t1 = time(NULL);	
	thread_func(&args[0]);
	
	t2 = time(NULL);	
	for (i=1; i<P; i++)
		pthread_join(thr[i], NULL);
	t4 = time(NULL);
	printf("\nTime elapsed : creation %d, merge : %d, joining : %d\n",t1-t3, t2-t1, t4-t2);

	return 0;
}


