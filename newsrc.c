/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	.newsrc parsing and update.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "articles.h"
#include "active.h"
#include "db.h"
#include "newsrc.h"
#include "nn.h"
#include "options.h"
#include "regexp.h"
#include "nn_term.h"

/* newsrc.c */

struct rc_info;

static int      dump_file(char *path, int mode);
static void     dump_newsrc(void);
static void     dump_select(void);
static time_t   get_last_new(void);
static void     update_last_new(group_header * lastg);
static article_number get_last_article(group_header * gh);
static void     init_rctest(group_header * gh, register struct rc_info * r);
static attr_type rctest(register article_header * ah, register struct rc_info * r);
static void     append_range(int pp, char delim, article_number rmin, article_number rmax);
static void     begin_rc_update(register group_header * gh);
static void     end_rc_update(register group_header * gh);
static void     mark_article(register article_header * ah, attr_type how);


#ifdef sel
/* Remove Gould SELbus software name collision */
#undef sel
#endif

#define TR(arg) tprintf arg

extern char    *news_lib_directory, *db_directory;
extern char    *home_directory;
extern char    *pname;

extern int      verbose;
extern int      silent;

extern long     n_selected;
extern char    *tmp_directory;

int             keep_rc_backup = 1;
char           *bak_suffix = ".bak";
char           *initial_newsrc_path = ".defaultnewsrc";

int             no_update = 0;
int             use_selections = 1;
int             quick_unread_count = 1;	/* make a quick count of unread art. */
int             newsrc_update_freq = 1;	/* how often to write .newsrc */
char           *newsrc_file = NULL;

#define RCX_NEVER	0	/* ignore missing groups */
#define RCX_HEAD	1	/* prepend missing groups to .newsrc when
				 * read */
#define	RCX_TAIL	2	/* append missing groups to .newsrc when read */
#define RCX_TIME	3	/* append NEW groups as they arrive */
#define RCX_TIME_CONF	4	/* append NEW groups with confirmation */
#define RCX_RNLAST	5	/* .rnlast compatible functionality */

int             new_group_action = RCX_TIME;	/* append new groups to
						 * .newsrc */
int             keep_unsubscribed = 1;	/* keep unsubscribed groups in
					 * .newsrc */
int             keep_unsub_long = 0;	/* keep unread in unsubscribed groups */

int             tidy_newsrc = 0;/* remove obsolete groups from .newsrc */

int             auto_junk_seen = 1;	/* junk seen articles ... */
int             conf_junk_seen = 0;	/* ... if confirmed by user ... */
int             retain_seen_status = 0;	/* ... or remember seen articles. */

long            unread_articles;/* estimate of unread articles */
int             unread_groups;

group_header   *rc_sequence = NULL;

static char    *sel_path = NULL;


/* delimitors on newsrc lines */

#define RC_SUBSCR	':'	/* subscription to group */
#define RC_UNSUBSCR	'!'	/* no subscription to group */

#define RC_DELIM	','	/* separator on rc lines */
#define RC_RANGE	'-'	/* range */

/* delimitors on select lines */

#define SEL_RANGE	'-'	/* range */
#define SEL_SELECT	','	/* following articles are selected */
#define SEL_LEAVE	'+'	/* following articles are left over */
#define SEL_SEEN	';'	/* following articles are seen */
#define SEL_UNREAD	'~'	/* in digests */
#define SEL_DIGEST	'('	/* start digest list */
#define SEL_END_DIGEST	')'	/* end digest list */
#define SEL_NEW		'&'	/* new group (group.name&nnn) */

#define END_OF_LIST	0x7fffffffL	/* Greater than any article number */

/* line buffers */

#define RC_LINE_MAX	32768

static char     rcbuf[RC_LINE_MAX];
static char     selbuf[RC_LINE_MAX];

static group_header *rc_seq_tail = NULL;

static int      newsrc_update_count = 0, select_update_count = 0;

#define DM_NEWSRC	0
#define DM_SELECT	1
#define DM_ORIG_NEWSRC	2
#define DM_ORIG_SELECT	3

static int
dump_file(char *path, int mode)
{
    FILE           *f = NULL;
    register group_header *gh;
    char           *line = NULL;

    Loop_Groups_Newsrc(gh) {
	switch (mode) {
	    case DM_NEWSRC:
		if (tidy_newsrc) {
		    if ((gh->master_flag & M_VALID) == 0)
			continue;
		    if (!keep_unsubscribed && (gh->group_flag & G_UNSUBSCRIBED))
			continue;
		}
		line = gh->newsrc_line;
		break;
	    case DM_SELECT:
		if (tidy_newsrc && (gh->master_flag & M_VALID) == 0)
		    continue;
		if (gh->group_flag & G_UNSUBSCRIBED)
		    continue;
		line = gh->select_line;
		break;
	    case DM_ORIG_NEWSRC:
		line = gh->newsrc_orig;
		break;
	    case DM_ORIG_SELECT:
		line = gh->select_orig;
		break;
	}
	if (line == NULL)
	    continue;
	if (f == NULL)
	    f = open_file(path, OPEN_CREATE | MUST_EXIST);
	fputs(line, f);
    }
    if (f != NULL)
	if (fclose(f) == EOF)
	    return -1;
    return 0;
}


static void
dump_newsrc(void)
{
    char            bak[FILENAME];
    static int      first = 1;

    if (no_update)
	return;
    if (++newsrc_update_count < newsrc_update_freq)
	return;

    if (first && keep_rc_backup) {
	sprintf(bak, "%s%s", newsrc_file, bak_suffix);
	if (dump_file(bak, DM_ORIG_NEWSRC))
	    nn_exitmsg(1, "Cannot backup %s", newsrc_file);
	first = 0;
    }
    if (dump_file(newsrc_file, DM_NEWSRC)) {
	char            temp[FILENAME];
	sprintf(temp, "%s/newsrc-%d", tmp_directory, process_id);
	if (dump_file(temp, DM_NEWSRC))
	    nn_exitmsg(1, "Cannot update %s -- restore %s file!!!",
		       newsrc_file, bak_suffix);
	else
	    nn_exitmsg(1, "Cannot update %s -- saved in %s", newsrc_file, temp);
    }
    newsrc_update_count = 0;
}

static void
dump_select(void)
{
    char            bak[FILENAME];
    static int      first = 1;

    if (no_update)
	return;
    if (++select_update_count < newsrc_update_freq)
	return;

    if (first && keep_rc_backup) {
	sprintf(bak, "%s%s", sel_path, bak_suffix);
	dump_file(bak, DM_ORIG_SELECT);
	first = 0;
    }
    dump_file(sel_path, DM_SELECT);

    select_update_count = 0;
}

#define RN_LAST_GROUP_READ	0
#define RN_LAST_TIME_RUN	1
#define RN_LAST_ACTIVE_SIZE	2
#define RN_LAST_CREATION_TIME	3
#define RN_LAST_NEW_GROUP	4
#define RN_ACTIVE_TIMES_OFFSET	5

#define MAX_RNLAST_LINE 6

static char    *rnlast_line[MAX_RNLAST_LINE];
#define rnlast_path relative(home_directory, ".rnlast")

static          time_t
get_last_new(void)
{
    FILE           *lf;
    char            buf[FILENAME];
    register int    i;
    time_t          t;

    if (new_group_action == RCX_RNLAST) {
	lf = open_file(rnlast_path, OPEN_READ);
	if (lf == NULL)
	    goto no_file;

	for (i = 0; i < MAX_RNLAST_LINE; i++) {
	    if (fgets(buf, FILENAME, lf) == NULL)
		break;
	    rnlast_line[i] = copy_str(buf);
	}
	if (i != MAX_RNLAST_LINE) {
	    while (i <= MAX_RNLAST_LINE) {
		rnlast_line[i] = copy_str("");
		i++;
	    }
	    fclose(lf);
	    return (time_t) atol(rnlast_line[RN_LAST_TIME_RUN]);
	}
	fclose(lf);
	return (time_t) atol(rnlast_line[RN_LAST_CREATION_TIME]);
    }
    lf = open_file(relative(nn_directory, "LAST"), OPEN_READ);
    if (lf == NULL)
	goto no_file;
    if (fgets(buf, FILENAME, lf) == NULL)
	goto no_file;

    fclose(lf);
    return (time_t) atol(buf);

no_file:
    if (lf != NULL)
	fclose(lf);
    t = file_exist(newsrc_file, (char *) NULL);
    return t > 0 ? t : (time_t) (-1);
}

static void
update_last_new(group_header * lastg)
{
    FILE           *lf;
    register int    i;
    struct stat     st;

    if (no_update)
	return;

    if (new_group_action == RCX_RNLAST) {
	if (rnlast_line[0] == NULL)
	    for (i = 0; i < MAX_RNLAST_LINE; i++)
		rnlast_line[i] = copy_str("");
	lf = open_file(rnlast_path, OPEN_CREATE | MUST_EXIST);
	fputs(rnlast_line[RN_LAST_GROUP_READ], lf);	/* as good as any */
	fprintf(lf, "%ld\n", (long) cur_time());	/* RN_LAST_TIME_RUN */
	fprintf(lf, "%ld\n", (long) master.last_size);	/* RN_LAST_ACTIVE_SIZE */

	fprintf(lf, "%ld\n", (long) lastg->creation_time);	/* RN_LAST_CREATION_TIME */
	fprintf(lf, "%s\n", lastg->group_name);	/* RN_LAST_NEW_GROUP */

	if (stat(relative(news_lib_directory, "active.times"), &st) == 0)
	    fprintf(lf, "%ld\n", (long) st.st_size);
	else			/* can't be perfect -- don't update */
	    fputs(rnlast_line[RN_ACTIVE_TIMES_OFFSET], lf);
	for (i = 0; i < MAX_RNLAST_LINE; i++)
	    freeobj(rnlast_line[i]);
    } else {
	lf = open_file(relative(nn_directory, "LAST"), OPEN_CREATE | MUST_EXIST);
	fprintf(lf, "%ld\n%s\n", (long) lastg->creation_time, lastg->group_name);
    }

    fclose(lf);
}

static          article_number
get_last_article(group_header * gh)
{
    register char  *line;

    if ((line = gh->newsrc_line) == NULL)
	return -1;

    line += gh->group_name_length + 1;
    while (*line && isspace(*line))
	line++;
    if (*line == NUL)
	return -1;

    if (line[0] == '1') {
	if (line[1] == RC_RANGE)
	    return atol(line + 2);
	if (!isdigit(line[1]))
	    return 1;
    }
    return 0;
}


void
visit_rc_file(void)
{
    FILE           *rc, *sel;
    register group_header *gh;
    int             subscr;
    register char  *bp;
    register int    c;
    char            bak[FILENAME];
    time_t          last_new_group = 0, rc_age, newsrc_age;
    group_header   *last_new_gh = NULL;

    if (newsrc_file == NULL)
	newsrc_file = home_relative(".newsrc");

    sel_path = mk_file_name(nn_directory, "select");

    Loop_Groups_Header(gh) {
	gh->newsrc_line = NULL;
	gh->newsrc_orig = NULL;
	gh->select_line = NULL;
	gh->select_orig = NULL;
    }

    if ((rc_age = file_exist(relative(nn_directory, "rc"), (char *) NULL))) {
	if (who_am_i != I_AM_NN)
	    nn_exitmsg(1, "A release 6.3 rc file exists. Run nn to upgrade");

	sprintf(bak, "%s/upgrade_rc", lib_directory);

	if ((newsrc_age = file_exist(newsrc_file, (char *) NULL)) == 0) {
	    display_file("adm.upgrade1", CLEAR_DISPLAY);
	} else {
	    if (rc_age + 60 > newsrc_age) {
		/* rc file is newest (or .newsrc does not exist) */
		display_file("adm.upgrade2", CLEAR_DISPLAY);
		prompt("Convert rc file to .newsrc now? ");
		if (yes(1) <= 0)
		    nn_exit(0);
	    } else {
		/* .newsrc file is newest */
		display_file("adm.upgrade3", CLEAR_DISPLAY);
		prompt("Use current .newsrc file? ");
		if (yes(1) > 0) {
		    strcat(bak, " n");
		} else {
		    display_file("adm.upgrade4", CLEAR_DISPLAY);
		    prompt("Convert rc file to .newsrc? ");
		    if (yes(1) <= 0)
			nn_exitmsg(0, "Then you will have to upgrade manually");
		}
	    }
	}

	tprintf("\r\n\n");
	system(bak);
	any_key(prompt_line);
    }
    rc = open_file(newsrc_file, OPEN_READ);
    if (rc != NULL) {
	fseek(rc, 0, 2);
	if (ftell(rc))
	    rewind(rc);
	else {
	    fclose(rc);
	    rc = NULL;
	}
    }
    if (rc == NULL) {
	sprintf(bak, "%s%s", newsrc_file, bak_suffix ? bak_suffix : ".bak");
	if ((rc = open_file(bak, OPEN_READ))) {
	    int             ans;
	    time_t          rc_mtime;

	    tprintf("\nThere is no .newsrc file - restore backup? (y) ");
	    if ((ans = yes(0)) < 0)
		nn_exit(1);
	    if (!ans) {
		tprintf("\nConfirm building .newsrc from scratch: (y/n) ");
		if ((ans = yes(1)) <= 0)
		    nn_exit(1);
		fclose(rc);
		rc = NULL;
	    } else {
		switch (new_group_action) {
		    case RCX_TIME:
		    case RCX_TIME_CONF:
		    case RCX_RNLAST:
			last_new_group = get_last_new();
			rc_mtime = (rc ? m_time(rc) : 0);
			if (last_new_group < 0 || rc_mtime < last_new_group)
			    last_new_group = rc_mtime;
		}
	    }
	}
    }
    if (rc == NULL) {

	last_new_group = -1;	/* ignore LAST & .rnlast if no .newsrc */
	rc = open_file_search_path(initial_newsrc_path, OPEN_READ);
	if (rc == NULL)
	    goto new_user;
	/* ignore groups not in the initial newsrc file */
	last_new_gh = ACTIVE_GROUP(master.number_of_groups - 1);
	last_new_group = last_new_gh->creation_time;
    }
    while (fgets(rcbuf, RC_LINE_MAX, rc) != NULL) {
	gh = NULL;
	subscr = 0;
	for (bp = rcbuf; ((c = *bp)); bp++) {
	    if (isspace(c))
		break;		/* not a valid line */

	    if (c == RC_UNSUBSCR || c == RC_SUBSCR) {
		subscr = (c == RC_SUBSCR);
		*bp = NUL;
		gh = lookup_no_alias(rcbuf);
		*bp = c;
		break;
	    }
	}

	if (gh == NULL) {
	    gh = newobj(group_header, 1);
	    gh->group_flag |= G_FAKED;
	    gh->master_flag |= M_VALID;
	}
	if (gh->master_flag & M_ALIASED)
	    continue;

	if (rc_seq_tail == NULL)
	    rc_sequence = rc_seq_tail = gh;
	else {
	    rc_seq_tail->newsrc_seq = gh;
	    rc_seq_tail = gh;
	}

	gh->newsrc_orig = gh->newsrc_line = copy_str(rcbuf);
	if (gh->group_flag & G_FAKED)
	    gh->group_name = gh->newsrc_line;
	else if (!subscr)
	    gh->group_flag |= G_UNSUBSCRIBED;
    }
    fclose(rc);

new_user:
    Loop_Groups_Header(gh) {
	if (gh->master_flag & M_IGNORE_GROUP)
	    continue;
	if (gh->group_flag & G_UNSUBSCRIBED)
	    continue;
	if (gh->newsrc_line == NULL) {
	    char            buf[FILENAME];

	    /* NEW GROUP - ADD TO NEWSRC AS APPROPRIATE */

	    switch (new_group_action) {
		case RCX_NEVER:
		    /* no not add new groups */
		    gh->group_flag |= G_DONE | G_UNSUBSCRIBED;
		    continue;

		case RCX_HEAD:
		    /* insert at top */
		    gh->newsrc_seq = rc_sequence;
		    rc_sequence = gh;
		    break;


		case RCX_TIME:
		case RCX_TIME_CONF:
		case RCX_RNLAST:
		    if (last_new_group == 0) {
			last_new_group = get_last_new();
			if (last_new_group < 0 && new_group_action != RCX_RNLAST) {
			    /* maybe this is a first time rn convert ? */
			    int             nga = new_group_action;
			    new_group_action = RCX_RNLAST;
			    last_new_group = get_last_new();
			    new_group_action = nga;
			}
		    }
		    if (gh->creation_time <= last_new_group) {
			/* old groups not in .newsrc are unsubscribed */
			gh->group_flag |= G_UNSUBSCRIBED;
			continue;
		    }
		    if (last_new_gh == NULL || last_new_gh->creation_time <= gh->creation_time)
			last_new_gh = gh;

		    if (new_group_action != RCX_TIME && !no_update) {
			tprintf("\nNew group: %s -- append to .newsrc? (y)",
				gh->group_name);
			if (yes(0) <= 0) {
			    gh->group_flag |= G_DONE | G_UNSUBSCRIBED;
			    continue;
			}
		    }
		    sprintf(buf, "%s:\n", gh->group_name);
		    /* to avoid fooling the LAST mechanism, we must fake */
		    /* that the group was also in the original .newsrc */

		    gh->newsrc_orig = gh->newsrc_line = copy_str(buf);
		    newsrc_update_count++;

		    /* FALLTHRU */
		case RCX_TAIL:
		    /* insert at bottom */
		    if (rc_seq_tail == NULL)
			rc_sequence = rc_seq_tail = gh;
		    else {
			rc_seq_tail->newsrc_seq = gh;
			rc_seq_tail = gh;
		    }
		    break;
	    }

	    gh->last_article = -1;
	} else
	    gh->last_article = get_last_article(gh);

	if (gh->last_article < 0) {
	    gh->group_flag |= G_NEW;
	    gh->last_article = gh->first_db_article - 1;
	} else if (gh->first_db_article > gh->last_article)
	    gh->last_article = gh->first_db_article - 1;

	if (gh->last_article < 0)
	    gh->last_article = 0;
	gh->first_article = gh->last_article;
    }

    if (rc_seq_tail)
	rc_seq_tail->newsrc_seq = NULL;

    if (last_new_gh != NULL)
	update_last_new(last_new_gh);

    if (!use_selections)
	return;

    sel = open_file(sel_path, OPEN_READ);
    if (sel == NULL)
	return;

    while (fgets(selbuf, RC_LINE_MAX, sel) != NULL) {
	for (bp = selbuf; ((c = *bp)); bp++)
	    if (c == SP || c == SEL_NEW)
		break;

	if (c == NUL)
	    continue;
	*bp = NUL;
	gh = lookup_no_alias(selbuf);
	if (gh == NULL || gh->master_flag & M_ALIASED)
	    continue;
	*bp = c;
	if (c == SEL_NEW)
	    gh->group_flag |= G_NEW;
	gh->select_orig = gh->select_line = copy_str(selbuf);
    }
    fclose(sel);
}

/*
 * prepare to use newsrc & select information for a specific group
 */

static struct rc_info {
    char           *rc_p;	/* pointer into newsrc_line */
    article_number  rc_min;	/* current newsrc range min */
    article_number  rc_max;	/* current newsrc range max */
    char            rc_delim;	/* delimiter character */

    char           *sel_p;	/* pointer into select_line */
    char           *sel_initp;	/* rc_p after initialization */
    article_number  sel_min;	/* current select range min */
    article_number  sel_max;	/* current select range max */
    article_number  sel_digest;	/* current digest */
    attr_type       sel_type;	/* current select range type */
    char            sel_delim;	/* delimiter character */
}               orig, cur;

static int      rctest_mode;

static void
init_rctest(group_header * gh, register struct rc_info * r)
{
    if (r->rc_p == NULL) {
	r->rc_min = r->rc_max = END_OF_LIST;
    } else {
	r->rc_min = r->rc_max = -1;
	r->rc_delim = SP;
	r->rc_p += gh->group_name_length + 1;
    }

    r->sel_digest = 0;
    if (r->sel_p == NULL) {
	r->sel_min = r->sel_max = END_OF_LIST;
    } else {
	r->sel_p += gh->group_name_length + 1;
	r->sel_min = r->sel_max = -1;
	r->sel_delim = SP;
    }
}

void
use_newsrc(register group_header * gh, int mode)
{
    orig.rc_p = gh->newsrc_orig;
    orig.sel_p = gh->select_orig;
    cur.rc_p = gh->newsrc_line;
    cur.sel_p = gh->select_line;

    rctest_mode = mode;

    switch (mode) {
	case 0:
	    init_rctest(gh, &cur);
	    break;
	case 1:
	    init_rctest(gh, &orig);
	    break;
	case 2:
	    init_rctest(gh, &cur);
	    init_rctest(gh, &orig);
	    break;
    }
}

/*
#define TRC(wh)  TR( ("r%d>%-8.8s< %ld %ld %ld %c\n", wh, p ? p : "***", n, rc_min, rc_max, rc_delim) )
#define TSEL(wh) TR( ("s%d>%-8.8s< %ld %ld %ld %c\n", wh, p ? p : "***", n, sel_min, sel_max, sel_delim) )
*/
#define TRC(wh)
#define TSEL(wh)

static          attr_type
rctest(register article_header * ah, register struct rc_info * r)
{
    register char  *p;
    register int    c;
    register article_number   n = ah->a_number, x;

    while (n > r->rc_max) {
	/* get next interval from newsrc line */
	r->rc_min = -1;
	x = 0;
	p = r->rc_p;
	TRC(1);

	if (*p == RC_DELIM)
	    p++;
	if (*p == NUL || *p == NL)
	    r->rc_min = r->rc_max = END_OF_LIST;
	else {
	    for (; ((c = *p)) && c != RC_DELIM && c != NL; p++) {
		if (c == RC_RANGE) {
		    if (r->rc_min < 0)
			r->rc_min = x;
		    else
			msg("syntax error in rc file");
		    x = 0;
		    continue;
		}
		if (isascii(*p) && isdigit(*p))
		    x = x * 10 + c - '0';
	    }
	    r->rc_max = x;
	    if (r->rc_min < 0)
		r->rc_min = x;
	    r->rc_p = p;
	}
    }
    TRC(2);

    if (n >= r->rc_min && n <= r->rc_max)
	return A_READ;

    p = r->sel_p;
    if (r->sel_digest != 0) {
	if (n == r->sel_digest && (ah->flag & A_DIGEST)) {
	    if (*(r->sel_p) == SEL_END_DIGEST)
		return A_READ;
	    n = ah->fpos;
	} else {
	    if (n < r->sel_digest)
		return 0;
	    while (*p && *p++ != SEL_END_DIGEST);
	    r->sel_digest = 0;
	    r->sel_min = r->sel_max = -1;
	}
    }
    while (n > r->sel_max) {
	r->sel_min = -1;
	r->sel_type = A_SELECT;
	x = 0;
	TSEL(3);

	for (;;) {
	    switch (*p) {
		case SEL_SELECT:
		    r->sel_type = A_SELECT;
		    p++;
		    continue;
		case SEL_LEAVE:
		    r->sel_type = A_LEAVE;
		    p++;
		    continue;
		case SEL_SEEN:
		    r->sel_type = A_SEEN;
		    p++;
		    continue;
		case SEL_UNREAD:
		    r->sel_type = 0;
		    p++;
		    continue;
		case SEL_DIGEST:
		    while (*p && *p++ != SEL_END_DIGEST);
		    continue;
		case SEL_END_DIGEST:
		    if (r->sel_digest) {
			if (r->sel_digest == ah->a_number) {
			    r->sel_p = p;
			    return A_READ;
			}
			r->sel_digest = 0;
		    }
		    p++;
		    r->sel_type = A_SELECT;
		    continue;
		default:
		    break;
	    }
	    break;
	}

	if (*p == NUL || *p == NL) {
	    r->sel_min = r->sel_max = END_OF_LIST;
	    break;
	}
	for (; ((c = *p)); p++) {
	    switch (c) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		    x = x * 10 + c - '0';
		    continue;

		case SEL_SELECT:
		case SEL_LEAVE:
		case SEL_SEEN:
		case SEL_UNREAD:
		    break;

		case SEL_RANGE:
		    if (r->sel_min < 0)
			r->sel_min = x;
		    else
			msg("syntax error in sel file");
		    x = 0;
		    continue;

		case SEL_DIGEST:
		    n = ah->a_number;
		    if (n > x) {
			while (*p && (*p++ != SEL_END_DIGEST));
			x = -1;
			break;
		    }
		    p++;
		    r->sel_digest = x;
		    if (n < r->sel_digest) {
			r->sel_p = p;
			return 0;
		    }
		    n = ah->fpos;
		    x = -1;
		    break;

		case NL:
		    if (r->sel_digest == 0)
			break;
		    /* FALLTHRU */
		case SEL_END_DIGEST:
		    if (r->sel_digest == ah->a_number) {
			r->sel_p = p;
			return (ah->fpos == x) ? r->sel_type : A_READ;
		    }
		    r->sel_digest = 0;
		    x = -1;
		    break;
	    }
	    break;
	}
	r->sel_max = x;
	if (r->sel_min < 0)
	    r->sel_min = x;
	r->sel_p = p;
    }

    if (n >= r->sel_min && n <= r->sel_max)
	return r->sel_type;

    if (r->sel_digest)
	return A_READ;		/* only read articles are not listed */

    return 0;			/* unread, unseen, unselected */
}

attr_type
test_article(article_header * ah)
{
    attr_type       a;

    switch (rctest_mode) {
	case 0:
	    return rctest(ah, &cur);
	case 1:
	    return rctest(ah, &orig);
	case 2:
	    a = rctest(ah, &cur);
	    if (a != A_READ)
		return a;
	    if (rctest(ah, &orig) == A_READ)
		return A_KILL;
	    return a;
    }
    return 0;
}

/*
 * We only mark the articles that should remain unread
 */
static char    *rc_p;		/* pointer into newsrc_line */
static article_number rc_min;	/* current newsrc range min */
static char     rc_delim;	/* delimiter character */

static char    *sel_p;		/* pointer into select_line */
static char    *sel_initp;	/* rc_p after initialization */
static article_number sel_min;	/* current select range min */
static article_number sel_max;	/* current select range max */
static article_number sel_digest;	/* current digest */
static char     sel_delim;	/* delimiter character */

static void
append(int x, char *fmt,...)
{
    va_list         ap;
    register char  *p;
    char          **pp;

    va_start(ap, fmt);
    pp = x ? &sel_p : &rc_p;
    p = *pp;
    if (p > (x ? &selbuf[RC_LINE_MAX - 16] : &rcbuf[RC_LINE_MAX - 16])) {
	msg("%s line too long", x ? "select" : ".newsrc");
	va_end(ap);
	return;
    }
    vsprintf(p, fmt, ap);
    va_end(ap);

    while (*p)
	p++;
    *p = NL;
    p[1] = NUL;
    *pp = p;
}

static void
append_range(int pp, char delim, article_number rmin, article_number rmax)
{
    if (rmin == rmax)
	append(pp, "%c%ld", delim, (long) rmin);
    else
	append(pp, "%c%ld%c%ld", delim, (long) rmin, RC_RANGE, (long) rmax);
}

static int32    mark_counter;

static void
begin_rc_update(register group_header * gh)
{
    add_unread(gh, -1);
    mark_counter = 0;

    rc_p = rcbuf;
    rc_min = 1;
    append(0, "%s%c", gh->group_name,
	   gh->group_flag & G_UNSUBSCRIBED ? RC_UNSUBSCR : RC_SUBSCR);
    rc_delim = SP;
    sel_p = selbuf;
    sel_min = 0;
    sel_max = 0;
    sel_digest = 0;
    sel_delim = SP;
    append(1, "%s%c", gh->group_name,
	   (gh->group_flag & G_NEW) ? SEL_NEW : SP);
    /* sel_initp == sep_p => empty list */
    sel_initp = (gh->group_flag & G_NEW) ? NULL : sel_p;
}

static void
end_rc_update(register group_header * gh)
{
    if (rc_min <= gh->last_db_article)
	append_range(0, rc_delim, rc_min, gh->last_db_article);

    if (gh->newsrc_line != NULL && strcmp(rcbuf, gh->newsrc_line)) {
	if (gh->newsrc_orig != gh->newsrc_line)
	    freeobj(gh->newsrc_line);
	gh->newsrc_line = NULL;
    }
    if (gh->newsrc_line == NULL) {
	gh->newsrc_line = copy_str(rcbuf);
	dump_newsrc();
    }
    if (sel_digest)
	append(1, "%c", SEL_END_DIGEST);
    else if (sel_min)
	append_range(1, sel_delim, sel_min, sel_max);

    if (gh->select_line) {
	if (strcmp(selbuf, gh->select_line) == 0)
	    goto out;
    } else if (sel_p == sel_initp)
	goto out;

    if (gh->select_line && gh->select_orig != gh->select_line)
	freeobj(gh->select_line);

    gh->select_line = (sel_p == sel_initp) ? NULL : copy_str(selbuf);
    dump_select();

out:
    if ((gh->last_article = get_last_article(gh)) < 0)
	gh->last_article = 0;
    if (gh->last_article < gh->first_article)
	gh->first_article = gh->last_article;

    gh->group_flag |= G_READ;	/* should not call update_group again */
    if (mark_counter > 0) {
	gh->unread_count = mark_counter;
	add_unread(gh, 0);
    }
}

static void
mark_article(register article_header * ah, attr_type how)
{
    register article_number anum;
    char            delim = 0;

    switch (how) {
	case A_SELECT:
	    delim = SEL_SELECT;
	    break;
	case A_SEEN:
	    delim = SEL_SEEN;
	    break;
	case A_LEAVE:
	case A_LEAVE_NEXT:
	    delim = SEL_LEAVE;
	    break;
	case 0:
	    delim = SEL_UNREAD;
	    break;
    }

    mark_counter++;
    anum = ah->a_number;

    if (rc_min < anum) {
	append_range(0, rc_delim, rc_min, anum - 1);
	rc_delim = RC_DELIM;

	if ((ah->flag & A_DIGEST) == 0
	    && sel_min && delim == sel_delim && sel_max == (rc_min - 1))
	    sel_max = anum - 1;	/* expand select range over read articles */
    }
    rc_min = anum + 1;

    if (ah->flag & A_DIGEST) {
	if (sel_digest != anum) {
	    if (sel_digest) {
		append(1, "%c", SEL_END_DIGEST);
	    } else if (sel_min) {
		append_range(1, sel_delim, sel_min, sel_max);
		sel_min = 0;
	    }
	    append(1, "%c%ld%c", SEL_SELECT, (long) anum, SEL_DIGEST);
	    sel_digest = anum;
	}
	append(1, "%c%ld", delim, ah->fpos);
	return;
    }
    if (sel_digest) {
	append(1, "%c", SEL_END_DIGEST);
	sel_digest = 0;
    }
    if (sel_min) {
	if (delim != sel_delim || delim == SEL_UNREAD) {
	    append_range(1, sel_delim, sel_min, sel_max);
	    sel_delim = delim;
	    if (delim == SEL_UNREAD)
		sel_min = 0;
	    else
		sel_min = anum;
	} else
	    sel_max = anum;
    } else if (delim != SEL_UNREAD) {
	sel_min = sel_max = anum;
	sel_delim = delim;
    }
}

void
flush_newsrc(void)
{
    newsrc_update_freq = 0;
    if (select_update_count)
	dump_select();
    if (newsrc_update_count)
	dump_newsrc();
}

int
restore_bak(void)
{
    if (no_update)
	return 1;

    prompt("Are you sure? ");
    if (!yes(1))
	return 0;

    if (dump_file(newsrc_file, DM_ORIG_NEWSRC))
	nn_exitmsg(1, "Could not restore %s -- restore %s file manually",
		   newsrc_file, bak_suffix);

    prompt("Restore selections? ");
    if (yes(1))
	dump_file(sel_path, DM_ORIG_SELECT);

    no_update = 1;		/* so current group is not updated */
    return 1;
}

/*
 *	Update .newsrc for one group.
 *	sort_articles(-2) MUST HAVE BEEN CALLED BEFORE USE.
 */

int             rc_merged_groups_hack = 0;

void
update_rc(register group_header * gh)
{
    register article_header *ah, **ahp;
    register article_number art;
    static int      junk_seen = 0;

    if (!rc_merged_groups_hack)
	junk_seen = 0;

    if (gh->group_flag & (G_FOLDER | G_FAKED))
	return;

    begin_rc_update(gh);

    for (ahp = articles, art = 0; art < n_articles; ahp++, art++) {
	ah = *ahp;
	if (ah->a_group != NULL && ah->a_group != gh)
	    continue;

	switch (ah->attr) {
	    case A_READ:
	    case A_KILL:
		continue;

	    case A_LEAVE:
	    case A_LEAVE_NEXT:
	    case A_SELECT:
		mark_article(ah, ah->attr);
		continue;

	    case A_SEEN:
		if (junk_seen == 0) {
		    junk_seen = -1;
		    if (auto_junk_seen) {
			if (conf_junk_seen) {
			    prompt("\1Junk seen articles\1 ");
			    if (yes(0) > 0)
				junk_seen = 1;
			} else
			    junk_seen = 1;
		    }
		}
		if (junk_seen > 0)
		    continue;
		mark_article(ah, (attr_type) (retain_seen_status ? A_SEEN : 0));
		continue;

	    case A_AUTO_SELECT:
	    default:
		mark_article(ah, (attr_type) 0);
		continue;
	}
    }

    end_rc_update(gh);
}

void
update_rc_all(register group_header * gh, int unsub)
{
    if (unsub) {
	gh->group_flag &= ~G_NEW;
	gh->group_flag |= G_UNSUBSCRIBED;

	if (!keep_unsubscribed) {
	    add_unread(gh, -1);
	    if (gh->newsrc_line != NULL && gh->newsrc_orig != gh->newsrc_line)
		freeobj(gh->newsrc_line);
	    gh->newsrc_line = NULL;
	    if (gh->newsrc_orig != NULL)
		dump_newsrc();
	    if (gh->select_line != NULL && gh->newsrc_orig != gh->select_line)
		freeobj(gh->select_line);
	    gh->select_line = NULL;
	    if (gh->select_orig != NULL)
		dump_select();
	    return;
	}
	if (keep_unsub_long) {
	    update_rc(gh);
	    return;
	}
    }
    begin_rc_update(gh);
    end_rc_update(gh);
}

void
add_to_newsrc(group_header * gh)
{
    gh->group_flag &= ~G_UNSUBSCRIBED;

    if (gh->newsrc_seq != NULL || gh == rc_seq_tail) {
	update_rc(gh);
	return;
    }
    rc_seq_tail->newsrc_seq = gh;
    rc_seq_tail = gh;
    if (gh->last_db_article > 0)
	sprintf(rcbuf, "%s: %s%ld\n", gh->group_name,
		gh->last_db_article > 1 ? "1-" : "",
		(long) gh->last_db_article);
    else
	sprintf(rcbuf, "%s:\n", gh->group_name);
    gh->newsrc_line = copy_str(rcbuf);
    dump_newsrc();
}

article_number
restore_rc(register group_header * gh, article_number last)
{
    register article_number *numtab, n;
    register attr_type *attrtab, attr;
    register int32  at, atmax;
    article_header  ahdr;
    article_number  count;

    if (last > gh->last_db_article)
	return 0;

    if (gh->unread_count <= 0) {
	/* no unread articles to account for -- quick update */
	n = gh->last_db_article;/* fake for end_rc_update */
	gh->last_db_article = last;
	begin_rc_update(gh);
	end_rc_update(gh);
	gh->last_db_article = n;
	add_unread(gh, 1);	/* not done by end_rc_update bec.
				 * mark_counter==0 */
	return gh->unread_count;
    }
    /* there are unread articles in the group */
    /* we must truncate rc&select lines to retain older unread articles */

    atmax = at = 0;
    numtab = NULL;
    attrtab = NULL;

    use_newsrc(gh, 0);
    ahdr.flag = 0;
    count = gh->unread_count;

    for (n = gh->last_article + 1; n <= last; n++) {
	if (cur.rc_min == END_OF_LIST) {
	    /* current & rest is unread */
	    last = n - 1;
	    break;
	}
	ahdr.a_number = n;
	if ((attr = rctest(&ahdr, &cur)) == A_READ)
	    continue;
	if (at >= atmax) {
	    atmax += 100;
	    numtab = resizeobj(numtab, article_number, atmax);
	    attrtab = resizeobj(attrtab, attr_type, atmax);
	}
	numtab[at] = n;
	attrtab[at] = attr;
	at++;
    }

    begin_rc_update(gh);
    while (--at >= 0) {
	ahdr.a_number = *numtab++;
	mark_article(&ahdr, *attrtab++);
    }
    for (n = last + 1; n <= gh->last_db_article; n++) {
	ahdr.a_number = n;
	mark_article(&ahdr, (attr_type) 0);
    }
    end_rc_update(gh);
    return gh->unread_count - count;
}

int
restore_unread(register group_header * gh)
{
    if (gh->select_line != gh->select_orig) {
	if (gh->select_line != NULL)
	    freeobj(gh->select_line);
	gh->select_line = gh->select_orig;
	dump_select();
    }
    if (gh->newsrc_orig == gh->newsrc_line)
	return 0;

    add_unread(gh, -1);
    if (gh->newsrc_line != NULL)
	freeobj(gh->newsrc_line);
    gh->newsrc_line = gh->newsrc_orig;
    gh->last_article = gh->first_article;
    dump_newsrc();

    add_unread(gh, 1);

    return 1;
}

void
count_unread_articles(void)
{
    register group_header *gh;
    long            n = 0;

    unread_articles = 0;
    unread_groups = 0;

    Loop_Groups_Sequence(gh) {
	gh->unread_count = 0;

	if (gh->master_flag & M_NO_DIRECTORY)
	    continue;

	if (gh->last_db_article > gh->last_article) {
	    n = unread_articles;
	    add_unread(gh, 1);
	}
	if ((gh->group_flag & G_COUNTED) == 0)
	    continue;
	if (verbose)
	    tprintf("%6d %s\n", unread_articles - n, gh->group_name);
    }
}


void
prt_unread(register char *format)
{
    if (format == NULL) {
	clrdisp();
	tprintf("No News (is good news)\n");
	return;
    }
    while (*format) {
	if (*format != '%') {
	    tputc(*format++);
	    continue;
	}
	format++;
	switch (*format++) {
	    case 'u':
		tprintf("%ld unread article%s", unread_articles, plural((long) unread_articles));
		continue;
	    case 'g':
		tprintf("%d group%s", unread_groups, plural((long) unread_groups));
		continue;
	    case 'i':
		tprintf(unread_articles == 1 ? "is" : "are");
		continue;
	    case 'U':
		tprintf("%ld", unread_articles);
		continue;
	    case 'G':
		tprintf("%d", unread_groups);
		continue;
	}
    }
}

article_number
add_unread(group_header * gh, int mode)
 /* mode:  +1 => count + add, 0 => gh->unread_count, -1 => subtract */
{
    article_number  old_count;
    article_header  ahdr;

    old_count = gh->unread_count;

    if (mode == 0)
	goto add_directly;

    if (gh->group_flag & G_COUNTED) {
	unread_articles -= gh->unread_count;
	unread_groups--;
	gh->unread_count = 0;
	gh->group_flag &= ~G_COUNTED;
    }
    if (mode < 0)
	goto out;

    if (quick_unread_count)
	gh->unread_count = gh->last_db_article - gh->last_article;
    else {
	use_newsrc(gh, 0);
	ahdr.flag = 0;
	for (ahdr.a_number = gh->last_article + 1;
	     ahdr.a_number <= gh->last_db_article;
	     ahdr.a_number++) {
	    if (cur.rc_min == END_OF_LIST) {
		gh->unread_count += gh->last_db_article - ahdr.a_number + 1;
		break;
	    }
	    if (rctest(&ahdr, &cur) != A_READ)
		gh->unread_count++;
	}
    }

add_directly:
    if (gh->unread_count <= 0) {
	gh->unread_count = 0;
	goto out;
    }
    if (gh->group_flag & G_UNSUBSCRIBED)
	goto out;

    unread_articles += gh->unread_count;
    unread_groups++;
    gh->group_flag |= G_COUNTED;

out:
    return old_count != gh->unread_count;
}

/*
 *	nngrep
 */

static int
                grep_all = 0, grep_new = 0, grep_not_sequence = 0,
                grep_pending = 0, grep_read = 0, grep_sequence = 0,
                grep_unsub = 0, grep_long = 0, grep_patterns;

static 
Option_Description(grep_options)
{
    'a', Bool_Option(grep_all),
    'i', Bool_Option(grep_not_sequence),
    'n', Bool_Option(grep_new),
    'p', Bool_Option(grep_pending),
    'r', Bool_Option(grep_read),
    's', Bool_Option(grep_sequence),
    'u', Bool_Option(grep_unsub),
    'l', Bool_Option(grep_long),
    '\0',
};

void
opt_nngrep(int argc, char *argv[])
{
    grep_patterns =
    parse_options(argc, argv, (char *) NULL, grep_options, " pattern...");
}

void
do_grep(char **pat)
{
    register group_header *gh;
    register regexp **re = NULL;
    register int    i;
    int             header = 1;

    if (grep_patterns > 0) {
	re = newobj(regexp *, grep_patterns);
	for (i = 0; i < grep_patterns; i++)
	    re[i] = regcomp(pat[i]);
    }
    Loop_Groups_Sorted(gh) {
	if (gh->master_flag & M_IGNORE_GROUP)
	    continue;

	if (grep_pending && gh->unread_count <= 0)
	    continue;
	if (grep_read && gh->unread_count > 0)
	    continue;
	if (grep_sequence && (gh->group_flag & G_SEQUENCE) == 0)
	    continue;
	if (grep_not_sequence && (gh->group_flag & G_SEQUENCE))
	    continue;
	if (grep_new && (gh->group_flag & G_NEW) == 0)
	    continue;
	if (!grep_all) {
	    if (grep_unsub && (gh->group_flag & G_UNSUBSCRIBED) == 0)
		continue;
	    if (!grep_unsub && (gh->group_flag & G_UNSUBSCRIBED))
		continue;
	}
	if (grep_patterns > 0) {
	    for (i = 0; i < grep_patterns; i++)
		if (regexec(re[i], gh->group_name))
		    break;
	    if (i == grep_patterns)
		continue;
	}
	if (grep_long) {
	    if (header)
		tprintf("SUBSCR IN_RC NEW UNREAD SEQUENCE GROUP\n");
	    header = 0;

	    tprintf(" %s   %s   %s ",
		    (gh->group_flag & G_UNSUBSCRIBED) ? "no " : "yes",
		    (gh->newsrc_line == NULL) ? "no " : "yes",
		    (gh->group_flag & G_NEW) ? "yes" : "no ");

	    if (gh->unread_count > 0)
		tprintf("%6d ", gh->unread_count);
	    else
		tprintf("       ");
	    if (gh->group_flag & G_SEQUENCE)
		tprintf("  %4d   ", gh->preseq_index);
	    else
		tprintf("         ");
	}
	tprintf("%s\n", gh->group_name);
    }
}


/*
 *	nntidy
 */

static int
                tidy_unsubscribed = 0,	/* truncate lines for unsub groups */
                tidy_remove_unsub = 0,	/* remove lines for unsub groups */
                tidy_sequence = 0,	/* remove groups not in sequence */
                tidy_ignored = 0,	/* remove G_IGN groups */
                tidy_crap = 0,	/* remove unrecognized lines */
                tidy_all = 0;	/* all of the above */

static 
Option_Description(tidy_options)
{
    'N', Bool_Option(no_update),
    'Q', Bool_Option(silent),
    'v', Bool_Option(verbose),
    'a', Bool_Option(tidy_all),
    'c', Bool_Option(tidy_crap),
    'i', Bool_Option(tidy_ignored),
    'r', Bool_Option(tidy_remove_unsub),
    's', Bool_Option(tidy_sequence),
    'u', Bool_Option(tidy_unsubscribed),
    '\0',
};

int
opt_nntidy(int argc, char *argv[])
{
    return parse_options(argc, argv, (char *) NULL,
			 tidy_options, " [group]...");
}

void
do_tidy_newsrc(void)
{
    register group_header *gh;
    int             changed;
    char           *why;

    /* visit_rc_file has been called. */

    keep_rc_backup = 1;
    bak_suffix = ".tidy";

    tidy_newsrc = 0;
    changed = 0;

    if (tidy_all)
	tidy_sequence = tidy_ignored = tidy_crap = tidy_unsubscribed = 1;

    newsrc_update_freq = 9999;

    Loop_Groups_Newsrc(gh) {
	if ((gh->master_flag & M_VALID) == 0) {
	    why = "Unknown group:   ";
	    goto delete;
	}
	if (tidy_sequence && (gh->group_flag & G_SEQUENCE) == 0) {
	    why = "Not in sequence: ";
	    goto delete;
	}
	if (tidy_ignored && (gh->master_flag & M_IGNORE_GROUP)) {
	    why = "Ignored group:   ";
	    goto delete;
	}
	if (tidy_crap && (gh->group_flag & G_FAKED)) {
	    why = "Crap in .newsrc: ";
	    goto delete;
	}
	if (tidy_remove_unsub && (gh->group_flag & G_UNSUBSCRIBED)) {
	    if (gh->group_flag & G_FAKED)
		continue;
	    why = "Unsubscribed:    ";
	    goto delete;
	}
	if (tidy_unsubscribed && (gh->group_flag & G_UNSUBSCRIBED)) {
	    if (gh->group_flag & G_FAKED)
		continue;

	    begin_rc_update(gh);
	    gh->last_db_article = 0;
	    end_rc_update(gh);

	    if (gh->newsrc_line != gh->newsrc_orig) {
		why = "Truncated:       ";
		goto change;
	    }
	}
	if (verbose) {
	    why = "Ok:              ";
	    goto report;
	}
	continue;

delete:
	gh->newsrc_line = NULL;
	gh->select_line = NULL;

change:
	changed = 1;

report:
	if (!silent)
	    tprintf("%s%s\n", why, gh->group_name);
    }

    if (no_update) {
	if (changed)
	    tprintf("Files were NOT updated\n");
	return;
    }
    if (changed) {
	newsrc_update_freq = 0;
	dump_newsrc();
	dump_select();
	tprintf("NOTICE: Original files are saved with %s extention\n", bak_suffix);
    }
}

/*
 *	nngoback
 */

static int
                goback_interact = 0,	/* interactive nngoback */
                goback_days = -1, goback_alsounsub = 0;	/* unsubscribed groups
							 * also */

static 
Option_Description(goback_options)
{
    'N', Bool_Option(no_update),
    'Q', Bool_Option(silent),
    'd', Int_Option(goback_days),
    'i', Bool_Option(goback_interact),
    'u', Bool_Option(goback_alsounsub),
    'v', Bool_Option(verbose),
    '\0',
};

int
opt_nngoback(int argc, char ***argvp)
{
    int             n;

    n = parse_options(argc, *argvp, (char *) NULL, goback_options,
		      " days [groups]...");

    if (goback_days < 0) {
	if (n == 0 || !isdigit((*argvp)[1][0])) {
	    fprintf(stderr, "usage: %s [-NQvi] days [groups]...\n", pname);
	    nn_exit(1);
	}
	goback_days = atoi((*argvp)[1]);
	n--;
	++*argvp;
    }
    return n;
}

void
do_goback(void)
{
    char            back_act[FILENAME];
    FILE           *ba;
    register group_header *gh;
    article_number  count, total;
    int             groups, y;

    sprintf(back_act, "%s/active.%d", db_directory, goback_days);
    if ((ba = open_file(back_act, OPEN_READ)) == NULL) {
	fprintf(stderr, "Cannot go back %d days\n", goback_days);
	nn_exit(1);
    }
    read_active_file(ba, (FILE *) NULL);

    fclose(ba);

    /* visit_rc_file has been called. */

    keep_rc_backup = 1;
    bak_suffix = ".goback";
    newsrc_update_freq = 9999;
    quick_unread_count = 0;
    total = groups = 0;

    if (goback_interact) {
	if (no_update)
	    tprintf("Warning: changes will not be saved\n");
	init_term(1);
	nn_raw();
    }
    Loop_Groups_Sequence(gh) {
	if ((gh->master_flag & M_VALID) == 0)
	    continue;
	if (!goback_alsounsub && (gh->group_flag & G_UNSUBSCRIBED))
	    continue;

	add_unread(gh, 1);

	count = restore_rc(gh, gh->last_a_article);
	if (count > 0) {
	    if (goback_interact) {
		tprintf("%s + %ld ?  (y) ", gh->group_name, (long) count);
		fl;
		y = yes(0);
		tputc(CR);
		tputc(NL);
		switch (y) {
		    case 1:
			break;
		    case 0:
			gh->newsrc_line = gh->newsrc_orig;
			gh->select_line = gh->select_orig;
			continue;
		    case -1:
			if (total > 0) {
			    tprintf("\nSave changes sofar? (n) ");
			    fl;
			    if (yes(1) <= 0)
				nn_exit(0);
			}
			goto out;
		}
	    } else if (verbose)
		tprintf("%5ld\t%s\n", (long) count, gh->group_name);

	    total += count;
	    groups++;
	}
    }

out:

    if (goback_interact)
	unset_raw();

    if (total == 0) {
	tprintf("No articles marked\n");
	return;
    }
    flush_newsrc();

    if (verbose)
	tputc(NL);
    if (!silent)
	tprintf("%ld article%s marked unread in %d group%s\n",
		(long) total, plural((long) total),
		groups, plural((long) groups));

    if (no_update)
	tprintf("No files were updated\n");
}

/* fake this for read_active_file */

group_header   *
add_new_group(char *name)
{
    return NULL;
}
