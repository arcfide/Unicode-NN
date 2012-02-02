/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Accounting for news reading.
 *
 *	The nnacct program is called by nn in three cases:
 *
 *	- on startup (-q) to check for permission to run nn (at this time)
 *	- when the :cost command is executed (-cUSAGE -r) to
 *	  produce a "cost sofar" report, and
 *	- at exit (-uUSAGE -r) to add the USAGE to the user's account
 *	  and print a "cost" report.
 *
 *	It can also be invoked by nnusage to print a usage and cost
 *	report for the current user (default), or by the super user
 *	to produce a usage and cost report for all users.
 */

#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include "config.h"
#include "global.h"
#include "account.h"
#include "execute.h"
#include "options.h"
#include "proto.h"

extern char    *lib_directory;
extern int      errno;

struct account {
    off_t           ac_offset;	/* offset in acct file */
    int             ac_found;	/* present in acct file */

    char            ac_user[24];/* user name */

    long            ac_total;	/* total usage */
    time_t          ac_last;	/* last active */
    int             ac_policy;	/* assigned policy */
    int             ac_quota;	/* time quota */
};

/* account.c */

static void     put_entry(FILE * acctf, struct account * ac);
static int      get_entry(FILE * acctf, char *user, struct account * ac);
static void     do_cost(struct account * ac, int ses);
static void     do_report(struct account * ac, int hdr);
static void     do_report_all(FILE * acctf);
static void     do_zero(void);
static int      news_admin(char *caller);

/*
 * 	local authorization policy checking
 *
 *	return/exit values of policy_check (nnacct -P0) are:
 *
 *	0: access granted
 *	1: access granted, but cannot post
 *	2: access denied (not authorized)
 *	3: access denied (not allowed at this time of day)
 *	4: access denied (quota exceeded)
 */

#ifdef AUTHORIZE
#include <time.h>

static int
holiday(struct tm * tm)
{
    /* Kim's birthday - 23 April */
    if (tm->tm_mon == 3 && tm->tm_mday == 23)
	return 1;
    /* another birthday - 25 December :-) */
    if (tm->tm_mon == 11 && tm->tm_mday == 25)
	return 1;
    /* ... */
    return 0;
}

static int
policy_check(int policy)
{
    struct tm      *tm, *localtime();
    time_t          t;
    int             no_post = 0;

    if (policy >= NO_POST) {
	policy -= NO_POST;
	no_post = 1;
    }
    switch (policy % 10) {
	case DENY_ACCESS:
	    return 2;

	case ALL_HOURS:
	case FREE_ACCOUNT:
	    break;

	case OFF_HOURS:	/* adapt this to your local requirements */
	    time(&t);
	    tm = localtime(&t);
	    if (tm->tm_wday == 0)
		break;		/* Sunday */
	    if (tm->tm_wday == 6)
		break;		/* Saturday */
	    if (tm->tm_hour < 9)
		break;		/* morning */
	    if (tm->tm_hour > 16)
		break;		/* evening */
	    if (holiday(tm))
		break;		/* holidays */
	    /* etc. */
	    return 3;

	default:
	    return 2;
    }

    return no_post;
}

#endif

static int      add_usage = -1;
static int      show_cost = -1;
static int      report = 0;
static int      report_all = 0;
static int      quiet = 0;
static char    *acct_file = NULL;
static int      ck_policy = -1;
static int      new_policy = -1;
static int      new_quota = -1;
static int      who_am_caller = I_AM_ACCT;
static char    *zero_accounts = NULL;

Option_Description(acct_options)
{
    'C', Int_Option(show_cost),
	'U', Int_Option(add_usage),
	'W', Int_Option(who_am_caller),
	'P', Int_Option(ck_policy),
	'a', Bool_Option(report_all),
	'f', String_Option(acct_file),
	'p', Int_Option(new_policy),
	'q', Int_Option(new_quota),
	'r', Bool_Option(report),
	'Q', Bool_Option(quiet),
	'Z', String_Option(zero_accounts),
	'\0',
};

/*
 *	Accounting information:
 *
 *	xxxxxxx 00000000 00000000 00 00000\n
 *
 *	login	time used last	 pol quota
 *	name	(minutes) active icy (hours)
 *
 *	See the printf/scanf formats later on.
 */


#define INPUT_FMT	"%s %ld %ld %d %d\n"
#define OUTPUT_FMT	"%s %08ld %08lx %02d %05d\n"

static int
get_entry(FILE * acctf, char *user, struct account * ac)
{
    char            line[100];

    if (acctf != NULL && user != NULL)
	rewind(acctf);

    ac->ac_found = 0;

    for (;;) {
	ac->ac_policy = DEFAULT_POLICY;
	ac->ac_last = 0;
	ac->ac_total = 0;
	ac->ac_quota = DEFAULT_QUOTA;
	ac->ac_user[0] = NUL;

	if (acctf == NULL)
	    break;

	ac->ac_offset = ftell(acctf);

	if (fgets(line, 100, acctf) == NULL)
	    break;

	sscanf(line, INPUT_FMT,
	       ac->ac_user, &ac->ac_total, &ac->ac_last,
	       &ac->ac_policy, &ac->ac_quota);

	if (user == NULL)
	    return 1;

	if (strcmp(user, ac->ac_user) == 0) {
	    ac->ac_found = 1;
	    return 1;
	}
    }

    if (user != NULL)
	strcpy(ac->ac_user, user);
    return 0;
}

static void
put_entry(FILE * acctf, struct account * ac)
{
    if (ac->ac_found)
	fseek(acctf, ac->ac_offset, 0);
    else
	fseek(acctf, (off_t) 0, 2);

    fprintf(acctf, OUTPUT_FMT,
	    ac->ac_user, ac->ac_total, ac->ac_last,
	    ac->ac_policy, ac->ac_quota);
}

static void
do_cost(struct account * ac, int ses)
{
    long            r;

#ifdef COST_PER_MINUTE

#ifndef COST_UNIT
#define COST_UNIT ""
#endif

    if (ses >= 0)
	printf("Cost this session: %ld %s   Period total:",
	       ((long) ses * COST_PER_MINUTE) / 100, COST_UNIT);
    printf("%6ld %s",
	   (ac->ac_total * COST_PER_MINUTE) / 100, COST_UNIT);
#else
    printf("Time used this session: %d:%02d   Period total: %ld:%02ld",
	   ses / 60, ses % 60, ac->ac_total / 60, ac->ac_total % 60);
#endif

    if (ses >= 0 && ac->ac_quota > 0) {
	r = ac->ac_quota - ac->ac_total / 60;
	printf("  Quota: %ld hour%s", r, plural(r));
    }
    fl;
}

static void
do_report(struct account * ac, int hdr)
{
    if (hdr) {
	printf("USER        USAGE  QUOTA  LAST_ACTIVE");

#ifdef COST_PER_MINUTE
	printf("   COST/PERIOD");
#endif

#ifdef AUTHORIZE
	printf("   POLICY");
#endif

	putchar(NL);
    }
    printf("%-8.8s  %4ld.%02ld  %5d  %-12.12s  ",
	   ac->ac_user,
	   ac->ac_total / 60, ac->ac_total % 60,
	   ac->ac_quota,
	   ac->ac_last ? date_time(ac->ac_last) : "");

#ifdef COST_PER_MINUTE
    do_cost(ac, -1);
#endif

#ifdef AUTHORIZE
    printf("     %2d  ", ac->ac_policy);
#endif

    printf("\n");
}

static void
do_report_all(FILE * acctf)
{
    struct account  ac;
    int             first = 1;

    while (get_entry(acctf, (char *) NULL, &ac)) {
	do_report(&ac, first);
	first = 0;
    }
}

static char    *ZERO_STAMP = "(Zeroed)";

static void
do_zero(void)
{
    FILE           *old, *new;
    char           *acct, bak[FILENAME];
    struct account  ac;

    acct = relative(lib_directory, "acct");
    old = open_file(acct, OPEN_READ);
    if (old == NULL)
	goto err;

    sprintf(bak, "%s.old", acct);
    if (unlink(bak) < 0 && errno != ENOENT)
	goto err;
    if (link(acct, bak) < 0)
	goto err;
    if (unlink(acct) < 0) {
	unlink(bak);
	goto err;
    }
    umask(0177);
    new = open_file(acct, OPEN_CREATE);
    if (new == NULL)
	goto err2;
    ac.ac_found = 0;
    strcpy(ac.ac_user, ZERO_STAMP);
    ac.ac_total = ac.ac_policy = ac.ac_quota = 0;
    time(&(ac.ac_last));
    put_entry(new, &ac);

    while (get_entry(old, (char *) NULL, &ac)) {
	if (strcmp(ac.ac_user, ZERO_STAMP) == 0)
	    continue;
	ac.ac_total = 0;
	ac.ac_found = 0;
	put_entry(new, &ac);
    }
    fclose(old);
    if (fclose(new) == EOF)
	goto err2;
    return;

err2:
    unlink(acct);
    if (link(bak, acct) == 0)
	unlink(bak);
err:
    fprintf(stderr, "ZERO of accounts failed -- check permissions etc.\n");
}

static
int
news_admin(char *caller)
{
    FILE           *adm;
    char            line[80];
    int             len, ok;

    adm = open_file(relative(lib_directory, "admins"), OPEN_READ);
    if (adm == NULL)
	return 2;
    len = strlen(caller);
    ok = 0;

    while (fgets(line, 80, adm)) {
	if (line[len] != NL)
	    continue;
	line[len] = NUL;
	if (strcmp(caller, line) == 0) {
	    ok = 1;
	    break;
	}
    }
    fclose(adm);
    return ok;
}

int
main(int argc, char *argv[])
{
    char           *caller;
    FILE           *acctf;
    char           *fname = NULL;
    int             users, i;
    struct account  ac, *actab = NULL;

    who_am_i = I_AM_ACCT;

    init_global();

    users = parse_options(argc, argv, (char *) NULL, acct_options, "");

    if (zero_accounts && strcmp(zero_accounts, "ERO")) {
	fprintf(stderr, "Must specify -ZERO to clear accounts\n");
	exit(1);
    }
    if (user_id != 0) {
	caller = user_name();
	if (news_admin(caller) == 1)
	    goto caller_ok;

	if (report_all) {
	    fprintf(stderr, "Only root can request complete reports\n");
	    exit(9);
	}
	if (new_policy >= 0) {
	    fprintf(stderr, "Only root can change user authorization\n");
	    exit(9);
	}
	if (new_quota >= 0) {
	    fprintf(stderr, "Only root can change user quotas\n");
	    exit(9);
	}
	if (zero_accounts) {
	    fprintf(stderr, "Only root can zero user accounts\n");
	    exit(9);
	}
	if (users > 0) {
	    fprintf(stderr, "Only root can request reports for other users\n");
	    exit(9);
	}
    } else
	caller = "root";

caller_ok:
    if ((new_policy >= 0 || new_quota >= 0) && users == 0) {
	fprintf(stderr, "usage: %s -pPOLICY -qQUOTA user...\n", argv[0]);
	exit(1);
    }
    if (add_usage == 0 && report) {
	show_cost = 0;
	add_usage = -1;
    }
    if (zero_accounts || add_usage > 0 || new_policy >= 0 || new_quota >= 0) {
	if (acct_file) {
	    fprintf(stderr, "Can only update current acct file %s\n", acct_file);
	    exit(2);
	}
	proto_lock(I_AM_ACCT, PL_SET_QUICK);
    }
    if (zero_accounts) {
	do_zero();
	proto_lock(I_AM_ACCT, PL_CLEAR);
	exit(0);
    }
    if (acct_file) {
	if ((acctf = open_file(acct_file, OPEN_READ)) == NULL)
	    acctf = open_file(relative(lib_directory, acct_file), OPEN_READ);
	if (acctf == NULL) {
	    fprintf(stderr, "Accounting file %s not found\n", acct_file);
	    if (add_usage > 0 || new_policy >= 0 || new_quota >= 0)
		proto_lock(I_AM_ACCT, PL_CLEAR);
	    exit(1);
	}
    } else {
	fname = relative(lib_directory, "acct");
	acctf = open_file(fname, OPEN_READ);
    }

    if (report_all) {
	do_report_all(acctf);
	fclose(acctf);
	exit(0);
    }
    if (ck_policy >= 0) {

#ifdef AUTHORIZE
	get_entry(acctf, caller, &ac);

#ifdef ACCOUNTING
	if (ac.ac_quota > 0 && ac.ac_quota < ac.ac_total / 60)
	    exit(4);
#endif

	exit(policy_check(ac.ac_policy));
#else
	exit(0);
#endif
    }
    if (show_cost >= 0) {
	get_entry(acctf, caller, &ac);
	if (ac.ac_policy == FREE_ACCOUNT)
	    exit(0);
	ac.ac_total += show_cost;
	do_cost(&ac, show_cost);
	exit(0);
    }
    if (add_usage > 0) {
	get_entry(acctf, caller, &ac);
	if (ac.ac_policy == FREE_ACCOUNT)
	    goto unlock;
	ac.ac_total += add_usage;
	time(&ac.ac_last);
    } else if (users > 0) {
	actab = newobj(struct account, users + 1);
	for (i = 1; i <= users; i++) {
	    get_entry(acctf, argv[i], &actab[i]);
	    if (new_policy >= 0 || new_quota >= 0) {
		if (new_policy >= 0)
		    actab[i].ac_policy = new_policy;
		if (new_quota >= 0)
		    actab[i].ac_quota = new_quota;
	    } else
		do_report(&actab[i], i == 1);
	}
    } else if (report) {
	if (get_entry(acctf, caller, &ac))
	    do_report(&ac, 1);
	exit(0);
    }
    if (acctf)
	fclose(acctf);

    if (add_usage <= 0 && new_policy < 0 && new_quota < 0)
	exit(0);

    umask(0177);
    acctf = open_file(fname, OPEN_UPDATE | MUST_EXIST);

    if (new_policy >= 0 || new_quota >= 0) {
	for (i = 1; i <= users; i++)
	    put_entry(acctf, &actab[i]);
	fclose(acctf);
	goto unlock;
    }
    if (add_usage > 0) {
	put_entry(acctf, &ac);
	if (report) {
	    do_cost(&ac, add_usage);
	}
	fclose(acctf);

#ifdef ACCTLOG
	fname = relative(lib_directory, "acctlog");
	acctf = open_file(fname, OPEN_APPEND | MUST_EXIST);
	fprintf(acctf, "%s\t%s\t%ld\n",
		caller, date_time(ac.ac_last), (long) add_usage);
	fclose(acctf);
#endif

	goto unlock;
    }
unlock:
    proto_lock(I_AM_ACCT, PL_CLEAR);
    exit(0);
    /* NOTREACHED */
}

void
nn_exit(int n)
{
    exit(n);
}

void
nn_exitmsg(int n, char *fmt,...)
{
    va_list         ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    putchar(NL);
    va_end(ap);

    nn_exit(n);
    /* NOTREACHED */
}

/*
 * dummy routines - should never be called by nnacct
 */

int             no_update = 0;

int
set_variable(char *variable, int on, char *val_string)
{
    return 0;
}

void
msg(char *fmt,...)
{
}


#ifdef HAVE_JOBCONTROL
int
suspend_nn(void)
{
    return 0;
}

#endif
