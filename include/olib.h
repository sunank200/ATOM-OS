#ifndef __OLIB_H__

#define __OLIB_H__
#include "mtype.h"
#include "stdarg.h"
int input(FILE *stream, const char *format, va_list arglist);
int scanf(const char *fmt, ...);
int fscanf(FILE *stream, const char *fmt, ...);

double modf(double x, double *iptr);
double floor(double x);
double pow(double x, double y);
double ylog2x(double x, double y);
double log(double x);
double ldexp(double y, int exp);
double frexp(double x, int *exp); // Fully in asm
double exp(double arg);
float sqrt(float number);
double atan2(double arg1,double arg2);
double atan(double arg);
double asin(double arg) ;
double acos(double arg) ;
double cos(double arg);
double sin(double arg);
double tan(double arg);

int vfscanf (FILE *const file, const char *const format, va_list ap);
int vsscanf (const char *string, const char *format, va_list ap);
int sscanf (const char *string, const char *format, ...);

void srand48(long int seedval);
int rand_r(unsigned int *seed);
int rand(void);
long int random(void);
void srandom(unsigned int seed);
void srand(long int seedval);
void srand48(long int seedval);
double drand48(void);
long lrand48(void);

char * gets(char *s);
char * fgets(char *s, int size, FILE *stream);
int getc(FILE *stream);
int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);
int getcharf(int fd);
int getchar(void);
int ungetcharf(int c, int fd);
int getscanchar(void);
void setcursor(int x, int y);
int puts(char *str);
int putchar(char c);
void exit(int status);

FILE *fopen(char *fname, char *mode);
int fclose(FILE *f);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
unsigned long inet_addr(char *ipstr);
void err_dump(char *mesg);
short htons(short val);
long htonl(long val);
long htonl(long val);
short htons(short val);
void err_dump(char *mesg);
unsigned long inet_addr(char *ipstr);
void perror(const char *str);
int fork(void);


void *memcpy(void *dest,const void *src,size_t size);
void *memmove(void *dest,const void *src,size_t size);
int readline(char *buf, int size);

int getopt(int argc, char **argv, char *opts);
long int strtol(const char *nptr,char **endptr,int base);
unsigned long int strtoul(const char *nptr,char **endptr,int base);

int sscanf(const char *buf, const char *fmt, ...);

double strtod(const char *str, char **endptr);
double atof(const char *str);
long atol(const char *str);
int atoi(const char *str);


char *ecvtbuf(double arg, int ndigits, int *decpt, int *sign, char *buf);
char *fcvtbuf(double arg, int ndigits, int *decpt, int *sign, char *buf);
int vsprintf(char *buf, const char *fmt, va_list args);

int sprintf(char *buf, const char *fmt, ...);
int printf(const char * fmt, ...);

#endif

