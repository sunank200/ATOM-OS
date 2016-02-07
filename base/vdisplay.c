
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

#include "vdisplay.h"
#include "mklib.h"
#include "process.h"
#include "stdarg.h"
#include "alfunc.h"

#define INDEX_REG 	0x3d4	/* For selecting a regster[0-11h] */
#define DATA_REG	0x3d5	/* Selected register		  */
#define MODE_REG	0x3d8	
#define COLOR_REG	0x3d9	/* Color selection in text mode	  */
#define STATUS_REG	0x3da
#define CURSOR_START_REGNO	0x0a
#define CURSOR_END_REGNO	0x0b
#define START_ADDR_MSB_REGNO	0x0c
#define START_ADDR_LSB_REGNO	0x0d
#define CURSOR_ADDR_MSB_REGNO	0x0e
#define CURSOR_ADDR_LSB_REGNO	0x0f

#define MODE_80x25_TEXT	0x42

extern volatile struct process *current_process, *kbd_focus;;
extern volatile struct kthread *current_thread;
static void set_cursor(int x, int y);
void output_proc(char *text, int len);
void scroll_up(int lines);
void scroll_down(int lines);
 
void initialize_display(void)
{
	/*
	 * Set mode to 80x25 text mode.
	 */
	out_byte(MODE_REG,MODE_80x25_TEXT);
	in_byte(0x61);
	/*
	 * Set color to white on black.
	 */
	out_byte(COLOR_REG,0x07);
	in_byte(0x61);
	/*
	 * Set the display page no 0.
	 */
	out_byte(INDEX_REG,START_ADDR_MSB_REGNO);
	in_byte(0x61);
	out_byte(DATA_REG,0x00);
	in_byte(0x61);
	out_byte(INDEX_REG,START_ADDR_LSB_REGNO);
	in_byte(0x61);
	out_byte(DATA_REG,0x00);
	in_byte(0x61);
	/*
	 * Set the cursor position to (0,0) = (80 * row + col);
	 */
	out_byte(INDEX_REG,START_ADDR_MSB_REGNO);
	in_byte(0x61);
	out_byte(DATA_REG,0x00);
	in_byte(0x61);
	out_byte(INDEX_REG,START_ADDR_LSB_REGNO);
	in_byte(0x61);
	out_byte(DATA_REG,0x00);
	in_byte(0x61);
	return;
}

int syscall_putchar(char c)
{
	output_proc(&c,1);
	return 0;
}

int syscall_puts(char *str)
{
	output_proc(str, strlen(str));
	return 0;
}

void syscall_setcursor(int x, int y)
{
	if(((x >= 0 && x < 80) || x == -1) && ((y >= 0 && y < 25) || (y == -1)))
	{
		if (current_process != NULL)
		{
			if (x != -1)
				current_process->proc_outputinf->cursor_x = x ;
			if (y != -1)
				current_process->proc_outputinf->cursor_y = y ;
		}

		if (current_process == kbd_focus)
			set_cursor(kbd_focus->proc_outputinf->cursor_x,kbd_focus->proc_outputinf->cursor_y);                
	}
        return ;
}

int X=0, Y=0;

void output_proc(char *text, int len)
{
	int i;
	char *video_mem_base;
	int cursor_x, cursor_y;
	char attrib;

	if (current_process != NULL)
	{
		cursor_x = current_process->proc_outputinf->cursor_x;
		cursor_y = current_process->proc_outputinf->cursor_y;
		video_mem_base = current_process->proc_outputinf->video_mem_base;
	}
	else
	{
		video_mem_base = (char *)0xb8000;
		cursor_x = X; cursor_y = Y;
	}

	attrib = ((current_process != NULL) ? current_process->proc_outputinf->attrib : 0x07);

	/*
 	 * Copy character and attribute pairs to video display memory.
	 */
	for (i=0; i<len; i++)
	{
		if (text[i] == '\n' || text[i]=='\r' )
		{
			for ( ; cursor_x < 80; cursor_x ++)
			{
				video_mem_base[(cursor_y * 80 + cursor_x) * 2] = ' ';
				video_mem_base[(cursor_y * 80 + cursor_x ) * 2 + 1] = attrib;
			}
		}
		else if (text[i] == '\t')
		{
			for ( cursor_x++; (cursor_x < 80) && (cursor_x % 8 != 0); cursor_x++)
			{
				video_mem_base[(cursor_y * 80 + cursor_x) * 2] = ' ';
				video_mem_base[(cursor_y * 80 + cursor_x ) * 2 + 1] = attrib;
			}
		}
		else if(text[i] == 0x08)	//back space
		{
			cursor_x--;
			video_mem_base[(cursor_y * 80 + cursor_x) * 2] = ' ';
			video_mem_base[(cursor_y * 80 + cursor_x) * 2 + 1] = attrib;
		}
		else 
		{
			video_mem_base[(cursor_y * 80 + cursor_x) * 2] = text[i];
			video_mem_base[(cursor_y * 80 + cursor_x ) * 2 + 1] = attrib;
			cursor_x ++;
		}

		if (cursor_x >= 80)
		{
			cursor_x = 0;
			cursor_y ++;
		}

		if (cursor_y >= 25)
		{
			scroll_up(1);
			cursor_y = 24;
		}
	}

	/*
	 * Update current_process->cursor position.
	 */
	
	if (current_process != NULL)
	{
		current_process->proc_outputinf->cursor_x = cursor_x ;
		current_process->proc_outputinf->cursor_y = cursor_y ;
	}
	else
	{
		X = cursor_x; Y = cursor_y;
	}

	if (current_process ==  kbd_focus) 
		set_cursor(cursor_x, cursor_y);
}


static void set_cursor(int x, int y)
{
	int t = y * 80 + x;

	out_byte(0x3d4,14);
	out_byte(0x3d5, t>>8);
	out_byte(0x3d4,15);
	out_byte(0x3d5,t);

}
void scroll_up(int lines)
{
	int i,j,k;
	char *video_mem_base; // = (char *)0xb8000;
	char attrib=0x7;

	if (current_process != NULL)
		video_mem_base = current_process->proc_outputinf->video_mem_base;
	else
		video_mem_base = (char *)0xb8000;

	if (lines <= 0) return;
	if (lines > 25) lines=25;
	i = 0;
	j = 80*lines*2;

	/*
	 * Moving the lines up.
	 */
	for (k=0; k<(25-lines)*80; k++)
	{
		video_mem_base[i++] = video_mem_base[j++];
		video_mem_base[i++] = video_mem_base[j++];
	}
	/*
	 * Clearing the bottom lines.
	 */
	attrib = ((current_process != NULL) ? current_process->proc_outputinf->attrib : 0x07);
	
	for (k=0; k<lines*80; k++)
	{
		video_mem_base[i++] = ' ';
		video_mem_base[i++] = attrib;
	}
}

void scroll_down(int lines)
{
	int i,j,k;
	char *video_mem_base; // = (char *)0xb8000;
	char attrib = 0x7;

	if (current_process != NULL)
		video_mem_base = current_process->proc_outputinf->video_mem_base;
	else
		video_mem_base = (char *)0xb8000;


	if (lines <= 0) return;
	if (lines > 25) lines=25;
	i = (25*80*2);
	j = 80*(25-lines)*2;

	/*
	 * Moving the lines down.
	 */
	for (k=0; k<(25-lines)*80; k++)
	{
		video_mem_base[--i] = video_mem_base[--j];
		video_mem_base[--i] = video_mem_base[--j];
	}
	/*
	 * Clearing the bottom lines.
	 */
	attrib = ((current_process != NULL) ? current_process->proc_outputinf->attrib : 0x07);
	
	for (k=0; k<lines*80; k++)
	{
		video_mem_base[--i] = ' ';
		video_mem_base[--i] = attrib;
	}
}

void change_screen(struct process *newprocess)
{
	unsigned long old_cr0;

	// Scroll off for current kbd_focus task (oldtask)
	old_cr0 = clear_paging();
	bcopy((char *)0xb8000, kbd_focus->proc_outputinf->video_mem_backup, 4000);
	kbd_focus->proc_outputinf->video_mem_base = kbd_focus->proc_outputinf->video_mem_backup;
	// Set new task as having the kbd_focus and display its screen
	newprocess->proc_outputinf->video_mem_base = (char *)0xb8000;
	bcopy( newprocess->proc_outputinf->video_mem_backup,(char *)0xb8000, 4000);
	set_cursor(newprocess->proc_outputinf->cursor_x,newprocess->proc_outputinf->cursor_y);
	restore_paging(old_cr0, current_thread->kt_tss->cr3);
}
struct video_param get_video_param(void)
{
	struct video_param t;
	t.attrib = 0x07;
	t.cursor_y = Y;
	t.cursor_x = X;
	return t;
}
int set_video_param(struct video_param * vp) { return 0; }

int skip_atoi(const char **s)
{
	int i=0;

	while (isdigit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define LARGE	64		/* use 'ABCDEF' instead of 'abcdef' */

char * number(char * str, long num, int base, int size, int precision, int type)
{
	char c,sign,tmp[66];
	unsigned int num1;
	const char *digits="0123456789abcdefghijklmnopqrstuvwxyz";
	int i,t;
	if (type & LARGE)
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	num1 = num;
	if (num1 == 0) tmp[i++]='0';
	else while (num1 != 0)
	{
		t = num1 % base;
	//	if ( t < 0) t = -t;
		num1 = num1 / base;
		tmp[i++] = digits[t];
	}
	if (i > precision) precision = i;
	size -= precision;
	if (!(type&(ZEROPAD+LEFT)))
		while(size-->0)
			*str++ = ' ';
	if (sign) *str++ = sign;
	if (type & SPECIAL) {
		if (base==8)
			*str++ = '0';
		else if (base==16) {
			*str++ = '0';
			*str++ = digits[33];
		}
	}
	if (!(type & LEFT))
		while (size-- > 0)
			*str++ = c;
	while (i < precision--)
		*str++ = '0';
	while (i-- > 0)
		*str++ = tmp[i];
	while (size-- > 0)
		*str++ = ' ';
	return str;
}

/**
 * vsprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * Call this function if you are already dealing with a va_list.
 * You probably want sprintf instead.
 */
int vsprintf(char *buf, const char *fmt, va_list args)
{
	int len;
	unsigned long long num;
	int i, base;
	char * str;
	const char *s;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */
	                        /* 'z' support added 23/7/1999 S.H.    */
				/* 'z' changed to 'Z' --davidm 1/25/99 */

	for (str=buf ; *fmt ; ++fmt) {
		if (*fmt != '%') {
			*str++ = *fmt;
			continue;
		}
			
		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) {
				case '-': flags |= LEFT; goto repeat;
				case '+': flags |= PLUS; goto repeat;
				case ' ': flags |= SPACE; goto repeat;
				case '#': flags |= SPECIAL; goto repeat;
				case '0': flags |= ZEROPAD; goto repeat;
				}
		
		/* get field width */
		field_width = -1;
		if (isdigit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			++fmt;
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;	
			if (isdigit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				++fmt;
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' || *fmt =='Z') {
			qualifier = *fmt;
			++fmt;
		}

		/* default base */
		base = 10;

		switch (*fmt) {
		case 'c':
			if (!(flags & LEFT))
				while (--field_width > 0)
					*str++ = ' ';
			*str++ = (unsigned char) va_arg(args, int);
			while (--field_width > 0)
				*str++ = ' ';
			continue;

		case 's':
			s = va_arg(args, char *);
			if (!s)
				s = "<NULL>";

			if (field_width > 0)
				len = strnlen(s, field_width);
			else
				len = strnlen(s, 1000);

			if (!(flags & LEFT))
				while (len < field_width--)
					*str++ = ' ';
			for (i = 0; i < len; ++i)
				*str++ = *s++;
			while (len < field_width--)
				*str++ = ' ';
			continue;

		case 'p':
			if (field_width == -1) {
				field_width = 2*sizeof(void *);
				flags |= ZEROPAD;
			}
			str = number(str,
				(unsigned long) va_arg(args, void *), 16,
				field_width, precision, flags);
			continue;


		case 'n':
			if (qualifier == 'l') {
				long * ip = va_arg(args, long *);
				*ip = (str - buf);
			} else if (qualifier == 'Z') {
				unsigned long * ip = va_arg(args, unsigned long *);
				*ip = (str - buf);
			} else {
				int * ip = va_arg(args, int *);
				*ip = (str - buf);
			}
			continue;

		case '%':
			*str++ = '%';
			continue;

		/* integer number formats - set up the flags and "break" */
		case 'o':
			base = 8;
			break;

		case 'X':
			flags |= LARGE;
		case 'x':
			base = 16;
			break;

		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			break;

		default:
			*str++ = '%';
			if (*fmt)
				*str++ = *fmt;
			else
				--fmt;
			continue;
		}
		if (qualifier == 'L')
			num = va_arg(args, long long);
		else if (qualifier == 'l') {
			num = va_arg(args, unsigned long);
			if (flags & SIGN)
				num = (signed long) num;
		} else if (qualifier == 'Z') {
			num = va_arg(args, unsigned long);
		} else if (qualifier == 'h') {
			num = (unsigned short) va_arg(args, int);
			if (flags & SIGN)
				num = (signed short) num;
		} else {
			num = va_arg(args, unsigned int);
			if (flags & SIGN)
				num = (signed int) num;
		}
		str = number(str, num, base, field_width, precision, flags);
	}
	*str = '\0';
	return str-buf;
}

/**
 * sprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @...: Arguments for the format string
 */
int sprintf(char * buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=vsprintf(buf,fmt,args);
	va_end(args);
	return i;
}
/*
 * Prints a message to the console.
 */
int printk(const char * fmt, ...)
{
	va_list args;
	char buf[2049]; // Assuming that at a time never one printk function 
			// exceeds this many characters.
	int i;

	va_start(args,fmt);
	i=vsprintf(buf, fmt, args);
	va_end(args);

	output_proc(buf, strnlen(buf,0xffff));
	return i;
}

/* At present this routine is not using a device specific 
 * i/o procedure.
 */
int print_p(const char *fmt, ...)
{
	va_list args;
	char buf[2049];
	int i;
	va_start(args,fmt);
	i=vsprintf(buf, fmt, args);
	va_end(args);
	output_proc(buf, strnlen(buf,0xffff));
	return i;
}

#ifdef DEBUG
/*
 * Prints the register context to the console/device.
 * Should do for both, at present it can print on console only.
 */
extern struct segdesc gdt_table[];
int print_context(struct PAR_REGS *regs, int error_code)
{
	unsigned long old_cr0;
	PTE *pte;
	unsigned char *addr;

	printk("Task [%s] : kt_tssdesc : %x, current_thread : %x, regs =  %x Error code [%x]\n EAX=%X\tEBX=%X\tECX=%X\tEDX=%X\n ESI=%X\tEDI=%X\tEBP=%X\tESP=%X\n DS=%X\tES=%X\tSS=%X\tFS=%X\tGS=%X\nCS:EIP=%X:%X\tEFLAGS=%X original error code : %x\n",current_thread->kt_name, current_thread->kt_tssdesc, current_thread, regs, error_code,regs->eax,regs->ebx,regs->ecx,regs->edx,regs->esi,regs->edi,regs->ebp,regs->oldsp,regs->ds,regs->es,regs->oldss,regs->fs,regs->gs,regs->cs,regs->eip,regs->eflags, regs->error_code);
	printk("cs : %x, ds : %x, es : %x, ss : %x, esp : %x, eip : %x ( %x %x )\n",current_thread->kt_tss->cs,current_thread->kt_tss->ds,current_thread->kt_tss->es,current_thread->kt_tss->ss,current_thread->kt_tss->esp,current_thread->kt_tss->eip, gdt_table[current_thread->kt_tssdesc >> 3].a,gdt_table[current_thread->kt_tssdesc >> 3].b);
	old_cr0 = clear_paging();
	pte = vm_getptentry(current_process->proc_vm, 0x20000);
	addr = (unsigned char *)(pte->page_base_addr << 12) + (regs->error_code & 0x00000fff);

	printk("page_base_addr : %x, rw : %d, present : %d : (%x %x %x) cr0 : %x, cr3 : %x, pgdir : %x\n",pte->page_base_addr, pte->read_write, pte->present, *(addr), *(addr+1), *(addr+2), get_cr0(), get_cr3(), current_process->proc_vm->vm_pgdir);
	restore_paging(old_cr0, current_thread->kt_tss->cr3);
	return 0;
}

#endif

