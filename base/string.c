
/////////////////////////////////////////////////////////////////////
// Copyright (C) 2008 Department of Computer SCience & Engineering
// National Institute of Technology - Warangal
// Andhra Pradesh 
// INDIA
// http://www.nitw.ac.in
//
// This software is developed for software based DSM system Project
// This is an experimental platform, freely useable and modifiable
// 
//
// Team Members:
// Prof. T. Ramesh 			Chapram Sudhakar
// A. Sundara Ramesh  			T. Santhosi
// K. Suresh 				G. Konda Reddy
// Vivek Kumar 				T. Vijaya Laxmi
// Angad 				Jhahnavi
//////////////////////////////////////////////////////////////////////

#define NULL	0
int strlen(const char *src);
char *strcpy(char *dest, const char *src)
{
	int i=0;
	if((dest==NULL)||(src==NULL)) 
		return dest;
	while(*(src+i))
	{	
		*(dest+i)=*(src+i);
		i++;
	}
	*(dest+i)=0;
	return dest;
  
}

char *strcat(char *dest, const char *src)
{
	int l,i;
	if(dest==NULL||src==NULL)
		return dest;
	l=strlen(src);
	dest+=strlen(dest);
	for(i=0;i<=l;i++)
		*(dest+i)=*(src+i);
	return dest;
}

int strcmp(const char *dest, const char *src)
{
	int i=0;
	if(dest==src)
		return 0;
	while((*(dest+i)==*(src+i))&&(*(dest+i)!=0)&&(*(src+i)!=0))
		i++;
	return *(dest+i)-*(src+i);
}

int strlen(const char *src)
{
	int i=0;
	while(*(src+i))  i++;
	return i; 
}

int strnlen(const char *src, int n)
{
	int i=0; 
	while( n && *(src+i)) { i++; n--; };
	return i; 
}

char *strncpy(char *dest, const char *src,unsigned int n)
{
	int i=0;
	if((dest==NULL)||(src==NULL)) 
		return dest;
	while((n-1) && *(src+i))
	{	
		*(dest+i)=*(src+i);
		i++;
		n--;
	}
	*(dest+i)=0;
	return dest;
}

char *strncat(char *dest, const char *src,unsigned int n)
{
	int l,i;
	if(dest==NULL||src==NULL)
		return dest;
	l=strlen(src);
	l = (l <= n) ? l : n;
	dest+=strlen(dest);
	for(i=0; i<=l; i++)
		*(dest+i)=*(src+i);
	return dest;
}

int strncmp(const char *dest,const char *src,unsigned int n)
{
	int i=0;
	if(dest==src)
		return 0;
	while((i<(n-1)) && (*(dest+i)==*(src+i))&&(*(dest+i)!=0)&&(*(src+i)!=0))
		i++;
	return *(dest+i)-*(src+i);
}

int toupper(int ch)
{
	if (ch >= 'a' && ch <= 'z') return ('A' + (ch - 'a'));
	else return ch;
}

int tolower(int ch)
{
	if (ch >= 'A' && ch <= 'Z') return ('a' + (ch - 'A'));
	else return ch;
}

int isspace(int ch)
{
	if (ch == ' ' || ch == '\t' || ch == '\n') return 1;
	else return 0;
}

int isalpha(int ch)
{
	if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) return 1;
	else return 0;
}

int isnum(int ch)
{
	if ((ch >= '0') && (ch <= '9')) return 1;
	else return 0;
}

int isdigit(int ch)
{
	if ((ch >= '0') && (ch <= '9')) return 1;
	else return 0;
}

int isxdigit(int ch)
{
	if (((ch >= '0') && (ch <= '9')) || ((ch >= 'a') && (ch <= 'f')) || ((ch >= 'A') && (ch <= 'F'))) return 1;
	else return 0;
}

char *strstr (register char *buf, register char *sub)
{
        register char *bp;
        register char *sp;

        if (!*sub)
                return buf;
        while (*buf)
        {
                bp = buf;
                sp = sub;
                do
                {
                        if (!*sp)
                                return buf;
                }
                while (*bp++ == *sp++);
                buf += 1;
        }
        return 0;
}

char *strchr(char *s, int c)
{
	for (;;)
	{
		if (*s == (char) c) return (char*)s;
		if (!*s++) return NULL;
	}
}

