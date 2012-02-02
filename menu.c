/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 * 	selection mode menu
 */

#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "articles.h"
#include "answer.h"
#include "db.h"
#include "execute.h"
#include "folder.h"
#include "group.h"
#include "init.h"
#include "keymap.h"
#include "kill.h"
#include "macro.h"
#include "match.h"
#include "menu.h"
#include "more.h"
#include "newsrc.h"
#include "regexp.h"
#include "save.h"
#include "sort.h"
#include "nn_term.h"

/* menu.c */

struct menu_info;

static article_number root_article(register article_number root);
static article_number next_root_article(register article_number root);
static void     set_root_if_closed(void);
static article_number thread_counters(article_number art);
static void     cursor_at_id(void);
static attr_type closed_attr(register struct menu_info * mi, char *cbuf);
static void     mark(void);
static void     toggle(void);
static int      do_auto_kill(void);
static int      do_auto_select(regexp * re, int mode);
static int      quit_preview(int cmd);
static void     count_selected_articles(void);
static int      show_articles(void);
static int      get_k_cmd_1(void);
static int      get_k_cmd(void);
static int      repl_attr(register article_number first, register article_number last, register attr_type old, register attr_type new, int update);
static int      repl_attr_subject(attr_type old, attr_type new, int update);
static int      repl_attr_all(attr_type old, attr_type new, int update);
static void     get_purpose(char *purpose);
static int      do_consolidation(void);

int             echo_prefix_key = 1;	/* echo prefix keys */
int             preview_window = 0;	/* size of preview window */
int             fmt_linenum = 1;/* menu line format */
int             fmt_rptsubj = 0;/* repeat identical subjects if !0 */
int             novice = 1;	/* novice mode -- use extended prompts */
int             long_menu = 0;	/* don't put empty lines around menu lines */
int             delay_redraw = 0;	/* prompt again if :-command clears
					 * screen */
int             slow_mode = 0;	/* mark selected articles with *s */
int             re_layout = 0;	/* Re: format presentation on menus */
int             collapse_subject = 25;	/* collapse long subjects at position */
int             conf_group_entry = 0;	/* ask whether group should be
					 * entered */
int             conf_entry_limit = 0;	/* ask only if more than .. unread */
int             mark_read_skip = 4;	/* effect of X command */
int             mark_read_return = 0;	/* effect of Z command */
int             mark_next_group = 0;	/* effect of N command */
int             show_purpose_mode = 1;	/* 0: never, 1: new, 2: always */
int             read_ret_next_page = 0;	/* Z returns to next page */

int             consolidated_menu = 0;	/* show only root articles */
int             save_closed_mode = 13;	/* ask how to save closed subj, dflt
					 * all */
int             auto_select_rw = 0;	/* select subject read or written */
int             auto_select_closed = 1;	/* select all in closed subject */
int             menu_spacing = 0;	/* number of screen lines per menu
					 * line */
char           *counter_delim_left = "[";
char           *counter_delim_right = "] ";
int             counter_padding = 5;	/* counters are padded to align
					 * subjects */

int             auto_preview_mode = 0;	/* preview rather than select */
int             preview_continuation = 12;	/* what to do after preview */
int             preview_mark_read = 1;	/* previewed articles are A_READ */
int             select_on_sender = 0;	/* find command selects on sender */
int             auto_select_subject = 0;	/* auto select articles with
						 * same subj. */
int             auto_read_limit = 0;	/* ignore auto_read_mode if less
					 * articles */

char            delayed_msg[100] = "";	/* give to msg() after redraw */

int             flush_typeahead = 0;

extern int      also_read_articles;
extern int      merged_menu;
extern int      case_fold_search;
extern int      ignore_fancy_select;
extern int      kill_file_loaded;
extern int      new_read_prompt;
extern int      any_message;
extern int      enable_stop;
extern int      alt_cmd_key, in_menu_mode;
extern int      mouse_y;	/* mouse event position */
extern int      mouse_state;
extern key_type erase_key;
extern int      get_from_macro;

static regexp  *regular_expr = NULL;

static int      firstl;		/* first menu line */

static article_number firsta;	/* first article on menu (0 based) */
static article_number nexta;	/* first article on next menu */
static article_number cura;		/* current article */
static article_number next_cura;	/* article to become cura if >= 0 */
static article_number numa;		/* no of articles on menu - 1 */
static attr_type last_attr;

#define INTERVAL1	('z' - 'a' + 1)
#define INTERVAL2	('9' - '0' + 1)

static char     ident[] = "abcdefghijklmnopqrstuvwxyz0123456789";

char            attributes[30] = " .,+=#! **";	/* Corresponds to A_XXXX in
						 * data.h */

static int      menu_length;	/* current no of line on menu */
static int      menu_articles;	/* current no of articles on menu */

static struct menu_info {	/* info for each menu line */
    int             mi_cura;	/* cura corresponding to this menu line */
    int             mi_total;	/* total number of articles with this subject */
    int             mi_unread;	/* no of unread articles with this subject */
    int             mi_selected;/* no of selected articles with this subject */
    int             mi_left;	/* no of articles marked for later viewing */
    int             mi_art_id;	/* article id (for mark()) */
}               menu_info[INTERVAL1 + INTERVAL2];

static struct menu_info *art_id_to_mi[INTERVAL1 + INTERVAL2];

#define IS_VISIBLE(ah)	(((ah)->flag & (A_CLOSED | A_ROOT_ART)) != A_CLOSED)

int
prt_replies(int level)
{
    int             re;

    if (level == 0)
	return 0;
    re = level & 0x80;
    level &= 0x7f;

    switch (re_layout) {
	case 1:
	    if (!re)
		return 0;
	    so_printf(">");
	    return 1;
	case 2:
	    switch (level) {
		case 0:
		    return 0;
		case 1:
		    so_printf(">");
		    return 1;
		default:
		    so_printf("%d>", level);
		    return level < 10 ? 2 : 3;
	    }
	case 3:
	    so_printf("Re: ");
	    return 4;
	case 4:
	    if (level == 0 && re)
		level++;
	    break;
    }

    if (level < 10) {
	so_printf("%-.*s", level, ">>>>>>>>>");
	return level;
    }
    so_printf(">>>%3d >>>>", level);
    return 11;
}

static          article_number
root_article(register article_number root)
{
    register article_header *ah = articles[root];

    if (ah->flag & A_ROOT_ART)
	return root;

    while (root > 0) {
	if (articles[root]->flag & A_ROOT_ART)
	    break;
	root--;
    }
    return root;
}

static          article_number
next_root_article(register article_number root)
{
    while (++root < n_articles)
	if (articles[root]->flag & A_ROOT_ART)
	    break;
    return root;
}

static void
set_root_if_closed(void)
{
    if (articles[firsta + cura]->flag & A_CLOSED)
	cura = root_article(firsta + cura) - firsta;
}

/*
 *	count info for thread containing article #art (must be on menu!)
 *	returns article number for next root article
 */

static          article_number
thread_counters(article_number art)
{
    register struct menu_info *mi;
    register article_number n;
    int             total, unread, selected, left;

    if (!(articles[art]->flag & A_CLOSED))
	return art + 1;

    total = unread = selected = left = 0;
    n = art = root_article(art);

    while (n < n_articles) {
	if (articles[n]->attr == 0)
	    unread++;
	else if (articles[n]->attr & A_SELECT)
	    selected++;
	else if (articles[n]->attr == A_LEAVE_NEXT)
	    left++;
	total++;
	if (++n == n_articles)
	    break;
	if (articles[n]->flag & A_ROOT_ART)
	    break;
    }
    unread += selected;

    mi = menu_info + articles[art]->menu_line;
    mi->mi_total = total;
    mi->mi_unread = unread;
    mi->mi_selected = selected;
    mi->mi_left = left;
    return n;
}

static void
cursor_at_id(void)
{
    gotoxy(0, firstl + articles[firsta + cura]->menu_line);
    fl;				/* place cursor at current article id */
    save_xy();
}

static          attr_type
closed_attr(register struct menu_info * mi, char *cbuf)
{
    char            lft[10], sel[10], unr[10];
    attr_type       cattr;

    if (mi->mi_total == mi->mi_left)
	cattr = A_LEAVE_NEXT;
    else if (mi->mi_unread == 0)
	cattr = A_READ;
    else if (mi->mi_total == mi->mi_selected)
	cattr = A_SELECT;
    else if (auto_select_closed == 1 && mi->mi_unread == mi->mi_selected)
	cattr = A_SELECT;
    else if (mi->mi_selected)
	cattr = A_KILL;		/* pseudo flag -> highlight cbuf */
    else
	cattr = 0;

    lft[0] = sel[0] = unr[0] = NUL;
    if (mi->mi_left && mi->mi_left < mi->mi_unread)
	sprintf(lft, "%d,", mi->mi_left);
    if (mi->mi_selected && mi->mi_selected < mi->mi_unread)
	sprintf(sel, "%d/", mi->mi_selected);
    if (mi->mi_unread && mi->mi_unread < mi->mi_total)
	sprintf(unr, "%d:", mi->mi_unread);
    sprintf(cbuf, "%s%s%s%d", lft, sel, unr, mi->mi_total);

    return cattr;
}

static int      subj_indent;

static void
mark(void)
{
    register article_header *ah;
    register struct menu_info *mi;
    int             lno, lnum, lsubj, lname;
    int             pad;
    char            cbuf[80];
    attr_type       cattr = 0;

    ah = articles[firsta + cura];
    if (!IS_VISIBLE(ah))
	return;

    last_attr = ah->attr;
    if (ah->disp_attr == A_NOT_DISPLAYED) {
	mi = &menu_info[ah->menu_line];
	lno = firstl + ah->menu_line;
	gotoxy(0, lno);
	tputc(ident[mi->mi_art_id]);
	cattr = closed_attr(mi, cbuf);
	goto print_line;
    }
    if (cura < 0 || cura > numa)
	return;

    lno = firstl + ah->menu_line;

    if (ah->flag & A_CLOSED) {
	struct menu_info old;
	char            oldctr[80];

	mi = &menu_info[ah->menu_line];
	old = *mi;
	thread_counters(firsta + cura);
	if (old.mi_total == mi->mi_total &&
	    old.mi_selected == mi->mi_selected &&
	    old.mi_left == mi->mi_left &&
	    old.mi_unread == mi->mi_unread)
	    return;

	cattr = closed_attr(mi, cbuf);

	if (!slow_mode)
	    goto print_line;
	closed_attr(&old, oldctr);
	if (strcmp(cbuf, oldctr))
	    goto print_line;
	last_attr = cattr;
    }
    if (last_attr == ah->disp_attr)
	return;

    /* A_AUTO_SELECT will not occur here! */

    if (!slow_mode) {
	if (last_attr == A_SELECT) {
	    if ((ah->disp_attr & A_SELECT) == 0) {
		goto print_line;
	    }
	} else {
	    if (ah->disp_attr & A_SELECT) {
		goto print_line;
	    }
	}
    }
    gotoxy(1, lno);
    tputc(attributes[ah->attr]);
    goto out;

print_line:

/* menu line formats:						*/
/*		1  3    8 10     20 22        xx		*/
/*		:  :    :  :      :  :         :		*/
/*	-1	id name:8 subject				*/
/*	0	id name           subject      +lines		*/
/*	1	id name       lines  subject			*/
/*	2	id  lines  subject				*/
/*	3	id subject					*/
/*	4	id   subject  (or as 1 if short subject)	*/

    if ((ah->flag & A_CLOSED) == 0) {
	cattr = ah->attr;
	cbuf[0] = NUL;
    }
    if (fmt_linenum > 4)
	fmt_linenum = 1;

    if (!slow_mode && (cattr & A_SELECT)) {
	if (so_gotoxy(1, lno, 1) == 0)
	    tputc(attributes[A_SELECT]);
    } else {
	gotoxy(1, lno);
	tputc(cattr == A_KILL ? ' ' : attributes[cattr]);
    }

    if (ah->lines < 10)
	lnum = 1;
    else if (ah->lines < 100)
	lnum = 2;
    else if (ah->lines < 1000)
	lnum = 3;
    else if (ah->lines < 10000)
	lnum = 4;
    else
	lnum = 5;

    lsubj = Columns - cookie_size - 2;	/* ident char + space */

    switch (fmt_linenum) {

	case -1:
	    lsubj -= 9;
	    so_printf("%-8.8s ", ah->sender);
	    goto no_counters;

	case 0:
	    lsubj -= Name_Length + 1 + 2 + lnum;	/* name. .subj. +.lines */
	    so_printf("%-*s ", Name_Length, ah->sender);
	    break;

	case 4:
	    if ((int) ah->subj_length > (lsubj - Name_Length - 5))
		if (fmt_rptsubj || lno == firstl || (ah->flag & A_SAME) == 0) {
		    so_printf("  ");
		    lsubj -= 2;
		    break;
		}
	    /* else use layout 1, so fall thru */

	    /* FALLTHROUGH */
	case 1:
	    lsubj -= Name_Length + 5;
	    /* name.lines.  .subj (name may be shortened) */
	    lname = Name_Length + 2 - lnum;
	    so_printf("%-*.*s ", lname, lname, ah->sender);
	    so_printf(ah->lines >= 0 ? "%d  " : "?  ", ah->lines);
	    break;

	case 2:
	    lsubj -= 6;
	    so_printf(ah->lines >= 0 ? "%5d " : "    ? ", ah->lines);
	    break;

	case 3:
	    break;
    }

    if (cbuf[0]) {
	so_printf("%s", counter_delim_left);
	if (cattr == A_KILL)
	    so_gotoxy(-1, -1, 0);
	so_printf("%s", cbuf);
	if (cattr == A_KILL)
	    so_end();
	so_printf("%s", counter_delim_right);
	pad = strlen(cbuf) + strlen(counter_delim_left) + strlen(counter_delim_right);
	if (cattr == A_KILL)
	    pad += cookie_size * 2;
	lsubj -= pad > subj_indent ? pad : subj_indent;
	pad = subj_indent - pad;
    } else {
	lsubj -= subj_indent;
	pad = subj_indent;
    }
    if (pad > 0) {
	if (pad > 20)
	    pad = 20;
	so_printf("                    " + 20 - pad);
    }
no_counters:
    if (!fmt_rptsubj && lno > firstl && ah->flag & A_SAME) {
	if (ah->replies == 0 || prt_replies(ah->replies) == 0)
	    so_printf("-");
    } else {
	lsubj -= prt_replies(ah->replies);
	if (lsubj >= (int) ah->subj_length)
	    so_printf("%s", ah->subject);
	else if (collapse_subject < 0)
	    so_printf("%-.*s", lsubj, ah->subject);
	else {
	    if (collapse_subject > 0)
		so_printf("%-.*s", collapse_subject, ah->subject);
	    so_printf("<>%s", ah->subject + ah->subj_length - lsubj + collapse_subject + 2);
	}
    }

    if (fmt_linenum == 0)
	so_printf(ah->lines >= 0 ? " +%d" : " +?", ah->lines);

    so_end();
    if ((ah->flag & A_CLOSED) && lsubj > (int) ah->subj_length)
	clrline_noflush();

out:
    ah->disp_attr = last_attr;
    return;
}

static void
toggle(void)
{
    last_attr = articles[firsta + cura]->attr =
    articles[firsta + cura]->attr & A_SELECT ? 0 : A_SELECT;
}

static int
do_auto_kill(void)
{
    register article_number i;
    register article_header *ah, **ahp;
    int             any = 0;

    for (i = 0, ahp = articles; i < n_articles; i++, ahp++) {
	ah = *ahp;
	if (auto_select_article(ah, 0)) {
	    ah->attr = A_KILL;
	    any = 1;
	}
    }
    return any;
}

/*
 *	perform auto selections that are not already selected
 *	if article is in range firsta..firsta+numa (incl) mark article
 */

static int
do_auto_select(regexp * re, int mode)
{
    register article_number i, o_cura;
    register article_header *ah, **ahp;
    int             count = 0, should_mark;

    if (mode == 1 && re == NULL)
	if (!kill_file_loaded && !init_kill())
	    return 0;

    o_cura = cura;
    should_mark = 0;

    /*
     * note: this code assumes that a visible article will be found before
     * anything is marked
     */
    for (i = 0, ahp = articles; i < n_articles; i++, ahp++) {
	ah = *ahp;
	if (IS_VISIBLE(ah)) {
	    if (should_mark) {
		mark();
		should_mark = 0;
	    }
	    cura = i - firsta;
	}
	if (re != NULL) {
	    if (!regexec_cf(re, select_on_sender ? ah->sender : ah->subject))
		continue;
	} else if (!auto_select_article(ah, mode))
	    continue;

	count++;
	if (ah->attr & A_SELECT)
	    continue;
	ah->attr = A_SELECT;
	if (firsta <= i && i <= (firsta + numa))
	    should_mark = 1;
    }

    if (should_mark)
	mark();

    if (count)
	msg("Selected %d article%s", count, plural((long) count));
    else
	msg("No selections");

    cura = o_cura;
    return 0;
}

static int
quit_preview(int cmd)
{
    int             op;

    if ((firsta + cura) >= n_articles)
	return 1;
    op = preview_continuation;
    if (cmd == MC_PREVIEW_NEXT)
	op /= 10;
    op %= 10;
    switch (op) {
	case 0:
	    return 1;
	case 1:
	    return 0;
	case 2:
	    return articles[firsta + cura]->flag & A_ROOT_ART;
    }
    return 0;
}

long            n_selected;
int             show_art_next_invalid;

static void
count_selected_articles(void)
{
    register article_number cur;

    n_selected = 0;
    for (cur = 0; cur < n_articles; cur++) {
	if (articles[cur]->attr & A_SELECT)
	    n_selected++;
    }
}

static int
show_articles(void)
{
    register article_number cur, next, prev = -1;
    register article_header *ah;
    article_number  elim_list[1];
    register int    mode;
    int             cmd, again;
    attr_type       o_attr;

    do {
	for (cur = 0; cur < n_articles; cur++) {
	    if (articles[cur]->attr & A_SELECT)
		break;
	}

	while (cur < n_articles) {

	    for (next = cur + 1; next < n_articles; next++) {
		if (articles[next]->attr & A_SELECT)
		    break;
	    }
	    show_art_next_invalid = 0;

    show:
	    ah = articles[cur];
	    o_attr = ah->attr;
	    ah->attr = 0;

	    if (auto_select_rw && !ignore_fancy_select &&
		!auto_select_article(ah, 1))
		enter_kill_file(current_group, ah->subject, 6, 30);

	    if (new_read_prompt)
		count_selected_articles();

	    mode = 0;
	    if (prev >= 0)
		mode |= MM_PREVIOUS;
	    if (next == n_articles)
		mode |= MM_LAST_SELECTED;
	    if ((cur + 1) >= n_articles)
		mode |= MM_LAST_ARTICLE;
	    if (cur == 0)
		mode |= MM_FIRST_ARTICLE;

	    cmd = more(ah, mode, 0);

	    switch (cmd) {

		case MC_DO_KILL:
		    if (do_auto_kill()) {
			elim_list[0] = cur;
			elim_articles(elim_list, 1);
			cur = elim_list[0];
			if (cur >= n_articles || cur < 0)
			    cur = n_articles;
			else {
			    for (prev = cur; prev >= 0; --prev)
				if ((articles[prev]->attr & A_READ) != 0)
				    break;
			    for (; cur < n_articles; ++cur)
				if ((articles[cur]->attr & A_SELECT) != 0)
				    break;
			}
			continue;
		    }
		    break;

		case MC_DO_SELECT:
		    break;

		case MC_PREV:
		    ah->attr = o_attr;
		    if (prev == next)
			break;

		    next = cur;
		    cur = prev;
		    prev = next;
		    goto show;

		case MC_NEXTSUBJ:
		    ah->attr = A_READ;
		    for (next = cur + 1; next < n_articles; next++) {
			if ((ah = articles[next])->flag & A_ROOT_ART)
			    break;
			ah->attr = A_READ;
		    }
		    for (; next < n_articles; next++) {
			if (articles[next]->attr & A_SELECT)
			    break;
		    }
		    break;

		case MC_ALLSUBJ:
		    ah->attr = A_READ;
		    for (next = cur + 1; next < n_articles; next++) {
			ah = articles[next];
			if (ah->flag & A_ROOT_ART)
			    break;
			ah->attr = A_SELECT;
		    }
		    for (next = cur + 1; next < n_articles; next++)
			if (articles[next]->attr & A_SELECT)
			    break;
		    break;

		case MC_MENU:
		    ah->attr = o_attr;
		    if (nexta - firsta < n_articles) {
			/* Keep a little article context by making the */
			/* current article go on menu line 6 if possible */
			if (IS_VISIBLE(articles[cur]))
			    firsta = cur;
			else
			    firsta = root_article(cur);
			for (next = 0; firsta > 0 && next < 5; next++) {
			    firsta--;
			    if (!IS_VISIBLE(articles[firsta]))
				firsta = root_article(firsta);
			}
		    }
		    next_cura = cur - firsta;

		    return MC_MENU;

		case MC_NEXT:
		    if (ah->attr == 0)	/* Not set by more (sufficient ?!?!) */
			ah->attr = A_READ;
		    break;

		case MC_BACK_ART:
		    ah->attr = o_attr ? o_attr : A_SEEN;
		    next = cur - 1;
		    break;

		case MC_FORW_ART:
		    ah->attr = o_attr ? o_attr : A_SEEN;
		    next = cur + 1;
		    break;

		case MC_NEXTGROUP:
		case MC_REENTER_GROUP:
		case MC_QUIT:
		    ah->attr = o_attr;
		    return cmd;

		case MC_READGROUP:
		    for (cur = 0; cur < n_articles; cur++) {
			ah = articles[cur];
			if (ah->attr == 0 || (ah->attr & A_SELECT))
			    ah->attr = A_READ;
		    }
		    return MC_NEXTGROUP;
	    }

	    if (show_art_next_invalid)
		for (next = cur + 1; next < n_articles; next++) {
		    if (articles[next]->attr & A_SELECT)
			break;
		}
	    prev = cur;
	    cur = next;
	}

	for (cur = 0; cur < n_articles; cur++)
	    if (articles[cur]->attr & A_SELECT)
		break;

	again = 0;
	if (cur < n_articles)
	    continue;
	for (cur = 0; cur < n_articles; cur++) {
	    ah = articles[cur];
	    if (ah->attr == A_LEAVE) {
		if (again == 0) {
		    prompt("Show left over articles again now? ");
		    if (yes(0) <= 0)
			break;
		}
		ah->attr = A_SELECT;
		again++;
	    }
	}

	if (again > 1)
	    sprintf(delayed_msg, "Showing %d articles again", again);
    } while (again);

    return MC_READGROUP;
}

static article_number      article_id;
static key_type cur_key;
static int      is_k_select;	/* set when K_ARTICLE_ID was really K_SELECT */

#define MOUSE   (mouse_y >= 0)

static int
mouse_map(int map)
{
    /* remap mouse functions back into normal functions */
    if (map == K_M_SELECT) {
	map = K_ARTICLE_ID;
    } else if (map == K_M_PREVIEW) {
	map = K_PREVIEW;
    } else if (map == K_M_SELECT_SUBJECT) {
	map = K_SELECT_SUBJECT;
    } else if (map == K_M_SELECT_RANGE) {
	map = K_SELECT_RANGE;
    } else
	return map;

    /* deal with mouse article selection */
    if (MOUSE) {
	if ((mouse_y >= firstl) && (mouse_y <= firstl + menu_length - 1)) {
	    article_id = mouse_y - firstl;
	    article_id = art_id_to_mi[article_id]->mi_cura;
	}
    }
    return map;
}

static int
get_k_cmd_1(void)
{
    register int    c, map;
    int            *key_map = menu_key_map;

    if (flush_typeahead)
	flush_input();

loop:

    article_id = -1;

    if ((c = get_c()) & GETC_COMMAND) {
	cur_key = K_interrupt;
	map = c & ~GETC_COMMAND;
    } else {
	cur_key = c;
	map = key_map[c];
    }
    if (s_hangup)
	map = K_QUIT;

    if (map & K_PREFIX_KEY) {
	key_map = keymaps[map & ~K_PREFIX_KEY].km_map;
	if (echo_prefix_key)
	    msg("%s", key_name(cur_key));
	goto loop;
    }
    is_k_select = 0;
    if (map == K_SELECT) {
	map = K_ARTICLE_ID;
	article_id = cura;
	is_k_select = 1;
    } else if (map & K_ARTICLE_ID) {
	article_id = map & ~K_ARTICLE_ID;
	map = K_ARTICLE_ID;

	if (article_id < 0 || article_id >= menu_articles) {
	    ding();
	    goto loop;
	}
	article_id = art_id_to_mi[article_id]->mi_cura;
    } else
	map = mouse_map(map);

    if (any_message && map != K_LAST_MESSAGE)
	clrmsg(-1);
    return map;
}

static int
get_k_cmd(void)
{
    register int    map;

    map = get_k_cmd_1();
    if (map & K_MACRO)
	map = orig_menu_map[cur_key];
    return map;
}


char           *
pct(long start, long end, long first, long last)
{
    long            n = end - start;
    static char     buf[16];
    char           *fmt;

    if (first <= start || n <= 0) {
	if (last >= end || n <= 0)
	    return "All";
	else
	    fmt = "Top %d%%";
    } else if (last >= end) {
	return "Bot";
    } else {
	fmt = "%d%%";
    }

    sprintf(buf, fmt, ((last - start) * 100) / n);
    return buf;
}

static int
repl_attr(register article_number first, register article_number last, register attr_type old, register attr_type new, int update)
{
    int             any;
    register article_header *ah;
    article_number  ocura = cura;

    if (new == old)
	return 0;
    if (new == A_KILL)
	update = 0;

    any = 0;
    cura = -1;
    while (first < last) {
	ah = articles[first];
	if (cura >= 0 && ah->flag & A_ROOT_ART) {
	    set_root_if_closed();
	    mark();
	    cura = -1;
	}
	if (old == A_KILL || ah->attr == old) {
	    ah->attr = new;
	    if (update && first >= firsta && first < nexta) {
		cura = first - firsta;
		if ((ah->flag & A_CLOSED) == 0) {
		    mark();
		    cura = -1;
		}
	    }
	    any = 1;
	}
	first++;
    }

    if (cura >= 0) {
	set_root_if_closed();
	mark();
    }
    cura = update ? last - firsta : ocura;
    return any;
}

static int
repl_attr_subject(attr_type old, attr_type new, int update)
{
    int             f, l;

    f = root_article(firsta + cura);
    l = next_root_article(firsta + cura);
    if (old == A_SELECT)
	(void) repl_attr(f, l, A_AUTO_SELECT, A_SELECT, update);
    return repl_attr(f, l, old, new, update);
}

static int
repl_attr_all(attr_type old, attr_type new, int update)
{
    return repl_attr((article_number) 0, n_articles, old, new, update);
}

static void
get_purpose(char *purpose)
{

#ifdef	CACHE_PURPOSE
    register char  *cp;

    cp = purp_lookup(current_group->group_name);
    strncpy(purpose, cp, 76);
    return;

#else
    FILE           *f;
    char            line[256], *group;
    register char  *cp, *pp;
    register int    len;

    if (current_group == NULL)
	return;
    if ((current_group->master_flag & M_VALID) == 0)
	return;
    if (current_group->group_flag & G_FAKED)
	return;

    if ((f = open_purpose_file()) == NULL)
	return;

    group = current_group->group_name;
    len = current_group->group_name_length;

    while (fgets(line, 256, f) != NULL) {
	if (!isascii(line[len]) || !isspace(line[len]))
	    continue;
	if (strncmp(line, group, len))
	    continue;
	cp = line + len;
	while (*cp && isspace(*cp))
	    cp++;
	for (pp = purpose, len = 76; --len >= 0 && *cp && *cp != NL;)
	    *pp++ = *cp++;
	*pp = NUL;
    }
#endif				/* CACHE_PURPOSE */
}

/*
 * bypass_consolidation may be set to temporarily overrule the global
 * consolidated_menu variable:
 *	1:	don't consolidate	(e.g. for G=...  )
 *	2:	do consolidate
 *	3:	use consolidated_mode
 */

int             bypass_consolidation = 0;
static int      cur_bypass = 0;	/* current bypass status (see below) */

static int
do_consolidation(void)
{
    int             consolidate;

    switch (bypass_consolidation) {
	case 0:
	    break;
	case 1:
	    cur_bypass = -1;	/* no consolidation */
	    break;
	case 2:
	    cur_bypass = 1;	/* force consolidation */
	    break;
	case 3:
	    cur_bypass = 0;	/* reset bypass to use consolidated_menu */
	    break;
    }
    bypass_consolidation = 0;

    if (cur_bypass)
	consolidate = cur_bypass > 0;
    else
	consolidate = consolidated_menu;

    if (consolidate)
	for (nexta = 0; nexta < n_articles; nexta++)
	    articles[nexta]->flag |= A_CLOSED;
    else
	for (nexta = 0; nexta < n_articles; nexta++)
	    articles[nexta]->flag &= ~A_CLOSED;

    return consolidate;
}

int
menu(fct_type print_header)
{
    register int    k_cmd, cur_k_cmd;
    register article_header *ah;
    register struct menu_info *mi;
    int             consolidate, o_bypass;
    int             last_k_cmd;
    int             menu_cmd = 0, temp;
    int             save_selected;
    article_number  last_save;
    attr_type       orig_attr, junk_attr;
    int             doing_unshar, did_unshar, junk_prompt;
    char           *fname, *savemode;
    int             maxa;	/* max no of articles per menu page */
    article_number  o_firsta, temp1 = 0, temp2;
    int             o_mode;	/* for recursive calls */
    static int      menu_level = 0;
    char            purpose[80], pr_fmt[60];
    article_number  elim_list[3];
    int             entry_check;
    int             auto_read;
    long            o_selected;

#define	menu_return(cmd) \
    { menu_cmd = (cmd); goto menu_exit; }

    flush_input();

    o_firsta = firsta;
    o_mode = in_menu_mode;
    o_selected = n_selected;

    in_menu_mode = 1;

    menu_level++;

    if (menu_level == 1) {
	if ((current_group->group_flag & G_COUNTED)
	    && n_articles != current_group->unread_count) {
	    add_unread(current_group, -1);
	    current_group->unread_count = n_articles;
	    add_unread(current_group, 0);
	}
	entry_check = conf_group_entry && n_articles > conf_entry_limit;
	auto_read = auto_read_limit < 0 || n_articles <= auto_read_limit;
    } else {
	entry_check = 0;
	auto_read = 0;
    }

    sprintf(pr_fmt,
	    menu_level == 1 ?
	    "\1\2-- SELECT %s-----%%s-----\1" :
	    "\1\2-- SELECT %s-----%%s-----<%s%d>--\1",
	    novice ? "-- help:? " : "",
	    novice ? "level " : "",
	    menu_level);

    purpose[0] = NUL;
    if (!merged_menu)
	switch (show_purpose_mode) {
	    case 0:
		break;
	    case 1:
		if ((current_group->group_flag & G_NEW) == 0)
		    break;
		/* FALLTHROUGH */
	    case 2:
		get_purpose(purpose);
		if (purpose[0])
		    strcpy(delayed_msg, purpose);
	}

    o_bypass = cur_bypass;
    cur_bypass = 0;

    consolidate = do_consolidation();

    firsta = 0;
    while (firsta < n_articles && articles[firsta]->attr == A_SEEN)
	firsta++;

    if (firsta == n_articles)
	firsta = 0;

    next_cura = -1;
    cur_k_cmd = K_UNBOUND;

#ifdef HAVE_JOBCONTROL
#define	REDRAW_CHECK	if (s_redraw) goto do_redraw

do_redraw:
    /* safe to clear here, because we are going to redraw anyway */
    s_redraw = 0;
#else
#define REDRAW_CHECK
#endif

redraw:
    s_keyboard = 0;

empty_menu_hack:		/* do: "s_keyboard=1; goto empty_menu_hack;" */
    if (!slow_mode)
	s_keyboard = 0;

    nexta = firsta;

    clrdisp();

    if (entry_check) {
	prompt_line = firstl = CALL(print_header) ();
	prompt("\1Enter?\1 ");
	if ((temp = yes(0)) <= 0) {
	    if (temp < 0) {
		prompt("\1Mark as read?\1 ");
		if ((temp = yes(0)) < 0)
		    menu_return(ME_QUIT);
		if (temp > 0)
		    repl_attr_all(A_KILL, A_READ, 0);
	    }
	    menu_return(ME_NEXT);
	}
	gotoxy(0, firstl);
	clrline();
    }
    if (auto_read) {
	auto_read = entry_check = 0;
	if (repl_attr_all(0, A_AUTO_SELECT, 0)) {
	    k_cmd = K_READ_GROUP_UPDATE;
	    if (purpose[0])
		strcpy(delayed_msg, purpose);
	    else
		sprintf(delayed_msg, "Entering %s, %ld articles",
			current_group->group_name, (long) n_articles);
	    goto do_auto_read;
	}
    }
    if (!entry_check)
	firstl = CALL(print_header) ();
    entry_check = 0;


    maxa = Lines - preview_window - firstl - 2;
    if (!long_menu)
	firstl++, maxa -= 2;

    if (maxa > (INTERVAL1 + INTERVAL2))
	maxa = INTERVAL1 + INTERVAL2;

nextmenu:

    no_raw();
    gotoxy(0, firstl);
    clrpage();

    if (articles[nexta]->flag & A_CLOSED)
	nexta = root_article(nexta);
    firsta = nexta;
    cura = 0;

    REDRAW_CHECK;

    menu_length = 0;
    menu_articles = 0;
    goto draw_menu;

partial_redraw:
    next_cura = cura;
partial_redraw_nc:
    nexta = firsta + cura;
    menu_length = articles[nexta]->menu_line;
    menu_articles = menu_info[menu_length].mi_art_id;
    gotoxy(0, firstl + menu_length);
    clrpage();

draw_menu:
    subj_indent = consolidate ? counter_padding : 0;

    if (!s_keyboard) {
	int             first_menu_line = menu_length;

	mi = menu_info + menu_length;
	while (nexta < n_articles) {
	    REDRAW_CHECK;

	    ah = articles[nexta];
	    if (ah->flag & A_HIDE) {
		ah->menu_line = menu_length;	/* just in case.... */
		continue;
	    }
	    if (menu_length > first_menu_line) {
		switch (menu_spacing) {
		    case 0:
			break;
		    case 1:
			if ((ah->flag & A_ROOT_ART) == 0)
			    break;
			/* XXX: bug? Another fall? */
		    case 2:
			menu_length++;
			mi++;
			break;
		}
		if (menu_length >= maxa)
		    break;
	    }
	    ah->menu_line = menu_length;
	    art_id_to_mi[menu_articles] = mi;
	    mi->mi_art_id = menu_articles++;

	    mi->mi_cura = cura = nexta - firsta;
	    if (ah->flag & A_CLOSED) {	/* skip rest of thread */
		nexta = thread_counters(nexta);
		if (nexta == firsta + cura + 1)
		    ah->flag &= ~A_CLOSED;
		else {
		    article_number  tmpa = firsta + cura;
		    while (++tmpa < nexta)
			articles[tmpa]->menu_line = menu_length;
		}
	    } else
		nexta++;
	    ah->disp_attr = A_NOT_DISPLAYED;
	    mark();
	    if (++menu_length >= maxa)
		break;
	    mi++;
	}
    }
    if (menu_length > maxa)
	menu_length = maxa;

    fl;
    s_keyboard = 0;

    prompt_line = firstl + menu_length;
    if (!long_menu || menu_length < maxa)
	prompt_line++;

    numa = nexta - firsta - 1;
    if (numa < 0)
	prompt_line++;

/* Changed by Stefan Schwarz (stefans@bauv.unibw-muenchen.de Nov. 17, 1992 */
    if (firsta + next_cura >= nexta)
	next_cura = -1;
/* End of changes*/

    if (next_cura >= 0) {
	cura = next_cura;
	set_root_if_closed();
	next_cura = -1;
    } else {
	cura = 0;
	for (article_id = firsta; cura < numa; article_id++, cura++) {
	    if ((articles[article_id]->attr & A_SELECT) == 0)
		break;		/* ??? */
	    if (!IS_VISIBLE(articles[article_id]))
		continue;
	}
	if (!IS_VISIBLE(articles[article_id]))
	    cura = root_article(article_id) - firsta;
    }

build_prompt:

    nn_raw();

Prompt:

    prompt(pr_fmt,
    pct(0L, (long) (n_articles - 1), (long) firsta, (long) (firsta + numa)));

    if (delayed_msg[0] != NUL) {
	msg(delayed_msg);
	delayed_msg[0] = NUL;
    }
same_prompt:

    if (cura < 0 || cura > numa)
	cura = 0;

    if (!IS_VISIBLE(articles[firsta + cura])) {
	cura = next_root_article(firsta + cura) - firsta;
	if (cura > numa)
	    cura = 0;
    }
    if (numa >= 0) {
	cursor_at_id();
    }
    last_k_cmd = cur_k_cmd;

get_next_k_cmd:
    k_cmd = get_k_cmd_1();

alt_key:
    if (k_cmd & K_MACRO) {
	m_invoke(k_cmd & ~K_MACRO);
	goto get_next_k_cmd;
    }
#define STATE(state)  {k_cmd = state; goto new_state;}

/* mouse clicks on top or bottom prompt bars  */
#define MOUSE_MENU					\
	 if (MOUSE) {					\
	     if (mouse_y > firstl + menu_length ) {     \
		 STATE(K_CONTINUE);			\
	     } else if (mouse_y == firstl + menu_length ) { \
		 STATE(K_UNBOUND);			\
	     } else if (mouse_y < firstl - 1) {		\
		 if (firsta > 0) {			\
		     STATE(K_PREV_PAGE);		\
		} else {				\
		     STATE(K_PREVIOUS);			\
		}					\
	     } else if (mouse_y == firstl - 1) {		\
		 STATE(K_UNBOUND);			\
	     }						\
	}

new_state:
    switch (cur_k_cmd = k_cmd) {

	case K_UNBOUND:
	    ding();
	    flush_input();

	    /* FALLTHROUGH */
	case K_INVALID:
	    goto same_prompt;

	case K_REDRAW:
	    next_cura = cura;
	    goto redraw;

	case K_LAST_MESSAGE:
	    msg((char *) NULL);
	    goto same_prompt;

	case K_HELP:
	    if (numa < 0)
		goto nextmenu;	/* give specific help here */
	    display_help("menu");
	    goto redraw;

	case K_SHELL:
	    if (group_file_name)
		*group_file_name = NUL;
	    if (shell_escape())
		goto redraw;
	    goto Prompt;

	case K_VERSION:
	    prompt(P_VERSION);
	    goto same_prompt;

	case K_EXTENDED_CMD:
	    temp = consolidated_menu;
	    switch (alt_command()) {

		case AC_UNCHANGED:
		    goto same_prompt;

		case AC_QUIT:
		    menu_return(ME_QUIT);
		    break;	/* for lint. we can't actually get here */

		case AC_PROMPT:
		    goto Prompt;

		case AC_REORDER:
		    firsta = 0;
		    consolidate = do_consolidation();
		    goto redraw;

		case AC_REDRAW:
		    if (temp != consolidated_menu)
			consolidate = do_consolidation();
		    goto redraw;

		case AC_KEYCMD:
		    k_cmd = alt_cmd_key;
		    goto alt_key;

		case AC_REENTER_GROUP:
		    menu_return(ME_REENTER_GROUP);
	    }
	    /* XXX: bug? fall */

	case K_QUIT:
	    menu_return(ME_QUIT);
	    break;		/* for lint. we can't actually get here */

	case K_M_TOGGLE:
	    if (mouse_state) {
		xterm_mouse_off();
		mouse_state = 0;
	    } else {
		xterm_mouse_on();
		mouse_state = 1;
	    }
	    goto Prompt;

	case K_CANCEL:
	    savemode = "Cancel";
	    fname = "";
	    goto cancel1;

	case K_SAVE_NO_HEADER:
	case K_SAVE_SHORT_HEADER:
	case K_SAVE_FULL_HEADER:
	case K_SAVE_HEADER_ONLY:
	case K_PRINT:
	case K_UNSHAR:
	case K_PATCH:
	case K_UUDECODE:

	    if (numa < 0)
		goto nextmenu;

	    fname = init_save(k_cmd, &savemode);
	    if (fname == NULL)
		goto Prompt;

    cancel1:
	    enable_stop = 0;
	    save_selected = 0;
	    doing_unshar = k_cmd == K_UNSHAR || k_cmd == K_PATCH;
	    did_unshar = 0;

	    m_startinput();

	    if (novice)
		msg(" * selected articles on this page, + all selected articles");

	    while (!save_selected && !did_unshar) {
		prompt("\1%s\1 %.*s Article (* +): ",
		       savemode, Columns - 25, fname);

		k_cmd = get_k_cmd();

		if (k_cmd == K_SELECT_SUBJECT) {
		    save_selected = 1;
		    cura = 0;
		    article_id = firsta;
		    last_save = firsta + numa;
		} else if (k_cmd == K_AUTO_SELECT) {
		    save_selected = 1;
		    cura = -firsta;
		    article_id = 0;
		    last_save = n_articles - 1;
		} else if (k_cmd == K_ARTICLE_ID) {
		    cura = article_id;
		    article_id += firsta;
		    last_save = article_id;
		    if (articles[article_id]->flag & A_CLOSED) {
			int             n = save_closed_mode % 10;

			if (article_id == n_articles - 1)
			    goto save_it;
			if (articles[article_id + 1]->flag & A_ROOT_ART)
			    goto save_it;

			if (save_closed_mode >= 10) {
			    prompt("\1%s thread\1 (r)oot (s)elected (u)nread (b)oth (a)ll  (%c)",
				   savemode, "rsuba"[n]);
			    switch (get_c()) {
				case 'r':
				    n = 0;
				    break;
				case 's':
				    n = 1;
				    break;
				case 'u':
				    n = 2;
				    break;
				case 'b':
				    n = 3;
				    break;
				case 'a':
				    n = 4;
				    break;
				case SP:
				case CR:
				case NL:
				    break;
				default:
				    ding();
				    goto Prompt;
			    }
			}
			switch (n) {
			    case 0:	/* save root only */
				break;
			    case 1:	/* save all selected */
				save_selected = 1;
				break;
			    case 2:	/* save all unread */
				save_selected = 2;
				break;
			    case 3:	/* save all selected + unread */
				save_selected = 3;
				break;
			    case 4:	/* save all articles */
				break;
			}
			if (n)
			    last_save = next_root_article(article_id) - 1;
			save_selected |= 8;	/* closed subject */
			temp1 = cura;
		    }
		} else
		    break;

	save_it:
		for (; article_id <= last_save; article_id++, cura++) {
		    ah = articles[article_id];
		    switch (save_selected & 0x3) {
			case 0:
			    break;
			case 3:
			    if (ah->attr == 0)
				break;
			    /* XXX: check fall */
			case 1:
			    if ((ah->attr & A_SELECT) == 0)
				continue;
			    break;
			case 2:
			    if (ah->attr == 0)
				break;
			    continue;
		    }

		    if (cur_k_cmd == K_CANCEL) {
			if (current_group->group_flag & G_FOLDER) {
			    fcancel(ah);
			} else
			    switch (cancel(ah)) {
				case -1:
				    did_unshar = 1;
				    continue;
				case 0:
				    ah->attr = A_CANCEL;
				    break;
				default:
				    continue;
			    }

			if (!did_unshar)
			    mark();

			continue;
		    }
		    if (doing_unshar) {
			did_unshar++;
		    } else if (ah->subject != NULL)
			prompt("Processing '%.50s'...", ah->subject);
		    else if (cura >= 0 && cura <= numa)
			prompt("Processing %c...", ident[cura]);
		    else
			prompt("Processing entry %d...", article_id);

		    if (save(ah)) {
			ah->attr = A_READ;
			if (doing_unshar)
			    continue;

			if (cura >= 0 && cura <= numa)
			    mark();
		    }
		}
		if (save_selected & 8) {
		    save_selected = 0;	/* select closed */
		    cura = temp1;
		    /* mark(); */
		}
	    }

	    if (save_selected)
		cura = 0;

	    m_endinput();

	    enable_stop = 1;
	    if (cur_k_cmd != K_CANCEL)
		end_save();

	    if (did_unshar) {
		tprintf("\r\n");
		any_key(0);
		goto redraw;
	    }
	    goto Prompt;

	case K_FOLLOW_UP:
	case K_REPLY:
	    if (numa < 0)
		goto nextmenu;

	    prompt(k_cmd == K_REPLY ?
		   "\1Reply to author\1 of article: " :
		   "\1Follow Up\1 to article: ");

	    if (get_k_cmd() == K_ARTICLE_ID)
		if (answer(articles[firsta + article_id], k_cmd, -1))
		    goto redraw;

	    goto Prompt;

	case K_POST:
	    if (post_menu())
		goto redraw;
	    goto Prompt;

	case K_MAIL_OR_FORWARD:
	    if (numa < 0)
		goto nextmenu;

	    prompt("\1Article to be forwarded\1 (SP if none): ");

	    if ((k_cmd = get_k_cmd()) == K_ARTICLE_ID) {
		if (answer(articles[firsta + article_id], K_MAIL_OR_FORWARD, 1))
		    goto redraw;
	    } else if (k_cmd == K_CONTINUE)
		if (answer((article_header *) NULL, K_MAIL_OR_FORWARD, 0))
		    goto redraw;

	    goto Prompt;
/*
      case K_CANCEL:
	 if (numa < 0) goto nextmenu;

	 if (current_group->group_flag & G_FOLDER) {
	     prompt("\1Cancel Folder\1 Article: ");
	     if (get_k_cmd() == K_ARTICLE_ID) {
		 cura = article_id;
		 fcancel(articles[firsta+article_id]);
		 mark();
	     }
	     goto Prompt;
	 }

	 prompt("\1Cancel\1 Article: ");

	 if (get_k_cmd() == K_ARTICLE_ID)
	     if (cancel(articles[firsta+article_id]) & 1) goto redraw;
	 goto Prompt;
*/
	case K_UNSUBSCRIBE:
	    if (unsubscribe(current_group)) {
		if (current_group->group_flag & G_UNSUBSCRIBED)
		    menu_return(ME_NEXT);
		home();
		CALL(print_header) ();
	    }
	    goto Prompt;

	case K_GROUP_OVERVIEW:
	    group_overview(-1);
	    goto redraw;

	case K_KILL_HANDLING:
	    switch (kill_menu((article_header *) NULL)) {
		case 0:	/* select */
		    do_auto_select((regexp *) NULL, 2);
		    break;
		case 1:	/* kill */
		    if (!do_auto_kill())
			break;
		    goto junk_killed_articles;
		default:
		    break;
	    }
	    goto Prompt;

	case K_CONTINUE:	/* goto next menu page or show the articles */
	    repl_attr(firsta, nexta, 0, A_SEEN, 0);
	    /* FALLTHROUGH */
	case K_CONTINUE_NO_MARK:	/* but don't mark unselected articles */
	    if (nexta < n_articles)
		goto nextmenu;
	    break;

	case K_READ_GROUP_THEN_SAME:
	    if ((temp = mark_read_return))
		goto do_marked_by;
	    break;

	case K_NEXT_GROUP_NO_UPDATE:
	    if ((temp = mark_next_group))
		goto do_marked_by;
	    menu_return(ME_NEXT);
	    break;		/* for lint. we can't actually get here */

	case K_READ_GROUP_UPDATE:
	    if ((temp = mark_read_skip) == 0)
		break;
    do_marked_by:
	    temp1 = 0;
	    temp2 = n_articles;
	    switch (temp) {
		case 1:
		    temp1 = firsta;
		    /* FALLTHROUGH */
		case 3:
		    temp2 = nexta;
		    break;
		case 2:
		    temp2 = firsta - 1;
		    break;
		case 4:
		    break;
	    }
	    repl_attr(temp1, temp2, 0, A_SEEN, 0);
	    if (k_cmd != K_NEXT_GROUP_NO_UPDATE)
		break;
	    menu_return(ME_NEXT);
	    break;		/* for lint. we can't actually get here */

	case K_PREVIOUS:
	    menu_return(ME_PREV);
	    break;		/* for lint. we can't actually get here */

	case K_ADVANCE_GROUP:
	case K_BACK_GROUP:
	    if (merged_menu) {
		msg("No possible on merged menu");
		goto same_prompt;
	    }
	    /* FALLTHROUGH */
	case K_GOTO_GROUP:
	    temp1 = n_articles;

	    switch (goto_group(k_cmd, (article_header *) NULL, (flag_type) 0)) {

		case ME_REDRAW:
		    firsta = 0;
		    if (temp1 != n_articles && consolidate)
			consolidate = do_consolidation();
		    goto redraw;

		case ME_NO_ARTICLES:
		    msg("No selections made.");

		    /* FALLTHROUGH */
		case ME_NO_REDRAW:
		    goto Prompt;

		case ME_QUIT:
		    menu_return(ME_QUIT);
		    break;	/* for lint. we can't actually get here */

		case ME_PREV:
		    goto redraw;

		case ME_NEXT:
		    s_keyboard = 1;
		    goto empty_menu_hack;
	    }

	    /* XXX: Fall ? */
	case K_OPEN_SUBJECT:
	    if (numa < 0)
		goto nextmenu;
	    prompt("\1Open subject\1");

	    k_cmd = get_k_cmd();
	    if (k_cmd != K_ARTICLE_ID) {
		if (k_cmd != cur_k_cmd)
		    goto Prompt;
		article_id = cura;
	    }
    open_subject:
	    ah = articles[firsta + article_id];
	    if (!(ah->flag & A_CLOSED) ||
		(ah->flag & (A_ROOT_ART | A_NEXT_SAME)) == A_ROOT_ART)
		goto Prompt;

	    cura = article_id = root_article(firsta + article_id) - firsta;
	    while (cura + firsta < n_articles) {
		ah = articles[firsta + cura];
		if (cura > article_id && ah->flag & A_ROOT_ART)
		    break;
		ah->flag &= ~A_CLOSED;
		cura++;
	    }
	    cura = article_id;
	    goto partial_redraw;

	case K_CLOSE_SUBJECT:
	    if (numa < 0)
		goto nextmenu;
	    prompt("\1Close subject\1");

	    k_cmd = get_k_cmd();
	    if (k_cmd != K_ARTICLE_ID) {
		if (k_cmd != cur_k_cmd)
		    goto Prompt;
		article_id = cura;
	    }
	    ah = articles[firsta + article_id];
	    if ((ah->flag & A_CLOSED) ||
		(ah->flag & (A_ROOT_ART | A_NEXT_SAME)) == A_ROOT_ART) {
		cura = next_root_article(firsta + cura) - firsta;
		goto Prompt;
	    }
	    cura = article_id = root_article(firsta + article_id) - firsta;
	    while (cura + firsta < n_articles) {
		ah = articles[firsta + cura];
		if (cura > article_id && ah->flag & A_ROOT_ART)
		    break;
		ah->flag |= A_CLOSED;
		cura++;
	    }
	    cura = article_id;
	    next_cura = next_root_article(firsta + cura) - firsta;
	    if (next_cura < 0 || next_cura > numa)
		next_cura = 0;
	    if (cura >= 0)
		goto partial_redraw_nc;
	    articles[cura + firsta]->menu_line = articles[firsta]->menu_line;
	    firsta += cura;
	    goto redraw;

	case K_LEAVE_NEXT:
	case K_JUNK_ARTICLES:
	    junk_prompt = cur_k_cmd == K_JUNK_ARTICLES ? 1 : 5;

	    for (;;) {
		switch (junk_prompt) {
		    case 1:
			if (novice)
			    msg("Use J repeatedly to select other functions");
			prompt("\1Mark read\1 S)een U)nmarked A)ll *+)selected a-z . [LN]");
			junk_attr = A_READ;
			break;
		    case 2:
			prompt("\1Unmark\1 S)een R)ead a-z [*+LAN.J] ");
			junk_attr = 0;
			break;
		    case 3:
			prompt("\1Select\1 L)eft-over, N(leave-next) [USRa-z.J]");
			junk_attr = A_SELECT;
			break;
		    case 4:
			prompt("\1Kill\1 R)ead S)een [LANU*+a-z.J]");
			junk_attr = A_KILL;
			break;
		    case 5:
			prompt("\1Leave\1 a-z .,/ * + U)nmarked [LANRSJ]");
			junk_attr = A_LEAVE_NEXT;
			break;
		    default:
			junk_prompt = 1;
			continue;
		}

	junk_another:
		if (cura < 0 || cura > numa)
		    cura = 0;
		cursor_at_id();

		switch (get_k_cmd()) {
		    case K_JUNK_ARTICLES:
			junk_prompt++;	/* can be 0 */
			continue;

		    case K_ARTICLE_ID:
			cura = article_id;
			if (junk_attr == A_KILL)
			    junk_attr = A_READ;
			if (auto_select_closed > 0 && articles[firsta + cura]->flag & A_CLOSED)
			    repl_attr_subject(A_KILL, junk_attr, 1);
			else {
			    articles[firsta + cura]->attr = junk_attr;
			    mark();
			    cura++;
			}
			goto junk_another;

		    case K_NEXT_LINE:
			cura++;
			goto junk_another;

		    case K_PREV_LINE:
			--cura;
			goto junk_another;

		    case K_SELECT_SUBJECT:
			if (junk_attr == A_KILL)
			    junk_attr = A_READ;
			repl_attr(firsta, nexta, A_AUTO_SELECT, A_SELECT, 0);
			repl_attr(firsta, nexta, A_SELECT, junk_attr, 1);
			goto Prompt;

		    case K_AUTO_SELECT:
			repl_attr_all(A_AUTO_SELECT, A_SELECT, 0);
			orig_attr = A_SELECT;
			break;

		    default:
			switch (cur_key) {
			    case 'S':
				orig_attr = A_SEEN;
				break;

			    case 'U':
				orig_attr = 0;
				break;

			    case 'L':
				if (junk_attr == A_KILL)
				    junk_attr = A_READ;
				orig_attr = A_LEAVE;
				break;

			    case 'A':
				orig_attr = A_KILL;
				break;

			    case 'N':
				orig_attr = A_LEAVE_NEXT;
				break;

			    case 'R':	/* kill read articles */
				orig_attr = A_READ;
				break;

			    default:
				goto Prompt;
			}
			break;
		}
		break;
	    }
	    if (nexta - firsta < n_articles) {
		prompt("On all menu pages? ");
		switch (yes(1)) {
		    case -1:
			goto Prompt;
		    case 0:
			if (!repl_attr(firsta, nexta, orig_attr, junk_attr, 1))
			    goto Prompt;
			break;
		    case 1:
			if (!repl_attr_all(orig_attr, junk_attr, 1))
			    goto Prompt;
			break;
		}
	    } else if (!repl_attr(firsta, nexta, orig_attr, junk_attr, 1))
		goto Prompt;

	    if (junk_attr != A_KILL)
		goto Prompt;

    junk_killed_articles:
	    elim_list[0] = firsta;
	    elim_list[1] = firsta + cura;
	    elim_list[2] = nexta;
	    if (elim_articles(elim_list, 3)) {
		firsta = elim_list[0];
		goto redraw;
	    }
	    firsta = elim_list[0];
	    cura = elim_list[1] - firsta;
	    nexta = elim_list[2];
	    goto Prompt;

	case K_ARTICLE_ID:
	    if (numa < 0)
		goto nextmenu;

	    MOUSE_MENU;

	    if (!is_k_select && auto_preview_mode)
		goto auto_preview;

	    cura = article_id;
	    if (!auto_select_closed || !(articles[firsta + cura]->flag & A_CLOSED)) {
		toggle();
		if (!is_k_select && auto_select_subject)
		    goto select_subject;
		mark();
		cura++;
		goto same_prompt;
	    }
	    if (auto_select_closed < 0) {
		article_id = cura;
		goto open_subject;
	    }
	    mi = menu_info + articles[firsta + cura]->menu_line;
	    if (mi->mi_unread == 0)
		repl_attr_subject(A_KILL, A_SELECT, 1);
	    else if (mi->mi_selected == mi->mi_unread)
		repl_attr_subject(auto_select_closed == 2 ? A_KILL : A_SELECT, 0, 1);
	    else
		repl_attr_subject(auto_select_closed == 2 ? A_KILL : 0, A_SELECT, 1);
	    goto same_prompt;

	case K_SELECT_INVERT:
	    if (numa < 0)
		goto nextmenu;

	    temp = cura;

	    no_raw();		/* for x-on/x-off */
	    for (cura = 0; cura <= numa; cura++) {
		toggle();
	    }
	    for (cura = 0; cura <= numa; cura++) {
		if (IS_VISIBLE(articles[firsta + cura]))
		    mark();
	    }
	    fl;

	    REDRAW_CHECK;
	    nn_raw();

	    cura = temp;
	    goto same_prompt;

	case K_UNSELECT_ALL:
	    if (last_k_cmd == K_UNSELECT_ALL)
		repl_attr_all(A_SELECT, 0, 1);
	    else
		repl_attr_all(A_AUTO_SELECT, 0, 1);
	    fl;
	    cura = 0;
	    goto same_prompt;

	case K_NEXT_LINE:
	    if (numa < 0)
		goto nextmenu;

	    cura++;
	    goto same_prompt;

	case K_PREV_LINE:
	    if (numa < 0)
		goto nextmenu;

	    if (--cura < 0)
		cura = numa;
	    set_root_if_closed();
	    goto same_prompt;

	case K_SELECT_SUBJECT:
	    if (numa < 0)
		goto nextmenu;

	    MOUSE_MENU;

	    if (MOUSE)
		cura = article_id;

	    if (last_k_cmd != K_ARTICLE_ID)
		toggle();

    select_subject:
	    repl_attr_subject(A_KILL, last_attr, 1);
	    goto same_prompt;

	case K_SELECT_RANGE:
	    if (numa < 0)
		goto nextmenu;

	    if (last_k_cmd == K_ARTICLE_ID) {
		cura--;
		if (cura < 0)
		    cura = numa;
	    } else
		last_attr = (articles[firsta + cura]->attr & A_SELECT) ? 0 : A_SELECT;

    range_again:

	    if (MOUSE) {
		if ((mouse_y < firstl) || (mouse_y > firstl + menu_length - 1))
		    goto same_prompt;
		/* stop selection when down click changes groups */
		if (last_k_cmd == K_INVALID)
		    goto same_prompt;
	    } else {
		prompt("\1%select range\1 %c-", last_attr ? "S" : "Des", ident[cura]);

		k_cmd = get_k_cmd();
		if (k_cmd == K_SELECT_RANGE) {
		    last_attr = last_attr ? 0 : A_SELECT;
		    goto range_again;
		}
		if (k_cmd != K_ARTICLE_ID)
		    goto Prompt;

	    }

	    if (article_id > cura) {
		article_number  tmp = cura;
		cura = article_id;
		article_id = tmp;
	    }
	    repl_attr(firsta + article_id, firsta + cura + 1, A_KILL, last_attr, 1);
	    goto Prompt;

	case K_AUTO_SELECT:
	    do_auto_select((regexp *) NULL, 1);
	    goto same_prompt;

	case K_GOTO_MATCH:
	    prompt("\1Select regexp\1 ");
	    if ((fname = get_s(NONE, NONE, NONE, NULL_FCT)) == NULL)
		goto Prompt;

	    if (*fname != NUL) {
		if (regular_expr)
		    freeobj(regular_expr);
		if (case_fold_search)
		    fold_string(fname);
		regular_expr = regcomp(fname);
	    }
	    if (regular_expr == NULL)
		msg("No previous expression");
	    else
		do_auto_select(regular_expr, 2);

	    goto Prompt;

	case K_NEXT_PAGE:
	    if (nexta < n_articles)
		goto nextmenu;
	    if (firsta == 0)
		goto same_prompt;

	    nexta = 0;
	    goto nextmenu;

	case K_PREV_PAGE:
	    if (firsta == 0 && nexta == n_articles)
		goto same_prompt;

    prevmenu:
	    nexta = (firsta > 0 ? firsta : n_articles);

	    for (menu_length = maxa; menu_length > 0 && --nexta >= 0;) {
		if (!IS_VISIBLE(articles[nexta]))
		    continue;
		if (--menu_length > 0) {
		    switch (menu_spacing) {
			case 0:
			    break;
			case 1:
			    if ((articles[nexta]->flag & A_ROOT_ART) == 0)
				break;
			    /* XXX: fall? */
			case 2:
			    --menu_length;
			    break;
		    }
		}
	    }
	    if (nexta < 0)
		nexta = 0;
	    goto nextmenu;

	case K_FIRST_PAGE:
	    if (firsta == 0)
		goto same_prompt;

	    nexta = 0;
	    goto nextmenu;

	case K_LAST_PAGE:
	    if (nexta == n_articles)
		goto same_prompt;
	    firsta = 0;
	    goto prevmenu;

	case K_PREVIEW:
	    if (numa < 0)
		goto nextmenu;

    preview_other:

	    MOUSE_MENU;
	    if (!MOUSE) {
		prompt("\1Preview article\1");
		k_cmd = get_k_cmd();

		if (k_cmd != K_ARTICLE_ID) {
		    if (k_cmd != K_PREVIEW)
			goto Prompt;
		    article_id = cura;
		}
	    }
    auto_preview:
	    temp = prompt_line;

    preview_next:
	    cura = article_id;
	    ah = articles[firsta + cura];

	    no_raw();
	    orig_attr = ah->attr;
	    ah->attr = 0;
	    menu_cmd = more(ah, MM_PREVIEW, prompt_line);
	    if (menu_cmd == MC_MENU) {
		if (ah->attr == 0)
		    ah->attr = orig_attr;
		if (prompt_line < 0) {
		    next_cura = cura;
		    goto redraw;
		}
		mark();
		prompt_line = temp;
		next_cura = -1;
		goto build_prompt;
	    }
	    if (ah->attr == 0)
		ah->attr = preview_mark_read ? A_READ : orig_attr;

	    if (prompt_line >= 0)
		mark();
	    next_cura = ++cura;

	    switch (menu_cmd) {

		case MC_DO_KILL:
		    if (!do_auto_kill())
			break;
		    elim_list[0] = firsta;
		    elim_list[1] = firsta + cura;
		    elim_articles(elim_list, 2);
		    firsta = elim_list[0];
		    next_cura = elim_list[1] - firsta;
		    goto redraw;

		case MC_DO_SELECT:
		    if (prompt_line >= 0) {	/* not redrawn */
			do_auto_select((regexp *) NULL, 2);
			break;
		    }
		    numa = -1;
		    do_auto_select((regexp *) NULL, 2);

		    /* FALLTHROUGH */
		case MC_QUIT:
		    menu_return(ME_QUIT);
		    break;	/* for lint. we can't actually get here */

		case MC_REENTER_GROUP:
		    menu_return(ME_REENTER_GROUP);
		    break;	/* for lint, we can't actually get here */

		case MC_NEXT:
		case MC_PREVIEW_NEXT:
		    if (prompt_line < 0) {	/* redrawn screen ! */
			if (quit_preview(menu_cmd))
			    goto redraw;
			prompt_line = Lines;
		    } else {
			if (quit_preview(menu_cmd)) {
			    next_cura = -1;
			    break;
			}
			prompt_line = temp;
		    }
		    article_id = cura;
		    goto preview_next;

		case MC_PREVIEW_OTHER:
		    prompt_line = temp;
		    nn_raw();
		    goto preview_other;

		default:
		    if (prompt_line < 0)
			goto redraw;
		    break;
	    }

	    prompt_line = temp;
	    goto build_prompt;

	case K_LAYOUT:
	    if (++fmt_linenum > 4)
		fmt_linenum = 0;
	    goto redraw;

	default:
	    msg("Command %d not supported", k_cmd);
	    goto same_prompt;
    }

    no_raw();

do_auto_read:
    switch (show_articles()) {

	case MC_MENU:
	    goto redraw;

	case MC_READGROUP:
	    if (k_cmd == K_READ_GROUP_THEN_SAME) {
		if (read_ret_next_page && nexta < n_articles)
		    firsta = nexta;
		goto redraw;
	    }
	    /* XXX: fall? */
	case MC_NEXTGROUP:
	    menu_cmd = ME_NEXT;
	    break;

	case MC_REENTER_GROUP:
	    menu_cmd = ME_REENTER_GROUP;
	    break;

	case MC_QUIT:
	    menu_cmd = ME_QUIT;
	    break;

	default:
	    sys_error("show_articles returned improper value");
    }

menu_exit:

    cur_bypass = o_bypass;
    n_selected = o_selected;
    firsta = o_firsta;
    in_menu_mode = o_mode;
    menu_level--;

    no_raw();
    return menu_cmd;
}


/*
 *	return article header for article on menu
 */

article_header *
get_menu_article(void)
{
    register article_header *ah;

    tprintf(" from article: ");
    fl;

    if (get_k_cmd() == K_ARTICLE_ID) {
	ah = articles[firsta + article_id];
	if (ah->a_group)
	    init_group(ah->a_group);
	return ah;
    }
    return NULL;
}



/*
 *	read command from command line
 */

int
alt_command(void)
{
    int             ok_val, macro_cmd;
    char           *cmd, brkchars[10];

    if (get_from_macro)
	ok_val = AC_UNCHANGED;
    else {
	prompt(":");
	ok_val = AC_PROMPT;
    }

again:

    sprintf(brkchars, "?%c ", erase_key);

    cmd = get_s(NONE, NONE, brkchars, alt_completion);
    if (cmd == NULL ||
	*cmd == NUL || *cmd == SP || (key_type) * cmd == erase_key)
	return ok_val;

    macro_cmd = get_from_macro;

    if (*cmd == '?') {
	display_file("help.extended", CLEAR_DISPLAY);
	ok_val = AC_REDRAW;
	goto new_prompt;
    }
    ok_val = parse_command(cmd, ok_val, (FILE *) NULL);
    if (ok_val != AC_REDRAW || !delay_redraw)
	return ok_val;

new_prompt:
    if (macro_cmd)
	return ok_val;

    prompt_line = -1;
    tprintf("\n\r:");
    fl;
    goto again;
}
