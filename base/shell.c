
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

#include "usyscalls.h"
#include "mklib.h"
#include "errno.h"

int copy(int argc, char argv[][500]);

int head(int argc,char argv[][500]);
int cd(int argc,char argv[][500]);
int rdir(int argc, char argv[][500]);
int pwd(int argc, char argv[][500]);
int cat(int argc, char argv[][500]);
int catb(int argc, char argv[][500]);
int tail(int argc, char argv[][500]);
int rm(int argc, char argv[][500]);
int md(int argc, char argv[][500]);
int ps(int argc, char argv[][500]);
int mv(int argc, char argv[][500]);
int wc(int argc, char argv[][500]);
int split(int argc, char argv[][500]);
int power(int x,int y);
void cp(char* file,char* dir);
void  tomode(char attrib, char *str);
int ls(int argc, char argv[][500]);
int touch(int argc,char argv[][500]);
int chartonum(char *c);
void setpwd(void);
int exec(int argc, char argv[][500]);
void modifypwd(const char *path);
int my_exit(int argc, char argv[][500]);
void parse_path(const char *path, int *drive, char *dir_path, char *name_comp);
int print_error(int err, char val[500]);
int killp(int argc,char argv[][500]);
int help(int argc, char argv[][500]);
int mem(int argc, char argv[][500]);
int locks(int argc, char argv[][500]);
int barrs(int argc, char argv[][500]);
int names(int argc, char argv[][500]);
int conds(int argc, char argv[][500]);
int removelocks(int argc, char argv[][500]);
int prelease(int argc,char argv[][500]);
#ifdef DEBUG
int pginfo(int argc,char argv[][500]);
#endif
int id(int argc,char argv[][500]);
int tim(int argc, char argv[][500]);
//int mpiexec(int argc, char argv[][500]);

int memstatus(void);
//int namestatus(void);
//int lockstatus(void);
//int condstatus(void);
//int barrstatus(void);
//int clearlocks(void);
int wait(int *status);

int (* funcptrs[50])(int argc, char argv[][500]) = { /*vi,*/copy,ls,cat,pwd,rdir,touch,killp,head,tail,rm,md,ps,/*chmod,*/mv,/* ln,*/wc,/* find,*/split,cd,my_exit, exec, catb, help, /*attrib,*/ mem, prelease, 
#ifdef DEBUG
pginfo, 
#endif
id, tim};
char *funcnames[50] = {/*"vi", */ "copy","ls","cat","pwd","rmdir","touch","kill","head","tail","rm","mkdir","ps",/* "chmod",*/"mv"/*,"ln"*/,"wc",/*"find",*/"split","cd","exit", "exec" , "catb", "help",/* "attrib",*/ "mem","prelease", 
#ifdef DEBUG
"pginfo", 
#endif
"id", "time"};
#ifdef DEBUG
int no_cmds = 25;
#else
int no_cmds = 25;
#endif
char PWD[512]=""; 
char p[50][500]; // Arguments
char *envp[50] = {"PATH=.","USER=chapram","OMP_NUM_THREADS=4", NULL};
int current_drive=0;
void print_fat(void);

int  main(int argc, char **argv, char **envp)
{
 	int i=0,j=0,max_i=0,int_c;
	char s[500],c;
	int k, err;
	int cursor_x = 2;

	setpwd();
	while(1)
	{
		printf("$ ");
		strcpy(s," ");
		for(i=0;i<50;i++)
			strcpy(p[i]," ");
		i=0;
		max_i=0;
		cursor_x = 2;
		while( (int_c = getscanchar()) != 0x1C0D)
		{
			setcursor(cursor_x, -1);
			c=(int_c) & 0xff;
			if(c == 0)	//special symbols
			{
				c = (int_c & 0xffff) >> 8;
				switch(c)
				{
					case 0x47:	//home
						if (i > max_i) max_i = i;
						cursor_x = 2;
						i=0;
						setcursor(cursor_x, -1);
						break;
					case 0x4f:	//end
						i= max_i;
						cursor_x = 2;
						for (j=0; j<max_i; j++)
							if (s[j] != '\t') cursor_x++;
							else cursor_x = (cursor_x / 8) * 8 + 8;
						
						setcursor(cursor_x, -1);
						break;
					case 0x4B:	//left arrow
						if(i > 0)
						{
							if(i > max_i) max_i = i;
							if (s[i] != '\t') cursor_x--;
							else
							{
								cursor_x = 2;
								for (j=0; j<i; j++)
									if (s[j] != '\t') cursor_x++;
									else cursor_x = (cursor_x / 8) * 8 + 8;
							}
							i--;
							setcursor( cursor_x , -1);
						}
						break;
					case 0x4D:	//right arrow
						if(i<max_i)
						{
							i++;
				
							if (s[i] != '\t') cursor_x++;
							else cursor_x = (cursor_x / 8) * 8 + 8;
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
							if (s[j] != '\t') cursor_x++;
							else cursor_x = (cursor_x / 8) * 8 + 8;
						if (i < max_i)
						{
							for (j=i; j<max_i; j++)
								s[j] = s[j+1];
							s[max_i] = 0;
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
		if(i > max_i) max_i = i;
		// remove traling blanks
		while (s[max_i-1] == ' ' || s[max_i-1] == '\t') max_i--;
		
		s[max_i]=0;
		k=0;
		i=0;
		j = 0;
		while (s[i] == ' ' || s[i] == '\t') i++; // Skip initial spaces
		for( ; i<max_i; i++)
		{

			if(s[i]==' ' || s[i] == '\t' || s[i] == 0)
			{
				p[j][k]=0;
				j++;
				k=0;
				while (s[i] == ' ' || s[i] == '\t') i++; // skip up to last but one space
				i--;
			}
			else p[j][k++]= s[i];
		}
		p[j][k]=0;

		if(j==0 && k==0) continue;

		for (k=0;k<no_cmds;k++)
		{
			if (strcmp(funcnames[k], p[0]) == 0)
			{
				if ((err = funcptrs[k](j+1, p)) != 0)
				{
					printf("After return\n");
					print_error(err, p[0]);
				}
				break;
			}
		}
		if (k >= no_cmds) print_error(0, "Unknown command!\n");
	}
}


int mem(int argc, char argv[][500]) { return memstatus(); }

// Executing local application or
int exec(int argc, char argv[][500])
{
	char *args[50];
	int i;

	for (i=0; i<argc-1; i++)
		args[i] = argv[i+1];
	args[i] = NULL;

	if (argc >= 2) 
	{
		if (strcmp(argv[0],"exec") == 0)
			return exectask(argv[1], args, envp, PROC_TYPE_APPLICATION);
		else  
		{
			printf("Error: Parallel execution is not supported\n");
			return -1;
		}
	}
	else return -1;
}

void setpwd(void) { strcpy(PWD,"/"); return; }

int print_error(int err, char val[500])
{
	switch (err)
	{
	case ENO_FREE_FILE_TABLE_SLOT:
		printf("Error: All file table slots are full!\n"); break;
	case EINVALID_DIR_INDEX:
		printf("Error: Invalid directory/file handle\n"); break;
	case EDIR_ENTRY_NOT_FOUND:
		printf("Error: %s File/Directory entry not found\n", val); break;
	case EDIR_FULL:
		printf("Error: %s Directory full!\n", val); break;
	case EDUPLICATE_ENTRY:
		printf("Error: Directory entry for %s already exists\n", val); break;
	case EDIR_NOT_EMPTY:
		printf("Error: Directory %s is not empty\n", val); break;
	case ENO_DISK_SPACE:
		printf("Error: No disk space\n"); break;
	case ENOT_A_DIRECTORY:
		printf("Error: %s is not a directory\n", val); break;
	case EFILE_CREATE_ERROR:
		printf("Error: Error creating the file %s\n", val); break;
	case EDIR_OPEN_AS_A_FILE :
		printf("Error: Trying to open a directory %s as a file\n", val); break;
	case EVOLUME_ENTRY:
		printf("Error: %s Is a volume name\n", val); break;
	case EFILE_NOT_FOUND:
		printf("Error: file %s not found\n", val); break;
	case ENOT_READABLE:
		printf("Error: File %s not readable\n", val); break;
	case ENOT_WRITEABLE:
		printf("Error: File %s not writeable\n", val); break;
	case EACCESS:
		printf("Error: %s Accessing problem\n",val); break;
	case EINVALIDNAME:
		printf("Error: Invalid file/directory name %s\n", val); break;
	case EINVALID_HANDLE:
		printf("Error: Invalid file handle\n"); break;
	case EINVALID_ARGUMENT:
		printf("Error: Invalid argument \n"); break;
	case ELONGPATH: 
		printf("Error: path too long %s\n", val); break;
	case EINVALID_DRIVE: 
		printf("Error: %s Invalid drive\n",val); break;
	case EINVALID_SEEK:
		printf("Error: Invalid file seek\n"); break;
	case EDEVICE_DIFFERENT:
		printf("Error: Trying to move to another device\n"); break;
	case ETARGET_EXISTS:
		printf("Error: Target %s already exists\n", val); break;
	case EPATH_NOT_EXISTS:
		printf("Error: Path %s doesn't exist\n", val); break;
	case EREADONLY:
		printf("Error: %s Read only. Cant be modified\n", val); break;
	case EOLDPATH_PARENT_OF_NEWPATH:
		printf("Error: Target is sub-directory of the source \n"); break;
	case ESRC_DEST_NOT_SAME_TYPE:
		printf("Error: Source and destination not of the same type\n"); break;

	default: printf("Error: %s --> %d\n", val, err);
	}

	return 0;
}

int my_exit(int argc, char argv[][500]) { return 0; }

int id(int argc, char argv[][500]) { machid(); return 0; }

int tim(int argc, char argv[][500])
{
	//unsigned int hi, lo;
	unsigned int t; //, t1;
	t = time(NULL);
	printf("Time in seconds : %u\n", t);
	//t1 = timed(NULL);
	//marktsc(&hi, &lo);
	//printf("Seconds : %d, (hi : %ud, lo : %ud) - %ud\n",t, hi, lo, t1);
	//gettsc(&hi, &lo, &msecs);
	return 0;
}

int chartonum(char *c)
{
	int i,l,num=0,temp;
	l=strlen(c);
	for(i=0;i<l;i++)
	{
		if(c[i]<'0' || c[i]>'9') return -1;
		temp=c[i]-48;
		num=num*10+temp;
	}
	return num;
}
int cd(int argc,char argv[][500])
{
	int e;
	char argvcp[512];
	if(argc==1)
	{
		e=chdir("/");
		if(!e)
		{
			strcpy(PWD,"/");
			printf("PWD:%s",PWD);
		}
	}
	else
	{
		e=chdir(argv[1]);
		if(!e)
		{
			strcpy(argvcp,argv[1]);
			modifypwd(argvcp);
			printf("%s\n",PWD);
		}
	}
	return e;
}	
	
void modifypwd(const char *path)
{
	int i=0,j,k;
	char tstr[50];
	
	if(path[0]=='/')
	{
		strcpy(PWD,"/");
		i=1;
	}
	strcpy(tstr,"");
	while(i<strlen(path))
	{
		k=0;
		do
		{
			tstr[k]=path[i];
			i++;
			k++;
				
		}while(path[i]!='/' && path[i]!=0);
		i++;
		tstr[k]='/';
		tstr[k+1]=0;
		if(tstr[0]=='.' && tstr[1]=='.')
		{
			for(j=strlen(PWD)-2; j>=0 ; j--)
				if(PWD[j]=='/')		break;
			PWD[j+1]=0;
		}
		else if(tstr[0]=='.') continue;
		else strcat(PWD,tstr);
		
	}
}

int tail(int argc,char argv[][500])
{
	int fd,i=1,count=0,j,k,wr=0,line=0;
	char buffer[512];
	if(argc==1) return(1);
	if(argv[1][0]=='-')
	{
		for(i=1;i<(strlen(argv[1]));i++)
			line=line*10+argv[1][i]-48;
		i=2;
	}
	else
	{	
		i=1;
		line=10;
	}
	for(;i<argc;i++)
	{
		count=0;
		wr=0;
		fd=open(argv[i],O_RDONLY,0);
		if(fd < 0)
		{
			print_error(EFILE_NOT_FOUND,argv[i]);
			continue; 
		}

		while((j=read(fd,buffer,sizeof(buffer)))>0)
		{
			for(k=0;k<j;k++)
			{
				if(buffer[k]=='\n') count++;
			}	
		}
		lseek(fd,0,0);
		
		count-=line;
		while((j=read(fd,buffer,sizeof(buffer)))>0)
		{
			for(k=0;k<j;k++)
			{
				if(buffer[k]=='\n') wr++;
				if(wr>=count) putchar(buffer[k]);	
			}
		}
		close(fd);
	}
	return 0;
}

int md(int argc,char argv[][500])
{
	int fd,i;
	if(argc<2)
	{
		printf("Too few Arguments to mkdir\n");
		printf("Usage:\n\tmkdir <dir_name1> <dir_name2>...\n\n");
		return(0);
	}
	for(i=1;i<argc;i++)
	{
		fd=mkdir(argv[i],0755);
		if(fd < 0) print_error(fd,argv[i]);
	}
	return(0);
}

int ps(int argc,char argv[][500])
{
	int i, np;
	struct uprocinfo pinf;
	int pids[100];
	struct date_time dt;


	np = getpids(pids, 100);
	printf("Total processes : %d\n",np);
	printf("Name\tType PID    PPID   STime\tEXTime\tMem\tFiles\tTL\tTG\n"); 
	for (i=0; i<np; i++)
	{
		if (getprocinfo(pids[i],&pinf) == 0)
		{
			dt = secs_to_date(pinf.uproc_starttimesec);
printf("%-14s  %c  %-6d %-6d %02d:%02d:%02d.%03d\t%d\t%dk\t%3d\t%3d\t%3d\n",
	pinf.uproc_name, ((pinf.uproc_type == PROC_TYPE_SYSTEM) ? 'S' : 'U'),
	pinf.uproc_id,pinf.uproc_ppid, dt.hours, dt.minutes, dt.seconds, 
	pinf.uproc_starttimemsec, pinf.uproc_totalcputimemsec,
	pinf.uproc_memuse*4, pinf.uproc_nopenfiles,
	pinf.uproc_nthreadslocal,pinf.uproc_nthreadsglobal);
		}
	}
	
	return(0);
}

int mv(int argc,char argv[][500])
{
	int fd,i,flag=0;
	struct stat thebuf;
	char target[500];
	char dir_path[486],name_comp[13];
	int drive;

	if(argc>3)
	{
		
		if((strcmp(argv[argc-1],"..")==0)||(strcmp(argv[argc-1],".")==0)) flag=1;
		if(!stat(argv[argc-1],&thebuf)|| flag==1)
		{
			if((thebuf.st_mode & S_IFMT) != S_IFDIR && flag==0)
			{
				print_error(0,"Last argument must be a directory while moving multiple files\n");
				//	exit(1);
				return(0);
			}
			else
			{
//				buf=(char *)get_current_dir_name();
				for(i=1;i<argc-1;i++)
				{
					parse_path(argv[i],&drive,dir_path,name_comp);
					strncpy(target,argv[argc-1],486);
					strcat(target,"/");
					strncat(target,name_comp,13);
						
					fd = rename(argv[i],target);
					if (fd < 0)
						print_error(fd, argv[i]);
				}
			}
		}
		else
		{
			print_error(EDIR_ENTRY_NOT_FOUND,argv[argc-1]);
			return 0;
		}
	}
	else
	{
		if(!stat(argv[2],&thebuf))
		{
			if((thebuf.st_mode & S_IFMT)!= S_IFDIR)
			{
				return rename(argv[1],argv[2]);
			}
			else
			{
				parse_path(argv[1],&drive,dir_path,name_comp);
				strncpy(target,argv[2],486);
				strncat(target,name_comp,13);

				return rename(argv[1],target);
			}
		}
		else return rename(argv[1],argv[2]);
	}
	return 0;
}

int wc(int argc,char argv[][500])
{
	int fd,lines,words,chars,i,j,stop=0,count;
	int tlines, twords, tchars;
	char t='a',c;
	char buffer[512];
	lines=words=chars=0;

	if(argc<2)
	{
		while((c=getchar())!=(-1))
		{
			chars++;
			if(isspace(c))
			{
				while(isspace(c))
				{
					if(c=='\n') lines++;
					if((c=getchar())==(-1))
					{
						stop=1; break;
					}
					else chars++;
				}
				if(!isspace(t)) words++;
			}
			if(stop==1) break;
			t=c;
		}
		words++;
		printf("\t%d\t%d\t%d\n",lines,words,chars);
		return(0);
	}
	// From files ...
	tlines = twords = tchars = 0;
	for(i=1;i<argc;i++)
	{
		fd=open(argv[i],O_RDONLY,0);
		if(fd < 0)
		{
			print_error(fd,argv[i]);
			continue;
		}
		lines = words = chars = 0;
		while((count=read(fd,buffer,sizeof(buffer)))>0)
		{
			chars+=count;
			for(j=0;j<count;j++)
			{
				if(isspace(buffer[j]))
				{
					while(isspace(buffer[j]))
					{
						if(buffer[j]=='\n') lines++;
						j++;
					}
					if(!isspace(t)) words++;
				}
				t=buffer[j]; 	
			}
		}
		close(fd);
		printf("%s\t%d\t%d\t%d\n",argv[i],lines,words,chars);
		tlines += lines;
		twords += words;
		tchars += chars;
	}
	printf("Total\t%d\t%d\t%d\n",tlines,twords,tchars);
	return(0);
}

int split(int argc,char argv[][500])
{
	int count,i,j,len,fd,fdnew,size=0;
	int sizeread=0;
	char name[15];
	int c=0;
	char buffer[512];

	if(argc!=4)
	{
		print_error(0, "Wrong number/type of parameters\nUsage:\n\tsplit <srcfile> <destfile> <spltsize>\n");
		return(0);
	}

	for(i=0; i<(strlen(argv[3])); i++)	// Find split file size
		size=size*10+argv[3][i]-48;
	if (size <= 0)
	{
		print_error(0, "Wrong number/type of parameters\nUsage:\n\tsplit <srcfile> <destfile> <spltsize>\n");
		return(0);
	}

	//  opening the file
	fd=open(argv[1],O_RDONLY,0);
	if(fd<0) return(fd); // Error code

	j=0;
	count=0;
	fdnew = -1; // No new output file.
	strncpy(name,argv[2],9);
	len=strlen(argv[2]);
	while((sizeread=read(fd,buffer,sizeof(buffer)))>0)
	{
		if (fdnew < 0)
		{	// Create output file
			//finding name of the target split
			strncpy(name,argv[2],9);
			name[len]='.';
			name[len+3]=(c % 100)+48;
			name[len+2]=((c/10) % 10)+48;
			name[len+1]=(c/100)+48;
			name[len+4]=0;
			fdnew=creat(name,0664);
			if(fdnew<0)
			{
				print_error(fdnew,name);
				close(fd);
				return(0);
			}
		}

		if(size>=count+sizeread)
		{
			write(fdnew,buffer,sizeread);
			count+=sizeread;
		}
		else
		{
			i=size-count;
			write(fdnew,buffer,i);
			close(fdnew);
		
			count=0;
			c++;
			name[len+3]=c%100+48;
			name[len+2]=((c/10) %10)+48;	
			name[len+1]=(c/100)+48;
			fdnew=open(name,O_CREAT,0);
			if(fdnew < 0)
			{
				print_error(fd, name);
				close(fd);
				return(0);
			}
			write(fdnew,buffer+i,sizeread-i);
		}
	}
	close(fd);
	if (fdnew >= 0) close(fdnew); // In case output file opened
	return 0;
}

int rm(int argc,char argv[][500])
{
	int fd=0,i;
	char c;
	if(argc<2)
	{
		print_error(0,"Invalid number of Arguments\n");
		print_error(0,"Usage:\n\trm <file>\n\n");
		return(0);
	}
	for(i=1;i<argc;i++)
	{
		if(strcmp(argv[i],"")!=0)
	   	{
			printf("rm: remove regular File \'%s\' ? ",argv[i]);
			read :c=getchar();
			if((c=='y')||(c=='Y'))
			{
				c=getchar();
				if(c == 0x08) goto read;
				if(c == 0x0d) fd=unlink(argv[i]);
				if (fd != 0) print_error(fd,argv[i]);
			}
			else
			{
				c=getchar();
				if(c == 0x08) goto read;
				if(c == 0x0d) return 0;
			}	
	   	}
	}
	return(0);
}
int head(int argc,char argv[][500])
{
	int fd,i,count=0,j,k,num=0,l;
	char buffer[512];

	if(argc==1)
	{
		print_error(0,"Wrong number of arguments\nUsage:\n\thead [<-nlines>] <file(s)>\n");
		return 0;
	}

	if(argv[1][0]=='-')
	{
		l=strlen(argv[1]);
		for(i=1;i<l;i++)
			num=num*10+argv[1][i]-48;
		i=2;
	}
	else
	{
		num=10;
		i=1;
	}

	for(;i<argc;i++)
	{
		fd=open(argv[i],O_RDONLY,0);
		if(fd < 0)
		{
			print_error(fd,argv[i]);
			continue;
		}
		
		printf("\n%s:\n",argv[i]);
		count=0;	 
		while ((count < 10) && ((j=read(fd,buffer,sizeof(buffer))) > 0))
		{
			for(k=0;k<j;k++)
			{
				if(count<num)
				{
					if(buffer[k]=='\n') count++;
					putchar(buffer[k]);
				}
				else break;
			}
		}
		close(fd);
	}
	return(0);
}


int killp(int argc,char argv[][500])
{
	int i,retval,p;
	for(i=1;i<argc;i++)
	{
		p=chartonum(argv[i]);
		printf("terminating process #%d\n",p);
		retval = kill(p);
		if (retval == EINVALIDPID) printf("There is no such process #%d\n",p);
	}
	return 0;
} 	

int prelease(int argc,char argv[][500])
{
	int i,retval, status, p;
	int _wait(int *status);

	for(i=1;i<argc;i++)
	{
		p=chartonum(argv[i]);
		printf("Releasing the process #%d\n",p);
		retval = _wait(&status);
		if (retval == EINVALIDPID)
			printf("There is no such process #%d\n",p);
		else if (retval == 1)
			printf("Process #%d marked for releasing\n",p);
	}
	return 0;
}

#ifdef DEBUG
int pginfo(int argc,char argv[][500])
{
	int i,retval,p1, p2, p3;
	if (argc >= 3)
	{
		p1 = chartonum(argv[1]);
		p2 = chartonum(argv[2]);
		if (argc > 3) p3 = chartonum(argv[3]);
		else p3 = p2;
		for (i=p2; i<=p3; i++)
		{
			retval = pageinfo(p1, i);
			if (retval == EINVALIDPID)
				printf("There is no such process #%d\n",p);
		}
	}
	return 0;
}
#endif

int touch(int argc,char argv[][500])
{
	int fd=1,i;

	if(argc>1)
	{
		for(i=1;i<argc;i++)
		{
			fd=open(argv[i],O_APPEND,0); // Open in append mode
			if(fd<0) fd=creat(argv[i],0644); // If not existing?
			if(fd < 0)
			{		     
				print_error(fd,argv[i]);
				return(0);
			}	     
			close(fd);
		}
	}
	return(0);
}

int rdir(int argc,char argv[][500])
{
	int fd;
	if(argc!=2)
	{
		printf("Invalid Directory\n");
		return(0);
	}
	fd=rmdir(argv[1]);
	if(fd < 0) print_error(EDIR_ENTRY_NOT_FOUND,argv[1]);
	return(0);
}

int pwd(int argc,char argv[][500]) { printf("%s\n",PWD); return 0; }

int copy(int argc, char argv[][500])
{
	int fdold,fdnew,count,drive;
	char buffer[512],name_comp[13],dir_path[501];
	struct stat thebuf;
	int i;
	if(argc!=3)
	{
		print_error(0,"Invalid Number of Arguments\n");
		print_error(0,"Usage:\n\tcopy <src> <dest>\n\n");
		return(0);
	}
	fdold=open(argv[1],O_RDONLY,0);
        if(fdold<0)
	{
		print_error(fdold,argv[1]);
		return(0);
	}

	parse_path(argv[2],&drive,dir_path,name_comp);

	if(strcmp(name_comp,"")==0)
	{
		parse_path(argv[1],&drive,dir_path,name_comp);

		strcat(argv[2],"/");
		strcat(argv[2],name_comp);
	}	
	else if(!stat(argv[2],&thebuf))
	{
		if((thebuf.st_mode & S_IFMT)==S_IFDIR)
		{
			close(fdold);
			cp(argv[1],argv[2]); // file to directory copy
			return 0;
		}
	}

	// Copy file to file
	fdnew=creat(argv[2],0);
	
	if(fdnew < 0)
	{
		print_error(fdnew,argv[2]);
		close(fdold);
		return(0);
	}
	i=0;
	while((count=read(fdold,buffer,sizeof(buffer)))>0)
	{
		if (write(fdnew,buffer,count) != count)
		{
			printf("Error in writing\n");
			break;
		}
		else i += count;
	}
	
	close(fdnew);
	close(fdold);
	return 0;
}
	
void cp(char *file,char *dir)
{
	char buffer[512];
	int fd,fdold,count;

	strncpy(buffer,dir,486); // destination dir
	strcat(buffer,"/");
	strcat(buffer,file); // +file

	fd=creat(buffer,0664);
	if(fd<0)
	{
		print_error(fd,buffer);
		return;
	}

	fdold=open(file,O_RDONLY,0);
	if(fdold<0)
	{
		close(fd);
		print_error(fdold, file);
		return;
	}

	while((count=read(fdold,buffer,sizeof(buffer)))>0)
		write(fd,buffer,count);
	close(fd);
	close(fdold);
}

char *chartime(unsigned int secs, char str[25])
{
	struct date_time dt;

	dt = secs_to_date(secs);
	sprintf(str," %02d:%02d:%02d %02d/%02d/%04d ",dt.hours,dt.minutes,dt.seconds,dt.date,dt.month,dt.year);
	return str;
}

int ls(int argc,char argv[][500])
{
	DIR *dirname;
	struct dirent *rdir;
	int i,l=0,a=0,d=0,j,count=0;
	char file[50], tem[20], stfile[512],dirfile[512];
	struct stat thebuf;
	int len;

	dirname=opendir(".");
	strcpy(dirfile,PWD);

	for(i=1;i<argc;i++)
	{
		if(argv[i][0]=='-')
		{
			for(j=1;j<strlen(argv[i]);j++)
			{
				switch(argv[i][j])
				{
					case 'l': l=1; break;
					case 'a': a=1; break;
					case 'd': d=1; break;
					default : break;
				}
			}
		}
		else
		{
			closedir(dirname);
			dirname=opendir(argv[i]);
			strcpy(dirfile,argv[i]);
		}
	}

	if (dirname == NULL)
	{
		print_error(0,"directory open failure!\n");	
		return 0;
	}

	while(1)
	{
		rdir=readdir(dirname);

		if(rdir==NULL)
		{
			closedir(dirname);
			printf("\n");
			return(0);
		}
		strcpy(file,rdir->d_name);

		if(l==1)
		{
			if(a!=1)
			{
				if(file[0]!='.')
				{
				lable:
					strcpy(stfile,dirfile);
					len = strlen(stfile);
					if (stfile[len-1] == '/' || stfile[len-1] == '\\')
						strcat(stfile,rdir->d_name);
					else
					{
						stfile[len] = '/';
						stfile[len+1] = 0;
						strcat(stfile,rdir->d_name);
					}

					if ((len = stat(stfile,&thebuf)) != 0)
						printf("stat: error %d\n",len);

					tem[0] = tem[1] = tem[2] = '-';
					tem[3] = 0; 
					switch(thebuf.st_mode & S_IFMT)
					{
					case S_IFDIR: printf("d"); break;

					case S_IFCHR: printf("c"); break;   
					case S_IFBLK: printf("b"); break;   
					case S_IFIFO: printf("f"); break;   
					default:      printf("-"); break;
					}
					printf("%s%3d ",tem,thebuf.st_nlink);
					printf("%5s","root");

					printf("%10s","all");

					chartime(thebuf.st_mtime, file);

					strcat(file,"  ");
					strcat(file,rdir->d_name);
					printf("%10d\t%s\n",thebuf.st_size,file);
				 }
			}
			else goto lable;
		}
		else
		{
			if(a!=1)
			{
				if(file[0]!='.')
				{
					if(count==7)
					{
						count=0;
						printf("\n");
					}
					printf("%-12s",file);
					count++;
				}
			}
			else printf("%-12s",file);
		}
	}
}

void tomode(char x,char *tem)
{

	if ((x & FTYPE_SYSTEM) != 0) tem[0] ='s';
	else if ((x & FTYPE_HIDDEN) != 0) tem[0] = 'h';
	else if (x & FTYPE_VOLUME) tem[0] = 'v';
	else tem[0] = 'r';

	if (x & FTYPE_READONLY) tem[1] = '-';
	else tem[1] = 'w';

	tem[2] = tem[3] = tem[4] = tem[5] = tem[6] = tem[7] = tem[8] = '-';
	tem[9] = 0;

	return;
}

int power(int x,int y)
{
	int i,num=1;
	for(i=0;i<y;i++)
		num=num*x;
	return num;
}

int cat(int argc, char argv[][500])
{
	int fdold,j,count,i=0;
	char buffer[512];

	for(i=1;i<argc;i++)
	{
		fdold=open(argv[i],O_RDONLY,0);

	        if(fdold<0)
		{
			print_error(fdold,argv[i]);
			continue;
		}

		while((count=read(fdold,buffer,sizeof(buffer)))>0)
		{
			for(j=0;j<count;j++)
				putchar(buffer[j]);
		}
		close(fdold);
	}
	return 0;	
}

int catb(int argc, char argv[][500])
{
	int fdold,j,count,i=0, c = 0;
	char buffer[512];

	for(i=1;i<argc;i++)
	{
		fdold=open(argv[i],O_RDONLY,0);

	        if(fdold<0)
		{
			print_error(fdold,argv[i]);
			continue;
		}

		while((count=read(fdold,buffer,sizeof(buffer)))>0)
		{
			for(j=0;j<count;j++)
				printf("[%5d]   %5d ",c+j,buffer[j]);
			c += count;
		}
		close(fdold);
	}
	return 0;	
}

void parse_path(const char *path, int *drive, char *dir_path, char *name_comp)
{
	int j,flag,i=0;

	strcpy(dir_path,"");
	strcpy(name_comp,"");
	flag=0;
	if (path[1] == ':')	// Drive name present
	{
		*drive = toupper(path[0]) - 'A';
		path = path + 2;
	}
	else *drive = current_drive;
	
	if(path[0]=='.'&&path[1]=='.')
	{
		strcat(dir_path,"..");
		i=2;
		flag=2;
	}
	else if(path[0]=='.')
	{
		strcat(dir_path,".");
		i=1;
		if(path[1]==0) flag=2;
	}	



	// Check if path contains any dir names
	// No directory component
	for ( ; i<strlen(path); i++)
	{
		if (path[i] == '/' || path[i] == '\\')
		{
			flag = 1;
			break;
		}
	}

	if (flag == 1)
	{
		strcpy(dir_path, path);
		// remove the last component
		j = 13;
		for (i=strlen(dir_path); (j>=0 && i>=0 && dir_path[i] != '/' && dir_path[i] != '\\'); i--) 
			name_comp[--j] = dir_path[i];

		if (j<0) j = 0; // Long name. Incorrect results
		if (i == 0) // special case
			dir_path[1] = 0; // only root dir
		else	dir_path[i] = 0; // replace last / with null char

		for (i=0; i<=12; i++)
			name_comp[i] = name_comp[j++];
	}
	else if(flag==0)
	{
		strcpy(name_comp,path);
		dir_path[0] = 0;
	}

}

int help(int argc, char argv[][500])
{
	printf("attrib  Changes permission attributes of files\n");
        printf("        Usage:  attrib <[+|-][R|A|S|H]> <file1> <file2>..\n");
        printf("cat:    Print files on the standard output\n");
        printf("        Usage:  cat <file1> <file2> ..\n");
        printf("catb:   Print files characters corresponding bytes\n");
        printf("        Usage:  catb <file1> <file2> ..\n");
        printf("cd:     Changes current working directory\n");
        printf("        Usage:  cd <dir>\n");
        printf("chmod:  Change file access permissions\n");
        printf("        Usage:\n");
        printf("copy:   Copy one file to another file\n");
        printf("        Usage:  copy <src> <dest>\n");
        printf("exec:   Executes program\n");
        printf("        Usage:  exec <filename>\n");
        printf("exit:   Exit shell\n");
        printf("        Usage:  exit\n");
        printf("find:   Search for files in a directory hierarchy\n");
        printf("        Usage:\n");
	printf("head:   Output the first part of files\n");
        printf("        Usage:  head [-no_of_lines] <file1> <file2> ..\n");
        printf("help:   Displays list of commands\n");
        printf("        Usage:  help\n");
        printf("kill:   Kills a process based on its pid\n");
        printf("        Usage:  kill <pid1> <pid2> ..\n");
        printf("ln:     Makes links between files\n");
        printf("        Usage:\n");
        printf("ls:     List directory contents\n");
        printf("        Usage:  ls <[-l | -a | -d]>\n");
        printf("mkdir:  Creates a new directory\n");
        printf("        Usage:  mkdir <dir1> <dir2> ..\n");
        printf("mv:     Moves (rename) files\n");
        printf("        Usage:\n");
        printf("ps:     Report a snapsort of current processes\n");
        printf("        Usage:  ps\n");
        printf("pwd:    Print name of current/working directory\n");
        printf("        Usage:  pwd\n");
        printf("rm:     Remove file\n");
        printf("        Usage:  rm <file>\n");
        printf("rmdir:  Removes empty directories\n");
        printf("        Usage:  rmdir <dir>\n");
        printf("split:  Split a file into pieces\n");
	printf("        Usage:  split <srcfile> <destfile> <splsize>\n");
        printf("tail:   Output last part of files\n");
        printf("        Usage:  tail [-no_of_lines] <file1> <file2> ..\n");
        printf("touch:  Change file time stamps or creates new files\n");
        printf("        Usage:  touch\n");

        printf("wc:     Print the number of newlines, words, and bytes in files\n");
        printf("        Usage:  wc <file1> <file2> ..\n");

	return 0;
}


