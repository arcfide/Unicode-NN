/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	nn database daemon (nnmaster)
 *
 *	maintains the article header database.
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "config.h"
#include "global.h"
#include "master.h"
#include "active.h"
#include "collect.h"
#include "db.h"
#include "digest.h"
#include "execute.h"
#include "expire.h"
#include "nntp.h"
#include "options.h"
#include "proto.h"

/* master.c */

static void     clean_group_internal(register group_header * gh);
static void     group_restriction(register group_header * gh);
static void     set_group_restrictions(char **restr, int n);
static void     visit_active_file(void);
static void     build_master(void);
static void     set_lock_message(void);
static void     do_reread_groups(void);
static int      receive_admin(void);

extern char    *bin_directory;
extern char     proto_host[];

/*
 * nnmaster options:
 *
 *	-e [N]	expire a group if more than N articles are gone
 *	-r N	repeat every N minutes
 *	-h N	don't update if active file is less than N *seconds* old
 *
 *	-f	foreground execution (use with -r)
 *	-y N	retry N times on error
 *
 *	-E [N]	expire mode (see expire.c) [N==1 if omitted]
 *	-F	Run ONLY expire ONCE and exit.
 *	-R N	auto-recollect mode (see expire.c)
 *
 *	-C	check consistency of database on start-up
 *	-b	include 'bad' articles (disables -B)
 *	-B	remove 'bad' articles (just unlink the files)
 *	-O N	Consider articles older than N days as bad articles.
 *
 *	-I [N]	initialize [ limit to N articles in each group ]
 *	-G	reread groups file.
 *	-X	clean ignored groups.
 *
 *	-l"MSG"	lock database with message MSG
 *	-l	unlock database
 *	-i	ignore lock (run collection on locked database)
 *	-k	kill the running master and take over.
 *
 *	-Q	quiet: don't write fatal errors to /dev/console (if no syslog).
 *	-t	trace collection of each group
 *	-v	print version and exit
 *	-u	update even if active is not modified
 *	-w	send wakeup to real master
 *	-Ltypes	exclude 'types' entries from the log
 *	-D [N]	debug, N = +(1 => verbose, 2 => nntp trace)
 *
 *	[master group]...	Collect these groups only.
 */


extern int      dont_write_console, expire_method, expire_level,
                mail_errors_mode, recollect_method, reread_groups_file,
                ignore_bad_articles, remove_bad_articles, retry_on_error;

#ifdef NNTP
extern int      nntp_local_server, nntp_debug;
#endif

extern          time_t max_article_age;

extern char    *log_entry_filter;

int             trace = 0, debug_mode = 0, Debug = 0, no_update = 0;

#ifdef NNTP
int             silent = 1;
#endif

int             Name_Length = 16;

static int      check_on_startup = 0, clean_ignored = 0, expire_once = 0,
                foreground = 0, ignore_lock = 0, initialize = -1,
                kill_running = 0, max_age_days = 0, prt_vers = 0,
                unconditional = 0, Unconditional = 0, wakeup_master = 0;

static unsigned hold_updates = 0, repeat_delay = 0;


static char    *lock_message = NULL;


static
Option_Description(master_options)
{

    'b', Bool_Option(ignore_bad_articles),
	'B', Bool_Option(remove_bad_articles),
	'C', Bool_Option(check_on_startup),
	'D', Int_Option_Optional(debug_mode, 1),
	'e', Int_Option_Optional(expire_level, 1),
	'E', Int_Option_Optional(expire_method, 1),
	'f', Bool_Option(foreground),
	'F', Bool_Option(expire_once),
	'G', Bool_Option(reread_groups_file),
	'h', Int_Option_Optional(hold_updates, 60),

#ifdef NNTP
	'H', Bool_Option(nntp_local_server),
#endif

	'i', Bool_Option(ignore_lock),
	'I', Int_Option_Optional(initialize, 0),
	'k', Bool_Option(kill_running),
	'l', String_Option_Optional(lock_message, ""),
	'L', String_Option(log_entry_filter),
	'M', Int_Option(mail_errors_mode),
	'O', Int_Option(max_age_days),
	'Q', Bool_Option(dont_write_console),
	'r', Int_Option_Optional(repeat_delay, 10),
	'R', Int_Option(recollect_method),
	't', Bool_Option(trace),
	'u', Bool_Option(unconditional),
	'U', Bool_Option(Unconditional),
	'v', Bool_Option(prt_vers),
	'w', Bool_Option(wakeup_master),
	'X', Bool_Option(clean_ignored),
	'y', Int_Option(retry_on_error),
	'\0',
};

extern char    *master_directory, *db_directory, *news_active;

static int      unlock_on_exit = 0;

/*
 * nn_exit() --- called whenever a program exits.
 */

void
nn_exit(int n)
{

#ifdef NNTP
    if (use_nntp)
	nntp_cleanup();
#endif				/* NNTP */

    close_master();

    if (unlock_on_exit)
	proto_lock(I_AM_MASTER, PL_CLEAR);

    if (n)
	log_entry('E', "Abnormal termination, exit=%d", n);
    else if (unlock_on_exit)
	log_entry('M', "Master terminated%s", s_hangup ? " (hangup)" : "");

    exit(n);
}


static void
clean_group_internal(register group_header * gh)
{				/* no write */
    gh->first_db_article = 0;
    gh->last_db_article = 0;

    gh->data_write_offset = 0;
    gh->index_write_offset = 0;

    if (init_group(gh)) {
	(void) open_data_file(gh, 'd', -1);
	(void) open_data_file(gh, 'x', -1);
    }
    gh->master_flag &= ~(M_EXPIRE | M_BLOCKED);
    if ((gh->master_flag & M_IGNORE_GROUP) == 0)
	gh->master_flag |= M_BLOCKED;

}

void
clean_group(group_header * gh)
{				/* does write */
    if (trace)
	log_entry('T', "CLEAN %s", gh->group_name);
    if (debug_mode)
	printf("CLEAN %s\n", gh->group_name);

    clean_group_internal(gh);

    db_write_group(gh);
}

static char   **restrictions = NULL;
static int     *restr_len, *restr_excl;

static void
group_restriction(register group_header * gh)
{
    register char **rp;
    register int   *lp, *xp;

    if (restrictions == NULL)
	return;

    for (rp = restrictions, lp = restr_len, xp = restr_excl; *lp > 0; rp++, lp++, xp++)
	if (strncmp(gh->group_name, *rp, *lp) == 0) {
	    if (*xp)
		break;
	    return;
	}
    if (*lp == 0)
	return;

    gh->master_flag |= M_IGNORE_G;
}

static void
set_group_restrictions(char **restr, int n)
{
    register group_header *gh;
    register int    i;

    restrictions = restr;
    restr_len = newobj(int, n + 1);
    restr_excl = newobj(int, n);

    for (i = 0; i < n; i++) {
	if (restrictions[i][0] == '!') {
	    restr_excl[i] = 1;
	    restrictions[i]++;
	} else
	    restr_excl[i] = 0;
	restr_len[i] = strlen(restrictions[i]);
    }

    restr_len[n] = -1;

    Loop_Groups_Header(gh) {
	if (gh->master_flag & M_IGNORE_GROUP)
	    continue;
	group_restriction(gh);
	if (clean_ignored && (gh->master_flag & M_IGNORE_G)) {
	    log_entry('X', "Group %s ignored", gh->group_name);
	    clean_group(gh);
	}
    }
}

/*
 * add new group to master file
 */

group_header   *
add_new_group(char *name)
{
    register group_header *gh;

    if (master.free_groups <= 0)
	db_expand_master();

    master.free_groups--;

#ifdef MALLOC_64K_LIMITATION
    gh = newobj(group_header, 1);
    active_groups[master.number_of_groups] = gh;
#else
    gh = &active_groups[master.number_of_groups];
#endif

    gh->group_name_length = strlen(name);
    gh->group_name = copy_str(name);

    gh->group_num = master.number_of_groups++;
    gh->creation_time = cur_time();

    db_rewrite_groups(1, (group_header *) NULL);

    group_restriction(gh);	/* done after append to avoid setting ! */
    clean_group(gh);

    db_write_master();

    sort_groups();

    log_entry('C', "new group: %s (%d)", gh->group_name, gh->group_num);

    return gh;
}


static void
visit_active_file(void)
{
    FILE           *act;
    FILE           *nntp_act = NULL;

#ifdef NNTP
    if (!use_nntp)		/* copy 'active' to DB/ACTIVE */
	nntp_act = open_file(relative(db_directory, "ACTIVE"), OPEN_CREATE | MUST_EXIST);
#endif

    act = open_file(news_active, OPEN_READ | MUST_EXIST);

    read_active_file(act, nntp_act);

    master.last_size = ftell(act);

    fclose(act);

#ifdef NNTP
    if (nntp_act != NULL)
	fclose(nntp_act);
#endif
}


/*
 *	Build initial master file.
 */

static void
build_master(void)
{
    char            command[512];
    char            groupname[512];
    group_header   *groups, *next_g, *gh;
    group_header   *dupg;
    FILE           *src;
    int             lcount, use_group_file, found_nn_group = 0;

    printf("Confirm initialization by typing 'OK': ");
    fl;
    fgets(command, sizeof(command), stdin);
    if (strcmp(command, "OK")) {
	printf("No initialization\n");
	nn_exit(0);
    }
    if (chdir(master_directory) < 0)	/* so we can use open_file (?) */
	sys_error("master");

#ifdef NNTP
    if (use_nntp && nntp_get_active() < 0)
	sys_error("Can't get active file");
#endif

    /* check active file for duplicates */

    sprintf(command, "awk 'NF>0{print $1}' %s | sort | uniq -d", news_active);

    src = popen(command, "r");
    if (src == NULL)
	sys_error("popen(%s) failed", command);

    for (lcount = 0; fgets(groupname, 512, src); lcount++) {
	if (lcount == 0)
	    printf("\n%s contains duplicate entries for the following groups:",
		   news_active);

	fputs(groupname, stdout);
    }

    pclose(src);

    if (lcount > 0) {
	printf("Do you want to repair this file before continuing ? (y)");
	fgets(command, sizeof(command), stdin);
	if (s_hangup ||
	    command[0] == NUL || command[0] == 'y' || command[0] == 'Y') {
	    printf("Initialization stopped.\n");
	    printf("Redo ./inst INIT after repairing active file\n");
	    nn_exit(0);
	}
    }
    /* if a "GROUPS" file exist offer to use that, else */
    /* read group names from active file */

    use_group_file = 0;

    if (open_groups(OPEN_READ)) {
	printf("\nA GROUPS file already exist -- reuse it? (y)");
	fl;
	fgets(command, sizeof(command), stdin);
	if (command[0] == NUL || command[0] == 'y' || command[0] == 'Y') {
	    use_group_file = 1;
	} else
	    close_groups();
	if (s_hangup)
	    return;
    }
    printf("\nBuilding %s/MASTER file\n", db_directory);
    fl;

    if (!use_group_file) {
	sprintf(command, "awk 'NF>0{print $1}' %s | sort -u", news_active);

	src = popen(command, "r");
	if (src == NULL)
	    sys_error("popen(%s) failed", command);
    }
    open_master(OPEN_CREATE);

    master.db_magic = NNDB_MAGIC;
    master.last_scan = 0;
    master.number_of_groups = 0;
    strcpy(master.db_lock, "Initializing database");

    db_write_master();

    groups = next_g = newobj(group_header, 1);
    next_g->next_group = NULL;

    for (;;) {
	if (s_hangup)
	    goto intr;
	gh = newobj(group_header, 1);

	gh->master_flag = 0;

	if (use_group_file) {
	    gh->group_name_length = 0;
	    if (db_parse_group(gh, 0) <= 0)
		break;
	} else {
	    if (fgets(groupname, 512, src) == NULL)
		break;

	    gh->group_name_length = strlen(groupname) - 1;	/* strip NL */
	    groupname[gh->group_name_length] = NUL;
	    gh->creation_time = 0;
	    gh->group_name = copy_str(groupname);
	    gh->archive_file = NULL;
	}

	for (dupg = groups->next_group; dupg != NULL; dupg = dupg->next_group)
	    if (strcmp(dupg->group_name, gh->group_name) == 0)
		break;
	if (dupg != NULL) {
	    printf("Ignored duplicate of group %s\n", dupg->group_name);
	    continue;
	}
	gh->group_num = master.number_of_groups++;

	if (trace || debug_mode)
	    printf("%4ld '%s' (%d)\n", gh->group_num, gh->group_name,
		   gh->group_name_length);

	next_g->next_group = gh;
	next_g = gh;
	gh->next_group = NULL;

	/* moderation flag will be set by first visit_active_file call */

	/* might have the INN control.newgrp, control.cancel, etc */
	if (strncmp(gh->group_name, "control", 7) == 0)
	    gh->master_flag |= M_CONTROL;

	if (strcmp(gh->group_name, "news.software.nn") == 0)
	    found_nn_group++;

	gh->master_flag &= ~M_MUST_CLEAN;
	clean_group_internal(gh);
	gh->master_flag |= M_VALID;	/* better than the reverse */
	db_write_group(gh);
    }

    if (use_group_file)
	close_groups();
    else
	pclose(src);

    printf("%s %s/GROUPS file\n",
	   use_group_file ? "Updating" : "Building", db_directory);

    db_rewrite_groups(use_group_file, groups);

    if (initialize > 0) {
	printf("Setting articles per group limit to %d...\n", initialize);
	db_write_master();
	open_master(OPEN_READ);
	open_master(OPEN_UPDATE);
	visit_active_file();
	Loop_Groups_Header(gh) {
	    gh->first_db_article = gh->last_a_article - initialize + 1;
	    if (gh->first_db_article <= gh->first_a_article)
		continue;
	    gh->last_db_article = gh->first_db_article - 1;
	    if (gh->last_db_article < 0)
		gh->last_db_article = 0;
	    db_write_group(gh);
	}
    }
    master.db_lock[0] = NUL;
    master.db_created = cur_time();

    db_write_master();

    close_master();

    printf("Done\n");
    fl;

    log_entry('M', "Master data base initialized");

    if (!found_nn_group)
	printf("\nNotice: nn's own news group `news.software.nn' was not found\n");

    return;

intr:
    printf("\nINTERRUPT\n\nDatabase NOT completed\n");
    log_entry('M', "Master data base initialization not completed (INTERRUPTED)");
}

static void
set_lock_message(void)
{
    if (lock_message[0] || master.db_lock[0]) {
	open_master(OPEN_UPDATE);
	strncpy(master.db_lock, lock_message, DB_LOCK_MESSAGE);
	master.db_lock[DB_LOCK_MESSAGE - 1] = NUL;
	db_write_master();
	printf("DATABASE %sLOCKED\n", lock_message[0] ? "" : "UN");
    }
}

static void
do_reread_groups(void)
{
    register group_header *gh;

    open_master(OPEN_UPDATE);
    Loop_Groups_Header(gh)
	if (gh->master_flag & M_MUST_CLEAN) {
	gh->master_flag &= ~M_MUST_CLEAN;
	clean_group(gh);
    } else
	db_write_group(gh);
    master.last_scan--;		/* force update */
    db_write_master();
    close_master();
    log_entry('M', "Reread GROUPS file");
}

#ifdef HAVE_HARD_SLEEP
/*
 * Hard sleeper...  The normal sleep is not terminated by SIGALRM or
 * other signals which nnadmin may send to wake it up.
 */

static int      got_alarm;

static          sig_type
catch_alarm(void)
{
    signal(SIGALRM, SIG_IGN);
    got_alarm = 1;
}

static int
take_a_nap(int t)
{
    got_alarm = 0;
    signal(SIGALRM, catch_alarm);
    alarm(repeat_delay);
    /* the timeout is at least 1 minute, so we ignore racing condition here */
    pause();
    if (!got_alarm) {
	signal(SIGALRM, SIG_IGN);
	alarm(0);
    }
}

#else
#define take_a_nap(t) sleep(t)
#endif

int
main(int argc, char **argv)
{
    register group_header *gh;
    time_t          age_active;
    int             group_selection;
    int             temp;
    int             skip_pass;
    long            pass_no;

    umask(002);			/* avoid paranoia */

    who_am_i = I_AM_MASTER;

    init_global();

    group_selection =
	parse_options(argc, argv, (char *) NULL, master_options, (char *) NULL);

    if (debug_mode) {

#ifdef NNTP
	nntp_debug = debug_mode & 2;
#endif

	debug_mode = debug_mode & 1;
    }
    if (debug_mode) {
	signal(SIGINT, catch_hangup);
    }
    if (wakeup_master) {
	if (proto_lock(I_AM_MASTER, PL_WAKEUP) < 0)
	    printf("master is not running\n");
	exit(0);
    }
    if (prt_vers) {
	printf("nnmaster release %s\n", version_id);
	exit(0);
    }
    if (kill_running) {
	if (proto_lock(I_AM_MASTER, PL_TERMINATE) < 0)
	    temp = 0;
	else {
	    int             mpid;

	    if (proto_host[0]) {
		printf("Can't kill master on another host (%s)\n", proto_host);
		log_entry('R', "Attempt to kill master on host %s", proto_host);
		exit(1);
	    }
	    for (temp = 10; --temp >= 0; sleep(3)) {
		sleep(3);
		mpid = proto_lock(I_AM_MASTER, PL_CHECK);
		if (mpid < 0)
		    break;
		sleep(1);
		kill(mpid, SIGTERM);	/* SIGHUP lost??? */
	    }
	}

	if (temp < 0) {
	    printf("The running master will not die....!\n");
	    log_entry('E', "Could not kill running master");
	    exit(1);
	}
	if (repeat_delay == 0 && !foreground &&
	    !reread_groups_file && lock_message == NULL &&
	    group_selection == 0 && initialize < 0)
	    exit(0);
    }
    if (!file_exist(db_directory, "drwx")) {
	fprintf(stderr, "%s invoked with wrong user privileges\n", argv[0]);
	exit(9);
    }
    if (proto_lock(I_AM_MASTER, PL_SET) != 0) {
	if (proto_host[0])
	    printf("The master is running on another host (%s)\n", proto_host);
	else
	    printf("The master is already running\n");
	exit(0);
    }
    unlock_on_exit = 1;

#ifdef NNTP
    nntp_check();
#endif

    if (initialize >= 0) {
	build_master();
	nn_exit(0);
    }
    open_master(OPEN_READ);

    if (lock_message != NULL) {
	set_lock_message();
	if (repeat_delay == 0 && !foreground &&
	    !reread_groups_file && group_selection == 0)
	    nn_exit(0);
    }
    if (!ignore_lock && master.db_lock[0]) {
	printf("Database locked (unlock with -l or ignore with -i)\n");
	nn_exit(88);
    }
    if (reread_groups_file) {
	do_reread_groups();
	nn_exit(0);
    }
    if (!debug_mode) {
	close(0);
	close(1);
	close(2);
	if (open("/dev/null", 2) == 0)
	    dup(0), dup(0);
    }
    if (repeat_delay && !debug_mode && !foreground) {
	while ((temp = fork()) < 0)
	    sleep(1);
	if (temp)
	    exit(0);		/* not nn_exit() !!! */

	process_id = getpid();	/* init_global saved parent's pid */

	proto_lock(I_AM_MASTER, PL_TRANSFER);

#ifdef DETACH_TERMINAL
	DETACH_TERMINAL
#endif
    }
    log_entry('M', "Master started -r%d -e%d %s-E%d",
	      repeat_delay, expire_level,
	      expire_once ? "-F " : "", expire_method);

    if (check_on_startup) {
	char            cmd[FILENAME];
	sprintf(cmd, "%s/nnadmin Z", bin_directory);
	system(cmd);
	log_entry('M', "Database validation completed");
    }
    repeat_delay *= 60;

    init_digest_parsing();

    open_master(OPEN_UPDATE);

    if (group_selection)
	set_group_restrictions(argv + 1, group_selection);

    if (max_age_days && !use_nntp)	/* we have to stat spool files */
	max_article_age = cur_time() - (time_t) max_age_days *24 * 60 * 60;
    else
	max_article_age = 0;

    if (expire_once) {
	if (group_selection)	/* mark selected groups for expire */
	    Loop_Groups_Header(gh) {
	    if (gh->master_flag & M_IGNORE_GROUP)
		continue;
	    gh->master_flag |= M_EXPIRE;
	    }
	unconditional = 1;
    }
    for (pass_no = 0; !s_hangup; pass_no++) {
	if (pass_no > 0) {
	    take_a_nap(repeat_delay);
	    if (s_hangup)
		break;
	}

#ifdef NNTP
	if (use_nntp && nntp_get_active() < 0) {
	    nntp_close_server();
	    current_group = NULL;	/* for init_group */
	    log_entry('N', "Can't access active file --- %s",
		      repeat_delay ? "sleeping" : "terminating");
	    if (repeat_delay == 0)
		nn_exit(1);
	    continue;
	}
#endif

	temp = 2;
	while ((age_active = file_exist(news_active, "fr")) == (time_t) 0) {
	    if (use_nntp)
		break;
	    if (--temp < 0)
		sys_error("Cannot access active file");
	    sleep(5);		/* maybe a temporary glitch ? */
	}

	skip_pass = 0;

	if (Unconditional)
	    unconditional = 1;

	if (unconditional) {
	    unconditional = 0;
	} else if (age_active <= master.last_scan ||
		 (hold_updates && (cur_time() - age_active) < hold_updates))
	    skip_pass = 1;

	if (receive_admin())
	    skip_pass = 0;

	if (skip_pass) {
	    if (repeat_delay == 0)
		break;
	    if (s_hangup)
		break;

#ifdef NNTP
	    if (use_nntp)
		nntp_cleanup();
#endif

	    if (debug_mode) {
		printf("NONE (*** SLEEP ***)\n");
	    }
	    if (trace)
		log_entry('T', "none");
	    continue;
	}
	visit_active_file();

	if (do_expire())
	    if (!expire_once && do_collect()) {
		if (Unconditional)
		    master.last_scan = cur_time();
		else
		    master.last_scan = age_active;
	    }
	db_write_master();

	if (expire_once || s_hangup)
	    break;
	if (repeat_delay == 0)
	    break;

#ifdef NNTP
	if (use_nntp)
	    nntp_cleanup();
#endif
    }

    nn_exit(0);
    /* NOTREACHED */
    return 0;
}



/*
 * receive commands from administrator
 */

static int
receive_admin(void)
{
    FILE           *gate;
    char            buffer[128], *bp;
    char            command, opt, *user_date;
    int32           arg;
    int             must_collect;
    register group_header *gh;

    gate = open_gate_file(OPEN_READ);
    if (gate == NULL)
	return 0;

    sleep(2);			/* give administrator time to flush buffers */

    must_collect = 0;

    while (fgets(buffer, 128, gate)) {
	bp = buffer;

	command = *bp++;
	if (*bp++ != ';')
	    continue;

	arg = atol(bp);
	if (arg >= master.number_of_groups)
	    continue;
	gh = (arg >= 0) ? ACTIVE_GROUP(arg) : NULL;
	if ((bp = strchr(bp, ';')) == NULL)
	    continue;
	bp++;

	opt = *bp++;
	if (*bp++ != ';')
	    continue;

	arg = atol(bp);
	if ((bp = strchr(bp, ';')) == NULL)
	    continue;

	user_date = ++bp;
	if ((bp = strchr(bp, ';')) == NULL)
	    continue;
	*bp++ = NUL;
	if (*bp != NL)
	    continue;

	log_entry('A', "RECV %c %s %c %ld (%s)",
	command, gh == NULL ? "(all)" : gh->group_name, opt, arg, user_date);

	switch (command) {

	    case SM_SET_OPTION:
		switch (opt) {
		    case 'r':
			repeat_delay = arg;
			continue;
		    case 'e':
			expire_level = arg;
			continue;
		    case 't':
			trace = (arg < 0) ? !trace : arg;
			continue;
		}
		continue;	/* XXX: Is this the right thing to do? */


	    case SM_EXPIRE:
		if (gh) {
		    gh->master_flag |= M_EXPIRE | M_BLOCKED;
		    db_write_group(gh);
		    break;
		}
		Loop_Groups_Header(gh) {
		    if (gh->master_flag & M_IGNORE_GROUP)
			continue;
		    if (gh->index_write_offset == 0)
			continue;
		    if (gh->master_flag & M_EXPIRE)
			continue;
		    gh->master_flag |= M_EXPIRE;
		    db_write_group(gh);
		}
		break;

	    case SM_SET_FLAG:
		if (opt == 's')
		    gh->master_flag |= (flag_type) arg;
		else
		    gh->master_flag &= ~(flag_type) arg;
		db_write_group(gh);
		continue;

	    case SM_RECOLLECT:	/* recollect */
		if (gh) {
		    if ((gh->master_flag & M_IGNORE_GROUP) == 0)
			clean_group(gh);
		} else
		    Loop_Groups_Header(gh)
		    if ((gh->master_flag & M_IGNORE_GROUP) == 0)
		    clean_group(gh);
		break;

	    case SM_SCAN_ONCE:	/* unconditional pass */
		unconditional++;
		break;

	    default:
		continue;
	}
	must_collect = 1;
    }

    fclose(gate);

    return must_collect;
}


void
write_error(void)
{

    /*
     * should wait for problems to clear out rather than die...
     */
    sys_error("DISK WRITE ERROR");
}

/*
 * dummy routines - should never be called by master
 */

char            delayed_msg[100] = "";

FILE           *
open_purpose_file(void)
{
    return NULL;
}

int
set_variable(char *variable, int on, char *val_string)
{
    return 0;
}

char           *
alloc_str(int len)
{
    return 0;
}

void
nn_exitmsg(int n, char *fmt,...)
{
}

void
msg(char *fmt,...)
{
}

void
init_term(int full)
{
}

char           *
full_name(void)
{
    return 0;
}

void
clrdisp(void)
{
}

void
prompt(char *fmt,...)
{
}

void
user_delay(int ticks)
{
    return;
}

#ifdef HAVE_JOBCONTROL
int
suspend_nn(void)
{
    return 0;
}

#endif
