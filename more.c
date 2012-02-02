/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Article browser.
 */

#include <stdlib.h>
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
#include "news.h"
#include "regexp.h"
#include "save.h"
#include "nn_term.h"

/* more.c */

static void     more_auto_select(void);
static char    *a_st_flags(flag_type flag);
static void     brief_header_line(register article_header * ah, int lno);

int             monitor_mode = 0;
int             compress_mode = 0;
int             show_article_date = 1;
int             first_page_lines = 0;
int             overlap = 2;
int             mark_overlap = 0;

#ifdef GNKSA
char           *header_lines = "=F_S_D_n_W_R";
#else
char           *header_lines = NULL;
#endif

int             min_pv_window = 7;
int             wrap_headers = 6;
int             data_bits = 7;
int             scroll_clear_page = 1;
int             expired_msg_delay = 1;
char           *trusted_escapes = NULL;
int             new_read_prompt = 1;
int             re_layout_more = -1;
int             scroll_last_lines = 0;
int             ignore_formfeed = 0;

extern int      preview_window;
extern int      novice;
extern int      slow_mode;
extern int      auto_preview_mode;
extern int      flush_typeahead;
extern int      case_fold_search;
extern int      echo_prefix_key;
extern int      re_layout;
extern char    *folder_save_file, *default_save_file;
extern int      show_art_next_invalid;
extern int      mouse_y;
extern int      mouse_state;
extern int      STANDOUT;
extern int      alt_cmd_key, in_menu_mode, any_message;
extern long     n_selected;

extern char     delayed_msg[];

static int      rot13_must_init = 1;
static char     rot13_table[128];
int             rot13_active = 0;
#define ROT13_DECODE(c) 	((c & 0x80) ? c : rot13_table[c])

static int      compress_space;

static regexp  *regular_expr = NULL;

#define LINEMAX	100		/* most articles are less than 100 lines */

int             mark_overlap_shading = 0;

static struct header_def {
    char            field;
    char           *text;
    char          **news;
    char          **digest;
}               header_defs[] = {

    'A', "Approved", &news.ng_appr, 0,
    'B', "Distribution", &news.ng_dist, 0,
    'c', "Comment-To", &news.ng_comment, 0,
    'C', "Control", &news.ng_control, 0,
    'D', "Date", &news.ng_date, &digest.dg_date,
    'F', "From", &news.ng_from, &digest.dg_from,
    'f', "Sender", &news.ng_sender, 0,
    'I', "Message-Id", &news.ng_ident, 0,
    'J', "Originator", &news.ng_origr, 0,
    'K', "Keywords", &news.ng_keyw, 0,
    'L', "Lines", &news.ng_xlines, 0,
    'N', "Newsgroups", &news.ng_groups, 0,
    'O', "Organization", &news.ng_org, 0,
    'P', "Path", &news.ng_path, 0,
    'R', "Reply-To", &news.ng_reply, 0,
    'S', "Subject", &news.ng_subj, &digest.dg_subj,
    'W', "Followup-To", &news.ng_follow, 0,
    'X', "References", &news.ng_ref, 0,
    'Y', "Summary", &news.ng_summ, 0,
    'd', "Date-Received", &news.ng_rdate, 0,
    'g', "Newsgroup", &news.ng_groups, 0,
    'G', "Newsgroup", &news.ng_groups, 0,
    'n', "Newsgroups", &news.ng_groups, 0,
    'x', "Back-Ref", &news.ng_bref, 0,
    'v', "Save-File", NULL, 0,
    'a', "Spool-File", NULL, 0,
    'i', "DB-Info", NULL, 0,
    0
};

static int
get_header_field(char code, char **namep, char **valp, register article_header * ah)
{
    static char     special[FILENAME];
    register struct header_def *hdef;
    char           *lp;

    lp = NULL;
    for (hdef = header_defs; hdef->field; hdef++) {
	if (hdef->field != code)
	    continue;

	if ((ah->flag & A_DIGEST) && hdef->digest)
	    lp = *(hdef->digest);
	if (lp == NULL && hdef->news)
	    lp = *(hdef->news);
	break;
    }

    switch (code) {
	case 'n':
	case 'g':
	    if (lp == NULL)
		break;
	    if (!(current_group->group_flag & G_MERGED) && strchr(lp, ',') == NULL)
		return 0;
	    if (code == 'n')
		break;
	    /* FALLTHROUGH */
	case 'G':
	    if (ah->a_number > 0) {
		sprintf(special, "%s/%ld",
			current_group->group_name, (long) ah->a_number);
		lp = special;
	    } else
		lp = current_group->group_name;
	    break;

	case 'a':
	    if (current_group->group_flag & G_FOLDER)
		lp = current_group->archive_file;
	    else if (group_file_name && *group_file_name)
		lp = group_path_name;
	    break;

	case 'v':
	    if ((lp = current_group->save_file) == NULL)
		lp = (current_group->group_flag & G_FOLDER) ?
		    folder_save_file : default_save_file;
	    if (lp == NULL)
		return 0;
	    if (expand_file_name(special, lp, 2))
		lp = special;
	    break;

	case 'i':
	    sprintf(special, "#%ld fl=%ld re=%d li=%d hp=%ld fp=%ld lp=%ld ts=%ld",
		    (long) ah->a_number, (long) ah->flag, (int) ah->replies,
		    ah->lines, ah->hpos, ah->fpos, (long) ah->lpos,
		    (long) ah->t_stamp);
	    lp = special;
	    break;
    }
    if (lp == NULL)
	return 0;

    *namep = hdef->text;
    *valp = lp;
    return 1;
}

static char    *scan_codes;
static article_header *scan_arthdr;

void
scan_header_fields(char *fields, article_header * ah)
{
    scan_codes = fields;
    scan_arthdr = ah;
}

int
next_header_field(char **namep, char **valp, fct_type * attrp)
{
    fct_type        attr;

    while (*scan_codes) {
	attr = NULL_FCT;
	*namep = NULL;

	switch (*scan_codes) {
	    case '*':
		scan_codes++;
		return 1;	/* name == NULL */

	    case '=':
		attr = highlight;
		scan_codes++;
		break;

	    case '_':
		attr = underline;
		scan_codes++;
		break;

	    case '+':
		attr = shadeline;
		scan_codes++;
		break;
	}

	if (*scan_codes == NUL)
	    break;

	if (attrp)
	    *attrp = attr;
	if (get_header_field(*scan_codes++, namep, valp, scan_arthdr))
	    return 1;
    }
    return 0;
}

static void
more_auto_select(void)
{
    register article_header *ah;
    register int32  n;
    int             count = 0;

    for (n = 0; n < n_articles; n++) {
	ah = articles[n];
	if (ah->attr == A_READ)
	    continue;
	if (ah->attr & A_SELECT)
	    continue;

	if (auto_select_article(ah, 2)) {
	    ah->attr = A_AUTO_SELECT;
	    show_art_next_invalid = 1;
	    count++;
	}
    }

    if (count)
	msg("Selected %d articles", count);
}

static char    *
a_st_flags(flag_type flag)
{
    static char     buf[40];
    register char  *cp;
    static flag_type prevflag = 0;

    flag &= A_ST_FILED | A_ST_REPLY | A_ST_FOLLOW;
    if (flag == 0) {
	prevflag = 0;
	return "";
    }
    if (flag == prevflag)
	return buf;
    prevflag = flag;

    cp = buf;
    *cp++ = '(';
    if (flag & A_ST_FILED) {
	*cp++ = 'F';
	*cp++ = 'i';
	*cp++ = 'l';
	*cp++ = 'e';
	*cp++ = 'd';
    }
    if (flag & A_ST_REPLY) {
	if (cp[-1] != '(')
	    *cp++ = SP;
	*cp++ = 'R';
	*cp++ = 'e';
    }
    if (flag & A_ST_FOLLOW) {
	if (cp[-1] != '(')
	    *cp++ = SP;
	*cp++ = 'F';
	*cp++ = 'o';
	*cp++ = 'l';
    }
    strcpy(cp, new_read_prompt ? ")--" : ")------");
    return buf;
}

static void
brief_header_line(register article_header * ah, int lno)
{
    int             o_re_layout;

    so_gotoxy(0, lno, 0);
    so_printf("%s: ", ah->sender);
    o_re_layout = re_layout;
    if (re_layout_more >= 0)
	re_layout = re_layout_more;
    prt_replies(ah->replies);
    re_layout = o_re_layout;
    so_printf("%s", ah->subject);
    so_end();
}

#ifdef NOV
/* fill in ah->?pos from the article itself */
/* This should really go into more itself */
void
setpos(register article_header * ah, register FILE * art)
{
    /* XXX: set to zero here? Digests? */
    register long   startpos = ftell(art);
    char            line[1024];

    ah->fpos = 0;		/* initialize in case not found */

    /* header position == beginning of file */
    ah->hpos = 0;

    /* find the length */
    fseek(art, 0L, 2);
    ah->lpos = ftell(art);

    /* always start from the beginning */
    fseek(art, 0L, 0);
    while (fgets(line, sizeof(line), art) != NULL)
	if (*line == '\n') {	/* end of header? */
	    ah->fpos = ftell(art);	/* start of article */
	    break;
	}
    fseek(art, startpos, 0);
}

#endif				/* NOV */


int
more(article_header * ah, int mode, int screen_offset)
{
    register char  *lp;
    register int    c, col, lno = 0;
    register FILE  *art;
    int             more_cmd, eof, skip_spaces, has_space, window_lines;
    int             form_feed, last_ff_line, ignore_nl = 0;
    long            lineposbuf[LINEMAX];
    long           *linepos = lineposbuf;
    int             linemax = LINEMAX;
    char            linebuf[200], skip_char;
    int             skip_wrap;
    news_header_buffer ngheader, dgheader;
    struct news_header news_save;
    struct digest_header digest_save;
    int             linenum, maxline, topline, print_lines, lno1;
    int             scroll_lines, scroll_from;
    off_t           scroll_offset;
    int             underline_line, fake_underline;
    int             match_lines, match_redraw, match_topline = 0, match_botline;
    int             goto_line, prev_goto, stop_line, extra_lines;
    flag_type       in_digest = ah->flag & A_DIGEST;
    article_header  digestah;
    char           *fname, *hdrline;
    char            pr_fmt[200], send_date[40];
    int             match_expr, shade_overlap, shade_line;
    int            *key_map;
    key_type        cur_key;
    fct_type        hdrattr;
    char           *match_start, *match_end = NULL;
    int             open_modes, o_mode;

#ifdef RESIZING
    int             entry_col = Columns;
#endif

#define more_return(cmd) \
    { more_cmd = cmd; goto more_exit; }

    if (ah->a_group != NULL)
	init_group(ah->a_group);

    open_modes = SKIP_HEADER;
    if (show_article_date || header_lines) {
	open_modes |= FILL_NEWS_HEADER;
	if (header_lines == NULL)
	    open_modes |= GET_DATE_ONLY;
	else
	    open_modes |= GET_ALL_FIELDS;
	if (in_digest)
	    open_modes |= FILL_DIGEST_HEADER;
    }
    art = open_news_article(ah, open_modes, ngheader, dgheader);

    if (art == NULL) {
	if (expired_msg_delay >= 0) {
	    msg("Canceled or expired: \"%s: %-.35s\"", ah->sender, ah->subject);
	    if ((mode & MM_PREVIEW) == 0 && expired_msg_delay > 0)
		user_delay(expired_msg_delay);
	}
	return MC_NEXT;
    }
    o_mode = in_menu_mode;
    in_menu_mode = 0;

    if (screen_offset) {
	if (preview_window < 1 && Lines - screen_offset < min_pv_window)
	    screen_offset = 0;
	else {
	    brief_header_line(ah, screen_offset++);
	    if (!STANDOUT)
		screen_offset++;
	    clrline();
	}
    }
    if (show_article_date) {
	if (in_digest && digest.dg_date)
	    strncpy(send_date, digest.dg_date, 40);
	else if (news.ng_date) {
	    strncpy(send_date, news.ng_date, 40);
	} else
	    send_date[0] = NUL;
	send_date[39] = NUL;
	if ((lp = strrchr(send_date, ':')))
	    *lp = NUL;
    }
    linepos[0] = ah->hpos;
    linepos[1] = ah->fpos;
    maxline = 1;
    topline = 1;
    hdrline = screen_offset == 0 ? header_lines : "";

    rot13_active = 0;
    compress_space = compress_mode;
    last_ff_line = goto_line = -1, prev_goto = 1;
    skip_char = NUL;
    skip_wrap = 0;
    match_lines = match_redraw = match_expr = 0;
    underline_line = -1;
    fake_underline = 0;
    shade_overlap = 0;

    /*
     * initialization added by Peter Wemm..  These vars may be used before
     * set, what should they *really* be set to to be safe? XXXX
     */
    window_lines = 0;
    form_feed = 0;
    lno1 = 0;
    match_botline = 0;
    extra_lines = 0;
    cur_key = 0;
    /* end */

    scroll_lines = scroll_from = 0;
    scroll_offset = 0;
    if (scroll_last_lines < 0)
	scroll_from = -1;
    else if (scroll_last_lines > 0) {
	if (ah->lines > 0)
	    scroll_from = ah->lines - scroll_last_lines;
	else
	    scroll_offset = ah->lpos - (scroll_last_lines * 64);
    }
    stop_line = first_page_lines ? first_page_lines : -1;

    if (new_read_prompt) {
	char           *xp, *group_name = current_group->group_name;
	int             len, maxl;

	maxl = novice ? 18 : 25;

	if ((len = strlen(group_name)) > maxl) {
	    for (xp = group_name + len - maxl; *xp; xp++) {
		if (*xp == '.')
		    break;
		if (*xp == '/') {
		    xp++;
		    break;
		}
	    }
	    if (*xp)
		group_name = xp;
	    else
		group_name += len - maxl;
	}
	if (mode & (MM_PREVIEW | MM_DIGEST) || n_selected == 0)
	    sprintf(pr_fmt,
		    "\1\2--%s-- %s%s %s--%%s--%%s\1",
		    group_name,
		    (mode & MM_DIGEST) ? "DIGEST" :
		    (mode & MM_PREVIEW) ? "PREVIEW" : "LAST",
		    (ah->flag & A_NEXT_SAME) ? "+next" : "",
		    novice ? "--help:?" : "");
	else
	    sprintf(pr_fmt,
		    "\1\2--%s-- %ld MORE%s %s--%%s--%%s\1",
		    group_name,
		    n_selected,
		    (ah->flag & A_NEXT_SAME) ? "+next" : "",
		    novice ? "--help:?" : "");
    } else
	sprintf(pr_fmt,
		"\1\2-- %s%s %s-----%%s%s-----%%s\1",
		(mode & MM_PREVIEW) ? "PREVIEW " : "",
		(mode & MM_DIGEST) ? "FULL DIGEST" :
		(mode & MM_LAST_SELECTED) ? "LAST ARTICLE" : "ARTICLE",
		novice ? "-- help:? " : "",
		(ah->flag & A_NEXT_SAME) ? " (+next)" : "");

    if (screen_offset)
	goto safe_redraw;

redraw:			/* redraw that will destroy whole screen */
    screen_offset = 0;

safe_redraw:			/* redraw of "more window" only */
    linenum = topline;

next_page:
    no_raw();

    s_keyboard = 0;

    if (stop_line) {
	lno = screen_offset;
	if (scroll_clear_page || linenum <= 1) {
	    if (lno) {
		gotoxy(0, lno);
		clrpage();
	    } else
		clrdisp();
	}
	if (linenum == 1) {
	    hdrline = screen_offset == 0 ? header_lines : "";
	    if (hdrline && hdrline[0] == '@') {
		linenum = 0;
		fseek(art, linepos[0], 0);
		hdrline = NULL;
	    }
	}
print_header:
	if (hdrline == NULL || *hdrline == '*') {
	    if (hdrline && *++hdrline == NUL)
		hdrline = NULL;

	    if (linenum <= 1) {
		if (linenum == 0 || (mode & MM_DIGEST)) {
		    if (screen_offset) {
			lno--;
			if (!STANDOUT)
			    lno--;
			gotoxy(0, lno);
		    }
		    so_printxy(0, lno,
			       "Newsgroup: %s, article: %ld%s",
			       current_group->group_name,
			       (long) (ah->a_number),
			       ((mode & MM_DIGEST) || in_digest)
			       ? "  *DIGEST*" : "");
/*		    fseek(art, linepos[0], 0); */

		    lno++;
		    if (!STANDOUT)
			lno++;
		} else {
		    if (screen_offset == 0 && linenum == 1) {
			if (show_article_date)
			    so_printxy(-1, 0, send_date);

			brief_header_line(ah, lno++);
			if (!STANDOUT)
			    lno++;
		    }
		}
	    }
	}
	if (hdrline && screen_offset == 0) {

	    scan_header_fields(hdrline, ah);
	    while (next_header_field(&fname, &hdrline, &hdrattr)) {

		if (fname == NULL) {
		    hdrline = --scan_codes;	/* this is a hack! */
		    goto print_header;
		}
		lp = hdrline;

		gotoxy(0, lno++);
		tprintf("%s: ", fname);
		c = col = strlen(fname) + 2;

	split_header_line:
		if (hdrattr)
		    CALL(hdrattr) (1);

		while (*lp && c < Columns) {
		    if (isspace(*lp)) {
			while (lp[1] && isspace(lp[1]))
			    lp++;
			if (wrap_headers > 0 &&
			    (c + wrap_headers) >= Columns &&
			    (int) strlen(lp) >= wrap_headers) {
			    lp++;
			    break;
			}
			*lp = SP;
		    }
		    tputc(*lp++);
		    c++;
		}
		if (hdrattr)
		    CALL(hdrattr) (0);
		if (*lp && wrap_headers >= 0) {
		    gotoxy(col, lno++);
		    c = col;
		    goto split_header_line;
		}
	    }

	    hdrline = NULL;
	    tputc(NL);
	    lno++;
	}
	lno1 = lno;
	topline = linenum;

	window_lines = Lines - lno - 2;
	print_lines = window_lines;

	ignore_nl = 1;		/* ignore blank lines at top op screen */
    } else {
	tputc(CR);
	clrline();
	print_lines = extra_lines;	/* LINT complaints here -- ignore */
    }

    if (stop_line > 0) {
	if (print_lines > stop_line) {
	    extra_lines = print_lines - stop_line;
	    print_lines = stop_line;
	    underline_line = -1;
	    shade_overlap = 0;
	}
	stop_line = 0;
    } else
	stop_line = -1;

next_line:

    if (linenum == linemax) {
	linemax += 500;
	if (linepos == lineposbuf) {
	    linepos = newobj(long, linemax);
	    for (linenum = 0; linenum < LINEMAX; linenum++)
		linepos[linenum] = lineposbuf[linenum];
	} else
	    linepos = resizeobj(linepos, long, linemax);
    }
    if (goto_line == linenum) {
	goto_line = -1;
	goto next_page;
    }
    eof = 0;

    if (linenum > maxline)
	linepos[++maxline] = ftell(art);
    else if (linenum > 0)
	fseek(art, linepos[linenum], 0);


    if (linepos[linenum] >= ah->lpos) {
	if (match_expr) {
	    match_expr = 0;
	    topline = match_topline;	/* LINT complaints here -- ignore */
	    linenum = match_botline;	/* LINT complaints here -- ignore */
	    fseek(art, linepos[linenum], 0);
	    msg("Not found");
	    goto Prompt;
	}
	eof++;
	if (goto_line > 0) {
	    goto_line = -1;
	    linenum -= window_lines / 2;
	    goto next_page;
	}
	goto Prompt;
    }
    if (linenum == 0) {
	if (ftell(art) >= linepos[1]) {
	    linenum = 2;	/* current line is 1st line ! */
	    lno1 = lno;
	}
    } else {
	if (match_expr && linenum == match_botline)
	    msg("Searching...");
	linenum++;
    }

    lp = linebuf;
    col = 0;
    form_feed = 0;

next_char:

    c = getc(art);
    if (c == EOF) {
	eof++;
	if (lp == linebuf)
	    goto Prompt;
	goto end_line;
    }
    if (c & 0200) {
	if (monitor_mode || data_bits != 8) {
	    col += 4;
	    if (col > Columns) {/* then there is no room for M-^X */
		ungetc(c, art);
		goto long_line;
	    }
	    c &= 0177;
	    *lp++ = 'M';
	    *lp++ = '-';
	    if (c < SP) {
		*lp++ = '^';
		c += '@';
	    } else
		col--;
	}
    } else if (c < SP) {
	if (monitor_mode) {
	    if (c == NL) {
		*lp++ = '$';
		goto end_line;
	    }
	    if (col + 2 > Columns) {
		*lp++ = '\\';
		ungetc(c, art);
		goto end_line;
	    }
	    *lp++ = '^';
	    c += '@';
	    col++;
	} else
	    switch (c) {

		case '\f':
		    if (ignore_formfeed == 1)
			goto default_control;
		    last_ff_line = linenum;
		    if (lp == linebuf) {
			if (goto_line > 0 || skip_char || match_expr || lno == lno1)
			    goto next_line;
			form_feed = 1;
			goto Prompt;
		    }
		    form_feed = 1;
		    goto end_line;

		case CR:
		    if (lp == linebuf || ignore_nl)
			goto next_char;
		    ignore_nl = 1;
		    goto end_line;

		case NL:
		    if (ignore_nl) {
			ignore_nl = 0;
			if (lp == linebuf) {
			    if (lno == lno1) {
				ignore_nl = 1;
				goto next_line;
			    }
			    goto next_char;
			}
		    }
		    goto end_line;

		case BS:
		    if (col) {
			lp--;
			col--;
		    }
		    goto next_char;

		case TAB:
		    if (col + 8 - (col & 07) >= Columns)
			goto long_line;

		    do {
			*lp++ = SP;
			col++;
		    } while (col & 07);
		    goto next_char;

		case 033:	/* ESC may be a start/end of kanji or similar */
		    if (trusted_escapes != NULL) {
			if (col + 3 > Columns) {
			    ungetc(c, art);
			    goto long_line;
			}
			if (strcmp(trusted_escapes, "all") == 0)
			    break;
			c = getc(art);
			if (c == EOF)
			    goto next_char;
			ungetc(c, art);
			for (fname = trusted_escapes; *fname; fname++)
			    if (c == *fname)
				break;
			c = 033;
			if (*fname != NUL)
			    break;
		    }
		    /* FALLTHROUGH */

	    default_control:
		default:
		    if (col + 2 > Columns) {
			ungetc(c, art);
			goto long_line;
		    }
		    *lp++ = '^';
		    c += '@';
		    col++;
		    break;
	    }
    } else if (rot13_active && linenum > 0)
	c = ROT13_DECODE(c);

    *lp++ = c;
    col++;
    ignore_nl = 0;

    if (col < Columns)
	goto next_char;

long_line:
    ignore_nl = 1;

end_line:
    /* if we are seaching for a specific line, repeat until it is found */
    if (skip_wrap) {
	skip_wrap = ignore_nl;
	goto next_line;
    }
    if (goto_line >= linenum)
	goto next_line;
    if (skip_char) {
	if (lp == linebuf || linebuf[0] == skip_char) {
	    skip_wrap = ignore_nl;
	    goto next_line;
	}
	skip_char = NUL;
	if (overlap > 0) {
	    underline_line = linenum - 1;
	    linenum -= overlap + 1;
	    shade_overlap = mark_overlap_shading;
	    goto next_page;
	}
    }
    *lp++ = NUL;

    if (match_expr) {
	if (!regexec_cf(regular_expr, linebuf))
	    goto next_line;
	match_expr = 0;
	match_lines = 1;
	if (linenum > match_botline) {
	    match_redraw = 0;
	    if (last_ff_line > linenum)
		last_ff_line = -1;
	    linenum -= 5;
	    if (linenum < last_ff_line)
		linenum = last_ff_line;
	    goto next_page;
	}
	match_redraw = (stop_line < 0);
	stop_line = -1;
	lno = lno1 + linenum - topline - 1;
	print_lines = window_lines - lno + lno1;
    }
    /* now print the line */

    shade_line = 0;
    if ((shade_overlap > 0 && linenum <= underline_line) ||
	(shade_overlap < 0 && linenum > underline_line)) {
	if (match_redraw)
	    goto no_print;
	match_start = NULL;
	shade_line = 1;
    } else if (match_lines && underline_line != linenum &&
	       regexec_cf(regular_expr, linebuf)) {
	match_start = regular_expr->startp[0];
	match_end = regular_expr->endp[0];
	if (match_start == match_end) {
	    match_start = NULL;	/* null string */
	    if (match_redraw)
		goto no_print;
	}
    } else {
	if (match_redraw)
	    goto no_print;
	match_start = NULL;
    }

    gotoxy(0, lno);
    if (!scroll_clear_page)
	clrline();

    if (shade_line)
	shadeline(1);
    else if (mark_overlap && underline_line == linenum)
	if (!underline(1))
	    fake_underline = 1;
    skip_spaces = has_space = 0;

    for (lp = linebuf; ((c = *lp)); lp++) {

	if (match_start) {
	    if (lp == match_start)
		highlight(1);
	    if (lp == match_end) {
		highlight(0);
		match_start = NULL;
		if (regexec_cf(regular_expr, lp)) {
		    match_start = regular_expr->startp[0];
		    match_end = regular_expr->endp[0];
		    lp--;
		    continue;
		}
		if (match_redraw)
		    goto no_print;
	    }
	}
	if (c == SP) {
	    if (skip_spaces) {
		if (has_space)
		    continue;
		has_space++;
	    }
	    if (fake_underline)
		c = '_';
	} else {
	    if (compress_space && c != ' ') {
		skip_spaces = 1;
		has_space = 0;
	    }
	}

	tputc(c);
    }

    if (match_start)
	highlight(0);

    if (shade_line) {
	shadeline(0);
	if (shade_overlap > 0 && underline_line == linenum) {
	    underline_line = -1;
	    shade_overlap = 0;
	}
    } else if (mark_overlap && underline_line == linenum) {
	while (lp - linebuf < 10) {
	    tputc(fake_underline ? '_' : ' ');
	    lp++;
	}
	underline(0);
	underline_line = -1;
	fake_underline = 0;
    }
no_print:

    ++lno;
    if (--print_lines > 0 && s_keyboard == 0 && form_feed == 0)
	goto next_line;

    if (shade_overlap) {
	underline_line = -1;
	shade_overlap = 0;
    }
    if (!eof && linenum >= maxline) {
	if (ignore_nl) {
	    c = getc(art);
	    if (c == EOF)
		eof++;
	    else if (c != NL)
		ungetc(c, art);
	    else
		ignore_nl = 0;
	}
	if (!eof && ftell(art) >= ah->lpos)
	    eof++;
    }
    match_redraw = 0;

Prompt:

    if (eof && lno == screen_offset)
	more_return(MC_NEXT);

    if (scroll_lines > 0) {
	if (eof)
	    scroll_lines = 0;
	else {
	    print_lines = 1;
	    scroll_lines--;
	    prompt_line = lno;
	    goto scroll_next;
	}
    }
    nn_raw();

    prompt_line = lno;

    if (!scroll_clear_page)
	clrpage();

dflt_prompt:

    prompt(pr_fmt,
	   pct(ah->fpos, (long) (ah->lpos),
	       linepos[topline], ftell(art)),
	   a_st_flags(ah->flag));

    if (delayed_msg[0] != NUL) {
	msg(delayed_msg);
	delayed_msg[0] = NUL;
    }
same_prompt:

    if (flush_typeahead)
	flush_input();

    key_map = more_key_map;

prefix_key:
    if ((c = get_c()) & GETC_COMMAND)
	c &= ~GETC_COMMAND;
    else
	c = key_map[cur_key = c];

    if (s_hangup)
	c = K_QUIT;

    if (c & K_PREFIX_KEY) {
	key_map = keymaps[c & ~K_PREFIX_KEY].km_map;
	if (echo_prefix_key)
	    msg("%s", key_name(cur_key));
	goto prefix_key;
    }
    if (any_message && c != K_LAST_MESSAGE)
	clrmsg(0);

    if (c & K_MACRO) {
	m_invoke(c & ~K_MACRO);
	goto same_prompt;
    }
#define STATE(new_state)  c = new_state; goto alt_key;

alt_key:

    switch (c) {
	case K_UNBOUND:
	    ding();
	    /* FALLTHROUGH */
	case K_INVALID:
	    goto same_prompt;

	case K_REDRAW:

#ifdef RESIZING
	    if (Columns != entry_col) {
		entry_col = Columns;
		maxline = topline = 1;
	    }
#endif

	    goto redraw;

	case K_M_CONTINUE:
	    if (mouse_y >= 0) {
		if (mouse_y >= prompt_line) {
		    STATE(K_NEXT_ARTICLE);
		} else if (mouse_y == 0) {
		    if (topline == 1) {
			STATE(K_PREVIOUS);
		    } else {
			STATE(K_PREV_PAGE);
		    }
		} else {
		    STATE(K_CONTINUE);
		}
	    }
	    /* fall through??? */

	case K_NEXT_PAGE:
	    if (eof) {
		ding();
		goto same_prompt;
	    }
	    /* FALLTHROUGH */
	case K_CONTINUE:
	    if (eof)
		break;
	    if (screen_offset == 0 && form_feed == 0 && stop_line) {
		if ((scroll_from && linenum > scroll_from) ||
		    (scroll_offset && ftell(art) > scroll_offset)) {
		    scroll_lines = Lines - 2 - overlap;
		    print_lines = 1;
		    goto scroll;
		}
		if (linenum > overlap) {
		    underline_line = linenum;
		    linenum -= overlap;
		    shade_overlap = mark_overlap_shading;
		}
	    }
	    goto next_page;

	case K_LAST_MESSAGE:
	    msg((char *) NULL);
	    goto dflt_prompt;

	case K_HELP:
	    display_help("more");
	    goto redraw;

	case K_SHELL:
	    tputc(CR);
	    if (shell_escape())
		goto redraw;
	    goto dflt_prompt;

	case K_VERSION:
	    prompt(P_VERSION);
	    goto same_prompt;

	case K_M_TOGGLE:
	    if (mouse_state) {
		xterm_mouse_off();
		mouse_state = 0;
	    } else {
		xterm_mouse_on();
		mouse_state = 1;
	    }
	    goto dflt_prompt;

	case K_EXTENDED_CMD:
	    news_save = news;
	    digest_save = digest;
	    more_cmd = alt_command();
	    news = news_save;
	    digest = digest_save;

	    switch (more_cmd) {

		case AC_UNCHANGED:
		    goto same_prompt;

		case AC_QUIT:
		    more_return(MC_QUIT);
		    break;	/* for lint */

		case AC_PROMPT:
		    goto dflt_prompt;

		case AC_REENTER_GROUP:
		    more_return(MC_REENTER_GROUP);
		    break;	/* for lint */

		case AC_REORDER:
		    more_return(MC_MENU);
		    break;	/* for lint */

		case AC_REDRAW:
		    goto redraw;

		case AC_KEYCMD:
		    c = alt_cmd_key;
		    goto alt_key;
	    }
	    /* XXX: fall-thru? */
	case K_QUIT:
	    ah->attr = A_LEAVE_NEXT;
	    more_return(MC_QUIT);
	    break;		/* for lint */

	case K_SAVE_NO_HEADER:
	case K_SAVE_SHORT_HEADER:
	case K_SAVE_FULL_HEADER:
	case K_SAVE_HEADER_ONLY:
	case K_PRINT:
	case K_UNSHAR:
	case K_PATCH:
	case K_UUDECODE:
	    news_save = news;
	    digest_save = digest;

	    tputc(CR);
	    if (init_save(c, (char **) NULL) != NULL) {
		if (c == K_UNSHAR)
		    prompt_line = Lines - 2;

		save(ah);
		end_save();
	    }
	    news = news_save;
	    digest = digest_save;
	    if (!slow_mode && (c == K_UNSHAR || c == K_PATCH)) {
		tprintf("\r\n\n");
		any_key(0);
		goto redraw;
	    }
	    goto Prompt;

	case K_FOLLOW_UP:
	case K_REPLY:
	case K_MAIL_OR_FORWARD:
	    news_save = news;
	    digest_save = digest;
	    more_cmd = answer(ah, c, -1);
	    news = news_save;
	    digest = digest_save;
	    if (more_cmd) {
		if (slow_mode)
		    clrdisp();
		else
		    goto redraw;
	    }
	    goto Prompt;

	case K_POST:
	    if (post_menu()) {
		if (slow_mode)
		    clrdisp();
		else
		    goto redraw;
	    }
	    goto Prompt;

	case K_CANCEL:
	    if (current_group->group_flag & G_FOLDER) {
		prompt("%s this folder entry",
		       (ah->attr == A_CANCEL) ? "UNcancel" : "Cancel");
		if (yes(0))
		    fcancel(ah);
		goto Prompt;
	    }
	    if (cancel(ah) > 0)
		goto Prompt;
	    more_return(MC_NEXT);
	    break;		/* for lint */

	case K_UNSUBSCRIBE:
	    if (!unsubscribe(current_group))
		goto Prompt;
	    if ((current_group->group_flag & G_UNSUBSCRIBED) == 0)
		goto Prompt;
	    more_return(MC_NEXTGROUP);
	    break;		/* for lint */

	case K_GROUP_OVERVIEW:
	    group_overview(-1);
	    goto redraw;

	case K_KILL_HANDLING:
	    switch (kill_menu(ah)) {
		case 0:
		    more_auto_select();
		    ah->attr = 0;
		    break;
		case 1:
		    more_return(MC_DO_KILL);
		default:
		    break;
	    }
	    goto Prompt;

	case K_READ_GROUP_UPDATE:
	    if (mode & MM_PREVIEW)
		more_return(MC_MENU);
	    prompt("Mark rest of current group as read?");
	    if (yes(1) <= 0)
		goto Prompt;
	    more_return(MC_READGROUP);
	    break;		/* for lint */

	case K_NEXT_GROUP_NO_UPDATE:
	    if (mode & MM_PREVIEW)
		more_return(MC_MENU);
	    more_return(MC_NEXTGROUP);
	    break;		/* for lint */

	case K_BACK_TO_MENU:
	    more_return(MC_MENU);
	    break;		/* for lint */

	case K_PREVIOUS:
	    if ((mode & MM_PREVIOUS) == 0) {
		msg("No previous article");
		goto dflt_prompt;
	    }
	    more_return(MC_PREV);

	    /* FALLTHROUGH */
	case K_ADVANCE_GROUP:
	case K_BACK_GROUP:
	case K_GOTO_GROUP:
	    news_save = news;
	    digest_save = digest;
	    more_cmd = goto_group(c, ah, (flag_type) 0);
	    news = news_save;
	    digest = digest_save;

	    switch (more_cmd) {
		case ME_NO_REDRAW:
		    goto Prompt;

		case ME_QUIT:
		    more_return(ME_QUIT);

		default:
		    goto redraw;
	    }

	case K_NEXT_LINE:
	    if (eof)
		break;
	    if (screen_offset)
		goto same_prompt;

	    print_lines = 1;
	    goto scroll;

	case K_NEXT_HALF_PAGE:
	    if (eof)
		break;
	    if (screen_offset)
		goto same_prompt;

	    print_lines = window_lines / 2;

    scroll:
	    gotoxy(0, prompt_line);
	    clrpage();
	    no_raw();

	    if (print_lines + lno < (Lines - 1))
		goto next_page;

	    stop_line = -1;

    scroll_next:
	    gotoxy(0, Lines - 1);
	    c = print_lines + lno - Lines + 2;
	    while (--c >= 0) {
		tputc(NL);
		if (--lno1 < 0)
		    topline++;
		prompt_line--;
	    }
	    if (lno1 < 0)
		lno1 = 0;
	    if (prompt_line < 0)
		prompt_line = 0;
	    lno = prompt_line;
	    goto next_line;

	case K_PREV_HALF_PAGE:
	    /* XXX: Bug: will not back over headers */
	    if (topline <= 1)
		goto Prompt;
	    linenum = topline - window_lines / 2;
	    if (linenum < 1)
		linenum = 1;
	    goto next_page;

	case K_PREV_PAGE:
	    /* XXX: Bug: will not back over headers */
	    if (topline <= 1)
		goto Prompt;
	    linenum = topline - window_lines + overlap;	/* not perfect after FF */
	    underline_line = topline;
	    shade_overlap = -mark_overlap_shading;
	    if (linenum < 1)
		linenum = 1;
	    goto next_page;

	case K_SKIP_LINES:
	    skip_char = linebuf[0];
	    goto next_page;

	case K_GOTO_LINE:
	    prompt("\1Go to line:\1 ");
	    if ((fname = get_s(NONE, NONE, "$^", NULL_FCT)) == NULL)
		goto Prompt;

	    if (*fname == NUL) {
		if (prev_goto < 0)
		    goto Prompt;
		goto_line = prev_goto;

	    } else if (*fname == '$')
		goto_line = 30000;
	    else if (*fname == '^')
		goto_line = 1;
	    else {
		goto_line = atoi(fname);
		if (goto_line <= 0) {
		    goto_line = -1;
		    goto Prompt;
		}
	    }

    goto_page:
	    prev_goto = topline;

	    if (goto_line <= maxline) {
		linenum = goto_line;
		goto_line = -1;
	    }
	    goto next_page;

	case K_SELECT_SUBJECT:
	    more_return(MC_ALLSUBJ);
	    break;		/* for lint */

	case K_HEADER_PAGE:
	    fseek(art, linepos[0], 0);
	    goto_line = 0;
	    goto goto_page;

	case K_FIRST_PAGE:
	    goto_line = 1;
	    goto goto_page;

	case K_LAST_PAGE:
	    goto_line = 30000;
	    goto goto_page;

	case K_GOTO_MATCH:
	    prompt("\1/\1");
	    if ((fname = get_s(NONE, NONE, "/", NULL_FCT)) == NULL)
		goto Prompt;

	    if (*fname && *fname != '/') {
		if (regular_expr)
		    freeobj(regular_expr);
		if (case_fold_search)
		    fold_string(fname);
		regular_expr = regcomp(fname);
		match_lines = 0;
	    }
	    /* XXX: fall here? */
	case K_NEXT_MATCH:
	    if (regular_expr == NULL) {
		msg("No previous expression");
		goto Prompt;
	    }
	    match_expr = 1;
	    if (match_topline != topline)
		prev_goto = topline;
	    match_topline = topline;
	    match_botline = linenum;
	    if (match_lines == 0 && topline <= 1)
		linenum = topline;
	    match_lines = 0;
	    goto next_line;	/* don't clear the screen if no match */

	case K_FULL_DIGEST:
	    if (mode & MM_DIGEST)
		more_return(MC_NO_REDRAW);

	    if (!in_digest)
		goto same_prompt;

	    /* could do something more clever here later */
	    digestah = *ah;
	    digestah.flag &= ~A_DIGEST;
	    digestah.hpos = digestah.fpos = 0;
	    fseek(art, 0L, 2);
	    digestah.lpos = ftell(art);

	    switch (more(&digestah, mode | MM_DIGEST, screen_offset)) {

		case MC_REDRAW:
		    goto redraw;

		case MC_NO_REDRAW:
		    goto safe_redraw;

		case MC_QUIT:
		    more_return(MC_QUIT);
		    break;	/* for lint */

		case MC_REENTER_GROUP:
		    more_return(MC_REENTER_GROUP);

		default:
		    goto safe_redraw;
	    }

	case K_LEAVE_NEXT:
	    ah->attr = A_LEAVE_NEXT;
	    more_return(MC_PREVIEW_NEXT);
	    break;		/* for lint */

	case K_LEAVE_ARTICLE:
	    ah->attr = (mode & MM_PREVIEW) ? A_SELECT : A_LEAVE;

	    /* FALLTHROUGH */
	case K_NEXT_ARTICLE:
	    if ((mode & MM_PREVIEW) == 0)
		break;
	    more_return(MC_PREVIEW_NEXT);
	    break;		/* for lint */

	case K_BACK_ARTICLE:
	    if (mode & MM_FIRST_ARTICLE) {
		msg("First article is displayed");
		goto same_prompt;
	    }
	    more_return(MC_BACK_ART);
	    break;		/* for lint */

	case K_FORW_ARTICLE:
	    if (mode & MM_LAST_ARTICLE) {
		msg("Last article is displayed");
		goto same_prompt;
	    }
	    more_return(MC_FORW_ART);
	    break;		/* for lint */

	case K_NEXT_SUBJECT:
	    more_return(MC_NEXTSUBJ);
	    break;		/* for lint */

	case K_ROT13:
	    if (rot13_must_init) {
		register int    i;
		for (i = 0; i <= 127; i++) {
		    c = i;
		    if (c >= 'a' && c <= 'm')
			c += 13;
		    else if (c >= 'n' && c <= 'z')
			c -= 13;
		    else if (c >= 'A' && c <= 'M')
			c += 13;
		    else if (c >= 'N' && c <= 'Z')
			c -= 13;
		    rot13_table[i] = c;
		}
		rot13_must_init = 0;
	    }
	    rot13_active = !rot13_active;
	    goto safe_redraw;

	case K_COMPRESS:
	    compress_space = !compress_space;
	    goto safe_redraw;

	case K_PREVIEW:
	    if (mode & MM_PREVIEW)
		more_return(MC_PREVIEW_OTHER);

	    /* fall thru to "default" */

	default:
	    msg("Command %d not supported", c);
	    goto dflt_prompt;
    }

    more_return(MC_NEXT);

more_exit:
    in_menu_mode = o_mode;
    rot13_active = 0;

    if (linepos != lineposbuf)
	freeobj(linepos);

    no_raw();
    fclose(art);

    if (mode & MM_PREVIEW)
	if (more_cmd != MC_QUIT && more_cmd != MC_REENTER_GROUP) {
	    gotoxy(0, screen_offset);
	    clrpage();
	    if (auto_preview_mode && ah->attr == 0)
		ah->attr = A_READ;
	    if (screen_offset == 0)
		prompt_line = -1;
	}
    return more_cmd;
}


void
rot13_line(register char *cp)
{
    register int    c;

    while ((c = *cp))
	*cp++ = ROT13_DECODE(c);
}
