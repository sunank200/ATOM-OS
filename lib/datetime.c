#include "utimer.h"
#define NULL	0L

struct date_time secs_to_date(unsigned int secs)
{
	struct date_time dt;
	int days,mdays;
	int month_days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
	days = secs / (24 * 3600);
	secs = secs % (24 * 3600);
	dt.year = 1970  + 4 * (days / 1461);
	days = days % 1461;
	if (days > 1096) 
	{ 
		dt.year += 3; days -= 1096; 
	}
	else if (days > 730) 
	{
		dt.year += 2; days -= 730;
	}
	else if (days > 365) 
	{
		dt.year += 1;
		days -= 365;
	}

	if ((dt.year % 4) == 0)
		month_days[2] = 29;
	else
		month_days[2] = 28;
	dt.month = 1;
	mdays = 0;
	while ( mdays + month_days[dt.month] <= days ) 
		mdays += month_days[dt.month++];
	dt.date = days - mdays + 1;
	dt.hours = secs / (3600);
	secs = secs % (3600);
	dt.minutes = secs / (60);
	dt.seconds = secs % 60;

	return dt;
}

unsigned int date_to_secs(struct date_time dt)
{
	unsigned int secs = 0;
	int i,days;
	int month_days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

	if ((dt.year % 4) == 0) month_days[2] = 29;
	else month_days[2] = 28;
	secs += ((dt.year-1970)/4) * (1461 * 24 * 3600);
	dt.year = (dt.year - 1970) % 4;
	secs += dt.year * (365 * 24 * 3600);
	if (dt.year > 2) days = 1;
	else days = 0;
	for (i=1; i<dt.month; i++)
		days += month_days[i];
	days += dt.date -1; 
	secs += ((days * 24 * 3600) + ( dt.hours * 3600) +
		(dt.minutes * 60) + (dt.seconds)); 
	return secs;
}

char *asctime(const struct tm *tm)
{
	return NULL;
}

struct tm *localtime(const time_t *timep)
{
	return NULL;
}

