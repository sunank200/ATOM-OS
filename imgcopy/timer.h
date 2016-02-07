#ifndef __TIMER_H__
#define __TIMER_H__

/* Date and time structure.	*/
struct date_time {
	short seconds; 
	short minutes; 
	short hours; 
	short date; 
	short month; 
	short year;
};

extern void enable_timer(void);
extern void tsleep(unsigned int msec);
extern unsigned int get_time(void);
extern void set_time(unsigned set_secs);
extern struct date_time secs_to_date(unsigned int secs);
extern unsigned int date_to_secs(struct date_time dt);
extern void timer_init(void);

#endif


