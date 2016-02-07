#include "mklib.h"
#include "olib.h"

#define RAND_MAX (2147483647 - 1)
static unsigned int next = 1;

void srand48(long int seedval);

int rand_r(unsigned int *seed)
{
        *seed = *seed * 1103515245 + 12345;
        return (*seed % ((int)RAND_MAX + 1));
}

int rand(void)
{
        return (rand_r(&next));
}

long int random(void)
{
	return (rand_r(&next));
}

void srandom(unsigned int seed)
{
	srand48(seed);
}

void srand(long int seedval)
{
	srand48(seedval);
}

void srand48(long int seedval)
{
	next = seedval;
}

double drand48(void)
{
        unsigned int x = (rand() << 16) | rand();
	return ((double)x / (double)0xffffffff);
}

long lrand48(void)
{
        unsigned int x = (rand() << 16) | rand();
	return x;
}

