/*
 *      Copyright (c) 1996-2003 Michael T Pins.  All rights reserved.
 */

/*
 *      Accumulated accounting information is saved in the file $LIB/acct.
 *      If ACCTLOG is defined, a sequential log is maintained in $LIB/acctlog.
 *
 *      You can define a COST_PER_MINUTE and a COST_UNIT to make
 *      the user get a cost calculation (maybe just for the fun of it -
 *      you can't imagine how expensive it is to read news here :-).
 *
 *      The COST_PER_MINUTE should be the price per minute multiplied
 *      by 100 (to allow prices like $0.03/minute).
 *      The definitions below corresponds to 1 dollar per minute.
 *
 *      If COST_PER_MINUTE is not defined, a "Time used" rather than a
 *      "Cost" report is produced.
 */

/* #define ACCTLOG	*/

 /* #define COST_PER_MINUTE	100	*//* price(in UNITs)/min * 100 */
 /* #define COST_UNIT		"dollars"	*//* Currency */

/*
 *      DEFAULT POLICY AND QUOTA FOR NEW USERS
 *
 *      Notice that QUOTA is measued in hours.
 *      Both ACCOUNTING and AUTHORIZE must be defined for
 *      the quota mechanism to work.
 *      (See config.h)
 */

/*
 *	Don't change these.
 */

#define DENY_ACCESS     0
#define FREE_ACCOUNT    1
#define ALL_HOURS       2
#define OFF_HOURS       3
#define NO_POST         40	/* add if cannot post */

/*
 *	Edit these to local needs.
 */

#define DEFAULT_POLICY  ALL_HOURS	/* all time w/accounting */
#define DEFAULT_QUOTA   0	/* unlimited use */
