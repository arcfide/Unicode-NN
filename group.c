/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	News group access.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include "config.h"
#include "global.h"
#include "articles.h"
#include "db.h"
#include "folder.h"
#include "macro.h"
#include "match.h"
#include "menu.h"
#include "newsrc.h"
#include "nn.h"
#include "nntp.h"
#include "regexp.h"
#include "sort.h"
#include "nn_term.h"
#include "variable.h"

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

/* group.c */

static int      print_header(void);
static int      l_re_query(char *pr, group_header * gh);
static int      merged_header(void);
static int      disp_group(group_header * gh);



int             dont_split_digests = 0;
int             dont_sort_articles = 0;
int             also_cross_postings = 0;
int             also_unsub_groups = 0;
int             entry_message_limit = 400;
int             merge_report_rate = 1;

extern int      article_limit, also_read_articles;
extern int      no_update, novice, case_fold_search;
extern int      merged_menu, keep_unsubscribed, keep_unsub_long;
extern int      killed_articles;
extern int      seq_cross_filtering;
extern char    *default_save_file, *folder_save_file;

extern char     delayed_msg[];
extern int32    db_read_counter;

extern long     unread_articles;
extern int      unread_groups;
extern int      rc_merged_groups_hack;
extern int      get_from_macro;
extern group_header *jump_to_group;
extern int      bypass_consolidation;

/*
 * completion of group name
 */

int
group_completion(char *hbuf, int ix)
{
    static group_number next_group, n1, n2;
    static char    *head, *tail, *last;
    static int      tail_offset, prev_lgt, l1, l2;
    static group_header *prev_group, *p1, *p2;
    register group_header *gh;
    register char  *t1, *t2;
    int             order;

    if (ix < 0)
	return 0;

    if (hbuf) {
	n2 = next_group = 0;
	p2 = prev_group = NULL;
	l2 = 0;

	if ((head = strrchr(hbuf, ',')))
	    head++;
	else
	    head = hbuf;
	tail = hbuf + ix;
	tail_offset = ix - (head - hbuf);
	if ((last = strrchr(head, '.')))
	    last++;
	else
	    last = head;
	return 1;
    }
    if (ix) {
	n1 = next_group, p1 = prev_group, l1 = prev_lgt;
	next_group = n2, prev_group = p2, prev_lgt = l2;
	list_completion((char *) NULL);
    }
    *tail = NUL;

    while (next_group < master.number_of_groups) {
	gh = sorted_groups[next_group++];
	if (gh->master_flag & M_IGNORE_GROUP)
	    continue;
	if (gh->group_name_length <= tail_offset)
	    continue;

	if (prev_group &&
	    strncmp(prev_group->group_name, gh->group_name, prev_lgt) == 0)
	    continue;

	order = strncmp(gh->group_name, head, tail_offset);
	if (order < 0)
	    continue;
	if (order > 0)
	    break;

	t1 = gh->group_name + tail_offset;
	if ((t2 = strchr(t1, '.'))) {
	    strncpy(tail, t1, t2 - t1 + 1);
	    tail[t2 - t1 + 1] = NUL;
	} else
	    strcpy(tail, t1);

	prev_group = gh;
	prev_lgt = tail_offset + strlen(tail);
	if (ix) {
	    if (list_completion(last) == 0)
		break;
	} else
	    return 1;
    }

    if (ix) {
	n2 = next_group, p2 = prev_group, l2 = prev_lgt;
	if (n2 > master.number_of_groups)
	    n2 = 0, p2 = NULL, l2 = 0;
	next_group = n1, prev_group = p1, prev_lgt = l1;
	return 1;
    }
    next_group = 0;
    prev_group = NULL;
    return 0;
}

static int      group_level = 0;
static article_number entry_first_article;
static int      only_unread_articles;	/* menu contains only unread art. */

static int
print_header(void)
{
    so_printxy(0, 0, "Newsgroup: %s", current_group->group_name);

    if (current_group->group_flag & G_MERGED)
	tprintf("    MERGED");

    clrline();

    so_gotoxy(-1, 0, 0);

    so_printf("Articles: %d", n_articles);

    if (no_update) {
	so_printf(" NO UPDATE");
    } else {
	if ((current_group->group_flag & G_MERGED) == 0 &&
	    current_group->current_first > entry_first_article)
	    so_printf((killed_articles > 0) ? "(L-%ld,K-%d)" : "(L-%ld)",
		      current_group->current_first - entry_first_article,
		      killed_articles);
	else if (killed_articles > 0)
	    so_printf(" (K-%d)", killed_articles);

	if (unread_articles > 0) {
	    so_printf(" of %ld", unread_articles);
	    if (unread_groups > 0)
		so_printf("/%d", unread_groups);
	}
	if (current_group->group_flag & G_UNSUBSCRIBED)
	    so_printf(" UNSUB");
	else if (current_group->group_flag & G_NEW)
	    so_printf(" NEW");

	if (current_group->unread_count <= 0)
	    so_printf(" READ");

	if (group_level > 1)
	    so_printf(" *NO*UPDATE*");
    }

    so_end();

    return 1;			/* number of lines in header */
}

/*
 * interpretation of first_art:
 *	>0	Read articles first_art..last_db_article (also read)
 *	-1	Read unread or read articles accoring to -x and -aN flags
 *
 * This function is far to long and uses goto's all over! UGH!
 */

int
group_menu(register group_header * gh, article_number first_art, register flag_type access_mode, char *mask, fct_type menu)
{
    register group_header *mg_head;
    article_number  was_unread;
    int             status, unread_at_reentry = 0, menu_cmd;
    int             o_killed, o_only_unread;

#ifdef RENUMBER_DANGER
    article_number  prev_last;
#endif

    article_number  n, o_entry_first;
    flag_type       entry_access_mode = access_mode;

#define	menu_return(cmd) { menu_cmd = (cmd); goto menu_exit; }

    o_entry_first = entry_first_article;
    o_only_unread = only_unread_articles;
    o_killed = killed_articles;
    mark_var_stack();

    mg_head = (group_level == 0 && gh->group_flag & G_MERGE_HEAD) ? gh : NULL;
    group_level++;

reenter:
    for (; gh; gh = gh->merge_with) {
	if ((gh->master_flag & M_IGNORE_GROUP) || init_group(gh) <= 0) {
	    if (mg_head != NULL)
		continue;
	    menu_return(ME_NEXT);
	}
	if (unread_at_reentry)
	    restore_unread(gh);

	m_invoke(-1);		/* invoke macro */

	access_mode = entry_access_mode;
	if (access_mode & ACC_PARSE_VARIABLES)
	    access_mode |= parse_access_flags();

	killed_articles = 0;

	/*
	 * don't lose (a few) new articles when reentering a read group (the
	 * user will expect the menu to be the same, and may overlook the new
	 * articles)
	 */
	if (gh->group_flag & G_READ)
	    goto update_unsafe;

#ifdef RENUMBER_DANGER
	prev_last = gh->last_db_article;
#endif

	was_unread = add_unread(gh, -1);

#ifndef NOV			/* This is bad for NOV. reopens .overview */
	switch (update_group(gh)) {
	    case -1:
		status = -1;
		goto access_exception;
	    case -3:
		return ME_QUIT;
	}
#endif				/* NOV */

#ifdef RENUMBER_DANGER
	if (gh->last_db_article < prev_last) {
	    /* expire + renumbering */
	    flag_type       updflag;

	    if ((gh->last_article = gh->first_db_article - 1) < 0)
		gh->last_article = 0;
	    gh->first_article = gh->last_article;
	    updflag = gh->group_flag & G_READ;
	    gh->group_flag &= ~G_READ;
	    update_rc_all(gh, 0);
	    gh->group_flag &= ~G_READ;
	    gh->group_flag |= updflag;
	}
#endif

	if (was_unread)
	    add_unread(gh, 1);

update_unsafe:
	if ((gh->current_first = first_art) < 0) {
	    if (access_mode & (ACC_ORIG_NEWSRC | ACC_MERGED_NEWSRC))
		gh->current_first = gh->first_article + 1;
	    else if (access_mode & ACC_ALSO_READ_ARTICLES)
		gh->current_first = gh->first_db_article;
	    else
		gh->current_first = gh->last_article + 1;
	}
	if (gh->current_first <= 0)
	    gh->current_first = 1;
	entry_first_article = gh->current_first;
	only_unread_articles = (access_mode & ACC_ALSO_READ_ARTICLES) == 0;

	if (article_limit > 0) {
	    n = gh->last_db_article - article_limit + 1;
	    if (gh->current_first < n)
		gh->current_first = n;
	}
	if (mg_head != NULL) {
	    access_mode |= ACC_MERGED_MENU | ACC_DONT_SORT_ARTICLES;
	    if (mg_head != gh)
		access_mode |= ACC_EXTRA_ARTICLES;
	}
	/* entry_message_limit = 1; DEBUG */
	if (entry_message_limit &&
	    (n = gh->last_db_article - gh->current_first + 1) >= entry_message_limit) {
	    clrdisp();
	    tprintf("Reading %s: %ld articles...", gh->group_name, (long) n);
	} else
	    home();
	fl;

#ifdef NOV
	/* the real work is done here :-) */
#endif				/* NOV */

	status = access_group(gh, gh->current_first, gh->last_db_article,
			      access_mode, mask);

#ifndef NOV
access_exception:
#endif

	if (status < 0) {
	    if (status == -2) {
		clrdisp();
		tprintf("Read error on group %s (NFS timeout?)\n\r",
			current_group->group_name);
		tprintf("Skipping to next group....  (interrupt to quit)\n\r");
		if (any_key(0) == K_interrupt)
		    nn_exit(0);
	    }
	    if (status <= -10) {
		clrdisp();
		msg("DATABASE CORRUPTED FOR GROUP %s", gh->group_name);

#ifdef HAVE_SYSLOG
		openlog("nn", LOG_CONS, LOG_DAEMON);
		syslog(LOG_ALERT, "database corrupted for newsgroup %s.",
		       gh->group_name);
		closelog();
#endif

		user_delay(5);
	    }
	    gh->master_flag |= M_BLOCKED;
	    if (mg_head != NULL)
		continue;
	    menu_return(ME_NEXT);
	}
	if (mg_head == NULL)
	    break;
    }

    if (n_articles == 0) {
	m_break_entry();
	menu_return(ME_NO_ARTICLES);
    }
    if (mg_head != NULL) {
	if (dont_sort_articles)
	    no_sort_articles();
	else
	    sort_articles(-1);
	init_group(mg_head);
    }
samemenu:
    menu_cmd = CALL(menu) (print_header);

    if (menu_cmd == ME_REENTER_GROUP) {
	if (group_level != 1) {
	    strcpy(delayed_msg, "can only unread groups at topmost level");
	    goto samemenu;
	}
	free_memory();
	if (mg_head)
	    gh = mg_head;
	unread_at_reentry = 1;
	goto reenter;
    }
menu_exit:

    n = 1;			/* must unsort articles */
    if (mg_head != NULL)
	gh = mg_head;

    do {
	gh->current_first = 0;
	if (gh->master_flag & M_BLOCKED)
	    continue;
	if (mask != NULL)
	    continue;
	if (access_mode & ACC_ALSO_READ_ARTICLES || first_art >= 0)
	    continue;
	if (menu_cmd != ME_NO_ARTICLES)
	    gh->group_flag &= ~G_NEW;
	if (gh->group_flag & G_UNSUBSCRIBED && !keep_unsub_long)
	    continue;
	if (n)
	    sort_articles(-2);
	n = 0;
	update_rc(gh);
	rc_merged_groups_hack = 1;
    } while (mg_head != NULL && (gh = gh->merge_with) != NULL);

    rc_merged_groups_hack = 0;

    killed_articles = o_killed;
    entry_first_article = o_entry_first;
    only_unread_articles = o_only_unread;
    restore_variables();
    group_level--;

    return menu_cmd;
}

static int
l_re_query(char *pr, group_header * gh)
{
    if (pr == NULL)
	return 1;

    prompt("\1%s\1 %s ? ", pr, gh->group_name);
    return yes(0);
}

group_header   *
lookup_regexp(char *name, char *pr, int flag)
 /* flag		1=>seq order, 2=>msg(err) */
{
    group_header   *gh;
    regexp         *re;
    int             y, any;
    char           *err;

    if ((gh = lookup(name)))
	return gh;

    if ((re = regcomp(name)) == NULL)
	return NULL;
    y = any = 0;

    if (pr && (flag & 1))
	m_advinput();

    if (flag & 1)
	Loop_Groups_Sequence(gh) {
	if (gh->last_db_article == 0)
	    continue;
	if (gh->last_db_article < gh->first_db_article)
	    continue;
	if (!regexec(re, gh->group_name))
	    continue;
	any++;
	if ((y = l_re_query(pr, gh)))
	    goto ok;
	}

    Loop_Groups_Sorted(gh) {
	if (flag & 1) {
	    if (gh->master_flag & M_IGNORE_GROUP)
		continue;
	    if (gh->group_flag & G_SEQUENCE)
		continue;
	}
	if (!regexec(re, gh->group_name))
	    continue;
	any++;
	if ((y = l_re_query(pr, gh)))
	    goto ok;
    }

    err = any ? "No more groups" : "No group";
    if (flag & 2)
	msg("%s matching `%s'", err, name);
    else
	tprintf("\n\r%s matching `%s'\n\r", err, name);

    gh = NULL;

ok:
    freeobj(re);
    return y < 0 ? NULL : gh;
}

int
goto_group(int command, article_header * ah, flag_type access_mode)
{
    register group_header *gh;
    char            ans1, *answer, *mask, buffer[FILENAME], fbuffer[FILENAME];
    article_number  first;
    memory_marker   mem_marker;
    group_header   *orig_group;
    int             menu_cmd, cmd_key;
    article_number  o_cur_first = -1;
    group_header   *read_mode_group;

#define goto_return( cmd ) \
    { menu_cmd = cmd; goto goto_exit; }

    m_startinput();

    gh = orig_group = current_group;

    if (ah != NULL) {		/* always open new level from reading mode */
	read_mode_group = orig_group;
	orig_group = NULL;
    } else
	read_mode_group = NULL;

    mask = NULL;
    if (access_mode == 0)
	access_mode |= ACC_PARSE_VARIABLES;

    if (command == K_GOTO_GROUP)
	goto get_group_name;

    if (command == K_ADVANCE_GROUP)
	gh = gh->next_group;
    else
	gh = gh->prev_group;

    for (;;) {
	if (gh == NULL)
	    goto_return(ME_NO_REDRAW);

	if (gh->first_db_article < gh->last_db_article && gh->current_first <= 0) {
	    sprintf(buffer, "%s%s%s) ",
		    (gh->group_flag & G_UNSUBSCRIBED) ? " UNSUB" : "",
		    (gh->group_flag & G_MERGE_HEAD) ? " MERGED" : "",
		    gh->unread_count <= 0 ? " READ" : "");
	    buffer[0] = buffer[0] == ')' ? NUL : '(';

	    prompt("\1Enter\1 %s %s?  (ABGNPy) ", gh->group_name, buffer);

	    cmd_key = get_c();
	    if (cmd_key & GETC_COMMAND) {
		command = cmd_key & ~GETC_COMMAND;
		if (command == K_REDRAW)
		    goto_return(ME_REDRAW);
	    } else {
		if (cmd_key == 'y')
		    break;
		command = menu_key_map[cmd_key];
		if (command & K_MACRO)
		    command = orig_menu_map[cmd_key];
	    }
	}
	switch (command) {
	    case K_CONTINUE:
	    case K_CONTINUE_NO_MARK:
		break;

	    case K_ADVANCE_GROUP:
		gh = gh->next_group;
		continue;

	    case K_BACK_GROUP:
		gh = gh->prev_group;
		continue;

	    case K_GOTO_GROUP:
		goto get_group_name;

	    case K_PREVIOUS:
		while (gh->prev_group) {
		    gh = gh->prev_group;
		    if (gh->group_flag & G_MERGE_SUB)
			continue;
		    if (gh->group_flag & G_COUNTED)
			break;
		    if (gh->newsrc_line != gh->newsrc_orig)
			break;
		}
		continue;

	    case K_NEXT_GROUP_NO_UPDATE:
		while (gh->next_group) {
		    gh = gh->next_group;
		    if (gh->group_flag & G_MERGE_SUB)
			continue;
		    if (gh->group_flag & G_COUNTED)
			break;
		    if (gh->newsrc_line != gh->newsrc_orig)
			break;
		}
		continue;

	    default:
		goto_return(ME_NO_REDRAW);
	}
	break;
    }

    if (gh == orig_group)
	goto_return(ME_NO_REDRAW);

    goto get_first;

get_group_name:

    if (current_group == NULL) {
	prompt("\1Enter Group or Folder\1 (+./~) ");
	answer = get_s(NONE, NONE, "+./~", group_completion);
    } else {

#ifndef ART_GREP
	prompt("\1Group or Folder\1 (+./~ %%%s=sneN) ",
#else
	prompt("\1Group or Folder\1 (+./~ %%%s=sneNbB) ",	/* add 'bB' for BODY */
#endif				/* ART_GREP */

	       (gh->master_flag & M_AUTO_ARCHIVE) ? "@" : "");
	strcpy(buffer, "++./0123456789~=% ");
	if (gh->master_flag & M_AUTO_ARCHIVE)
	    buffer[0] = '@';
	answer = get_s(NONE, NONE, buffer, group_completion);
    }

    if (answer == NULL)
	goto_return(ME_NO_REDRAW);

    if ((ans1 = *answer) == NUL || ans1 == SP) {
	if (current_group == NULL)
	    goto_return(ME_NO_REDRAW);
	goto get_first;
    }
    sprintf(buffer, "%c", ans1);

    switch (ans1) {

	case '@':
	    if (current_group == NULL ||
		(current_group->master_flag & M_AUTO_ARCHIVE) == 0)
		goto_return(ME_NO_REDRAW);
	    answer = current_group->archive_file;
	    goto get_folder;

	case '%':
	    if (current_group == NULL)
		goto_return(ME_NO_REDRAW);
	    if ((current_group->group_flag & G_FOLDER) == 0) {
		if (!ah) {

#ifdef NNTP
		    if (use_nntp) {
			msg("Can only use G%% in reading mode");
			goto_return(ME_NO_REDRAW);
		    }
#endif

		    prompt("\1READ\1");
		    if ((ah = get_menu_article()) == NULL)
			goto_return(ME_NO_REDRAW);
		}
		if ((ah->flag & A_DIGEST) == 0) {

#ifdef NNTP
		    if (use_nntp) {
			answer = nntp_get_filename(ah->a_number, current_group);
			goto get_folder;
		    }
#endif

		    *group_file_name = NUL;
		    sprintf(fbuffer, "%s%ld", group_path_name, ah->a_number);
		    answer = fbuffer;
		    goto get_folder;
		}
	    }
	    msg("cannot split articles inside a folder or digest");
	    goto_return(ME_NO_REDRAW);

	    /* FALLTHROUGH */
	case '.':
	case '~':
	    strcat(buffer, "/");
	    /* FALLTHROUGH */
	case '+':
	case '/':
	    if (!get_from_macro) {
		prompt("\1Folder\1 ");
		answer = get_s(NONE, buffer, NONE, file_completion);
		if (answer == NULL || answer[0] == NUL)
		    goto_return(ME_NO_REDRAW);
	    }
	    goto get_folder;

	case 'a':
	    if (answer[1] != NUL && strcmp(answer, "all"))
		break;
	    if (current_group == NULL)
		goto_return(ME_NO_REDRAW);
	    access_mode |= ACC_ALSO_READ_ARTICLES | ACC_ALSO_CROSS_POSTINGS;
	    first = 0;
	    goto more_articles;

	case 'u':
	    if (answer[1] != NUL && strcmp(answer, "unread"))
		break;
	    if (current_group == NULL)
		goto_return(ME_NO_REDRAW);
	    access_mode |= ACC_ORIG_NEWSRC;
	    first = gh->first_article + 1;
	    goto enter_new_level;

#ifdef ART_GREP
	case 'B':		/* Body search all articles in database */
	case 'b':		/* Body search unread articles */
	    if (answer[1] != NUL && answer[1] != '=')
		break;
	    if (current_group == NULL)
		goto_return(ME_NO_REDRAW);
	    access_mode |= ACC_ALSO_READ_ARTICLES | ACC_ALSO_CROSS_POSTINGS;
	    goto get_mask;
#endif				/* ART_GREP */

	case 'e':
	case 'n':
	case 's':
	    if (answer[1] != NUL && answer[1] != '=')
		break;
	    /* FALLTHROUGH */
	case '=':
	    if (current_group == NULL)
		goto_return(ME_NO_REDRAW);
	    goto get_mask;

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
	    if (current_group == NULL)
		goto_return(ME_NO_REDRAW);
	    if (gh->current_first <= gh->first_db_article) {
		msg("No extra articles");
		flush_input();
		goto_return(ME_NO_REDRAW);
	    }
	    if (!get_from_macro) {
		prompt("\1Number of extra articles\1 max %ld: ",
		       gh->current_first - gh->first_db_article);
		sprintf(buffer, "%c", ans1);
		answer = get_s(NONE, buffer, NONE, NULL_FCT);
		if (answer == NULL || *answer == NUL)
		    goto_return(ME_NO_REDRAW);
	    }
	    first = gh->current_first - (article_number) atol(answer);
	    goto more_articles;

	default:
	    break;
    }

    if ((gh = lookup_regexp(answer, "Goto", 3)) == NULL)
	goto_return(ME_NO_REDRAW);


get_first:

    m_advinput();

    if (gh->master_flag & M_BLOCKED ||
	gh->last_db_article == 0 ||
	gh->last_db_article < gh->first_db_article) {
	msg("Group %s is %s", gh->group_name,
	    (gh->master_flag & M_BLOCKED) ? "blocked" : "empty");
	goto_return(ME_NO_REDRAW);
    }
    if (gh != orig_group) {
	if (gh == read_mode_group) {
	    o_cur_first = gh->current_first;
	    gh->current_first = 0;
	}
	if (gh->current_first > 0) {
	    msg("Group %s already active", gh->group_name);
	    goto_return(ME_NO_REDRAW);
	}
	if (o_cur_first < 0)
	    o_cur_first = 0;
	gh->current_first = gh->last_db_article + 1;
    }
    ans1 = ah ? 's' : 'a';
    if (gh == read_mode_group) {
	ans1 = 's';
    } else if (gh != orig_group) {
	if (gh->unread_count > 0)
	    ans1 = 'j';
    } else {
	if (gh->unread_count > 0 && gh->current_first > entry_first_article)
	    ans1 = 'u';
	else if (gh->current_first <= gh->first_db_article)
	    ans1 = 's';		/* no more articles to read */
    }

    prompt("\1Number of%s articles\1 (%sua%ssne)  (%c) ",
	   gh == orig_group ? " extra" : "",
	   (gh->unread_count > 0) ? "j" : "",
	   (gh->master_flag & M_AUTO_ARCHIVE) ? "@" : "",
	   ans1);

    if (novice)
	msg("Use: j)ump u)nread a)ll @)archive s)ubject n)ame e)ither or number");

#ifndef ART_GREP
    answer = get_s(NONE, NONE, " jua@s=ne", NULL_FCT);
#else
    answer = get_s(NONE, NONE, " jua@s=nebB", NULL_FCT);	/* add 'bB' for BODY */
#endif				/* ART_GREP */

    if (answer == NULL)
	goto_return(ME_NO_REDRAW);
    if (*answer == NUL || *answer == SP)
	answer = &ans1;

    switch (*answer) {
	case '@':
	    if ((gh->master_flag & M_AUTO_ARCHIVE) == 0) {
		msg("No archive");
		goto_return(ME_NO_REDRAW);
	    }
	    answer = gh->archive_file;
	    goto get_folder;

	case 'u':
	    access_mode |= ACC_ORIG_NEWSRC;
	    first = gh->first_article + 1;
	    goto enter_new_level;

	case 'j':
	    if (gh == orig_group || gh->unread_count <= 0) {
		msg("Cannot jump - no unread articles");
		goto_return(ME_NO_REDRAW);
	    }
	    if (orig_group == NULL) {	/* nn -g */
		first = -1;
		goto enter_new_level;
	    }
	    jump_to_group = gh;
	    goto_return(ME_QUIT);
	    break;		/* for lint */

	case 'a':
	    first = 0;
	    access_mode |= ACC_ALSO_READ_ARTICLES | ACC_ALSO_CROSS_POSTINGS;
	    goto more_articles;

	case '=':
	case 's':
	case 'n':
	case 'e':
	    goto get_mask;

#ifdef ART_GREP
	case 'b':		/* Body search unread articles */
	case 'B':		/* Body search all articles */
	    access_mode |= ACC_ALSO_READ_ARTICLES | ACC_ALSO_CROSS_POSTINGS;
	    goto get_mask;
#endif				/* ART_GREP */

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
	    first = gh->current_first - atol(answer);
	    goto more_articles;

	default:
	    ding();
	    goto_return(ME_NO_REDRAW);
    }


more_articles:
    if (first > gh->last_db_article)
	goto_return(ME_NO_REDRAW);

    if (gh != orig_group)
	goto enter_new_level;

    if (!only_unread_articles)

#ifdef NOV
	if (gh->current_first <= gh->first_a_article)
#else
	if (gh->current_first <= gh->first_db_article)
#endif				/* NOV */

	{
	    msg("No extra articles");
	    goto_return(ME_NO_REDRAW);
	}

#ifdef NOV
    if (first < gh->first_a_article)
	first = gh->first_a_article;
#else
    if (first < gh->first_db_article)
	first = gh->first_db_article;
#endif				/* NOV */

    if (access_group(gh, first, only_unread_articles ?
		     gh->last_db_article : gh->current_first - 1,
		     ACC_PARSE_VARIABLES |
		     ACC_EXTRA_ARTICLES | ACC_ALSO_CROSS_POSTINGS |
		     ACC_ALSO_READ_ARTICLES | ACC_ONLY_READ_ARTICLES,
		     (char *) NULL) < 0) {
	msg("Cannot read extra articles (now)");
	goto_return(ME_NO_REDRAW);
    }
    only_unread_articles = 0;
    gh->current_first = first;
    goto_return(ME_REDRAW);

get_folder:
    m_endinput();
    if (strcmp(answer, "+") == 0)
	answer = (gh && gh->save_file != NULL) ? gh->save_file :
	    (gh && gh->group_flag & G_FOLDER) ? folder_save_file :
	    default_save_file;
    menu_cmd = folder_menu(answer, 0);
    gh = NULL;
    goto goto_exit;

get_mask:
    first = 0;
    access_mode |= ACC_ALSO_READ_ARTICLES | ACC_ALSO_CROSS_POSTINGS;
    switch (*answer) {
	case '=':
	    *answer = 's';
	    /* FALLTHROUGH */
	case 's':
	    access_mode |= ACC_ON_SUBJECT;
	    break;
	case 'n':
	    access_mode |= ACC_ON_SENDER;
	    break;
	case 'e':
	    access_mode |= ACC_ON_SUBJECT | ACC_ON_SENDER;
	    break;

#ifdef ART_GREP
	case 'b':		/* Body search unread articles */
	    access_mode |= ACC_ON_GREP_UNREAD;
	    break;
	case 'B':		/* Body search all articles in database */
	    access_mode |= ACC_ON_GREP_ALL;
	    break;
#endif				/* ART_GREP */
    }

#ifdef ART_GREP
    if ((*answer == 'b') || (*answer == 'B')) {
	prompt("Article body search pattern=");
	mask = get_s(NONE, NONE, NONE, NULL_FCT);
	if (mask == NULL)
	    goto_return(ME_NO_REDRAW);
    } else
#endif

    if (get_from_macro || answer[1] == '=') {
	mask = answer + 1;
	if (*mask == '=')
	    mask++;
    } else {
	prompt("%c=", *answer);
	mask = get_s(ah == NULL ? NONE :
		  (access_mode & ACC_ON_SUBJECT ? ah->subject : ah->sender),
		     NONE, ah ? NONE : "%=", NULL_FCT);
	if (mask == NULL)
	    goto_return(ME_NO_REDRAW);
	if (*mask == '%' || *mask == '=') {
	    if ((ah = get_menu_article()) == 0)
		goto_return(ME_NO_REDRAW);
	    *mask = NUL;
	}
    }

    if (*mask == NUL) {
	if (ah == NULL)
	    goto_return(ME_NO_REDRAW);
	strncpy(mask, (access_mode & ACC_ON_SUBJECT) ? ah->subject : ah->sender, GET_S_BUFFER);
	mask[GET_S_BUFFER - 1] = NUL;
	bypass_consolidation = 1;
    }
    if (*mask) {
	if (case_fold_search)
	    fold_string(mask);
    } else
	mask = NULL;

enter_new_level:
    mark_memory(&mem_marker);
    m_endinput();
    if (o_cur_first < 0)
	o_cur_first = gh->current_first;
    menu_cmd = group_menu(gh, first, access_mode, mask, menu);
    bypass_consolidation = 0;	/* in case no articles were found */
    release_memory(&mem_marker);

goto_exit:
    if (gh && o_cur_first >= 0)
	gh->current_first = o_cur_first;

    if (read_mode_group)
	orig_group = read_mode_group;

    if (gh != orig_group) {
	if (orig_group)
	    init_group(orig_group);
    }
    m_endinput();
    return menu_cmd;
}

static int
merged_header(void)
{
    so_printxy(0, 0, "MERGED NEWS GROUPS:  %d ARTICLES", n_articles);
    clrline();

    return 1;
}

void
merge_and_read(flag_type access_mode, char *mask)
{
    register group_header *gh;
    group_header    dummy_group;
    time_t          t1, t2;
    time_t          trefr = 0;
    long            kb = 0, kbleft = 0;

    time(&t1);
    t2 = 0;

    free_memory();

    access_mode |= ACC_DONT_SORT_ARTICLES | ACC_MERGED_MENU;
    if (!seq_cross_filtering)
	if (also_read_articles || mask || also_cross_postings)
	    access_mode |= ACC_ALSO_CROSS_POSTINGS;
    if (seq_cross_filtering && also_unsub_groups)
	access_mode |= ACC_ALSO_UNSUB_GROUPS;
    if (dont_split_digests)
	access_mode |= ACC_DONT_SPLIT_DIGESTS;

    Loop_Groups_Sequence(gh) {
	if (gh->group_flag & G_FOLDER)
	    continue;
	if (gh->master_flag & M_NO_DIRECTORY)
	    continue;
	if ((gh->group_flag & G_UNSUBSCRIBED) && !also_unsub_groups)
	    continue;
	if (!also_read_articles && gh->last_article >= gh->last_db_article)
	    continue;

	kbleft += gh->data_write_offset;
    }

    gotoxy(0, Lines - 1);
    Loop_Groups_Sequence(gh) {
	if (s_hangup || s_keyboard)
	    break;

	if (gh->group_flag & G_FOLDER) {
	    tprintf("\n\rIgnoring folder: %s\n\r", gh->group_name);
	    continue;
	}
	if (gh->master_flag & M_NO_DIRECTORY)
	    continue;
	if ((gh->group_flag & G_UNSUBSCRIBED) && !also_unsub_groups)
	    continue;

	if (also_read_articles)
	    access_mode |= ACC_ALSO_READ_ARTICLES;
	else if (gh->last_article >= gh->last_db_article)
	    continue;

	if (init_group(gh) <= 0)
	    continue;

#define SILLY 1			/* print dumb progress report */

#ifdef SILLY
	if (t2 >= trefr) {
	    trefr = t2 + merge_report_rate;

	    if (t2 > 2 && kb > 50000 && kb >= t2)
		tprintf("\r%4lds\t%lds\t%s", kbleft / (kb / t2),
			(long) t2, gh->group_name);
	    else
		tprintf("\r\t%lds\t%s", (long) t2, gh->group_name);

	    clrline();
	}
#else
	{
	    static int      grcnt = 0;
	    if (++grcnt % 10 == 0) {
		tputc('.');
		/* Hmm.. would a  fflush(stdout); be in order here? */
	    }
	}
#endif

	/* db_read_group(gh); *//* open the database file */

	access_group(gh, (article_number) (-1), gh->last_db_article,
		     access_mode, mask);

	time(&t2);
	t2 -= t1;
	kb += gh->data_write_offset;
	kbleft -= gh->data_write_offset;
    }
    merge_memory();
    if (n_articles == 0)
	return;
    if (dont_sort_articles)
	no_sort_articles();
    else
	sort_articles(-1);

    dummy_group.group_flag = G_FAKED;
    dummy_group.master_flag = 0;
    dummy_group.save_file = NULL;
    dummy_group.group_name = "dummy";
    dummy_group.kill_list = NULL;

    current_group = &dummy_group;

    kb = (kb + 1023) >> 10;
    sprintf(delayed_msg, "Read %ld articles in %ld seconds (%ld kbyte/s)",
	    (long) db_read_counter, (long) t2, t2 > 0 ? kb / t2 : kb);

    menu(merged_header);

    free_memory();
}

int
unsubscribe(group_header * gh)
{
    if (no_update) {
	msg("nn started in \"no update\" mode");
	return 0;
    }
    if (gh->group_flag & G_FOLDER) {
	msg("cannot unsubscribe to a folder");
	return 0;
    }
    if (gh->group_flag & G_UNSUBSCRIBED) {
	prompt("Already unsubscribed.  \1Resubscribe to\1 %s ? ",
	       gh->group_name);
	if (yes(0) <= 0)
	    return 0;

	add_to_newsrc(gh);
	add_unread(gh, 1);
    } else {
	prompt("\1Unsubscribe to\1 %s ? ", gh->group_name);
	if (yes(0) <= 0)
	    return 0;

	add_unread(gh, -1);
	update_rc_all(gh, 1);
    }

    return 1;
}

static int
disp_group(group_header * gh)
{
    if (pg_next() < 0)
	return -1;

    tprintf("%c%6ld%c%s%s%s",
	    (gh->group_flag & G_MERGED) == 0 ? ' ' :
	    (gh->group_flag & G_MERGE_HEAD) ? '&' : '+',

	    (long) (gh->unread_count),

	    (gh == current_group) ? '*' : ' ',

	    gh->group_name,

	    (gh->group_flag & G_NEW) ? " NEW" :
	    (gh->group_flag & G_UNSUBSCRIBED) ? " (!)" : "",

	    gh->enter_macro ? " %" : "");

    return 0;
}

/*
 * amount interpretation:
 *	-1	presentation sequence, unread,subscribed+current
 * 	0	unread,subscribed
 *	1	unread,all
 *	2	all,all 3=>unsub
 */

void
group_overview(int amount)
{
    register group_header *gh;

    clrdisp();

    pg_init(0, 2);

    if (amount < 0) {
	Loop_Groups_Sequence(gh) {
	    if (gh->group_flag & G_FAKED)
		continue;
	    if (gh->master_flag & M_NO_DIRECTORY)
		continue;
	    if (gh != current_group) {
		if (gh->unread_count <= 0)
		    continue;
		if (gh->group_flag & G_UNSUBSCRIBED && !also_unsub_groups)
		    continue;
	    }
	    if (disp_group(gh) < 0)
		break;
	}
    } else
	Loop_Groups_Sorted(gh) {
	if (gh->master_flag & (M_NO_DIRECTORY | M_IGNORE_GROUP))
	    continue;
	if (amount <= 1 && gh->unread_count <= 0)
	    continue;
	if (amount == 0 && (gh->group_flag & G_UNSUBSCRIBED))
	    continue;
	if (amount == 3 && (gh->group_flag & G_UNSUBSCRIBED) == 0)
	    continue;
	if (amount == 4 && (gh->group_flag & G_SEQUENCE) == 0)
	    continue;

	if (disp_group(gh) < 0)
	    break;
	}

    pg_end();
}
