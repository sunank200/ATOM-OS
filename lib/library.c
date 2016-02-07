#include "syscalls.h"
#include "mklib.h"
#include "change.h"
#include "olib.h"

void *malloc(unsigned int size);
void free(void *);

// Unsafe
int getc(FILE *stream)
{
	int fd;
	char c;

	fd = *stream;
	c = getcharf(fd);
	return c;	
}
int fgetc(FILE *stream)
{
	return getc(stream);
}
int ungetc(int c, FILE *stream)
{
	int fd;

	fd = *stream;
	return ungetcharf(c, fd);
}

char * gets(char *s)
{
	int i=0;
	while (((s[i] = getchar()) != EOF) && (s[i] != '\n') && (s[i] != '\r'))
		i++;
	s[i] = 0;
	return s;
}

char * fgets(char *s, int size, FILE *stream)
{
	int i=0;
	while ((i < (size - 1)) && ((s[i] = fgetc(stream)) != EOF) && (s[i] != '\n') && (s[i] != '\r'))
		i++;
	s[i] = 0;
	return s;
}
FILE *fopen(char *fname, char *mode)
{
	int imode = 0;
	int fd;
	FILE *f;

	if (strcmp(mode, "w") == 0) imode = O_CREAT | O_TRUNC;
	if ((strcmp(mode, "r+") == 0) || (strcmp(mode, "rw") == 0) || (strcmp(mode, "w+") == 0)) imode = O_RDWR;
	if (strcmp(mode, "r") == 0) imode = O_RDONLY;
	if (strcmp(mode, "a") == 0) imode = O_APPEND;
	
	if ((fd = open(fname, imode, 0)) >= 0)
	{
		f = (FILE *)malloc(sizeof(FILE));	
		*f = fd;
		return f;
	}
	else return NULL;
}

int fclose(FILE *f)
{
	int fd;

	fd = *f;
	if (fd >= 0)
		return close(fd);

	free(f);		
	return 0;
}
void *memcpy(void *dest, const void *src, size_t n)
{
	if (dest != NULL)
		bcopy((char *)src, (char *)dest, n);
	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
	return memcpy(dest, src, n);
}
unsigned long inet_addr(char *ipstr);
void err_dump(char *mesg);
short htons(short val);
long htonl(long val);


int readline(char s[], int n)
{
	int i, j, max_i, cursor_x;
	int int_c;
	char c;

	i=0;
	max_i=0;
	cursor_x = 2;
	
	while( (i < n) && ((int_c = getscanchar()) != 0x1C0D))
	{
		setcursor(cursor_x, -1);

		c=(int_c) & 0xff;
		if(c == 0)	//special symbols
		{
			c = (int_c & 0xffff) >> 8;
			switch(c)
			{
			case 0x47:	//home
				if (i > max_i)
					max_i = i;
				cursor_x = 2;
					i=0;
				setcursor(cursor_x, -1);
				break;
			case 0x4f:	//end
				i= max_i;
				cursor_x = 2;
				for (j=0; j<max_i; j++)
					if (s[j] != '\t')
						cursor_x++;
					else
						cursor_x = (cursor_x / 8) * 8 + 8;
				
				setcursor(cursor_x, -1);
				break;
			case 0x4B:	//left arrow
				if(i > 0)
				{
					if(i > max_i)
						max_i = i;
					if (s[i] != '\t')
						cursor_x--;
					else
					{
						cursor_x = 2;
						for (j=0; j<i; j++)
							if (s[j] != '\t')
								cursor_x++;
							else
								cursor_x = (cursor_x / 8) * 8 + 8;
					}
					i--;
					setcursor( cursor_x , -1);
				}
				break;
			case 0x4D:	//right arrow
				if(i<max_i)
				{
					i++;
		
					if (s[i] != '\t')
						cursor_x++;
					else
						cursor_x = (cursor_x / 8) * 8 + 8;
					setcursor( cursor_x, -1);
				}
				break;
			}
		}
		else
		{
			if( c== 0x08)	//back space
			{
				if(i > 0)
				{
					i--;
					cursor_x = 2;
					for (j=0; j<i; j++)
						if (s[j] != '\t')
							cursor_x++;
						else
							cursor_x = (cursor_x / 8) * 8 + 8;
					if (i < max_i)
					{
						for (j=i; j<max_i; j++)
							s[j] = s[j+1];
						s[max_i] = 0 ;
						max_i--;
						setcursor(2,-1);
						puts(s);
					}
					else
					{
						s[i] = ' ';
						setcursor(cursor_x, -1);
						putchar(' ');
					}
					setcursor(cursor_x, -1);
				}
			}
			else
			{
				s[i++]=c;
				if (c == '\t')
				{
					cursor_x = (cursor_x / 8) * 8 + 8;
					setcursor(cursor_x, -1);
				}
				else
				{
					putchar(c);
					cursor_x++;
				}
			}
		}	
	}
	putchar('\r');
	if(i > max_i)
	{
		max_i = i;
	}
	// remove traling blanks
	while (s[max_i-1] == ' ' || s[max_i-1] == '\t') max_i--;
	
	s[max_i]=0;
	return max_i;
}


long htonl(long val)
{ return val; }
short htons(short val)
{ return val; }
void err_dump(char *mesg)
{
	printf("Error: %s\n",mesg);
	exit(0);
}

unsigned long inet_addr(char *ipstr)
{
	int i;
	unsigned long ipno=0, sub;

	i=0;
	sub = 0;
	while (ipstr[i] != 0)
	{
		if (ipstr[i] == '.')
		{
			ipno = (ipno << 8) + sub;
			sub = 0;
		}
		else sub = sub * 10 + (ipstr[i] - '0');
		i++;
	}
	ipno = (ipno << 8) + sub; // For the last subpart

	return ipno;	
}

void perror(const char *str)
{
	printf("Error : %s\n", str); // Actually informative message based on 
			//errno should be displayed. at present not supported.
}

int fork(void)
{
	return 0;
}

char *getenv(const char *name)
{
	char **envp = (char **)(USERSTACK_SEGMENT_END - 0x00001000 + 3696);
	int i, len;
	char *valp;

	len = strlen(name);
	for (i=0; i<100; i++)
	{
		if (envp[i] == NULL) return NULL;
		if (strncmp(name, envp[i], len) == 0)
		{
			valp = envp[i];
			valp = valp + len;
			while ((*valp) != 0 && (*valp) != '=') valp++;
			
			if ((*valp) == '=') valp++;
			return valp;
		}
	}
	return NULL;
}

void abort(void)
{
	exit(0);
}

