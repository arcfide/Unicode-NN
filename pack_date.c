/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Calculate an approximate "time_stamp" value for a date
 *	string.  The actual value is not at all critical,
 *	as long as the "ordering" is ok.
 *
 *	The result is NOT a time_t value, i.e. ctime() will
 *	not produce the original Date string.
 *
 *	The date must have format:  [...,] [D]D Mmm YY hh:mm:ss TZONE
 *
 *	Thanks to Wayne Davison for the timezone decoding code.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"

/* pack_date.c */

#undef W
#undef E
#undef DST
#undef UTC
#define W	* (-60) -
#define E	* 60 +
#define DST	+ 60
#define UTC	60 *

static struct zonetab {
    char           *tz_name;
    int             tz_offset;
}               ztab[] = {

    {
	"GMT", 0
    },				/* Greenwich Mean */
    {
	"UT", 0
    },				/* Universal */
    {
	"UTC", 0
    },				/* Universal Coordinated */
    {
	"CUT", 0
    },				/* Coordinated Universal */
    {
	"WET", 0
    },				/* Western Europe */
    {
	"BST", 0 DST
    },				/* British Summer */
    {
	"NST", 3 W 30
    },				/* Newfoundland Standard */
    {
	"NDT", 3 W 30 DST
    },				/* Newfoundland Daylight */
    {
	"AST", 4 W 0
    },				/* Atlantic Standard */
    {
	"ADT", 4 W 0 DST
    },				/* Atlantic Daylight */
    {
	"EST", 5 W 0
    },				/* Eastern Standard */
    {
	"EDT", 5 W 0 DST
    },				/* Eastern Daylight */
    {
	"CST", 6 W 0
    },				/* Central Standard */
    {
	"CDT", 6 W 0 DST
    },				/* Central Daylight */
    {
	"MST", 7 W 0
    },				/* Mountain Standard */
    {
	"MDT", 7 W 0 DST
    },				/* Mountain Daylight */
    {
	"PST", 8 W 0
    },				/* Pacific Standard */
    {
	"PDT", 8 W 0 DST
    },				/* Pacific Daylight */
    {
	"YST", 9 W 0
    },				/* Yukon Standard */
    {
	"YDT", 9 W 0 DST
    },				/* Yukon Daylight */
    {
	"AKST", 9 W 0
    },				/* Alaska Standard */
    {
	"AKDT", 9 W 0 DST
    },				/* Alaska Daylight */
    {
	"HST", 10 W 0
    },				/* Hawaii Standard */
    {
	"HDT", 10 W 0 DST
    },				/* Hawaii Daylight */
    {
	"HAST", 10 W 0
    },				/* Hawaii-Aleutian Standard */
    {
	"HADT", 10 W 0 DST
    },				/* Hawaii-Aleutian Daylight */
    {
	"CET", 1 E 0
    },				/* Central European */
    {
	"CES", 1 E 0 DST
    },				/* Central European Summer */
    {
	"MET", 1 E 0
    },				/* Middle European */
    {
	"MES", 1 E 0 DST
    },				/* Middle European Summer */
    {
	"MEWT", 1 E 0
    },				/* Middle European Winter */
    {
	"MEST", 1 E 0 DST
    },				/* Middle European Summer */
    {
	"EET", 2 E 0
    },				/* Eastern Europe */
    {
	"MSK", 3 E 0
    },				/* Moscow Winter */
    {
	"MSD", 3 E 0 DST
    },				/* Moscow Summer */
    {
	"WAST", 8 E 0
    },				/* West Australian Standard */
    {
	"WADT", 8 E 0 DST
    },				/* West Australian Daylight */
    {
	"HKT", 8 E 0
    },				/* Hong Kong */
    {
	"CCT", 8 E 0
    },				/* China Coast */
    {
	"JST", 9 E 0
    },				/* Japan Standard */
    {
	"KST", 9 E 0
    },				/* Korean Standard */
    {
	"KST", 9 E 0 DST
    },				/* Korean Daylight */
    {
	"CAST", 9 E 30
    },				/* Central Australian Standard */
    {
	"CADT", 9 E 30 DST
    },				/* Central Australian Daylight */
    {
	"EAST", 10 E 0
    },				/* Eastern Australian Standard */
    {
	"EADT", 10 E 0 DST
    },				/* Eastern Australian Daylight */
    {
	"NZST", 12 E 0
    },				/* New Zealand Standard */
    {
	"NZDT", 12 E 0 DST
    },				/* New Zealand Daylight */
    {
	"A", UTC 1
    },				/* UTC+1h */
    {
	"B", UTC 2
    },				/* UTC+2h */
    {
	"C", UTC 3
    },				/* UTC+3h */
    {
	"D", UTC 4
    },				/* UTC+4h */
    {
	"E", UTC 5
    },				/* UTC+5h */
    {
	"F", UTC 6
    },				/* UTC+6h */
    {
	"G", UTC 7
    },				/* UTC+7h */
    {
	"H", UTC 8
    },				/* UTC+8h */
    {
	"I", UTC 9
    },				/* UTC+9h */
    {
	"K", UTC 10
    },				/* UTC+10h */
    {
	"L", UTC 11
    },				/* UTC+11h */
    {
	"M", UTC 12
    },				/* UTC+12h */
    {
	"N", UTC - 1
    },				/* UTC-1h */
    {
	"O", UTC - 2
    },				/* UTC-2h */
    {
	"P", UTC - 3
    },				/* UTC-3h */
    {
	"Q", UTC - 4
    },				/* UTC-4h */
    {
	"R", UTC - 5
    },				/* UTC-5h */
    {
	"S", UTC - 6
    },				/* UTC-6h */
    {
	"T", UTC - 7
    },				/* UTC-7h */
    {
	"U", UTC - 8
    },				/* UTC-8h */
    {
	"V", UTC - 9
    },				/* UTC-9h */
    {
	"W", UTC - 10
    },				/* UTC-10h */
    {
	"X", UTC - 11
    },				/* UTC-11h */
    {
	"Y", UTC - 12
    },				/* UTC-12h */
    {
	"Z", 0
    },				/* Greenwich Mean */
    {
	NULL, 0
    },
};

#undef MAXZ
#define MAXZ	6

static int      month_table[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#define leap_year(y) (((y)&3) == 0  &&  ((y)%100 != 0 || (y)%400 == 0))


/*
 * month_days
 *
 *	Returns:	How many days in the month.
 */

static int
month_days(int year, int month)
{
    return month_table[month] + (month == 1 && leap_year(year));
}


/*
 * numeric_zone
 *
 *	Parameters:	"date" is the numeric offset	{+-}[H]H[MM]
 *
 *	Returns:	number of minutes offset from GMT
 */

static int
numeric_zone(register char *date)
{
    register int    n;
    static char     num[MAXZ];
    int             adjust = 0, sign;

    switch (*date) {
	case '-':
	    date++;
	    sign = -1;
	    break;
	case '+':
	    date++;
	default:		/* FALLTHROUGH */
	    sign = 1;
	    break;
    }

    for (n = 0; n < MAXZ && *date && isdigit((int) *date);)
	num[n++] = *date++;
    num[n] = '\0';

    switch (n) {
	case 4:		/* +HHMM */
	    adjust = atoi(num + 2);
	    num[2] = '\0';
	case 2:		/* +HH *//* FALLTHROUGH */
	    adjust += atoi(num) * 60;
	    break;
	case 3:		/* +HMM */
	    adjust = atoi(num + 1);
	    num[1] = '\0';
	case 1:		/* +H *//* FALLTHROUGH */
	    adjust += atoi(num) * 60;
	    break;
	default:		/* bad form */
	    break;
    }
    adjust *= sign;
    return adjust;
}


/*
 * tzone
 *
 *	Paremeters:	"date" is the strings containing TIMEZONE info
 *
 *	Returns:	number of minutes offset from GMT
 */

static int
tzone(register char *date)
{
    register int    i = 0;
    static char     zone[MAXZ];
    register struct zonetab *z;

    while (*date && isspace((int) *date))
	date++;

    if (*date == '+' || *date == '-' || isdigit((int) *date))
	return numeric_zone(date);

    for (; *date && isascii(*date); date++) {
	if (isspace((int) *date))
	    break;
	if (!isalnum((int) *date))
	    continue;		/* p.s.t. -> pst */
	if (i == MAXZ)
	    continue;
	zone[i++] = islower((int) *date) ? toupper((int) *date) : *date;
    }

    while (*date && isspace((int) *date))
	date++;
    if (i == 0)
	return 0;
    if (*date == '+' || *date == '-' || isdigit((int) *date))
	return numeric_zone(date);

    zone[i] = '\0';

    for (z = ztab; z->tz_name != NULL; z++) {
	i = strcmp(zone, z->tz_name);
	if (i != 0)
	    continue;
	return z->tz_offset;
    }
    return 0;
}


/*
 * next_int
 *
 *	Parameters:	"dp" is the string to process
 *
 *	Returns:	the integer value of the first "number"
 *			or 0 if none
 *
 *	Side Effects:	moves *dp to the char after "number"
 */

static int
next_int(char **dp)
{
    register char  *str = *dp;
    register int    i;

    while (*str && !isdigit((int) *str))
	str++;

    i = atoi(str);
    while (*str && isdigit((int) *str))
	str++;

    *dp = str;
    return i;
}

/*
 * pack_date
 *
 *	Parameters:	"date" is the date string to be parsed
 *
 *	Returns:	roughly the number of seconds since the beginning
 *			of the epoch to "date"
 */

time_stamp
pack_date(char *date)
{
    register int    sec, min, hour, day, month, year, i;

    if (date == NULL || (day = next_int(&date)) == 0)
	return 0;

    while (*date && isspace((int) *date))
	date++;

    if (date[0] == '\0' || date[1] == '\0' || date[2] == '\0')
	return 0;
    switch (date[0]) {
	case 'J':
	case 'j':
	    if (date[1] == 'a' || date[1] == 'A') {
		month = 0;
		break;
	    }
	    if (date[2] == 'n' || date[2] == 'N') {
		month = 5;
		break;
	    }
	    month = 6;
	    break;
	case 'F':
	case 'f':
	    month = 1;
	    break;
	case 'M':
	case 'm':
	    if (date[2] == 'r' || date[2] == 'R') {
		month = 2;
		break;
	    }
	    month = 4;
	    break;
	case 'A':
	case 'a':
	    if (date[1] == 'p' || date[1] == 'P') {
		month = 3;
		break;
	    }
	    month = 7;
	    break;
	case 'S':
	case 's':
	    month = 8;
	    break;
	case 'O':
	case 'o':
	    month = 9;
	    break;
	case 'N':
	case 'n':
	    month = 10;
	    break;
	case 'D':
	case 'd':
	    month = 11;
	    break;
	default:
	    return 0;
    }

    year = next_int(&date);
    hour = next_int(&date);
    min = next_int(&date);
    if (*date == ':')
	sec = next_int(&date);
    else
	sec = 0;

    if (year <= 1000)
	year += 1900;		/* YY -> xxYY */
    if (year < 1970)
	year += 100;		/* must be after 1999 */

    /* Set `min' to be the number of minutes after midnight UTC.  */
    min += hour * 60 - tzone(date);
    for (; min < 0; min += 24 * 60)
	if (--day <= 0) {
	    if (--month < 0) {
		--year;
		month = 11;
	    }
	    day = month_days(year, month);
	}
    for (; 24 * 60 <= min; min -= 24 * 60)
	if (month_days(year, month) < ++day) {
	    if (11 < ++month) {
		++year;
		month = 0;
	    }
	    day = 1;
	}
    day += (year - 1970) * 366;
    for (i = 0; i < month; i++)
	day += month_days(year, i);
    --day;

    return (day * 24 * 60 * 60) + (min * 60) + sec;
}
