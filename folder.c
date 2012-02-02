/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Folder handling
 */

#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "articles.h"
#include "db.h"
#include "digest.h"
#include "dir.h"
#include "match.h"
#include "menu.h"
#include "pack_date.h"
#include "pack_name.h"
#include "pack_subject.h"
#include "save.h"
#include "sort.h"
#include "nn_term.h"

/* folder.c */

static char    *tilde_expansion(char **srcp, int compl);
static int      folder_header(void);
static void     rewrite_folder(void);


extern char    *home_directory;
extern int      current_folder_type;
extern int      use_mail_folders;
extern int      use_mmdf_folders;

extern int      bypass_consolidation;

int             dont_sort_folders = 0;
char           *folder_directory = NULL;
int             folder_rewrite_trace = 1;
char           *backup_folder_path = "BackupFolder~";
int             keep_backup_folder = 1;
int             convert_folder_mode = 0;	/* ignore folder's format on
						 * rewrite */
int             consolidated_manual = 0;
int             ignore_fancy_select = 0;	/* turn off select features
						 * for folders */

extern int      fmt_linenum;
extern char    *header_lines;

extern struct passwd *getpwnam();

/*
 *	expand ~[user][/...] form
 *	src ptr is advanced to last char of user name.
 */

static char    *
tilde_expansion(char **srcp, int compl)
{
    struct passwd  *pwd;
    register char  *name = *srcp;
    register char  *tail, x;

    tail = ++name;		/* skip ~ */
    while (*tail && isascii(*tail) && !isspace(*tail) && *tail != '/')
	tail++;

    if (compl && *tail != '/')
	return NULL;
    if (tail == name)
	return home_directory;

    *srcp = tail - 1;
    x = *tail;
    *tail = NUL;
    pwd = getpwnam(name);
    if (pwd == NULL && !compl)
	msg("User %s not found", name);
    *tail = x;

    return (pwd == NULL) ? NULL : pwd->pw_dir;
}

/*
 * 	file name completion and expansion
 *
 *	expand_mode bits:
 *		1:	expand path names
 *		2:	don't expand $N
 *		4:	don't expand any $?  (but $(...) is expanded)
 *		8:	don't complain about ~... (shell will do that)
 *		10:	doing filename completion
 */


int
expand_file_name(char *dest, char *src, int expand_mode)
{
    register char  *cp, *dp, c;
    int             parse, remap;
    char           *cur_grp, *cur_art;

    cur_grp = current_group ? current_group->group_name : NULL;
    cur_art = (group_file_name && *group_file_name) ? group_path_name : NULL;

    for (dp = dest, parse = 1; ((c = *src)); src++) {

	if (parse) {

	    if ((expand_mode & 1) && c == '+') {
		if (folder_directory == NULL) {
		    if (!(cp = getenv("FOLDER")))
			cp = FOLDER_DIRECTORY;
		    folder_directory = home_relative(cp);
		}
		cp = folder_directory;
		goto cp_str;
	    }
	    if ((expand_mode & 1) && c == '~') {
		if ((cp = tilde_expansion(&src, (expand_mode & 0x10))) == NULL) {
		    return 0;
		}
	cp_str:
		while (*cp)
		    *dp++ = *cp++;
		if (dp[-1] != '/')
		    *dp++ = '/';
		goto no_parse;
	    }
	    if ((expand_mode & 4) == 0 &&
		cur_art && c == '%' && (src[1] == ' ' || src[1] == NUL)) {
		cp = cur_art;
		while (*cp)
		    *dp++ = *cp++;
		goto no_parse;
	    }
	}
	if (c == '$' && src[1] == '(') {
	    char            envar[64];
	    for (src += 2, cp = envar; (c = *src) != NUL && c != ')'; src++)
		*cp++ = c;
	    *cp = NUL;
	    if (cp != envar) {
		if ((cp = getenv(envar)) != NULL)
		    while (*cp)
			*dp++ = *cp++;
		else {
		    msg("Environment variable $(%s) not set", envar);
		    return 0;
		}
	    }
	    goto no_parse;
	}
	if ((expand_mode & 4) == 0 && c == '$' && !isalnum(src[2])) {
	    remap = 0;
	    cp = NULL;

	    switch (src[1]) {
		case 'A':
		    cp = cur_art;
		    break;
		case 'F':
		    cp = cur_grp;
		    remap = 1;
		    break;
		case 'G':
		    cp = cur_grp;
		    break;
		case 'L':
		    if ((cp = strrchr(cur_grp, '.')))
			cp++;
		    else
			cp = cur_grp;
		    break;
		case 'N':
		    if (expand_mode & 2)
			goto copy;
		    if (cur_art)
			cp = group_file_name;
		    if (cp == NULL)
			goto copy;
		    break;
		default:
		    goto copy;
	    }
	    src++;

	    if (!cp) {
		msg("$%c not defined on this level", c);
		return 0;
	    }
	    while (*cp)
		if (remap && *cp == '.')
		    cp++, *dp++ = '/';
		else
		    *dp++ = *cp++;
	    goto no_parse;
	}
	if (c == '/')

#ifdef ALLOW_LEADING_DOUBLE_SLASH
	    if (dp != &dest[1])
#endif

		if (dp != dest && dp[-1] == '/')
		    goto no_parse;

copy:
	*dp++ = c;
	parse = isspace(c);
	continue;

no_parse:
	parse = 0;
    }

    *dp = NUL;

    return 1;
}


int
file_completion(char *path, int index)
{
    static int      dir_in_use = 0;
    static char    *head, *tail = NULL;
    static int      tail_offset;

    char            nbuf[FILENAME], buffer[FILENAME];
    char           *dir, *base;

    if (path) {
	if (dir_in_use) {
	    close_directory();
	    dir_in_use = 0;
	}
	if (index < 0)
	    return 0;

	head = path;
	tail = path + index;
    }
    if (!dir_in_use) {
	path = head;
	*tail = NUL;

	if (*path == '|')
	    return -1;		/* no completion for pipes */

	if (*path == '+' || *path == '~') {
	    if (!expand_file_name(nbuf, path, 0x11))
		return 0;	/* no completions */
	} else
	    strcpy(nbuf, path);

	if ((base = strrchr(nbuf, '/'))) {
	    if (base == nbuf) {
		dir = "/";
		base++;
	    } else {
		*base++ = NUL;
		dir = nbuf;
	    }
	} else {
	    base = nbuf;
	    dir = ".";
	}

	tail_offset = strlen(base);

	dir_in_use = list_directory(dir, base);

	return dir_in_use;
    }
    if (index)
	return compl_help_directory();

    if (!next_directory(buffer, 1))
	return 0;

    strcpy(tail, buffer + tail_offset);

    return 1;
}


static int      cancel_count;

void
fcancel(article_header * ah)
{
    if (ah->attr == A_CANCEL) {
	cancel_count--;
	ah->attr = 0;
    } else {
	cancel_count++;
	ah->attr = A_CANCEL;
    }
}

static int
folder_header(void)
{
    so_printxy(0, 0, "Folder: %s", current_group->group_name);

    return 1;			/* number of header lines */
}

/*
 *	mode values:
 *		0: normal folder or digest
 *		1: online manual
 *		2: mailbox
 */

int
folder_menu(char *path, int mode)
{
    FILE           *folder;
    register article_header *ah;
    news_header_buffer dgbuf;
    char            buffer[256];
    int             more, length, re, menu_cmd, was_raw;
    memory_marker   mem_marker;
    group_header    fake_group;
    int             cc_save;
    char            folder_name[FILENAME], folder_file[FILENAME];
    int             orig_layout;
    char           *orig_hdr_lines;

    orig_layout = fmt_linenum;
    orig_hdr_lines = header_lines;

    strcpy(folder_name, path);
    fake_group.group_name = folder_name;
    fake_group.kill_list = NULL;
    if (!expand_file_name(folder_file, folder_name, 1))
	return ME_NO_REDRAW;
    fake_group.archive_file = path = folder_file;
    fake_group.next_group = fake_group.prev_group = NULL;
    fake_group.group_flag = G_FOLDER | G_FAKED;
    fake_group.master_flag = 0;
    fake_group.save_file = NULL;
    current_group = NULL;
    init_group(&fake_group);

    folder = open_file(path, OPEN_READ);
    if (folder == NULL) {
	msg("%s not found", path);
	return ME_NO_REDRAW;
    }
    switch (get_folder_type(folder)) {
	case 0:
	    msg("Reading: %-.65s", path);
	    break;
	case 1:
	case 2:
	    msg("Reading %s folder: %-.50s",
		current_folder_type == 1 ? "mail" : "mmdf", path);
	    break;
	default:
	    msg("Folder is empty");
	    fclose(folder);
	    return ME_NO_REDRAW;
    }
    rewind(folder);

    was_raw = no_raw();
    s_keyboard = 0;

    current_group = &fake_group;

    mark_memory(&mem_marker);

    ah = alloc_art();

    more = 1;
    while (more && (more = get_digest_article(folder, dgbuf)) >= 0) {
	if (s_keyboard)
	    break;

	ah->a_number = 0;
	ah->flag = A_FOLDER;
	ah->attr = 0;

	ah->lines = digest.dg_lines;

	ah->hpos = digest.dg_hpos;
	ah->fpos = digest.dg_fpos;
	ah->lpos = digest.dg_lpos;

	if (digest.dg_from) {
	    length = pack_name(buffer, digest.dg_from, Name_Length);
	    ah->sender = alloc_str(length);
	    strcpy(ah->sender, buffer);
	    ah->name_length = length;
	    if (mode == 1)
		fold_string(ah->sender);
	} else {
	    ah->sender = "";
	    ah->name_length = 0;
	}

	if (digest.dg_subj) {
	    length = pack_subject(buffer, digest.dg_subj, &re, 255);
	    ah->replies = re;
	    ah->subject = alloc_str(length);
	    strcpy(ah->subject, buffer);
	    ah->subj_length = length;
	    if (mode == 1 && length > 1)
		fold_string(ah->subject + 1);
	} else {
	    ah->replies = 0;
	    ah->subject = "";
	    ah->subj_length = 0;
	}

	ah->t_stamp = digest.dg_date ? pack_date(digest.dg_date) : 0;

	add_article(ah);
	ah = alloc_art();
    }

    fclose(folder);

    if (s_keyboard) {
	menu_cmd = ME_NO_REDRAW;
    } else if (n_articles == 0) {
	msg("Not a folder (no article header)");
	menu_cmd = ME_NO_REDRAW;
    } else {
	if (n_articles > 1) {
	    clrdisp();
	    prompt_line = 2;
	    if (mode == 0 && !dont_sort_folders)
		sort_articles(-1);
	    else if (mode == 1) {
		article_number  n;
		for (n = 0; n < n_articles; n++) {
		    ah = articles[n];
		    if (n == 0)
			ah->flag |= A_ROOT_ART;
		    else if (strcmp(ah->sender, articles[n - 1]->sender) == 0)
			articles[n - 1]->flag |= A_NEXT_SAME;
		    else
			ah->flag |= A_ROOT_ART;
		}
		bypass_consolidation = consolidated_manual ? 2 : 1;
	    } else
		no_sort_articles();
	} else
	    no_sort_articles();

	cc_save = cancel_count;
	cancel_count = 0;
	if (mode == 1) {
	    fmt_linenum = -1;
	    header_lines = "*";
	}
reenter_menu:
	ignore_fancy_select = 1;
	menu_cmd = menu(folder_header);
	ignore_fancy_select = 0;

	if (mode == 0 && cancel_count) {
	    register article_number cur;

	    cancel_count = 0;
	    for (cur = 0; cur < n_articles; cur++) {
		if (articles[cur]->attr == A_CANCEL)
		    cancel_count++;
	    }
	}
	if (mode == 0 && cancel_count) {
	    clrdisp();
	    tprintf("\rFolder: %s\n\rFile:   %s\n\n\r", folder_name, folder_file);
	    if (cancel_count == n_articles)
		tprintf("Cancel all articles and remove folder? ");
	    else
		tprintf("Remove %d article%s from folder? ",
			cancel_count, plural((long) cancel_count));
	    fl;

	    switch (yes(1)) {
		case 1:
		    tprintf("\n\n");
		    if (cancel_count == n_articles) {
			if (unlink(group_path_name) < 0 &&
			    nn_truncate(group_path_name, (off_t) 0) < 0) {
			    tprintf("\rCould not unlink %s\n\r", group_path_name);
			    any_key(0);
			}
		    } else
			rewrite_folder();
		    break;
		case 0:
		    break;
		default:
		    goto reenter_menu;
	    }
	}
	cancel_count = cc_save;
    }

    release_memory(&mem_marker);
    if (fmt_linenum == -1) {
	fmt_linenum = orig_layout;
	header_lines = orig_hdr_lines;
    }
    if (was_raw)
	nn_raw();

    return menu_cmd;
}

static void
rewrite_folder(void)
{
    register FILE  *src, *dst;
    char           *oldfile;
    register int    c;
    register long   cnt;
    register article_header *ah, **ahp;
    register article_number n;

    if (strchr(backup_folder_path, '/'))
	oldfile = backup_folder_path;
    else
	oldfile = relative(nn_directory, backup_folder_path);

    if (move_file(group_path_name, oldfile, 1) < 0) {
	tprintf("\r\n\nCannot backup folder in %s\n", oldfile);
	goto confirm;
    }
    if ((src = open_file(oldfile, OPEN_READ)) == NULL) {
	tprintf("\rCannot open %s\n\r", oldfile);
	goto move_back;
    }
    if ((dst = open_file(group_path_name, OPEN_CREATE)) == NULL) {
	fclose(src);
	tprintf("\rCannot create %s\n\r", group_path_name);
	goto move_back;
    }
    sort_articles(-2);

    tprintf("\rCompressing folder...\n\r");
    fl;

    get_folder_type(src);

    for (ahp = articles, n = n_articles; --n >= 0; ahp++) {
	ah = *ahp;
	cnt = ah->lpos - ah->hpos;
	if (folder_rewrite_trace)
	    tprintf("%s\t%s (%ld-%ld=%ld)\n\r",
		    ah->attr == A_CANCEL ? "CANCEL" : "KEEP",
		    ah->subject, ah->hpos, (long) (ah->lpos), cnt);
	if (ah->attr == A_CANCEL)
	    continue;
	fseek(src, ah->hpos, 0);
	mailbox_format(dst, -1);
	while (--cnt >= 0) {
	    if ((c = getc(src)) == EOF)
		break;
	    putc(c, dst);
	}
	mailbox_format(dst, 0);
    }

    fclose(src);
    if (fclose(dst) == EOF)
	goto move_back;
    if (!keep_backup_folder)
	unlink(oldfile);
    if (folder_rewrite_trace)
	goto confirm;
    return;

move_back:
    if (move_file(oldfile, group_path_name, 2) == 0)
	tprintf("Cannot create new file -- Folder restored\n\r");
    else
	tprintf("Cannot create new file\n\n\rFolder saved in %s\n\r", oldfile);

confirm:
    any_key(0);
}
