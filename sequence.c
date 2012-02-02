/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Read presentation sequence file
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "db.h"
#include "debug.h"
#include "macro.h"
#include "nn.h"
#include "sequence.h"
#include "nn_term.h"
#include "variable.h"

/* sequence.c */

static int      enter_sequence(int mode, group_header * gh);
static void     faked_entry(char *name, flag_type flag);
static void     end_sequence(void);
static int      visit_presentation_file(char *directory, char *seqfile, FILE * hook);

group_header   *group_sequence;
char           *read_mail = NULL;
int             also_subgroups = 1;
int             hex_group_args = 0;

static int      seq_break_enabled = 1;	/* !! enabled */
static int      ignore_done_flag = 0;	/* % toggle */

static group_header *tail_sequence = NULL;
static group_header *final_sequence = NULL;

static int      gs_more_groups;

extern group_header *rc_sequence;

int
only_folder_args(char **args)
{
    register char  *arg;

    while ((arg = *args++)) {
	if (*arg == '+' || *arg == '~' || *arg == '/')
	    continue;
	if (file_exist(arg, "fr"))
	    continue;
	return 0;
    }
    return 1;
}


#define	SHOW_NORMAL	0	/* : put this in at current pos */
#define	SHOW_FIRST	1	/* < : show these groups first */
#define	SHOW_LAST	2	/* > : show this as late as possible */
#define	IGNORE_ALWAYS	3	/* ! : ignore these groups completely */
#define IGN_UNLESS_RC	4	/* !:X ignore these groups unless in rc */
#define	IGN_UNLESS_NEW	5	/* !:O ignore these groups unless new */
#define IGN_UNL_RC_NEW	6	/* !:U ignore unsubscribed */
#define	IGN_IF_NEW	7	/* !:N ignore these groups if new */

#define	SHOW_MODES	" <>!-?*"

static int
enter_sequence(int mode, group_header * gh)
{

#ifdef SEQ_TEST
    if (Debug & SEQ_TEST && mode != SHOW_NORMAL)
	tprintf("SEQ(%c), %s\n", SHOW_MODES[mode], gh->group_name);
#endif

    if (gh->master_flag & M_IGNORE_GROUP)
	return 0;
    if (ignore_done_flag) {
	if (gh->group_flag & G_SEQUENCE)
	    return 0;
    } else if (gh->group_flag & G_DONE)
	return 0;

    switch (mode) {
	case IGN_UNLESS_NEW:
	    if ((gh->group_flag & G_NEW) == 0)
		gh->group_flag |= G_DONE;
	    return 0;

	case IGN_IF_NEW:
	    if (gh->group_flag & G_NEW)
		gh->group_flag |= G_DONE;
	    return 0;

	case IGN_UNL_RC_NEW:
	    if (gh->group_flag & G_NEW)
		return 0;
	    if (gh->newsrc_line == NULL || (gh->group_flag & G_UNSUBSCRIBED))
		gh->group_flag |= G_DONE;
	    return 0;

	case IGN_UNLESS_RC:
	    if (gh->newsrc_line == NULL || (gh->group_flag & (G_UNSUBSCRIBED | G_NEW)))
		gh->group_flag |= G_DONE;
	    return 0;

	case IGNORE_ALWAYS:
	    gh->group_flag |= G_DONE;
	    return 0;

	default:
	    gh->group_flag |= G_DONE;
	    break;
    }

    gh->group_flag |= G_SEQUENCE;

    if (gh->master_flag & M_NO_DIRECTORY)
	return 0;		/* for nntidy -s */

    switch (mode) {
	case SHOW_FIRST:
	    if (tail_sequence) {
		gh->next_group = group_sequence;
		group_sequence = gh;
		break;
	    }
	    /* FALLTHRU */
	case SHOW_NORMAL:
	    if (tail_sequence)
		tail_sequence->next_group = gh;
	    else
		group_sequence = gh;
	    tail_sequence = gh;
	    break;

	case SHOW_LAST:
	    gh->next_group = final_sequence;
	    final_sequence = gh;
	    break;
    }
    return 1;
}


static void
faked_entry(char *name, flag_type flag)
{
    group_header   *gh;

    gh = newobj(group_header, 1);

    gh->group_name = name;
    gh->group_flag = flag | G_FAKED;
    gh->master_flag = 0;

    /* "invent" an unread article for read_news */
    gh->last_article = 1;
    gh->last_db_article = 2;

    enter_sequence(SHOW_NORMAL, gh);
}

static void
end_sequence(void)
{
    register group_header *gh, *backp;
    register int    seq_ix;

    if (tail_sequence)
	tail_sequence->next_group = NULL;

    /* set up backward pointers */

    backp = NULL;
    seq_ix = 0;
    Loop_Groups_Sequence(gh) {
	gh->preseq_index = ++seq_ix;
	gh->prev_group = backp;
	backp = gh;
    }

#ifdef SEQ_DUMP
    if (Debug & SEQ_DUMP) {
	for (gh = group_sequence; gh; gh = gh->next_group)
	    tprintf("%s\n", gh->group_name);
	tputc(NL);

	nn_exit(0);
    }
#endif

}


#ifdef MAIL_READING
static int
mail_check(void)
{
    static group_header mail_group;
    struct stat     st;

    if (read_mail == NULL)
	return;
    if (stat(read_mail, &st) < 0)
	return;
    if (st.st_size == 0 || st.st_mtime < st.st_atime)
	return;

    mail_group.group_name = read_mail;
    gh->group_flag = G_FOLDER | G_MAILBOX | G_FAKED;
    gh->master_flag = 0;

    /* "invent" an unread article for read_news */
    gh->last_article = 1;
    gh->last_db_article = 2;


    if (tail_sequence) {
	mail_group.next_group = group_sequence;
	group_sequence = mail_group;
    } else
	enter_sequence(SHOW_NORMAL, &mail_group);
}

#endif



static int
visit_presentation_file(char *directory, char *seqfile, FILE * hook)
{
    register FILE  *sf;
    register int    c;
    register group_header *gh;
    group_header   *mp_group;
    char            group[FILENAME], *gname;
    char            savefile[FILENAME], *dflt_save, *enter_macro;
    register char  *gp;
    int             mode, merge_groups;

    if (gs_more_groups == 0)
	return 0;

    if (hook != NULL)
	sf = hook;		/* hook to init file */
    else if ((sf = open_file(relative(directory, seqfile), OPEN_READ)) == NULL)
	return 0;

#ifdef SEQ_TEST
    if (Debug & SEQ_TEST)
	tprintf("Sequence file %s/%s\n", directory, seqfile);
#endif

    mode = SHOW_NORMAL;
    savefile[0] = NUL;
    dflt_save = NULL;

    while (gs_more_groups) {

	if ((c = getc(sf)) == EOF)
	    break;
	if (!isascii(c) || isspace(c))
	    continue;

	switch (c) {
	    case '!':
		mode = IGNORE_ALWAYS;
		if ((c = getc(sf)) == EOF)
		    continue;
		if (c == '!') {
		    if (seq_break_enabled) {
			fclose(sf);
			return 1;
		    }
		    mode = SHOW_NORMAL;
		    continue;
		}
		if (c == ':') {
		    if ((c = getc(sf)) == EOF)
			continue;
		    if (!isascii(c) || isspace(c) || !isupper(c))
			continue;
		    switch (c) {
			case 'O':
			    mode = IGN_UNLESS_NEW;
			    continue;
			case 'N':
			    mode = IGN_IF_NEW;
			    continue;
			case 'U':
			    mode = IGN_UNL_RC_NEW;
			    continue;
			case 'X':
			    mode = IGN_UNLESS_RC;
			    continue;
			default:
			    /* should give error here */
			    mode = SHOW_NORMAL;
			    continue;
		    }
		}
		ungetc(c, sf);
		continue;

	    case '<':
		mode = SHOW_FIRST;
		continue;

	    case '>':
		mode = SHOW_LAST;
		continue;

	    case '%':
		ignore_done_flag = !ignore_done_flag;
		continue;

	    case '@':
		seq_break_enabled = 0;
		mode = SHOW_NORMAL;
		continue;

	    case '#':
		do
		    c = getc(sf);
		while (c != EOF && c != NL);
		mode = SHOW_NORMAL;
		continue;

	}

	gp = group;
	merge_groups = 0;
	do {
	    *gp++ = c;
	    if (c == ',')
		merge_groups = 1;
	    c = getc(sf);
	} while (c != EOF && isascii(c) && !isspace(c));

	*gp = NUL;

	while (c != EOF && (!isascii(c) || isspace(c)))
	    c = getc(sf);
	if (c == '+' || c == '~' || c == '/') {
	    gp = savefile;
	    if (c == '+') {
		c = getc(sf);
		if (c == EOF || (isascii(c) && isspace(c)))
		    goto use_same_savefile;
		*gp++ = '+';
	    }
	    do {
		*gp++ = c;
		c = getc(sf);
	    } while (c != EOF && isascii(c) && !isspace(c));
	    *gp = NUL;
	    dflt_save = savefile[0] ? copy_str(savefile) : NULL;
	} else
	    dflt_save = NULL;

use_same_savefile:
	while (c != EOF && (!isascii(c) || isspace(c)))
	    c = getc(sf);
	if (c == '(') {
	    enter_macro = parse_enter_macro(sf, getc(sf));
	} else {
	    enter_macro = NULL;
	    if (c != EOF)
		ungetc(c, sf);
	}

	mp_group = NULL;
	for (gp = group; *gp;) {
	    gname = gp;
	    if (merge_groups) {
		while (*gp && *gp != ',')
		    gp++;
		if (*gp)
		    *gp++ = NUL;
	    }
	    start_group_search(gname);

	    while ((gh = get_group_search())) {
		if (merge_groups && gh->group_flag & G_UNSUBSCRIBED)
		    continue;

		if (!enter_sequence(mode, gh))
		    continue;

		if (merge_groups) {
		    if (mp_group == NULL) {
			gh->group_flag |= G_MERGE_HEAD;
		    } else {
			mp_group->merge_with = gh;
			gh->group_flag |= G_MERGE_SUB;
		    }
		    mp_group = gh;
		}
		if (gh->save_file == NULL)	/* not set by "save-files" */
		    gh->save_file = dflt_save;
		if (gh->enter_macro == NULL)	/* not set by "on entry" */
		    gh->enter_macro = enter_macro;
	    }
	    if (!merge_groups)
		*gp = NUL;
	}
	if (merge_groups && mp_group != NULL)
	    mp_group->merge_with = NULL;
	mode = SHOW_NORMAL;
    }

    fclose(sf);
    return 0;
}

void
parse_save_files(register FILE * sf)
{
    register int    c;
    register group_header *gh;
    char            group[FILENAME];
    char           *savefile = NULL;
    char            namebuf[FILENAME];
    register char  *gp;

    for (;;) {
	if ((c = getc(sf)) == EOF)
	    break;
	if (!isascii(c) || isspace(c))
	    continue;
	if (c == '#') {
	    do
		c = getc(sf);
	    while (c != EOF && c != NL);
	    continue;
	}
	gp = group;
	do {
	    *gp++ = c;
	    c = getc(sf);
	} while (c != EOF && isascii(c) && !isspace(c));
	*gp = NUL;

	if (strcmp(group, "end") == 0)
	    break;

	while (c != EOF && (!isascii(c) || isspace(c)))
	    c = getc(sf);

	gp = namebuf;
	do {
	    *gp++ = c;
	    c = getc(sf);
	} while (c != EOF && isascii(c) && !isspace(c));
	*gp = NUL;
	if (namebuf[0] == NUL)
	    break;
	if (strcmp(namebuf, "+"))
	    savefile = copy_str(namebuf);

	start_group_search(group);

	while ((gh = get_group_search()))
	    gh->save_file = savefile;
    }
}

void
named_group_sequence(char **groups)
{
    register group_header *gh;
    register char  *group;
    register char  *value;
    int             non_vars;
    int             found, errors, gnum;

    group_sequence = NULL;
    also_subgroups = 0;

    errors = 0;
    non_vars = 0;
    while ((group = *groups++)) {
	non_vars++;

	if (hex_group_args) {
	    sscanf(group, "%d", &gnum);
	    if (gnum < 0 || gnum >= master.number_of_groups)
		continue;
	    gh = ACTIVE_GROUP(gnum);
	    enter_sequence(SHOW_NORMAL, gh);
	    continue;
	}
	if ((gh = lookup(group))) {
	    enter_sequence(SHOW_NORMAL, gh);
	    continue;
	}
	if ((value = strchr(group, '='))) {
	    *value++ = NUL;
	    set_variable(group, 1, value);
	    non_vars--;
	    continue;
	}
	if (*group == '+' || *group == '~' || file_exist(group, "fr")) {
	    faked_entry(group, G_FOLDER);
	    continue;
	}

	found = 0;
	start_group_search(group);
	while ((gh = get_group_search())) {
	    found++;
	    enter_sequence(SHOW_NORMAL, gh);
	}

	if (!found) {
	    tprintf("Group %s not found\n", group);
	    fl;
	    errors++;
	}
    }

    if (non_vars == 0) {
	normal_group_sequence();
	return;
    }
    end_sequence();

    if (errors)
	user_delay(2);

    return;
}

FILE           *loc_seq_hook = NULL;	/* sequence in local "init" file */
FILE           *glob_seq_hook = NULL;	/* sequence in global "init" file */

void
normal_group_sequence(void)
{
    register group_header *gh;

    group_sequence = NULL;
    gs_more_groups = 1;

    /* visit_p_f returns non-zero if terminated by !! */

    if (visit_presentation_file(nn_directory, "seq", loc_seq_hook))
	goto final;

    if (visit_presentation_file(lib_directory, "sequence", glob_seq_hook))
	goto final;

    Loop_Groups_Sorted(gh) {
	enter_sequence(SHOW_NORMAL, gh);
    }

final:
    if (final_sequence) {
	if (tail_sequence) {
	    tail_sequence->next_group = final_sequence;
	    tail_sequence = NULL;
	} else {
	    group_sequence = final_sequence;
	}
    }

#ifdef MAIL_READING
    mail_check();
#endif

    end_sequence();
}



static char    *gs_group;
static int      gs_length, gs_index, gs_mode;
static group_header *gs_only_group = NULL;

#define GS_PREFIX0	0	/* group (or group*) */
#define	GS_PREFIX	1	/* group. */
#define	GS_SUFFIX	2	/* .group */
#define GS_INFIX	3	/* .group. */
#define GS_NEW_GROUP	4	/* new group */
#define GS_ALL		5	/* all / . */
#define	GS_NEWSRC	6	/* RC */

void
start_group_search(char *group)
{
    char           *dot;
    int             last;

    gs_index = master.number_of_groups;	/* loop will fail */

    if ((last = strlen(group) - 1) < 0)
	return;
    if (group[last] == '*')
	group[last] = NUL;
    else if (!also_subgroups && (gs_only_group = lookup(group)) != NULL)
	return;

    gs_index = 0;
    gs_more_groups = 0;
    gs_length = 0;
    gs_group = NULL;

    if (strcmp(group, "NEW") == 0) {
	gs_mode = GS_NEW_GROUP;
	return;
    }
    if (strncmp(group, "RC", 2) == 0) {
	gs_mode = GS_NEWSRC;
	gs_only_group = rc_sequence;
	gs_more_groups = 1;	/* we just can't know! */

	if (group[2] != ':')
	    return;
	if (isdigit(group[3]))
	    gs_index = atoi(group + 3);
	else {
	    gs_group = group + 3;
	    gs_length = strlen(gs_group);
	}
	return;
    }
    if (strcmp(group, "all") == 0 || strcmp(group, ".") == 0) {
	gs_mode = GS_ALL;
	return;
    }
    gs_mode = GS_PREFIX0;

    if (strncmp(group, "all.", 4) == 0)
	group += 3;

    if (*group == '.')
	gs_mode = GS_SUFFIX;

    if ((dot = strrchr(group, '.')) != NULL && dot != group) {
	if (dot[1] == NUL || strcmp(dot + 1, "all") == 0) {
	    dot[1] = NUL;
	    gs_mode = (gs_mode == GS_SUFFIX) ? GS_INFIX : GS_PREFIX;
	}
    }
    gs_length = strlen(group);
    gs_group = group;
}

#define STREQN(a, b, n) ((a)[0] == (b)[0] && strncmp(a, b, (int)(n)) == 0)

group_header   *
get_group_search(void)
{
    register group_header *gh, **ghp;
    register int    gsidx = gs_index;	/* cached gs_index (RW) */
    register int    mstngrps = master.number_of_groups;	/* cached (RO) */
    register int    gdonemask = (ignore_done_flag ? 0 : G_DONE);
    register int    tail;
    register int    gslen = gs_length;	/* cached (RO) */
    register int    gsmgrps = gs_more_groups;	/* cached (RW) */
    register char  *ghgrpnm;
    register char  *gsgrp = gs_group;	/* cached (RO) */
    register int    gsmode = gs_mode;	/* cached (RO) */
    register group_header *gsog = gs_only_group;	/* cached (RW) */

    /* most of start up CPU time goes here */
    if (gsmode == GS_NEWSRC) {
	/* gsgrp is usually "" */

#ifdef notdef			/* seemed like a good idea at the time;
				 * actually slower */
	if (gslen == 0) {	/* common case */
	    if (gsidx != 0)
		--gs_index;
	    gs_only_group = NULL;
	    return NULL;
	}
#endif

	do {
	    gh = gsog;
	    if (gh == NULL)
		break;
	    if (gsidx && --gsidx == 0)
		gsog = NULL;
	    else if (gsgrp && gh->group_name_length >= gslen &&
		     strncmp(gh->group_name, gsgrp, gslen) == 0) {
		gsog = NULL;
	    } else
		gsog = gh->newsrc_seq;
	} while (gh->group_flag & gdonemask || gh->master_flag & M_IGNORE_GROUP);
	gs_only_group = gsog;
	gs_index = gsidx;
	return gh;
    }
    if (gsog != NULL) {
	gh = gsog;
	gs_only_group = NULL;
	if (gh->group_flag & gdonemask || gh->master_flag & M_IGNORE_GROUP)
	    return NULL;
	return gh;
    }
    for (ghp = &sorted_groups[gsidx]; gsidx < mstngrps; ++ghp) {
	gh = *ghp;
	gsidx++;
	if (gh->group_flag & gdonemask || gh->master_flag & M_IGNORE_GROUP)
	    continue;
	gsmgrps++;
	if ((tail = gh->group_name_length - gslen) < 0)
	    continue;

	ghgrpnm = gh->group_name;
	if (gsmode == GS_PREFIX0) {
	    /* the big winner */
	    if (((tail = ghgrpnm[gslen]) != NUL && tail != '.') ||
		!STREQN(ghgrpnm, gsgrp, gslen))
		continue;
	} else if (gsmode == GS_PREFIX) {
	    if (!STREQN(ghgrpnm, gsgrp, gslen))
		continue;
	} else
	    switch (gsmode) {
		case GS_NEW_GROUP:
		    if ((gh->group_flag & G_NEW) == 0 || gh->group_flag & G_UNSUBSCRIBED)
			continue;
		    break;

		case GS_SUFFIX:
		    if (strcmp(ghgrpnm + tail, gsgrp) != 0)
			continue;
		    break;

		case GS_INFIX:
		    nn_exitmsg(1, ".name. notation not supported (yet)");
		    break;

		case GS_ALL:
		    break;
	    }
	gsmgrps--;
	gs_more_groups = gsmgrps;
	gs_only_group = gsog;
	gs_index = gsidx;
	return gh;
    }
    gs_more_groups = gsmgrps;
    gs_only_group = gsog;
    gs_index = gsidx;
    return NULL;
}
