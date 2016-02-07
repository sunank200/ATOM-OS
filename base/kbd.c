
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


/*
 * File : kbd.c
 * Keyboard driver program. This handles the keyboard interrupts 
 * and stores the entered keys into character lists associated with 
 * the present key board owning task.
 */

#include "mklib.h"
#include "process.h"
#include "timer.h"
#include "ne2k.h"
#include "lock.h"
#include "alfunc.h"

/*
 * Key board status code positions in 'kb_flags'.
 */

#define SCROLLOCK	0x0001
#define	NUMLOCK		0x0002
#define CAPSLOCK	0x0004
#define LSHIFT		0x0008
#define RSHIFT		0x0010
#define LCTRL		0x0020
#define RCTRL		0x0040
#define LALT		0x0080
#define RALT		0x0100

static void scroll_off(void);
static void scroll_on(void);
static short int check_caps_lock(short int scan_ascii, int cur_code);
static void input_insert(unsigned short scan_ascii);
static void send_ctrl_break(void);

extern void irq_entry_0_1(void);
static void change_kbd_focus(void);
static unsigned short int kbd_getchar(void);
static void display_thread_status(void);

#ifdef DEBUG
volatile struct kthread *ttt;
#endif
extern struct kthread *intr_threads[];
extern volatile struct process * used_processes, *current_process;
extern volatile struct kthread *current_thread, *ready_qhead[MAXREADYQ];
extern volatile unsigned short int ready_qlock;
extern volatile struct process *kbd_focus;
extern struct segdesc gdt_table[];
unsigned short int kb_flags = 0;
struct lock_t kbd_wait_lock;


/* Normal	Shifted		WithCtrl	WithAlt  */
int key_table[0x53][4]  = {
/* ESC	*/	{ 0x011b, 0x011b, 0x011b, 0x0100 },
/* 1,2..9,0 */	{ 0x0231, 0x0221, 0x0000, 0x7800 },
		{ 0x0332, 0x0340, 0x0300, 0x7900 }, 
		{ 0x0433, 0x0423, 0x0000, 0x7a00 },
		{ 0x0534, 0x0524, 0x0000, 0x7b00 },
		{ 0x0635, 0x0625, 0x0000, 0x7c00 },
		{ 0x0736, 0x075e, 0x071e, 0x7d00 },
		{ 0x0837, 0x0826, 0x0000, 0x7e00 },
		{ 0x0938, 0x092A, 0x0000, 0x7F00 },
		{ 0x0A39, 0x0A28, 0x0000, 0x8000 },
		{ 0x0B30, 0x0B29, 0x0000, 0x8100 },
/* -=BS,TAB */  { 0x0C2D, 0x0C5F, 0x0C1F, 0x8200 },
		{ 0x0D3D, 0x0D2B, 0x0000, 0x8300 },
		{ 0x0E08, 0x0E08, 0x0E7F, 0x0E00 },
		{ 0x0F09, 0x0F00, 0x9400, 0xA500 },

/*qwertyuiop*/	{ 0x1071, 0x1051, 0x1011, 0x1000 },
		{ 0x1177, 0x1157, 0x1117, 0x1100 },
		{ 0x1265, 0x1245, 0x1205, 0x1200 },
		{ 0x1372, 0x1352, 0x1312, 0x1300 },
		{ 0x1474, 0x1454, 0x1414, 0x1400 },
		{ 0x1579, 0x1559, 0x1519, 0x1500 },
		{ 0x1675, 0x1655, 0x1615, 0x1600 },
		{ 0x1769, 0x1749, 0x1709, 0x1700 },
		{ 0x186F, 0x184F, 0x180F, 0x1800 },
		{ 0x1970, 0x1950, 0x1910, 0x1900 },
/*[]enter,lctrl*/
		{ 0x1A5B, 0x1A7B, 0x1A1B, 0x1A00 },
		{ 0x1B5D, 0x1B7D, 0x1B1D, 0x1B00 },
		{ 0x1C0D, 0x1C0D, 0x1C0A, 0xA600 },
		{ 0x0000, 0x0000, 0x0000, 0x0000 },
/*asdfghjkl;'~*/
		{ 0x1E61, 0x1E41, 0x1E01, 0x1E00 },
		{ 0x1F73, 0x1F53, 0x1F13, 0x1F00 },
		{ 0x2064, 0x2044, 0x2004, 0x2000 },
		{ 0x2166, 0x2146, 0x2106, 0x2100 },
		{ 0x2267, 0x2247, 0x2207, 0x2200 },
		{ 0x2368, 0x2348, 0x2308, 0x2300 },
		{ 0x246A, 0x244A, 0x240A, 0x2400 },
		{ 0x256B, 0x254B, 0x250B, 0x2500 },
		{ 0x266C, 0x264C, 0x260C, 0x2600 },
		{ 0x273B, 0x273A, 0x0000, 0x2700 },
		{ 0x2827, 0x2822, 0x0000, 0x0000 },
		{ 0x2960, 0x297E, 0x0000, 0x0000 },
/* Leftshift */	{ 0x0000, 0x0000, 0x0000, 0x0000 },
/*\zxcvbnm,./ */
		{ 0x2B5C, 0x2B7C, 0x2B1C, 0x2600 },
		{ 0x2C7A, 0x2C5A, 0x2C1A, 0x2C00 },
		{ 0x2D78, 0x2D58, 0x2D18, 0x2D00 },
		{ 0x2E63, 0x2E42, 0x2E03, 0x2E00 },
		{ 0x2F76, 0x2F56, 0x2F16, 0x2F00 },
		{ 0x3062, 0x3042, 0x3002, 0x3000 },
		{ 0x316E, 0x314E, 0x310E, 0x3100 },
		{ 0x326D, 0x324D, 0x320D, 0x3200 },
		{ 0x332C, 0x333C, 0x0000, 0x0000 },
		{ 0x342E, 0x343E, 0x0000, 0x0000 },
		{ 0x352F, 0x353F, 0x0000, 0x0000 },
/* Keypad /     0x352F      0x352F	 0x9500	  0xA400 */
/*Rightshift */ { 0x0000, 0x0000, 0x0000, 0x0000 },
/* keypad *  */ { 0x372A, 0x0000, 0x9600, 0x3700 },
/* LeftAlt   */ { 0x0000, 0x0000, 0x0000, 0x0000 },
/* Space bar */ { 0x3920, 0x3920, 0x3920, 0x3920 },
/* Cpas lock */ { 0x0000, 0x0000, 0x0000, 0x0000 },

/* F1 .. F10 */	{ 0x3B00, 0x5400, 0x5E00, 0x6800 },
		{ 0x3C00, 0x5500, 0x5F00, 0x6900 },
		{ 0x3D00, 0x5600, 0x6000, 0x6A00 },
		{ 0x3E00, 0x5700, 0x6100, 0x6B00 },
		{ 0x3F00, 0x5800, 0x6200, 0x6C00 },
		{ 0x4000, 0x5900, 0x6300, 0x6D00 },
		{ 0x4100, 0x5A00, 0x6400, 0x6E00 },
		{ 0x4200, 0x5B00, 0x6500, 0x6F00 },
		{ 0x4300, 0x5C00, 0x6600, 0x7000 },
		{ 0x4400, 0x5D00, 0x6700, 0x7100 },
/* Num lock  */ { 0x0000, 0x0000, 0x0000, 0x0000 },
/* scrolllock*/ { 0x0000, 0x0000, 0x0000, 0x0000 },
/* Home	     */ { 0x4700, 0x4737, 0x7700, 0x9700 },
/* Up Arrow  */ { 0x4800, 0x4838, 0x8D00, 0x9800 },
/* PgUp	     */ { 0x4900, 0x4939, 0x8400, 0x9900 },
/* Keypad -  */ { 0x4A2D, 0x4A2D, 0x8E00, 0x4A00 },
/* Left Arrow*/ { 0x4B00, 0x4B34, 0x7300, 0x9B00 },
/* Keypad 5  */ { 0x0000, 0x4C35, 0x8F00, 0x0000 },
/* Right Arrow*/{ 0x4D00, 0x4D36, 0x7400, 0x9D00 },
/* Keypad +  */ { 0x4E2B, 0x4E2B, 0x0000, 0x4E00 },
/* End	     */ { 0x4F00, 0x4F31, 0x7500, 0x9F00 },
/* Down Arrow*/ { 0x5000, 0x5032, 0x9100, 0xA000 },
/* PgDn	     */ { 0x5100, 0x5133, 0x7600, 0xA100 },
/* Ins	     */ { 0x5200, 0x5230, 0x9200, 0xA200 },
/* Del	     */ { 0x5300, 0x532E, 0x9300, 0xA300 }};

/* Special cases F11, F12, Left WIN, Right WIN, Note Pad */

extern struct process system_process;
extern volatile struct kthread *used_threads;
unsigned short int kb_flags ;
void kbd_intr_handler(int irq);
struct kthread *kbd_thread;

/*
 * This interrupt handler task will be called from
 * an assembly language interrupt handler for keyboard interrupts.
 */

void kbd_intr_handler(int irq)
{
	static unsigned modebyte = 0;
	static int mode_set = 0;
	unsigned int cur_code,col;
	unsigned short scan_ascii;
	static unsigned prev_ext = 0;
	static unsigned pause_code_count = 0;
	struct kthread *ithr;

	// Remember the previous interrupted thread and clear its task busy bit
	// Change the high level task related pointers to point to 
	// this kbd task.
	ithr = (struct kthread *)current_thread;
#ifdef DEBUG
	ttt = ithr;
#endif
	current_process = &system_process;
	current_thread = kbd_thread;
	current_thread->kt_schedinf.sc_state = THREAD_STATE_RUNNING;
	current_thread->kt_schedinf.sc_tqleft = TIME_QUANTUM;
	ithr->kt_schedinf.sc_state = THREAD_STATE_READY;
	ithr->kt_cr0 = get_cr0();
	gdt_table[ithr->kt_tssdesc >> 3].b &= 0xfffffdff;
	math_state_save(&ithr->kt_fps);
	math_state_restore((struct fpu_state *)&kbd_thread->kt_fps);
	add_readyq(ithr);
	// Enable interrupts now
	sti_intr();
	cur_code = in_byte(0x60);

	if ( prev_ext == 0 && pause_code_count == 0 && cur_code <= 0x53)
	{
		/* some make code that has to be translated */
		if ((kb_flags & LSHIFT) || (kb_flags & RSHIFT))
			col = 1;	
		else if ((kb_flags & LCTRL) || (kb_flags & RCTRL))
			col = 2;
		else if ((kb_flags & LALT) || (kb_flags & RALT))
			col = 3;
		else	col = 0;	/* Normal Key code */

		scan_ascii = key_table[cur_code-1][col];
		if (scan_ascii == 0x0000)
		{
			switch (cur_code)
			{
			case 0x1d:	/* Left Ctrl	*/
				kb_flags = kb_flags | LCTRL;
				break;
			case 0x2a:	/* Left Shift	*/
				kb_flags = kb_flags | LSHIFT;
				break;
			case 0x36:	/* Right Shift	*/
				kb_flags = kb_flags | RSHIFT;
				break;
			case 0x38:	/* Left Alt	*/
				kb_flags = kb_flags | LALT;
				break;
			case 0x3a:	/* Caps Lock	*/
				kb_flags = kb_flags ^ CAPSLOCK;
				/* Update led indicators */
				modebyte = modebyte ^ CAPSLOCK;
				mode_set = 1;
				out_byte(0x60,0xed);
				/* 
				 * Later ACK byte wiil be recived 
				 * from key board
				 */
				break;
			case 0x45:	/* Num Lock	*/

				kb_flags = kb_flags ^ NUMLOCK;
				/* Update led indicators */
				modebyte = modebyte ^ NUMLOCK;
				mode_set = 1;
				out_byte(0x60,0xed);
				break;
			case 0x46:	/* Scroll lock	*/
				kb_flags = kb_flags ^ SCROLLOCK;
				if (kb_flags & SCROLLOCK)
					scroll_off();
				else	
					scroll_on();
				/* Update led indicators */
				modebyte = modebyte ^ SCROLLOCK;
				mode_set = 1;
				out_byte(0x60,0xed);
				break;
			}

		}
		else	/* Insert the key code	*/
		{
			scan_ascii = check_caps_lock(scan_ascii, cur_code);
			input_insert(scan_ascii);
		}
	}
	else if (prev_ext != 0)
	{
		/*
		 * some extended key. Print screen and control break 
		 * keys must be handled diffrently. Really some 
		 * thing more than this can be done for some 
		 * special key codes.
		 */
		switch (cur_code)
		{
		case 0x38:	/* Right Alt	*/
			kb_flags  = kb_flags | RALT;
			break;
		case 0x1d:	/* Right Ctrl	*/
			kb_flags = kb_flags | RCTRL;
			break;
		case 0x46:	/* Ctrl break	*/
			send_ctrl_break();
			break;
		default :
			if (cur_code <= 0x53)
			{
			if ((kb_flags & LSHIFT) || (kb_flags & RSHIFT))
				col = 1;	
			else if ((kb_flags & LCTRL) || (kb_flags & RCTRL))
				col = 2;
			else if ((kb_flags & LALT) || (kb_flags & RALT))
				col = 3;
			else	col = 0;

			scan_ascii = key_table[cur_code-1][col];
			if (scan_ascii != 0x0000)
			{
				scan_ascii = check_caps_lock(scan_ascii, cur_code);
				input_insert(scan_ascii);
			}
			}
			/*
			 * Windows keys are not handled.
			 */
			if (cur_code == 0x5b || cur_code == 0x5c || cur_code == 0x5d)	;
			break;
		}
		prev_ext = 0; /* Clear Extended key code flag */

	}
	else if (pause_code_count > 0) pause_code_count--;
		/* Pause key is not handled. */
	else
	{	/* 
		 * Otherwise key code >= 81 so break code of some key 
		 * or extented key or pause key first code byte.
		 */
		if (cur_code == 0xfa)
		{
			/*
			 * ACK to prev command. But we will send only
			 * set mode indicators command. Other commands
			 * are not used.
			 */
			if (mode_set == 1)
			{
				/* Option byte must be sent. */
				mode_set = 0;
				out_byte(0x60,modebyte);
			}
		}
		if (cur_code == 0xe0)
		{
			/* Extended key pressed. */
			prev_ext = 1;

		}
		else if (cur_code == 0xe1)
		{
			/* 
			 * Pause key pressed. Ignore next 
			 * five code bytes also.
			 */
			pause_code_count = 5; 

		}
		else if (cur_code > 0x53 && cur_code < 0x81)
		{
			printk("Un handled key code : %x\n", cur_code);
		}
		switch (cur_code)
		{
		case 0x9d:	/* Left Ctrl	*/
			kb_flags = kb_flags & ~(LCTRL);
			break;
		case 0xaa:	/* Left Shift	*/
			kb_flags = kb_flags & ~(LSHIFT);
			break;
		case 0xb6:	/* Right Shift	*/
			kb_flags = kb_flags & ~(RSHIFT);
			break;
		case 0xb8:	/* Left Alt	*/
			kb_flags = kb_flags & ~(LALT);
			break;
		}
		/* Otherwise this must be break code of some key. */
	}

	// schedule the interrupted thread or other 
	// high priority thread at this point
	cli_intr();
	current_thread->kt_schedinf.sc_state = THREAD_STATE_SLEEP;
	enable_irq(1);
	scheduler();
}

static short int check_caps_lock(short int scan_ascii, int cur_code)
{
	if ((kb_flags & CAPSLOCK) && 
		((scan_ascii & 0x00ff) >= 0x61) &&
		((scan_ascii & 0x00ff) <= 0x7a)) 
		scan_ascii = key_table[cur_code-1][1];
	else if ((kb_flags & CAPSLOCK) && 
		((scan_ascii & 0x00ff) >= 0x41) &&
		((scan_ascii & 0x00ff) <= 0x5a)) 
		scan_ascii = key_table[cur_code-1][0];
	return scan_ascii;
}

int insertkb_char(int c)
{
	int kbhead;
	unsigned short scan_ascii;
	int retval;

	scan_ascii = ((c < 32) ? (c << 8) : (c | (c << 8)));

	// KBD buff is full ?
	_lock(&kbd_focus->proc_inputinf->input_buf_lock);
	kbhead = kbd_focus->proc_inputinf->kbhead - 1;

	if (kbhead < 0) kbhead = KB_BUF_SIZE - 1;

	if (kbd_focus->proc_inputinf->kbtail != kbhead) 
	{
		kbd_focus->proc_inputinf->input_buf[kbd_focus->proc_inputinf->kbhead] = scan_ascii;
		kbd_focus->proc_inputinf->kbhead = kbhead;
		retval = c;
	}
	else retval = -1;
	event_wakeup(&kbd_focus->proc_inputinf->input_available);
	unlock(&kbd_focus->proc_inputinf->input_buf_lock);
	return retval;
}

static void input_insert(unsigned short scan_ascii)
{
	// Check for kbd focus switch (alt+a)
	if (scan_ascii == 0x1E00)
	{
		change_kbd_focus();
		return;
	}

	if (scan_ascii == 0x1E01)
	{
#ifdef DEBUG
		display_thread_status();	
#endif
		return;
	}


	// KBD buff is full ?
	_lock(&kbd_focus->proc_inputinf->input_buf_lock);
	if ((kbd_focus->proc_inputinf->kbtail+1)%KB_BUF_SIZE != kbd_focus->proc_inputinf->kbhead) 
	{
		kbd_focus->proc_inputinf->input_buf[kbd_focus->proc_inputinf->kbtail] = scan_ascii;
		kbd_focus->proc_inputinf->kbtail = (kbd_focus->proc_inputinf->kbtail + 1) % KB_BUF_SIZE;
	}
	event_wakeup(&kbd_focus->proc_inputinf->input_available);
	unlock(&kbd_focus->proc_inputinf->input_buf_lock);
	// may be waiting for input, wakeup all threads of kbd_focus process.
	return;
}

static void change_kbd_focus()
{
	struct process *p1;

	_lock(&kbd_wait_lock);
	p1 = (struct process *)kbd_focus->proc_next;
	if (p1 == NULL)
		p1 = (struct process *)used_processes;
	change_screen((struct process *)p1);
	kbd_focus = p1;
	unlock(&kbd_wait_lock);
	
	return;
}

static void scroll_on(void)
{
	unlock(&kbd_focus->proc_outputinf->output_lock);	
	return;
}

int syscall_getchar(void)
{
	unsigned short int code;
	char asc;

	code = kbd_getchar();

	if((code & 0xff)!=0)
	{
		asc = code & 0xff;
		output_proc(&asc, 1);
	}

	return code;
}

int syscall_getchare(void) { return kbd_getchar() & 0xff; }

int syscall_getscanchar(void) { return kbd_getchar(); }

int kbd_read(char *buf, int nbytes)
{
	//short int char_code;
	int n = 0;

	// KBD buffer empty?
	_lock(&current_process->proc_inputinf->input_buf_lock);
kbd_read_lab1:	
	if (current_process->proc_inputinf->kbtail == current_process->proc_inputinf->kbhead)
	{
		event_sleep(&current_process->proc_inputinf->input_available, &current_process->proc_inputinf->input_buf_lock);
				
		goto kbd_read_lab1;
	}

	// input available
	while ((current_process->proc_inputinf->kbtail != current_process->proc_inputinf->kbhead) && (n < nbytes))
	{
		buf[n] = current_process->proc_inputinf->input_buf[kbd_focus->proc_inputinf->kbhead] & 0xFF;
		n++;
		current_process->proc_inputinf->kbhead = (current_process->proc_inputinf->kbhead + 1) % KB_BUF_SIZE;
	}
	unlock(&current_process->proc_inputinf->input_buf_lock);

	return n;
}

static unsigned short int kbd_getchar(void)
{
	short int char_code;

	// KBD buffer empty?
	_lock(&current_process->proc_inputinf->input_buf_lock);
kbd_getchar_lab1:	
	if (current_process->proc_inputinf->kbtail == current_process->proc_inputinf->kbhead)
	{
		event_sleep(&current_process->proc_inputinf->input_available, &current_process->proc_inputinf->input_buf_lock);
				
		goto kbd_getchar_lab1;
	}

	// input available
	char_code = current_process->proc_inputinf->input_buf[kbd_focus->proc_inputinf->kbhead];
	current_process->proc_inputinf->kbhead = (current_process->proc_inputinf->kbhead + 1) % KB_BUF_SIZE;
	unlock(&current_process->proc_inputinf->input_buf_lock);

	return char_code;
}
static void scroll_off(void)
{
	_lock(&kbd_focus->proc_outputinf->output_lock);	
	return;
}
/* NOT COMPLETE */
static void send_ctrl_break(void) { return; }
void initialize_keyboard(void)
{
	lockobj_init(&kbd_wait_lock);
	intr_threads[1] = kbd_thread = initialize_irqtask(1, "KBD", irq_entry_0_1, IT_PRIORITY_MIN + 10);
}

#ifdef DEBUG
extern volatile struct frame_buffer *rx_q_head, *rx_q_tail, *tx_q_head, *tx_q_tail;
extern snic nic;

void display_sockets(void);
static void display_thread_status(void)
{
	volatile struct kthread *t;
	struct sleep_q *slq;
	extern struct lock_t kmalloc_lock, socklist_lock, txq_lock, rxq_lock, timerq_lock; 
	extern struct lock_t socklist_lock, timerq_lock; 
	extern frame_buffer *tx_wait_pkt;
	extern volatile unsigned int secs, nicwait_start;
	int i;

	printk("<socklist_lock: %x, (%hd, %x, %x, %x, %x), (%x, %s), %hd >",&socklist_lock, socklist_lock.l_slq.sl_bitval,socklist_lock.l_slq.sl_head,socklist_lock.l_slq.sl_tail,socklist_lock.l_slq.sl_next,socklist_lock.l_slq.sl_prev,socklist_lock.l_owner_thread, ((struct kthread *)(socklist_lock.l_owner_thread))->kt_name, socklist_lock.l_val);
	printk("<rxq_lock: %x, (%hd, %x, %x, %x, %x), (%x, %s), %hd >",&rxq_lock, rxq_lock.l_slq.sl_bitval,rxq_lock.l_slq.sl_head,rxq_lock.l_slq.sl_tail,rxq_lock.l_slq.sl_next,rxq_lock.l_slq.sl_prev,rxq_lock.l_owner_thread, ((struct kthread *)(rxq_lock.l_owner_thread))->kt_name, rxq_lock.l_val);
	printk("<txq_lock: %x, (%hd, %x, %x, %x, %x), (%x, %s), %hd >",&txq_lock, txq_lock.l_slq.sl_bitval,txq_lock.l_slq.sl_head,txq_lock.l_slq.sl_tail,txq_lock.l_slq.sl_next,txq_lock.l_slq.sl_prev,txq_lock.l_owner_thread, ((struct kthread *)(txq_lock.l_owner_thread))->kt_name, txq_lock.l_val);
	printk("<timerq_lock: %x, (%hd, %x, %x, %x, %x), (%x, %s), %hd >",&timerq_lock, timerq_lock.l_slq.sl_bitval,timerq_lock.l_slq.sl_head,timerq_lock.l_slq.sl_tail,timerq_lock.l_slq.sl_next,timerq_lock.l_slq.sl_prev,timerq_lock.l_owner_thread, ((struct kthread *)(timerq_lock.l_owner_thread))->kt_name, timerq_lock.l_val);
	printk("<kmalloc_lock: %x, (%hd, %x, %x, %x, %x), (%x, %s), %hd >",&kmalloc_lock, kmalloc_lock.l_slq.sl_bitval,kmalloc_lock.l_slq.sl_head,kmalloc_lock.l_slq.sl_tail,kmalloc_lock.l_slq.sl_next,kmalloc_lock.l_slq.sl_prev,kmalloc_lock.l_owner_thread, ((struct kthread *)(kmalloc_lock.l_owner_thread))->kt_name, kmalloc_lock.l_val);
	t = used_threads;
	i = 0;	
	while (t != NULL)
	{
		printk("TASK [%s]->%d, %x, %x, %x eip : %x, pri : %d, %d, %d q: %d ",t->kt_name, t->kt_schedinf.sc_state, t->kt_slpchannel, t->synchtry, t->synchobtained, t->kt_tss->eip, t->kt_schedinf.sc_usrpri, t->kt_schedinf.sc_cpupri, t->kt_schedinf.sc_inhpri, i);
		slq = (struct sleep_q *)t->kt_synchobjlist;
		while (slq != NULL)
		{
			printk("%x-%d,",slq,slq->sl_type);
			slq = (struct sleep_q *)slq->sl_next;
		}
		t = t->kt_uselist;
	}
	t = ttt;
	printk("TASK [%s]->%d, %x, %x, %x eip : %x, pri : %d, %d, %d q: %d ",t->kt_name, t->kt_schedinf.sc_state, t->kt_slpchannel, t->synchtry, t->synchobtained, t->kt_tss->eip, t->kt_schedinf.sc_usrpri, t->kt_schedinf.sc_cpupri, t->kt_schedinf.sc_inhpri, i);

	for (i=0; i<MAXREADYQ; i++)
	{
	t = ready_qhead[i];
	while (t != NULL)
	{
		printk("TASK [%s]->%d, %x, %x, %x, eip : %x, pri : %d, %d, %d q: %d",t->kt_name, t->kt_schedinf.sc_state, t->kt_slpchannel, t->synchtry, t->synchobtained, t->kt_tss->eip, t->kt_schedinf.sc_usrpri, t->kt_schedinf.sc_cpupri, t->kt_schedinf.sc_inhpri, i);
		slq = (struct sleep_q *)t->kt_synchobjlist;
		while (slq != NULL)
		{
			printk("%x-%d,",slq,slq->sl_type);
			slq = (struct sleep_q *)slq->sl_next;
		}
		t = t->kt_qnext;
	}
	}

	printk("\ntx_q head : %x, tx_q_tail : %x, rx_q_head : %x, rx_q_tail : %x ",tx_q_head,tx_q_tail, rx_q_head, rx_q_tail);
	printk("tx_wait_pkt : %x, nicwait_start: %d , secs : %d, nic->tx_frame : %x ",tx_wait_pkt, nicwait_start, secs, nic.tx_frame);
}

#endif
