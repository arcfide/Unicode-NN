/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Save (or print) articles
 */

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "config.h"
#include "global.h"
#include "db.h"
#include "decode.h"
#include "digest.h"
#include "folder.h"
#include "more.h"
#include "news.h"
#include "save.h"
#include "nn_term.h"
#include "unshar.h"

/* save.c */

static char    *ckdir_path(char *name);
static int      mkdirs_in_path(char *name, char *start);

char           *default_save_file = "+$F";
char           *folder_save_file = NULL;
int             suggest_save_file = 1;
char           *unshar_header_file = "Unshar.Headers";
int             use_mail_folders = 0;
int             use_path_in_from = 0;
int             use_mmdf_folders = 0;
int             save_report = 1;
int             quick_save = 0;
int             conf_append = 0;
int             conf_create = 1;
int             folder_format_check = 1;

char           *saved_header_escape = "~";

char           *print_header_lines = "FDGS";
char           *save_header_lines = "FDNS";
static char    *short_header_lines;

char           *save_counter_format = "%d";	/* format of save counter */
int             save_counter_offset = 0;

char            printer[FILENAME] = DEFAULT_PRINTER;
int             edit_print_command = 1;

char            patch_command[FILENAME] = "patch -p0";
int             edit_patch_command = 1;

char            unshar_command[FILENAME] = SHELL;
int             edit_unshar_command = 0;

extern char    *temp_file;
extern int      shell_restrictions;
extern char     delayed_msg[];

extern int      current_folder_type;

extern int      rot13_active;

extern char    *ctime();

static int      save_mode;
static char    *unshar_cmd;

#define	HEADER_HANDLING	0x0f	/* what should we do with the header */

#define	NO_HEADER	0	/* save without a header */
#define SHORT_HEADER	1	/* save with partial header */
#define	FULL_HEADER	2	/* save with full header */
#define	SHORT_HEADER_DG	3	/* save with partial header (digest) */
#define	HEADER_ONLY	4	/* save with *only* the header */

int             print_header_type = SHORT_HEADER;

#define	SEPARATE_FILES	0x0100	/* save as separate files */
#define UNIQUE_FILES	0x0200	/* save in unique files */
#define	FILE_IS_NEW	0x0400	/* this is a new file */
#define APPEND_ARTNUM	0x0800	/* append article number to file name */
#define	IS_PIPE		0x1000	/* output is on pipe */
#define	DO_UNSHAR	0x2000	/* unshar article (or patch) */
#define	DO_PATCH	0x4000	/* patch article */
#define	DO_DECODE	0x8000	/* uudecode article */

/* open modes for open_news_article for the various HEADER_HANDLINGs */

static int      open_mode[] = {
    SKIP_HEADER,
    FILL_NEWS_HEADER | SKIP_HEADER | GET_ALL_FIELDS,
    0,
    FILL_NEWS_HEADER | FILL_DIGEST_HEADER | SKIP_HEADER | GET_ALL_FIELDS,
    FILL_NEWS_HEADER
};

static FILE    *save_file;	/* output stream for saved files */
static char    *save_name;	/* save file name */

static int      uniq_counter;	/* separate files counter */
static char     uniq_format[FILENAME];	/* sprintf format for '*' expansion */

static char     last_dir[FILENAME] = "";

/*
 * return pointer to first path name component, that does not exist
 */

static char    *
ckdir_path(char *name)
{
    char           *slash;
    char           *component;

    component = name;

    while ((slash = strchr(component, '/'))) {
	if (slash == component) {
	    /* ...//...  */
	    component++;
	    continue;
	}
	*slash = NUL;
	if (file_exist(name, "drx")) {
	    *slash++ = '/';
	    component = slash;
	    continue;
	}
	if (file_exist(name, (char *) NULL)) {
	    *slash = '/';
	    return NULL;
	}
	*slash = '/';
	break;
    }
    return component;
}

/*
 * create directories in path name, starting from start
 */

static int
mkdirs_in_path(char *name, char *start)
{
    char           *slash;
    char           *component;

    component = start;

    while ((slash = strchr(component, '/'))) {
	if (slash == component) {
	    /* ...//...  */
	    component++;
	    continue;
	}
	*slash = NUL;
	if (mkdir(name, 0755)) {
	    msg("Cannot make %s/", name);
	    *slash = '/';
	    return 0;
	}
	*slash++ = '/';
	component = slash;
    }
    return 1;
}

static void
set_folder_type(char *name)
{
    FILE           *f;

    current_folder_type = -1;
    if (!folder_format_check)
	return;

    if ((f = open_file(name, OPEN_READ)) != NULL) {
	get_folder_type(f);
	fclose(f);
    }
}

char           *
init_save(int command, char **mode_textp)
{
    char           *mode_text;
    static char     last_input[FILENAME] = "";
    static char     name_buf[512];	/* buffer for file name expansion */
    char           *start, *last, *np;

    uniq_counter = 0;
    short_header_lines = save_header_lines;

    switch (command) {

	case K_SAVE_FULL_HEADER:
	    save_mode = FULL_HEADER;
	    mode_text = "Save";
	    goto cont_save;

	case K_SAVE_SHORT_HEADER:
	    save_mode = SHORT_HEADER;
	    mode_text = "Output";
	    goto cont_save;

	case K_SAVE_HEADER_ONLY:
	    save_mode = HEADER_ONLY;
	    mode_text = "Header_save";
	    goto cont_save;

	case K_SAVE_NO_HEADER:
	    save_mode = NO_HEADER;
	    mode_text = "Write";

    cont_save:
	    if (quick_save && (current_group->group_flag & G_FOLDER) == 0) {
		if (current_group->save_file)
		    save_name = current_group->save_file;
		else
		    save_name = default_save_file;
		strcpy(last_input, save_name);
		save_name = last_input;
	    } else {
		prompt("\1%s on\1 (+~|) ", mode_text);

		save_name = current_group->save_file;
		if (save_name == NULL && suggest_save_file)
		    save_name = (current_group->group_flag & G_FOLDER) ?
			folder_save_file : default_save_file;
		if (save_name != NULL) {
		    if (!expand_file_name(name_buf, save_name, 2))
			return NULL;
		    save_name = name_buf;
		}
		save_name = get_s(last_input, save_name, NONE, file_completion);
		if (save_name == NULL || *save_name == NUL)
		    return NULL;

		if (save_name[1] == NUL && save_name[0] == '+')
		    save_name = (current_group->group_flag & G_FOLDER) ?
			folder_save_file : default_save_file;
		else if (current_group->save_file == NULL ||
			 strcmp(save_name, current_group->save_file))
		    strcpy(last_input, save_name);
		if (save_name == NULL || *save_name == NUL)
		    return NULL;
	    }

	    if (*save_name == '|') {
		if (shell_restrictions) {
		    msg("Restricted operation - pipes not allowed");
		    return NULL;
		}
		mode_text = "Pipe";
		save_name++;
		save_mode |= IS_PIPE;
		if (*save_name == '|') {
		    save_mode |= SEPARATE_FILES;
		    save_name++;
		}
	    }
	    if (!expand_file_name(name_buf, save_name, (save_mode & IS_PIPE) ? 11 : 3))
		return NULL;
	    save_name = name_buf;

	    if ((save_mode & IS_PIPE) == 0) {
		np = strrchr(save_name, '*');
		if (np != NULL) {
		    *np = NUL;
		    sprintf(uniq_format, "%s%s%s",
			    save_name, save_counter_format, np + 1);
		    *np = '*';
		    save_mode |= SEPARATE_FILES | UNIQUE_FILES;
		} else {
		    np = strrchr(save_name, '$');
		    if (np != NULL && np[1] == 'N') {
			if (current_group->group_flag & G_FOLDER) {
			    msg("$N is not defined in a folder");
			    return NULL;
			}
			*np = NUL;
			sprintf(uniq_format, "%s%%ld%s", save_name, np + 2);
			*np = '$';
			save_mode |= SEPARATE_FILES | APPEND_ARTNUM;
		    }
		}
	    }
	    break;

	case K_UUDECODE:
	    save_mode = NO_HEADER | DO_UNSHAR | DO_DECODE;
	    mode_text = "Decode";
	    goto unshar1;

	case K_PATCH:
	    save_mode = NO_HEADER | SEPARATE_FILES | DO_UNSHAR | DO_PATCH;
	    mode_text = "Patch";
	    if (!shell_restrictions && edit_patch_command) {
		prompt("\1Patch command:\1 ");
		save_name = get_s(NONE, patch_command, NONE, NULL_FCT);
		if (save_name == NULL || *save_name == NUL)
		    return NULL;
		strcpy(patch_command, save_name);
	    }
	    unshar_cmd = patch_command;
	    goto unshar1;

	case K_UNSHAR:
	    save_mode = NO_HEADER | SEPARATE_FILES | DO_UNSHAR;
	    mode_text = "Unshar";
	    if (!shell_restrictions && edit_unshar_command) {
		prompt("\1Unshar command:\1 ");
		save_name = get_s(NONE, unshar_command, NONE, NULL_FCT);
		if (save_name == NULL || *save_name == NUL)
		    return NULL;
		strcpy(unshar_command, save_name);
	    }
	    unshar_cmd = unshar_command;

    unshar1:
	    prompt("\1%s Directory:\1 ", mode_text);
	    save_name = current_group->save_file;
	    if (save_name != NULL) {
		if (!expand_file_name(name_buf, save_name, 10))
		    return NULL;
		save_name = name_buf;
	    }
	    save_name = get_s(last_dir, save_name, NONE, file_completion);
	    if (save_name == NULL || *save_name == NUL)
		return NULL;
	    strcpy(last_dir, save_name);
	    if (!expand_file_name(name_buf, save_name, 1))
		return NULL;
	    save_name = name_buf;
	    break;

	case K_PRINT:
	    if (print_header_type < NO_HEADER || print_header_type > FULL_HEADER) {
		msg("Invalid 'print-header-type' value %d", print_header_type);
		print_header_type = NO_HEADER;
	    }
	    short_header_lines = print_header_lines;
	    save_mode = print_header_type | IS_PIPE;

	    if (!shell_restrictions && edit_print_command) {
		prompt("\1Print command:\1 ");
		save_name = get_s(NONE, printer, NONE, NULL_FCT);
		if (save_name == NULL || *save_name == NUL)
		    return NULL;
		strcpy(printer, save_name);
	    }
	    if (!expand_file_name(name_buf, printer, 1))
		return NULL;
	    save_name = name_buf;
	    mode_text = "Print";
	    break;

	default:
	    msg("Illegal save command: %d", command);
	    return NULL;
    }

    if (save_name == NULL)
	return NULL;

    if (!(save_mode & IS_PIPE)) {
	if (file_exist(save_name, (save_mode & DO_UNSHAR) ? "wd" : "wf")) {
	    if (save_mode & DO_UNSHAR) {
		int             len = strlen(save_name);
		if (save_name[len - 1] != '/')
		    strcpy(save_name + len, "/");
	    } else if (conf_append) {
		tprintf("\rAppend to: %s ? ", save_name);
		clrline();
		if (!yes(0))
		    return NULL;
	    }
	} else {
	    if (errno != ENOENT) {
		msg("Cannot access %s", save_name);
		return NULL;
	    }
	    if (save_mode & DO_UNSHAR) {
		int             len = strlen(save_name);
		if (save_name[len - 1] != '/')
		    strcpy(save_name + len, "/");
	    }
	    start = ckdir_path(save_name);
	    if (start == NULL) {
		msg("No permission");
		return NULL;
	    }
	    last = strrchr(start, '/');
	    /* last != NULL => non-existing directories */

	    if (conf_create && (!(save_mode & SEPARATE_FILES) || last)) {
		tprintf("\rCreate ");
		for (np = save_name; *np; np++) {
		    if (np == start)
			tputc('\"');
		    tputc(*np);
		    if ((save_mode & SEPARATE_FILES) && np == last)
			break;
		}
		tprintf("\" ?");
		clrline();
		if (yes(last != NULL) <= 0)
		    return NULL;
	    }
	    if (last && !mkdirs_in_path(save_name, start))
		return NULL;
	}
    }
    if (mode_textp)
	*mode_textp = mode_text;

    save_mode |= FILE_IS_NEW;	/* so save() will open it */

    if (save_mode & DO_DECODE) {
	uud_start(save_name);
	save_mode &= ~DO_UNSHAR;
    }
    return save_name;
}


int
save(article_header * ah)
{
    register FILE  *art;
    register int    c, lcount = 0, mode;
    news_header_buffer hdrbuf;
    news_header_buffer dghdrbuf;
    int             was_raw = 0, set_visual = 0;
    char            copybuf[FILENAME * 4], uniqbuf[FILENAME];
    flag_type       nn_st_flag = A_ST_FILED;
    int             with_header;

    if (ah->a_group)
	init_group(ah->a_group);

    mode = save_mode & HEADER_HANDLING;
    if (mode == SHORT_HEADER && ah->flag & A_DIGEST)
	mode = SHORT_HEADER_DG;

    c = open_mode[mode];
    if (use_mail_folders && use_path_in_from && (c & FILL_NEWS_HEADER) == 0)
	c |= FILL_NEWS_HEADER | GET_ALL_FIELDS;

    art = open_news_article(ah, c, hdrbuf, dghdrbuf);
    if (art == NULL) {
	msg("Cannot read %s", group_path_name);
	return 0;
    }
    if (save_mode & DO_DECODE) {
	save_file = NULL;
	c = uudecode(ah, art);
	fclose(art);
	return (c < 0) ? 0 : 1;
    }
    if (save_mode & UNIQUE_FILES) {
	uniqbuf[0] = NUL;
	do {
	    strcpy(copybuf, uniqbuf);
	    uniq_counter++;
	    sprintf(uniqbuf, uniq_format, uniq_counter + save_counter_offset);
	    if (strcmp(uniqbuf, copybuf) == 0) {
		msg("save-counter \"%s\" does not generate unique file names",
		    save_counter_format);
		goto fatal;
	    }
	} while (file_exist(uniqbuf, (char *) NULL));
	save_name = uniqbuf;
	save_mode |= FILE_IS_NEW;
    } else if (save_mode & APPEND_ARTNUM) {
	sprintf(uniqbuf, uniq_format, (long) (ah->a_number));
	save_name = uniqbuf;
    }
    if (save_mode & FILE_IS_NEW) {
	if (save_mode & (IS_PIPE | DO_UNSHAR)) {
	    set_visual = 1;
	    was_raw = visual_off();
	}
	if (save_mode & IS_PIPE) {
	    if ((save_file = popen(save_name, "w")) == NULL) {
		msg("Cannot pipe to %s", save_name);
		goto fatal;
	    }
	} else if (save_mode & DO_UNSHAR) {
	    if ((save_mode & DO_PATCH) == 0) {
		if (!unshar_position(art))
		    goto fatal;
		if (unshar_header_file)
		    store_header(ah, art, save_name, unshar_header_file);
	    }
	    new_temp_file();
	    sprintf(copybuf,
	    "cd %s && { %s 2>&1 ; } | tee %s ; cat %s >> %s.Result ; rm %s",
		    save_name != NULL ? save_name : ".", unshar_cmd,
		    temp_file, temp_file,
		    (save_mode & DO_PATCH) ? "Patch" : "Unshar",
		    temp_file);

	    save_file = popen(copybuf, "w");
	    if (save_file == NULL) {
		msg("Cannot exec: '%s'", copybuf);
		goto fatal;
	    }
	    tprintf("\r\n%s %s\r\n",
		    save_mode & DO_PATCH ? "PATCHING FROM" : "UNPACKING",
		    ah->subject ? ah->subject : "ARTICLE");
	    fl;
	} else {
	    if ((save_file = open_file(save_name, OPEN_APPEND)) == NULL) {
		msg("Cannot write %s", save_name);
		fclose(art);
		return 0;
	    }
	    current_folder_type = -1;
	    if (ftell(save_file) != (off_t) 0) {
		if (mode != NO_HEADER)
		    set_folder_type(save_name);
		save_mode &= ~FILE_IS_NEW;
	    }
	}
    }
    clrline();
    s_pipe = 0;

    with_header = mode != NO_HEADER &&
	(save_mode & (IS_PIPE | DO_UNSHAR)) == 0;

    if (with_header)
	mailbox_format(save_file, 1);

    if (mode == FULL_HEADER || mode == HEADER_ONLY) {
	off_t           cnt = ah->fpos - ah->hpos;
	while (--cnt >= 0) {
	    if ((c = getc(art)) == EOF)
		break;
	    putc(c, save_file);
	}
    } else if (mode == SHORT_HEADER || mode == SHORT_HEADER_DG) {
	char           *name, *val;
	scan_header_fields(short_header_lines, ah);
	while (next_header_field(&name, &val, (fct_type *) NULL)) {
	    if (name == NULL)
		continue;
	    fprintf(save_file, "%s: %s\n", name, val);
	}
	fputc(NL, save_file);
    }
    fflush(save_file);
    if (s_pipe)
	goto broken_pipe;

    if (mode != HEADER_ONLY) {
	lcount = 0;
	while (ftell(art) < ah->lpos && fgets(copybuf, 512, art)) {
	    lcount++;
	    if (rot13_active)
		rot13_line(copybuf);
	    if (saved_header_escape && with_header && is_header_line(copybuf))
		fputs(saved_header_escape, save_file);
	    fputs(copybuf, save_file);
	    if (s_pipe)
		goto broken_pipe;
	}
    }
    if (with_header)
	lcount += mailbox_format(save_file, 0);

broken_pipe:
    fclose(art);

    if (save_mode & DO_UNSHAR) {
	if ((c = pclose(save_file)) != 0) {
	    sprintf(delayed_msg, "Save command failed; exit = %d", c);
	    nn_st_flag = 0;
	}
	save_file = NULL;
    } else {
	if (s_pipe)
	    msg("Command did not read complete article!");
	else if (save_report)
	    msg((save_mode & IS_PIPE) ? "%s: %d lines piped" :
		(save_mode & FILE_IS_NEW) ? "%s created: %d lines written" :
		"%s: %d lines appended", save_name, lcount);

	if (s_pipe || (save_mode & SEPARATE_FILES)) {
	    if (end_save() == 0)
		goto err;
	} else
	    save_mode &= ~FILE_IS_NEW;
    }

#ifdef MAIL_READING
    if (mail_auto_delete && (save_mode & IS_PIPE) == 0)
	if (current_group->group_flag & G_MAILBOX)
	    if (ah->attr != A_CANCEL)
		fcancel(ah);
#endif

    if (set_visual)
	visual_on();
    if (was_raw)
	nn_raw();

    ah->flag |= nn_st_flag;

    return !s_pipe || (save_mode & SEPARATE_FILES);

fatal:
    fclose(art);
err:
    if (set_visual)
	visual_on();
    if (was_raw)
	nn_raw();
    return 0;
}

int
end_save(void)
{
    int             c;
    FILE           *sf;
    sf = save_file;
    save_file = NULL;

    if (sf) {
	if (save_mode & (IS_PIPE | DO_UNSHAR)) {
	    if ((c = pclose(sf)) != 0) {
		if (c >= 256)
		    c >>= 8;	/* HACK */
		sprintf(delayed_msg, "Save command failed; exit = %d", c);
		return 0;
	    }
	} else if (fclose(sf) == EOF) {
	    sprintf(delayed_msg, "Save failed (disk full?)");
	    return 0;
	}
    }
    if (save_mode & DO_DECODE) {
	uud_end();
    }
    if (save_mode & DO_UNSHAR)
	sprintf(delayed_msg, "Output is saved in %s/%s.Result",
		save_name != NULL ? save_name : ".",
		(save_mode & DO_PATCH) ? "Patch" : "Unshar");
    return 1;
}

void
store_header(article_header * ah, FILE * f, char *dir, char *file)
{
    register int    c;
    off_t           endpos;
    FILE           *h;

    if (dir != (char *) NULL && file[0] != '/')
	file = relative(dir, file);
    if ((h = open_file(file, OPEN_APPEND)) == NULL) {
	msg("Cannot open %s", file);
	return;
    }
    fseek(h, 0, 2);
    if (ftell(h) > 0)
	set_folder_type(file);
    else
	current_folder_type = -1;
    if (!use_mmdf_folders && ftell(h) > 0)
	putc(NL, h);		/* just in case */
    mailbox_format(h, 1);
    endpos = ftell(f) - ah->hpos;
    fseek(f, ah->hpos, 0);
    while (--endpos >= 0 && (c = getc(f)) != EOF)
	putc(c, h);

    mailbox_format(h, 0);
    fclose(h);
}

int
mailbox_format(FILE * f, int top)
{
    time_t          now;
    int             do_mmdf, do_mail;

    do_mmdf = do_mail = 0;
    switch (current_folder_type) {
	case 0:
	    break;
	case 1:
	    do_mail = 1;
	    break;
	case 2:
	    do_mmdf = 1;
	    break;
	default:
	    do_mmdf = use_mmdf_folders;
	    do_mail = use_mail_folders;
	    break;
    }

    if (do_mmdf) {
	fprintf(f, "\001\001\001\001\n");
	return 0;
    }
    if (top == 0) {
	fputc(NL, f);
	return 1;
    }
    if (top > 0 && do_mail) {
	now = cur_time();
	fprintf(f, "From %s %s",
		(use_path_in_from && news.ng_path) ? news.ng_path :
		current_group->group_name, ctime(&now));
	return 1;
    }
    return 0;
}

char           *
run_mkdir(char *dir, char *name_buf)
{
    if (dir == NULL) {
	prompt("\1Directory: \1");
	dir = get_s(last_dir, NONE, NONE, file_completion);
	if (dir == NULL || *dir == NUL)
	    return NULL;
	strcpy(last_dir, dir);
    }
    if (*dir == '+' || *dir == '~') {
	if (!expand_file_name(name_buf, dir, 1))
	    return NULL;
	dir = name_buf;
    }
    if (file_exist(dir, (char *) NULL)) {
	msg("Directory %s already exists", dir);
	return NULL;
    }
    if (mkdir(dir, 0755)) {
	msg("Cannot make %s", dir);
	return NULL;
    }
    return dir;
}

int
change_dir(char *dir, int in_init)
{
    char            dir_buf[FILENAME];

    if (dir == NULL) {
	prompt("\1Directory: \1");
	dir = get_s(last_dir, NONE, NONE, file_completion);
    }
    if (dir == NULL || *dir == NUL)
	return 0;

    strcpy(last_dir, dir);

    if (*dir == '+' || *dir == '~') {
	if (!expand_file_name(dir_buf, dir, 1))
	    return in_init;
	dir = dir_buf;
    }
    if (chdir(dir) == 0) {
	if (!in_init)
	    msg("Directory: %s", dir);
	return 0;
    }
    if (in_init)
	return 1;

    if (errno == EACCES)
	msg("Cannot access directory %s", dir);
    else {
	/* should ask and create directory here */
	msg("Directory not found: %s", dir);
    }

    return 0;
}
