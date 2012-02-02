/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	The nn user interface main program
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include "config.h"
#include "global.h"
#include "admin.h"
#include "answer.h"
#include "articles.h"
#include "db.h"
#include "execute.h"
#include "folder.h"
#include "group.h"
#include "init.h"
#include "keymap.h"
#include "kill.h"
#include "libnov.h"
#include "macro.h"
#include "match.h"
#include "menu.h"
#include "newsrc.h"
#include "nn.h"
#include "nntp.h"
#include "options.h"
#include "proto.h"
#include "sequence.h"
#include "nn_term.h"

#ifdef USE_MALLOC_H
#include <malloc.h>
#endif

/* nn.c */

static int      nn_locked(void);
static group_header *last_group_maint(group_header * last, int upd);
static int      read_news(flag_type access_mode, char *mask);
static void     catch_up(void);
static int      do_nnspew(void);
static void     prt_version(void);
static void     do_db_check(void);

extern char    *bin_directory;

extern int
                seq_cross_filtering,	/* articles */
                dont_sort_folders,	/* folder.c */
                dont_split_digests, dont_sort_articles, also_unsub_groups,	/* group.c */
                also_cross_postings, case_fold_search,	/* match.c */
                preview_window, fmt_linenum, fmt_rptsubj,	/* menu.c */
                show_article_date, first_page_lines,	/* more.c */
                keep_rc_backup, no_update,	/* newsrc.c */
                hex_group_args,	/* sequence */
                show_current_time, conf_dont_sleep;	/* term.c */

int
                article_limit = -1, also_read_articles = 0,
                also_full_digest = 1, batch_mode = 0, conf_auto_quit = 0,
                do_kill_handling = 1, merged_menu = 0, prev_also_read = 1,
                repeat_group_query = 0, report_cost_on_exit = 1,
                show_motd_on_entry = 1, silent = 0, verbose = 0, Debug = 0;

int             check_db_update = 12 /* HOURS */ ;

extern char     proto_host[];
extern int      newsrc_update_freq, novice;
extern int      seq_cross_filtering;
extern char    *news_active;
extern long     unread_articles;
extern long     initial_memory_break;
extern int      first_time_user;
extern int      also_cross_postings;

static int
                group_name_args = 0, nngrab_mode = 0, prompt_for_group = 0;

static char
               *match_subject = NULL, *match_sender = NULL, *init_file = NULL;

static
Option_Description(nn_options)
{
    'a', Int_Option(article_limit),
    'B', Bool_Option(keep_rc_backup),
    'd', Bool_Option(dont_split_digests),
    'f', Bool_Option(dont_sort_folders),
    'g', Bool_Option(prompt_for_group),
    'G', Bool_Option(nngrab_mode),
    'i', Bool_Option(case_fold_search),
    'I', String_Option_Optional(init_file, NULL),
    'k', Bool_Option(do_kill_handling),
    'l', Int_Option(first_page_lines),
    'L', Int_Option_Optional(fmt_linenum, 3),
    'm', Bool_Option(merged_menu),
    'n', String_Option(match_sender),
    'N', Bool_Option(no_update),
    'q', Bool_Option(dont_sort_articles),
    'Q', Bool_Option(silent),
    'r', Bool_Option(repeat_group_query),
    's', String_Option(match_subject),
    'S', Bool_Option(fmt_rptsubj),
    'T', Bool_Option(show_current_time),
    'w', Int_Option_Optional(preview_window, 5),
    'W', Bool_Option(conf_dont_sleep),
    'x', Int_Option_Optional(also_read_articles, -1),
    'X', Bool_Option(also_unsub_groups),
    'Z', Int_Option(Debug),
    '\0',
};


static int
                report_number_of_articles = 0;
static char
               *check_message_format = NULL;
extern int
                quick_unread_count;

static
Option_Description(check_options)
{
    'Q', Bool_Option(silent),
    'r', Bool_Option(report_number_of_articles),
    'f', String_Option(check_message_format),
    't', Bool_Option(verbose),
    'v', Bool_Option(verbose),
    'c', Bool_Option(quick_unread_count),
    '\0',
};

/* program name == argv[0] without path */

char           *pname;

static int      must_unlock = 0;

static int
nn_locked(void)
{
    if (no_update)
	return 0;

    switch (who_am_i) {
	case I_AM_ADMIN:
	case I_AM_CHECK:
	case I_AM_POST:
	case I_AM_GREP:
	case I_AM_SPEW:
	case I_AM_VIEW:
	    return 0;		/* will not update .newsrc */
    }

    if (proto_lock(I_AM_NN, PL_SET)) {

	if (proto_host[0])
	    nn_exitmsg(1, "\nnn is running on host %s\nor %s/LOCK should be removed.\n\n", proto_host, nn_directory);
	else
	    nn_exitmsg(1, "\nAnother nn process is already running\n\n");
    }
    must_unlock = 1;
    return 0;
}

#define setup_access()	ACC_PARSE_VARIABLES

flag_type
parse_access_flags(void)
{
    flag_type       access_mode = 0;

    if (do_kill_handling)
	access_mode |= ACC_DO_KILL;
    if (also_cross_postings)
	access_mode |= ACC_ALSO_CROSS_POSTINGS;
    if (also_read_articles)
	access_mode |= ACC_ALSO_READ_ARTICLES;
    if (dont_split_digests)
	access_mode |= ACC_DONT_SPLIT_DIGESTS;
    if (also_full_digest)
	access_mode |= ACC_ALSO_FULL_DIGEST;
    if (dont_sort_articles)
	access_mode |= ACC_DONT_SORT_ARTICLES;

    return access_mode;
}

group_header   *jump_to_group = NULL;
int             enter_last_read_mode = 1;

static group_header *
last_group_maint(group_header * last, int upd)
{
    group_header   *gh;
    FILE           *f;
    char           *lg_file;
    char            buf[256], *cp;

    lg_file = relative(nn_directory, "NEXTG");
    if (upd) {
	if (last == NULL)
	    unlink(lg_file);
	else {
	    f = open_file(lg_file, OPEN_CREATE | MUST_EXIST);
	    fprintf(f, "%s\n", last->group_name);
	    fclose(f);
	}
	return NULL;
    }
    if (enter_last_read_mode == 0)
	return last;

    f = open_file(lg_file, OPEN_READ);
    if (f == NULL)
	return last;

    gh = NULL;
    if (fgets(buf, 256, f)) {
	if ((cp = strchr(buf, NL)))
	    *cp = NUL;
	gh = lookup(buf);
    }
    fclose(f);

    if (gh == NULL || gh == last || !(gh->group_flag & G_SEQUENCE))
	return last;

    switch (enter_last_read_mode) {
	case 1:		/* confirm if unread, skip if read */
	    if (gh->unread_count == 0)
		return last;
	    /* FALLTHROUGH */
	case 2:		/* confirm any case */
	    prompt_line = Lines - 1;
	    prompt("Enter %s (%ld unread)? ", gh->group_name, gh->unread_count);
	    if (!yes(0))
		return last;
	    break;
	case 3:		/* enter uncond if unread */
	    if (gh->unread_count == 0)
		return last;
	    /* FALLTHROUGH */
	case 4:		/* enter uncond */
	    break;
    }
    return gh;
}

static int
read_news(flag_type access_mode, char *mask)
{
    register group_header *gh, *prev, *tmp, *after_loop;
    flag_type       group_mode;
    int             menu_cmd;
    int             must_clear = 0, did_jump = 0;
    group_header   *last_group_read = NULL;

    prev = group_sequence;
    gh = group_sequence;
    after_loop = NULL;

    if (access_mode == 0 && !also_read_articles) {
	gh = last_group_maint(gh, 0);
	did_jump = gh != group_sequence;
	m_invoke(-2);
    }
    for (;;) {
	group_mode = access_mode;

	if (s_hangup)
	    break;

	if (gh == NULL) {
	    if (after_loop != NULL) {
		gh = after_loop;
		after_loop = NULL;
		if (gh->unread_count <= 0)
		    group_mode |= ACC_ORIG_NEWSRC;
		goto enter_direct;
	    }
	    if (did_jump) {
		did_jump = 0;
		gh = group_sequence;
		continue;
	    }
	    if (must_clear && conf_auto_quit) {
		prompt("\1LAST GROUP READ.  QUIT NOW?\1");
		switch (yes(1)) {
		    case 1:
			break;
		    case 0:
			gh = group_sequence;
		    default:
			after_loop = prev;
			continue;
		}
	    }
	    last_group_read = NULL;
	    break;
	}
	if (gh->group_flag & G_UNSUBSCRIBED) {
	    if (!also_unsub_groups) {
		gh = gh->next_group;
		continue;
	    }
	} else if (!also_read_articles && gh->unread_count <= 0) {
	    gh = gh->next_group;
	    continue;
	}
enter_direct:

	free_memory();

	if (gh->group_flag & G_FOLDER) {
	    menu_cmd = folder_menu(gh->group_name, 0);
	    if (menu_cmd == ME_NO_REDRAW) {
		menu_cmd = ME_NO_ARTICLES;
		gh->last_db_article = 0;
	    }
	} else {
	    group_mode |= setup_access();
	    if (group_mode & ACC_ORIG_NEWSRC)
		gh->last_article = gh->first_article;

	    menu_cmd = group_menu(gh, (article_number) (-1), group_mode, mask, menu);
	    group_mode = access_mode;
	}

	if (menu_cmd != ME_NO_ARTICLES) {
	    last_group_read = gh;
	    after_loop = NULL;
	    must_clear++;
	}
	switch (menu_cmd) {

	    case ME_QUIT:	/* or jump */
		if (!jump_to_group)
		    goto out;

		prev = jump_to_group;
		jump_to_group = NULL;
		did_jump = 1;
		/* FALLTHROUGH */

	    case ME_PREV:
		tmp = gh;
		gh = prev;
		prev = tmp;
		if (gh->unread_count <= 0)
		    group_mode |= ACC_ORIG_NEWSRC;
		else if (prev_also_read)
		    group_mode |= ACC_MERGED_NEWSRC;
		goto enter_direct;

	    case ME_NEXT:
		prev = gh;
		/* FALLTHROUGH */

	    case ME_NO_ARTICLES:
		if (gh->group_flag & G_MERGE_HEAD) {
		    do
			gh = gh->next_group;
		    while (gh && (gh->group_flag & G_MERGE_SUB));
		} else
		    gh = gh->next_group;
		continue;
	}
    }

out:
    if (access_mode == 0 && !also_read_articles)
	last_group_maint(last_group_read, 1);

    return must_clear;
}


static void
catch_up(void)
{
    register group_header *gh;
    char            answer1[50];
    int             mode, must_help;
    flag_type       access_mode;

    access_mode = setup_access();

    prt_unread("\nCatch-up on %u ? (auto)matically (i)nteractive ");
    fl;
    mode = 0;

    if (fgets(answer1, sizeof(answer1), stdin)) {
	if (strncmp(answer1, "auto", 4) == 0) {
	    tprintf("\nUPDATING .newsrc FILE....");
	    fl;
	    mode = -1;
	} else if (*answer1 == 'i')
	    mode = 1;
    }
    if (mode == 0) {
	tprintf("\nNO UPDATE\n");
	return;
    }
    newsrc_update_freq = 9999;
    must_help = novice;

    Loop_Groups_Sequence(gh) {

	if ((gh->group_flag & G_COUNTED) == 0)
	    continue;

	if (mode < 0) {
	    update_rc_all(gh, 0);
	    continue;
	}
again:
	if (must_help) {
	    tprintf("\n  y - mark all articles as read in current group\n");
	    tprintf("  n - do not update group\n");
	    tprintf("  r - read the group now\n");
	    tprintf("  U - unsubscribe to current group\n");
	    tprintf("  ? - this message\n");
	    tprintf("  q - quit\n\n");

	    must_help = 0;
	}
	tprintf("\rUpdate %s (%ld)? (ynrU?q) n\b", gh->group_name,
		(long) (gh->last_db_article - gh->last_article));
	fl;

	if (fgets(answer1, sizeof(answer1), stdin) == NULL || s_keyboard)
	    *answer1 = 'q';

	switch (*answer1) {

	    case 'q':
		tputc(NL);
		tprintf("Update rest? (yn) n\b");
		fl;
		if (fgets(answer1, sizeof(answer1), stdin) == NULL || *answer1 != 'y')
		    return;

		mode = -1;
		break;

	    case NUL:
	    case 'n':
		continue;

	    case 'r':
		nn_raw();
		group_menu(gh, (article_number) (-1), access_mode, (char *) NULL, menu);
		unset_raw();
		clrdisp();
		if (gh->unread_count > 0)
		    goto again;
		continue;

	    case 'U':
		update_rc_all(gh, 1);
		continue;

	    case 'y':
		break;

	    default:
		must_help = 1;
		goto again;
	}

	update_rc_all(gh, 0);
    }

    tprintf("DONE\n");

    flush_newsrc();
}

char           *mail_box = NULL;

int
unread_mail(time_t t)
{
    struct stat     st;
    static time_t   next = 0;
    static int      any = 0;

    if (next > t)
	return any;

    next = t + 60;
    any = 0;

    if (mail_box == NULL ||
	stat(mail_box, &st) != 0 ||
	st.st_size == 0 ||
	st.st_mtime < st.st_atime)
	return 0;

    any = 1;

    return 1;
}

#ifdef ACCOUNTING

#ifndef STATISTICS
#define STATISTICS 1
#endif

#define NEED_ACCOUNT
#else

#ifdef AUTHORIZE
#define NEED_ACCOUNT
#endif

#endif

#ifdef STATISTICS
static time_t   usage_time = 0;
static time_t   last_tick = 0;

void
stop_usage(void)
{
    last_tick = 0;
}

time_t
tick_usage(void)
{
    register time_t now;

    now = cur_time();

    /*
     * We ignore ticks > 2 minutes because the user has probably just left
     * the terminal inside nn and done something else
     */
    if (last_tick > (now - 120))
	usage_time += now - last_tick;

    last_tick = now;
    return now;
}

static void
log_usage(void)
{

#ifdef ACCOUNTING
    account('U', report_cost_on_exit);
#else
    if (usage_time < (STATISTICS * 60))
	return;			/* don't log short sessions */

    usage_time /= 60;
    log_entry('U', "USAGE %d.%02d", usage_time / 60, usage_time % 60);
#endif
}

#else
void
stop_usage(void)
{
}

time_t
tick_usage(void)
{
    return cur_time();
}

static void
log_usage(void)
{
}

#endif

#ifdef NEED_ACCOUNT
int
account(char option, int report)
{
    char           *args[10], **ap;
    char            buf1[16], buf2[16];
    int             ok;

    if (who_am_i != I_AM_NN && who_am_i != I_AM_POST)
	return 0;

    if (report) {
	tputc(CR);
	clrline();
	fl;
    }
    ap = args;
    *ap++ = "nnacct";
    if (report)
	*ap++ = "-r";

#ifdef STATISTICS
    sprintf(buf1, "-%c%ld", option, (long) usage_time / 60);
#else
    sprintf(buf1, "-%c0", option);
#endif

    *ap++ = buf1;
    sprintf(buf2, "-W%d", who_am_i);
    *ap++ = buf2;
    *ap++ = NULL;
    ok = execute(relative(bin_directory, "nnacct"), args, 0);
    if (option == 'U' && report)
	tputc(NL);
    return ok;
}

#endif

/* this will go into emacs_mode.c when it is completed someday */

static void
emacs_mode(void)
{
    tprintf("EMACS MODE IS NOT SUPPORTED YET.\n");
}


static int
do_nnspew(void)
{
    register group_header *gh;
    int             ix;

    ix = 0;
    Loop_Groups_Header(gh) {
	if (gh->master_flag & M_IGNORE_GROUP)
	    continue;
	gh->preseq_index = ++ix;
    }

    /* Only output each article once */
    /* Must also use this mode when using nngrab! */

    seq_cross_filtering = 1;

    Loop_Groups_Header(gh) {
	if (s_hangup)
	    return 1;
	if (gh->master_flag & M_IGNORE_GROUP)
	    continue;
	access_group(gh, gh->first_db_article, gh->last_db_article,
		     ACC_DONT_SORT_ARTICLES | ACC_ALSO_FULL_DIGEST |
		     ACC_SPEW_MODE, (char *) NULL);
    }
    return 0;
}

static void
prt_version(void)
{
    clrdisp();
    tprintf("Release %s\n  by Kim F. Storm, Peter Wemm, and Michael T Pins, 2005\n\n", version_id);
}

int
display_motd(int check)
{
    time_t          last_motd, cur_motd;
    char           *dot_motd, motd[FILENAME];

    strcpy(motd, relative(lib_directory, "motd"));
    cur_motd = file_exist(motd, (char *) NULL);
    if (cur_motd == 0)
	return 0;

    if (!check) {
	display_file(motd, CLEAR_DISPLAY | CONFIRMATION);
	return 1;
    }
    dot_motd = relative(nn_directory, ".motd");
    last_motd = file_exist(dot_motd, (char *) NULL);

    if (last_motd >= cur_motd)
	return 0;

    unlink(dot_motd);
    fclose(open_file(dot_motd, OPEN_CREATE | MUST_EXIST));

    clrdisp();

    so_printxy(0, 0, "Message of the day");
    gotoxy(0, 2);
    prt_version();

    display_file(motd, CONFIRMATION);
    return 1;
}

static void
do_db_check(void)
{
    time_t          last_upd;

#ifdef NOV
    time_t          active_time;

    if (use_nntp)
	return;

    active_time = file_exist(news_active, (char *) NULL);
    if (active_time == 0)
	return;			/* cannot stat/read/etc... */

    last_upd = (cur_time() - active_time) / (60 * 60);
    if (last_upd < check_db_update)
	return;
    tprintf("Notice: no news has arrived for the last %ld hours\n", last_upd);

#else				/* NOV */

    last_upd = (cur_time() - master.last_scan) / (60 * 60);
    if (last_upd < check_db_update)
	return;
    /* too old - but is nnmaster the culprit? */
    if (master.last_scan == file_exist(news_active, (char *) NULL))
	tprintf("Notice: no news has arrived for the last %ld hours\n", last_upd);
    else
	tprintf("Notice: nnmaster has not updated database in %ld hours\n", last_upd);
#endif				/* NOV */

    sleep(3);
}

int
main(int argc, char *argv[])
{
    int             say_welcome = 0, cmd;
    flag_type       access_mode = 0;
    char           *mask = NULL;
    initial_memory_break = (long) sbrk(0);

#ifdef USE_MALLOC_H

#ifdef MALLOC_MAXFAST
    mallopt(M_MXFAST, MALLOC_MAXFAST);
#endif

#ifdef MALLOC_FASTBLOCKS
    mallopt(M_NLBLKS, MALLOC_FASTBLOCKS);
#endif

#ifdef MALLOC_GRAIN
    mallopt(M_GRAIN, MALLOC_GRAIN);
#endif

#endif

#ifdef MALDEBUG
    mal_debug(getenv("MALDEBUG") ? atoi(getenv("MALDEBUG")) : 0);
#endif

    pname = program_name(argv);

    if (strcmp(pname, "nnadmin") == 0) {
	who_am_i = I_AM_ADMIN;
    } else if (strcmp(pname, "nnemacs") == 0) {
	who_am_i = I_AM_EMACS;
    } else if (strcmp(pname, "nncheck") == 0) {
	who_am_i = I_AM_CHECK;
    } else if (strcmp(pname, "nntidy") == 0) {
	who_am_i = I_AM_TIDY;
    } else if (strcmp(pname, "nngoback") == 0) {
	who_am_i = I_AM_GOBACK;
    } else if (strcmp(pname, "nngrep") == 0) {
	who_am_i = I_AM_GREP;
    } else if (strcmp(pname, "nnpost") == 0) {
	who_am_i = I_AM_POST;
    } else if (strcmp(pname, "nnview") == 0) {
	who_am_i = I_AM_VIEW;
    } else if (strcmp(pname, "nnbatch") == 0) {
	who_am_i = I_AM_NN;
	batch_mode = 1;
    } else {
	who_am_i = I_AM_NN;
    }

#ifdef NOV
#if defined(NOV_DIRECTORY) || defined(NOV_FILENAME)
    if (who_am_i != I_AM_VIEW) {

#ifdef NOV_DIRECTORY
	novartdir(NOV_DIRECTORY);
#endif				/* NOV_DIRECTORY */

#ifdef NOV_FILENAME
	novfilename(NOV_FILENAME);
#endif				/* NOV_FILENAME */
    }
#endif				/* NOV_DIRECTORY || NOV_FILENAME */
#endif				/* NOV */

    if (who_am_i == I_AM_NN || (who_am_i == I_AM_ADMIN && argc == 1)) {
	if (!batch_mode && !isatty(0)) {
	    if (who_am_i == I_AM_NN && argc == 2) {
		if (strcmp(argv[1], "-SPEW") == 0) {
		    who_am_i = I_AM_SPEW;
		    init_global();
		    goto nnspew;
		}
	    }
	    fprintf(stderr, "Input is not a tty\n");
	    exit(1);
	}
    }

#ifdef AUTHORIZE
    init_execute();
    if ((cmd = account('P', 0))) {
	switch (cmd) {
	    case 1:
		if (who_am_i == I_AM_POST) {
		    tprintf("You are not authorized to post news\n");
		    exit(41);
		}
		/* ok_to_post = 0; */
		cmd = 0;
		break;
	    case 2:
		tprintf("You are not authorized to read news\n");
		break;
	    case 3:
		tprintf("You cannot read news at this time\n");
		break;
	    case 4:
		tprintf("Your news reading quota is exceeded\n");
		break;
	    default:
		tprintf("Bad authorization -- reason %d\n", cmd);
		break;
	}
	if (cmd)
	    exit(40 + cmd);
    }
#endif

    /*
     * do this before nntp_check so nntp_server can be set on cmdline as nn
     * -someoptions nntp_server=[somehost OR /somefile] -moreoptionsmaybe
     *
     * You really want to do that if you are using multiple servers and/or
     * .newsrc files.  The downside is that you can't override an init file
     * setting on the command line, but this is something I very rarely have
     * seen a need for.
     */

    if ((say_welcome = init_global()) < 0)
	nn_exitmsg(1, "%s: nn has not been invoked to initialize user files", pname);
    parseargv_variables(argv, argc);
    if ((say_welcome = init_global2()) < 0)
	nn_exitmsg(1, "%s: nn has not been invoked to initialize user files", pname);

    init_key_map();

#ifndef AUTHORIZE
    init_execute();
#endif

    init_macro();

    switch (who_am_i) {
	case I_AM_VIEW:
	    /* FALLTHROUGH */
	case I_AM_NN:
	    init_term(1);
	    visit_init_file(0, argv[1]);
	    if (!silent)
		prt_version();
	    break;

	case I_AM_ADMIN:
	    if (argc == 1) {
		init_term(1);
		visit_init_file(0, (char *) NULL);
	    }
	    break;

	case I_AM_CHECK:
	    /* FALLTHROUGH */
	case I_AM_TIDY:
	    /* FALLTHROUGH */
	case I_AM_GOBACK:
	    /* FALLTHROUGH */
	case I_AM_GREP:
	    init_term(0);
	    visit_init_file(0, (char *) NULL);
	    if (!silent && who_am_i != I_AM_CHECK)
		prt_version();
	    break;
    }

    if (nn_locked())
	nn_exit(2);

#ifdef NNTP
    if (who_am_i != I_AM_VIEW) {
	nntp_check();
    }
#endif

nnspew:

    if (who_am_i != I_AM_VIEW)
	open_master(OPEN_READ);

    switch (who_am_i) {

	case I_AM_VIEW:
	    /* FALLTHROUGH */
	case I_AM_NN:
	    if (say_welcome) {
		first_time_user = 1;
		display_file("adm.welcome", CLEAR_DISPLAY | CONFIRMATION);
		clrdisp();
	    }
	    if (show_motd_on_entry)
		if (display_motd(1))
		    silent = 1;

	    group_name_args =
		parse_options(argc, argv, (char *) NULL, nn_options,
			      " [group | [+]folder]...");

	    if (also_read_articles) {
		if (article_limit < 0)
		    article_limit = also_read_articles;
		do_kill_handling = 0;
		no_update++;
	    }
	    if (match_subject) {
		mask = match_subject;
		access_mode = ACC_ON_SUBJECT;
	    } else if (match_sender) {
		mask = match_sender;
		access_mode = ACC_ON_SENDER;
	    } else {
		mask = NULL;
		access_mode = 0;
	    }

	    if (mask) {
		if (case_fold_search)
		    fold_string(mask);
		no_update++;
		do_kill_handling = 0;
	    }
	    if (nngrab_mode) {
		seq_cross_filtering = 1;
		hex_group_args = 1;
		no_update = 1;
	    }
	    if (merged_menu) {
		no_update++;
	    }
	    if (!no_update && group_name_args > 0)
		no_update = only_folder_args(argv + 1);

	    break;

	case I_AM_ADMIN:
	    admin_mode(argv[1]);
	    nn_exit(0);
	    break;

	case I_AM_CHECK:
	    silent = 0;		/* override setting in init file */
	    group_name_args =
		parse_options(argc, argv, (char *) NULL, check_options,
			      " [group]...");
	    no_update = 1;	/* don't update .newsrc and LAST */
	    break;

	case I_AM_EMACS:
	    silent = 1;
	    break;

	case I_AM_TIDY:
	    group_name_args = opt_nntidy(argc, argv);
	    break;

	case I_AM_GOBACK:
	    group_name_args = opt_nngoback(argc, &argv);
	    break;

	case I_AM_GREP:
	    opt_nngrep(argc, argv);
	    silent = 1;
	    no_update = 1;
	    break;

	case I_AM_POST:
	    do_nnpost(argc, argv);
	    nn_exit(0);
	    break;

	case I_AM_SPEW:
	    if (proto_lock(I_AM_SPEW, PL_SET) == 0) {
		cmd = do_nnspew();
		proto_lock(I_AM_SPEW, PL_CLEAR);
	    } else
		cmd = 1;	/* don't interfere with other nnspew */
	    nn_exit(cmd);
	    break;
    }

    if (who_am_i == I_AM_VIEW) {
	named_group_sequence(argv + 1);
	count_unread_articles();
	if (read_news(access_mode, mask))
	    clrdisp();
	nn_exit(0);
    }

    /*
     * at one point, the command line variable settings were handled in the
     * next routine (named_group_sequence()), which meant you couldn't change
     * the .newsrc file.  I moved that processing into parseargv_variables(),
     * which is called quite early.  Olson, 11/99
     */

    visit_rc_file();

    if (group_name_args > 0)
	named_group_sequence(argv + 1);
    else
	normal_group_sequence();

    if (who_am_i != I_AM_TIDY && who_am_i != I_AM_GOBACK)
	count_unread_articles();

    switch (who_am_i) {

	case I_AM_EMACS:
	    prt_unread("%U %G\n");
	    emacs_mode();
	    break;

	case I_AM_CHECK:
	    if (report_number_of_articles) {
		prt_unread("%U");
		break;
	    }
	    if (check_message_format) {
		if (unread_articles || !silent)
		    prt_unread(check_message_format);
	    } else if (!silent) {
		if (unread_articles || report_number_of_articles)
		    prt_unread("There %i %u in %g\n");
		else
		    prt_unread((char *) NULL);
	    }
	    nn_exit(unread_articles ? 0 : 99);
	    /* NOTREACHED */

	case I_AM_TIDY:
	    do_tidy_newsrc();
	    break;

	case I_AM_GOBACK:
	    do_goback();
	    break;

	case I_AM_NN:
	    if (check_db_update)
		do_db_check();

	    if (unread_articles == 0 &&
		group_name_args == 0 &&
		!also_read_articles &&
		!prompt_for_group) {
		if (!silent)
		    prt_unread((char *) NULL);
		break;
	    }
	    if (do_kill_handling)
		do_kill_handling = init_kill();

	    if (prompt_for_group) {

		if (mask != NULL)
		    nn_exitmsg(1, "Cannot use -s/-n with -g\n\r");

		also_cross_postings = 1;
		do {
		    nn_raw();
		    clrdisp();
		    current_group = NULL;
		    prompt_line = 2;
		    cmd = goto_group(K_GOTO_GROUP, (article_header *) NULL, setup_access());

		    if (cmd == ME_NO_REDRAW)
			sleep(2);
		} while (repeat_group_query && cmd != ME_QUIT && cmd != ME_NO_REDRAW);
		clrdisp();
		unset_raw();
		break;
	    }
	    if (!no_update && article_limit == 0) {
		catch_up();
		break;
	    }
	    if (merged_menu) {
		merge_and_read(access_mode | setup_access(), mask);
		clrdisp();
		break;
	    }
	    if (read_news(access_mode, mask)) {
		clrdisp();

		if (master.db_lock[0])
		    tprintf("Database has been locked:\n%s\n", master.db_lock);

		if (!also_read_articles &&
		    unread_articles > 0 &&
		    !silent && group_name_args == 0)
		    prt_unread("There %i still %u in %g\n\n\r");
		break;
	    }
	    gotoxy(0, Lines - 1);
	    if (group_name_args == 0) {
		clrdisp();
		tprintf("No News (is good news)\n");
	    } else
		tprintf("\r\n");
	    break;

	case I_AM_GREP:
	    do_grep(argv + 1);
	    break;
    }

    nn_exit(0);
    /* NOTREACHED */
    return 0;
}

/*
 * nn_exit() --- called whenever a program exits.
 */

void
nn_exit(int n)
{
    static int      loop = 0;

    if (loop)
	exit(n);
    loop++;

    visual_off();

#ifdef NNTP
    nntp_cleanup();
#endif				/* NNTP */

    close_master();
    flush_newsrc();

    if (who_am_i == I_AM_NN)
	log_usage();

    if (must_unlock)
	proto_lock(I_AM_NN, PL_CLEAR);

#if 0
    malloc_verify(0);
    malloc_shutdown();
#endif

    exit(n);
}
