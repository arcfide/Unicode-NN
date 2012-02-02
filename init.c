/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	.nn/init file handling
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "articles.h"
#include "admin.h"
#include "answer.h"
#include "db.h"
#include "execute.h"
#include "folder.h"
#include "group.h"
#include "hostname.h"
#include "init.h"
#include "keymap.h"
#include "kill.h"
#include "macro.h"
#include "menu.h"
#include "newsrc.h"
#include "nn.h"
#include "printconf.h"
#include "save.h"
#include "sequence.h"
#include "sort.h"
#include "nn_term.h"
#include "variable.h"

#ifdef USE_MALLOC_H
#include <malloc.h>
#endif

/* init.c */

static char    *strip_str(register char *cmd);
static int      split_command(register char *cmd);
static char    *argv(int i);
static int      is_sequence(char *cmd);
static void     load_init_file(char *name, FILE ** seq_hook_ptr, int only_seq);
static void     print_debug_info(void);
static void     print_command(char *str);
static int      do_show(char *table, int mode_arg);
static int      do_map(FILE * initf);
static void     parse_on_to_end(FILE * f);


extern char    *help_directory, *lib_directory;

int             in_init = 0;	/* true when parsing init file */
int             alt_cmd_key;	/* K_ when parse_command returns AC_KEYCMD */

long            initial_memory_break;	/* for :debug statistics */

int             first_time_user = 0;

static int      init_err = 0;	/* errors in init file */

extern char    *term_name;
extern FILE    *loc_seq_hook, *glob_seq_hook;
extern int      list_offset;
extern int      in_menu_mode;
extern char    *dflt_enter_macro;
/* extern char    *dflt_exit_macro; */
extern int      terminal_speed, slow_speed;
extern char    *pname;
extern char    *start_up_macro;
extern char    *newsrc_file;
extern int      in_menu_mode;
extern int      do_kill_handling;
extern char     printer[];
extern char    *mail_box;



void
init_message(char *fmt,...)
{
    va_list         ap;

    va_start(ap, fmt);

    if (in_init) {
	visual_off();
	printf("init error: ");
	vprintf(fmt, ap);
	putchar(NL);
	init_err++;
    } else
	vmsg(fmt, ap);

    va_end(ap);
}


#define MAXARG 10

static char    *argvec[MAXARG + 2];
static int      argc;

static char    *
strip_str(register char *cmd)
{
    if (cmd == NULL)
	return cmd;

    while (*cmd && isascii(*cmd) && isspace(*cmd))
	cmd++;
    if (*cmd == NUL || *cmd == NL)
	return NULL;

    return cmd;
}


static int
split_command(register char *cmd)
{
    /* split command string */

    for (argc = 0; argc < MAXARG + 2; argc++)
	argvec[argc] = NULL;
strip_more:
    if ((cmd = strip_str(cmd)) == NULL || *cmd == '#')
	return 0;
    if (*cmd == ':') {
	cmd++;
	goto strip_more;
    }
    argc = 0;
    argvec[0] = cmd;

    return 1;
}

static char    *
argv(int i)
{
    register char  *cmd;

    if (i > MAXARG)
	return NULL;

    if (argc <= i) {
	if (argvec[argc]) {
	    cmd = argvec[argc];
	    while (argc <= i) {
		while (*cmd && (!isascii(*cmd) || !isspace(*cmd)))
		    cmd++;
		if (*cmd == NUL) {
		    argc = MAXARG;
		    break;
		}
		*cmd++ = NUL;
		if ((cmd = strip_str(cmd)) == NULL) {
		    argc = MAXARG;
		    break;
		}
		argvec[++argc] = cmd;
	    }
	} else {
	    argc = MAXARG;
	}
    }
    return argvec[i];
}

static int
is_sequence(char *cmd)
{
    if (!split_command(cmd))
	return 0;
    if ((cmd = argv(0)) == NULL)
	return 0;
    return strcmp(cmd, "sequence") == 0;
}

#define START_SEQUENCE 555
#define CHAIN_FILE 556		/* chain file */
#define STOP_FILE 557		/* stop */

static void
load_init_file(char *name, FILE ** seq_hook_ptr, int only_seq)
{
    FILE           *init;
    char            cmdbuf[1024], *cmd, *term;

    /* use cmdbuf temporarily (to handle @ expansion) */
    for (cmd = cmdbuf; *name; name++)
	if (*name == '@') {
	    term = term_name;
	    while (term && *term)
		*cmd++ = *term++;
	} else
	    *cmd++ = *name;
    *cmd = NUL;
    name = cmdbuf;

chain_file:
    if (strchr(name, '/') == NULL)
	name = relative(nn_directory, name);

    init = open_file(name, OPEN_READ);
    if (init == NULL)
	return;

    while (fgets_multi(cmdbuf, 1024, init)) {
	if (only_seq) {
	    if (!is_sequence(cmdbuf))
		continue;
	    *seq_hook_ptr = init;
	    return;
	}
	/* we use AC_REDRAW to avoid !-commands clear the screen */
	switch (parse_command(cmdbuf, AC_REDRAW, init)) {
	    case CHAIN_FILE:
		fclose(init);
		name = argvec[argc];	/* ARGTAIL */
		if (name == NULL)
		    return;
		goto chain_file;

	    case STOP_FILE:
		fclose(init);
		return;

	    case START_SEQUENCE:
		if (seq_hook_ptr) {
		    *seq_hook_ptr = init;
		    return;	/* no close !! */
		} else {
		    init_message("load file contains 'sequence'");
		    fclose(init);
		    return;
		}
	}
    }

    fclose(init);
}

static char     dflt_init_files[] = ",init";

void
visit_init_file(int only_seq, char *first_arg)
{
    char           *next_arg;

    in_init = 1;
    load_init_file(relative(lib_directory, "setup"), (FILE **) NULL, 0);
    in_init = 0;

    if (first_arg && strncmp(first_arg, "-I", 2) == 0) {
	if (first_arg[2] == NUL)
	    return;
	first_arg += 2;
    } else
	first_arg = dflt_init_files;

    in_init = 1;
    while (first_arg) {
	next_arg = strchr(first_arg, ',');
	if (next_arg)
	    *next_arg++ = NUL;

	if (*first_arg == NUL) {
	    if (glob_seq_hook == NULL)
		load_init_file(relative(lib_directory, "init"), &glob_seq_hook, only_seq);
	} else {
	    if (loc_seq_hook != NULL) {
		fclose(loc_seq_hook);
		loc_seq_hook = NULL;
	    }
	    load_init_file(first_arg, &loc_seq_hook, only_seq);
	}
	first_arg = next_arg;
    }

    if (init_err)
	nn_exit(1);
    in_init = 0;
}


/*
 * parse a command (also :-commands)
 */

static char    *sw_string;
static int      sw_loop_once;

#define	SWITCH(str)	\
    for (sw_string = str, sw_loop_once = 1; --sw_loop_once == 0; )

#define CASE(str)	\
    if (strcmp(sw_string, str) == 0)


#define ARG(i, str)	(argv(i) && strcmp(argv(i), str) == 0)
#define ARGVAL(i)	atol(argv(i))
#define ARGTAIL		argvec[argc]

static struct alt_commands {
    char           *alt_name;
    int             alt_len;
    int             alt_type;
}               alt_commands[] = {

    "admin", 5, 0,
    "bug", 3, 0,
    "cd", 2, 1,
    "compile", 7, 0,
    "coredump", 8, 0,
    "cost", 4, 0,
    "decode", 6, 0,
    "define", 6, 0,
    "help", 4, 2,
    "load", 4, 0,
    "local", 5, 3,
    "lock", 4, 3,
    "make", 4, -1,
    "make map ", 9, -2,
    "man", 3, 0,
    "map", 3, 4,
    "mkdir", 5, 1,
    "motd", 4, 0,
    "patch", 5, 0,		/* QUICK HACK */
    "post", 4, 0,		/* QUICK HACK */
    "print", 5, -3,		/* QUICK HACK */
    "print-variables", 15, 0,
    "pwd", 3, 0,
    "rmail", 5, 0,
    "set", 3, 3,
    "show", 4, -1,
    "show config", 11, 0,
    "show groups", 11, -1,
    "show groups all", 15, 0,
    "show groups subscr", 18, 0,
    "show groups total", 17, 0,
    "show groups unsub", 17, 0,
    "show kill", 9, 0,
    "show map", 8, -1,
    "show map #", 10, 0,
    "show map key", 12, 0,
    "show map menu", 13, 0,
    "show map show", 13, 0,
    "show rc ", 8, 0,
    "sort", 4, -1,
    "sort arrival", 12, 0,
    "sort date", 9, 0,
    "sort lexical", 12, 0,
    "sort sender", 11, 0,
    "sort subject", 12, 0,
    "toggle", 6, 3,
    "unread", 6, 0,
    "unset", 5, 3,
    "unshar", 6, 0,		/* QUICK HACK */
    NULL, 0, 0
};

int
alt_completion(char *buf, int index)
{
    static char    *head, *tail = NULL, buffer[FILENAME];
    static int      len;
    static struct alt_commands *alt, *help_alt;
    static fct_type other_compl;
    int             temp;
    register char  *p, *q;

    if (other_compl) {
	temp = CALL(other_compl) (buf, index);
	if (index == 0 && temp == 1 && tail)
	    strcpy(tail, head);
	if (index < 0 || (index == 0 && temp == 0)) {
	    other_compl = NULL;
	    list_offset = 0;
	}
	return temp;
    }
    if (index < 0)
	return 0;

    if (buf) {
	if (index >= 1 && buf[0] == '!')
	    return -1;		/* :! is special */

	head = buf;
	tail = buf + index;
	alt = help_alt = alt_commands;
	len = tail - head;
	other_compl = NULL;

	for (; alt->alt_name; alt++) {
	    if (len <= alt->alt_len)
		continue;
	    if (head[alt->alt_len] != SP) {
		if (alt->alt_type != -2)
		    continue;
		if (strncmp(alt->alt_name, head, alt->alt_len))
		    continue;
		return -1;
	    }
	    index = strncmp(alt->alt_name, head, alt->alt_len);
	    if (index < 0)
		continue;
	    if (index > 0)
		break;

	    if (alt->alt_type < 0) {
		if (len > alt->alt_len)
		    continue;
		break;
	    }
	    if (alt->alt_type == 0)
		return -1;	/* cannot be further compl */

	    head += alt->alt_len;
	    while (*head && *head == SP)
		head++;
	    len = tail - head;
	    temp = -1;

	    switch (alt->alt_type) {
		case 1:
		    other_compl = file_completion;
		    tail = NULL;
		    temp = file_completion(head, len);
		    break;

		case 2:
		    other_compl = file_completion;
		    sprintf(buffer, "%s.%s",
			    relative(help_directory, "help"), head);
		    len = strlen(buffer);
		    head = buffer + len;
		    list_offset = 5;
		    temp = file_completion(buffer, len);
		    break;

		case 3:
		    /* [set ]variable[ value] */
		    for (p = head; *p;)
			if (*p++ == SP)
			    return -1;
		    other_compl = var_completion;
		    var_compl_opts((int) (tail - buf));
		    tail = NULL;
		    temp = var_completion(head, len);
		    break;

		case 4:
		    /* [map XXX ]Y command[ N] */
		    if (*head == '#')
			return -1;
		    for (p = head, temp = 0; *p;)
			if (*p++ == SP) {
			    while (*p && *p == SP)
				p++;
			    head = p;
			    temp++;
			}
		    if (temp == 0)
			other_compl = keymap_completion;
		    else if (temp == 2)
			other_compl = cmd_completion;
		    else
			return -1;

		    tail = NULL;
		    len = p - head;
		    temp = CALL(other_compl) (head, len);
		    break;
	    }
	    if (temp <= 0)
		other_compl = NULL;
	    return temp;
	}

	alt = alt_commands;
	return 1;
    }
    if (index) {
	list_completion((char *) NULL);
	if (help_alt->alt_name == NULL)
	    help_alt = alt_commands;
	list_offset = 0;
	if ((p = strrchr(head, ' ')))
	    list_offset = p - head;

	while (help_alt->alt_name) {
	    if (len > help_alt->alt_len ||
		(index = strncmp(help_alt->alt_name, head, len)) < 0) {
		help_alt++;
		continue;
	    }
	    if (index > 0) {
		help_alt = alt_commands;
		break;
	    }
	    p = help_alt->alt_name;
	    if (list_completion(p) == 0)
		break;
	    temp = help_alt->alt_len;

	    if (help_alt->alt_type == -3) {
		help_alt++;
		continue;
	    }
	    do
		help_alt++;
	    while (((q = help_alt->alt_name)) && help_alt->alt_len > temp &&
		   strncmp(p, q, temp) == 0);
	}
	fl;
	list_offset = 0;
	return 1;
    }
    for (; alt->alt_name; alt++) {
	if (len == 0)
	    index = 0;
	else
	    index = strncmp(alt->alt_name, head, len);
	if (index < 0)
	    continue;
	if (index > 0)
	    break;

	p = alt->alt_name;
	sprintf(tail, "%s%s", p + len, alt->alt_type <= -2 ? "" : " ");
	temp = alt->alt_len;

	if (alt->alt_type == -3) {
	    alt++;
	    return 1;
	}
	do
	    alt++;
	while (((q = alt->alt_name)) && alt->alt_len > temp &&
	       strncmp(p, q, temp) == 0);

	return 1;
    }

    cmd_completion(head, len);
    if ((temp = cmd_completion((char *) NULL, 0))) {
	other_compl = cmd_completion;
	tail = NULL;
    }
    return temp;
}

static void
print_debug_info(void)
{

#ifdef USE_MALLOC_H
    struct mallinfo mallinfo(), mi;
#endif

    static long     prev_mem = 0;
    long            cur_mem;

    clrdisp();
    tprintf("group=%s, nart=%ld\n\r", current_group->group_name, n_articles);

    cur_mem = (((long) sbrk(0)) - initial_memory_break) / 1024;

    tprintf("\nMemory usage: %ldk, previous: %ldk, change: %ldk\n\r",
	    cur_mem, prev_mem, cur_mem - prev_mem);
    prev_mem = cur_mem;

#ifdef USE_MALLOC_H
    mi = mallinfo();
    tprintf("\nMalloc info.  Total allocation: %d\n\r", mi.arena);
    tprintf("Ordinary blocks: %d, space in use: %d, space free: %d\n\r",
	    mi.ordblks, mi.uordblks, mi.fordblks);
    tprintf("Small blocks: %d, space in use: %d, space free: %d\n\r",
	    mi.smblks, mi.usmblks, mi.fsmblks);
    tprintf("Holding blocks: %d, space in headers: %d\n\r",
	    mi.hblks, mi.hblkhd);
#endif

    any_key(0);
}

static void
print_command(char *str)
{
    char          **av;
    char            buf[1024];

    if (!in_init) {
	msg(str);
	return;
    }
    buf[0] = NUL;
    for (av = argvec; *av; av++) {
	strcat(buf, " ");
	strcat(buf, *av);
    }
    init_message("%s: %s", str, buf);
}


static int
do_show(char *table, int mode_arg)
{
    register group_header *gh;
    int             ret = 1;

    if (in_init || table == NULL)
	return 0;

    no_raw();

    SWITCH(table) {

	CASE("config") {
	    clrdisp();
	    print_config(stdout);
	    any_key(0);
	    break;
	}

	CASE("kill") {
	    clrdisp();
	    dump_kill_list();
	    break;
	}

	CASE("groups") {

	    clrdisp();
	    if ARG
		(mode_arg, "all")
		    group_overview(1);
	    else if ARG
		(mode_arg, "total")
		    group_overview(2);
	    else if ARG
		(mode_arg, "unsub")
		    group_overview(3);
	    else if ARG
		(mode_arg, "subscr")
		    group_overview(4);
	    else if ARG
		(mode_arg, "sequence")
		    group_overview(-1);
	    else
		group_overview(0);

	    break;
	}

	CASE("map") {
	    char           *name;

	    if ((name = argv(mode_arg)) == NULL)
		name = in_menu_mode ? "menu" : "show";

	    if (name[0] == '#') {
		clrdisp();
		dump_multi_keys();
		break;
	    }
	    SWITCH(name) {

		CASE("key") {
		    clrdisp();
		    dump_global_map();
		    break;
		}
		if (dump_key_map(name) >= 0)
		    break;

		init_message("unknown map '%s'", argv(mode_arg));
		ret = 0;
		/* break;	goto err */
	    }

	    break;
	}

	CASE("rc") {
	    if (argv(2)) {
		gh = lookup(argv(2));
		if (gh == NULL) {
		    msg("Unknown: %s", argv(2));
		    break;
		}
	    } else
		gh = current_group;
	    if (gh->group_flag & G_FAKED)
		break;

	    clrdisp();

	    tprintf("Available: %ld - %ld  (unread %ld)\n\n\r",
		(long) (gh->first_db_article), (long) (gh->last_db_article),
		    (long) (gh->unread_count));
	    tprintf(".newsrc:\n\r>%s\r<%s\n\rselect:\n\r>%s\r<%s\n\r",
		    gh->newsrc_line ? gh->newsrc_line : "(null)\n",
		    gh->newsrc_orig == gh->newsrc_line ? "(same)\n" :
		    gh->newsrc_orig ? gh->newsrc_orig : "(null)\n",
		    gh->select_line ? gh->select_line : "(null)\n",
		    gh->select_orig == gh->select_line ? "(same)\n" :
		    gh->select_orig ? gh->select_orig : "(null)\n");
	    any_key(0);
	    break;
	}

	init_message("unknown table '%s'", table);
	ret = 0;
	/* break; */
	/* goto err; */
	/* NOTREACHED */
    }

    nn_raw();
    return ret;
/*
err:
    nn_raw();
    return 0;
*/
}


static int
do_map(FILE * initf)
{
    int             code, map_menu, map_show, must_redraw = 0;
    key_type        bind_to;
    register struct key_map_def *map_def;
    register int   *map;

    code = lookup_keymap(argv(1));
    if (code < 0) {
	print_command("unknown map");
	goto out;
    }
    map_def = &keymaps[code];

    if (map_def->km_flag & K_GLOBAL_KEY_MAP) {
	if (argv(3) == NULL)
	    goto mac_err;
	if (argv(2) == NULL) {
	    dump_global_map();
	    return 1;
	}
	global_key_map[parse_key(argv(2))] = parse_key(argv(3));
	return 0;
    }
    if (map_def->km_flag & K_MULTI_KEY_MAP) {
	key_type        multi_buffer[16], *mb;
	int             i;

	if (argv(1)[1] == NUL) {
	    dump_multi_keys();
	    return 1;
	}
	if (isdigit(argv(1)[1]))
	    bind_to = K_function(argv(1)[1] - '0');
	else {
	    bind_to = parse_key(argv(1) + 1);
	    if (bind_to < K_up_arrow || bind_to > K_right_arrow)
		goto mac_err;
	}

	for (i = 2, mb = multi_buffer; argv(i); i++)
	    *mb++ = parse_key(argv(i));
	*mb = NUL;

	enter_multi_key(bind_to, (key_type *) copy_str((char *) multi_buffer));
	return 0;
    }
    code = K_UNBOUND;
    map = map_def->km_map;
    map_show = map_def->km_flag & K_BOTH_MAPS;
    map_menu = map_def->km_flag & K_BIND_ORIG;

    if (ARG(3, "(")) {
	code = (int) m_define("-2", initf);
	must_redraw = 1;
	if (code == K_UNBOUND)
	    goto mac_err;
    }
    if (code == K_UNBOUND && argv(3))
	code = lookup_command(argv(3), map_def->km_flag & (K_ONLY_MENU | K_ONLY_MORE));

    switch (code) {
	case K_INVALID - 1:
	    init_message("Cannot bind '%s' in '%s' map", argv(3), argv(1));
	    goto out;

	case K_EQUAL_KEY:
	    if (argv(4) == NULL)
		goto mac_err;
	    code = map[parse_key(argv(4))];
	    break;

	case K_MACRO:
	case K_ARTICLE_ID:
	    if (argv(4) == NULL)
		goto mac_err;
	    code |= atoi(argv(4));
	    break;

	case K_PREFIX_KEY:
	    if (argv(4) == NULL)
		goto mac_err;
	    code = lookup_keymap(argv(4));
	    if (code < 0) {
		print_command("unknown prefix map");
		goto out;
	    }
	    code |= K_PREFIX_KEY;
	    break;
    }

    if (code == K_INVALID) {
	init_message("unknown key command: %s", argv(3));
	goto out;
    }
    bind_to = parse_key(argv(2));
    if (map_menu) {
	if (code & K_MACRO && orig_menu_map[bind_to] == 0)
	    orig_menu_map[bind_to] = map[bind_to];
    }
    map[bind_to] = code;
    if (map_show)
	more_key_map[bind_to] = code;
    goto out;

mac_err:
    print_command("map argument missing");
out:
    return must_redraw;
}

static void
parse_on_to_end(FILE * f)
{
    register char  *cp;
    char            buf[1024];
    static char    *last_cmd_res = NULL;
    int             i;
    int             err_flag = 0;

    if (ARGTAIL == NULL || *ARGTAIL == NUL)
	goto on_err;

    cp = NULL;
    switch (*ARGTAIL) {
	case '#':		/* on #... end: skipped (+hack for else) */
	    goto skip_to_end;

	case '`':		/* on `shell command` str1 str2 ... */
	    {
		FILE           *p;
		char           *cmd = ARGTAIL + 1, *t;

		if ((cp = strrchr(cmd, '`')) == NULL)
		    goto syntax_err;
		if ((t = strip_str(cp + 1)) == NULL)
		    goto syntax_err;
		*cp = NUL;
		ARGTAIL = t;

		if (cmd[0]) {
		    buf[0] = NUL;
		    if ((p = popen(cmd, "r"))) {
			if (fgets(buf, 1024, p))
			    buf[strlen(buf) - 1] = NUL;
			pclose(p);
		    }
		    if (last_cmd_res != NULL && strcmp(last_cmd_res, buf)) {
			free(last_cmd_res);
			last_cmd_res = NULL;
		    }
		    if (buf[0] == NUL)
			goto skip_to_end;
		    last_cmd_res = copy_str(buf);
		}
		for (i = 1; argv(i) != NULL; i++)
		    if (strcmp(argv(i), last_cmd_res) == 0)
			return;
	    }
	    goto skip_to_end;

	case '$':		/* on $VAR [ a b c ... ] */
	    cp = argv(1);
	    if ((cp = getenv(cp + 1)) == NULL)
		goto skip_to_end;
	    if (ARGTAIL == NULL)
		return;
	    for (i = 2; argv(i) != NULL; i++)
		if (strcmp(argv(i), cp) == 0)
		    return;
	    goto skip_to_end;

	case '!':		/* on !shell-command */
	    cp = ARGTAIL + 1;
	    break;

	case '[':		/* on [ test ] */
	    cp = ARGTAIL + strlen(ARGTAIL) - 1;
	    if (*cp != ']')
		goto syntax_err;
	    cp = ARGTAIL;
	    break;

	default:
	    break;
    }

    if (cp) {
	if (run_shell(cp, -2, 1) == 0)
	    return;
	goto skip_to_end;
    }
    if (argv(1) == NULL)
	goto on_err;

    SWITCH(argv(1)) {

	CASE("entry") {
	    group_header   *gh;
	    char           *macro;
	    int             ii;

	    macro = parse_enter_macro(f, NL);
	    if (ARGTAIL) {
		for (ii = 2; argv(ii); ii++) {
		    start_group_search(argv(ii));
		    while ((gh = get_group_search()))
			gh->enter_macro = macro;
		}
	    } else
		dflt_enter_macro = macro;
	    return;
	}

/*	CASE( "exit" ) {
	    dflt_exit_macro = parse_enter_macro(f, NL);
	    return;
	}
*/
	CASE("slow") {
	    if (terminal_speed <= (slow_speed / 10))
		return;
	    break;
	}

	CASE("fast") {
	    if (terminal_speed > (slow_speed / 10))
		return;
	    break;
	}

	CASE("term") {
	    int             ii;

	    for (ii = 2; argv(ii) != NULL; ii++)
		if (strcmp(argv(ii), term_name) == 0)
		    return;
	    break;
	}

	CASE("host") {
	    char            local_host[100];
	    int             ii;

	    nn_gethostname(local_host, 100);
	    for (ii = 2; argv(ii) != NULL; ii++)
		if (strcmp(argv(ii), local_host) == 0)
		    return;
	    break;
	}

	CASE("program") {
	    char           *pname1;
	    int             ii;

	    for (pname1 = pname; *pname1 == 'n'; pname1++);

	    for (ii = 2; argv(ii) != NULL; ii++) {
		if (strcmp(argv(ii), pname) == 0)
		    return;
		if (pname1[0] && strcmp(argv(ii), pname1) == 0)
		    return;
	    }
	    break;
	}

	CASE("start-up") {
	    start_up_macro = parse_enter_macro(f, NL);
	    return;
	}

	CASE("first-use") {
	    if (!first_time_user)
		break;
	    if (argv(2) == NULL || ARG(2, "all"))
		return;
	    if (newsrc_file == NULL)	/* == code from visit_rc_file == */
		newsrc_file = home_relative(".newsrc");
	    if (ARG(2, "old") && file_exist(newsrc_file, (char *) NULL))
		return;
	    if (ARG(2, "new") && !file_exist(newsrc_file, (char *) NULL))
		return;
	    break;
	}

	err_flag = 1;
/*	goto on_err; */
    }
    if (err_flag == 1)
	goto on_err;

skip_to_end:
    while (fgets_multi(buf, 1024, f)) {
	for (cp = buf; *cp && isascii(*cp) && isspace(*cp); cp++);
	if (*cp != 'e')
	    continue;
	if (strncmp(cp, "end", 3) == 0)
	    return;
	if (strncmp(cp, "else", 4) == 0)
	    return;
    }
    init_message("end missing (on %s)", argv(1));
    return;

on_err:
    init_message("on `what'?");
    return;

syntax_err:
    init_message("syntax error: on %s", ARGTAIL);
}

int
parse_command(char *cmd, int ok_val, FILE * initf)
{

    if (!split_command(cmd))
	return ok_val;

    if (*ARGTAIL == '!') {
	if (ok_val == AC_UNCHANGED) {	/* in macro */
	    if (ARGTAIL[1] == '!')	/* !!cmd => guarantee no output! */
		run_shell(ARGTAIL + 2, -2, 1);
	    else
		run_shell(ARGTAIL + 1, -1, 1);
	    return ok_val;
	}
	if (run_shell(ARGTAIL + 1, ok_val == AC_PROMPT ? 1 : 0, in_init) >= 0) {
	    any_key(0);
	    return AC_REDRAW;
	}
	return ok_val;
    }
    SWITCH(argv(0)) {

	CASE("unset") {
	    if (argv(1) == NULL)
		goto stx_err;

	    if (set_variable(argv(1), 0, (char *) NULL))
		return AC_REDRAW;
	    else
		return ok_val;
	}

	CASE("local") {
	    if (ARGTAIL == NULL)
		goto stx_err;

	    cmd = argv(1);
	    if (!push_variable(cmd))
		return ok_val;

	    if (ARGTAIL && set_variable(cmd, 1, ARGTAIL))
		return AC_REDRAW;
	    else
		return ok_val;
	}

	CASE("set") {
	    if (ARGTAIL == NULL || ARGTAIL[0] == '/') {
		disp_variables(0, ARGTAIL);
		return AC_REDRAW;
	    }
	    cmd = argv(1);	/* get ARGTAIL right */
	    if (cmd != NULL && strcmp(cmd, "all") == 0) {
		disp_variables(1, (char *) NULL);
		return AC_REDRAW;
	    }
	    if (set_variable(cmd, 1, ARGTAIL))
		return AC_REDRAW;
	    else
		return ok_val;
	}

	CASE("toggle") {
	    if (argv(1) == NULL)
		goto stx_err;
	    toggle_variable(argv(1));
	    break;
	}

	CASE("lock") {
	    if (argv(1) == NULL)
		goto stx_err;
	    lock_variable(argv(1));
	    break;
	}

	CASE("define") {
	    if (in_init) {
		if (argv(1) == NULL) {
		    init_message("macro number missing");
		    break;
		}
		m_define(argv(1), initf);
	    } else if (m_define(argv(1), (FILE *) NULL))
		return AC_REDRAW;

	    break;
	}

	CASE("map") {
	    if (argv(2) == NULL) {
		if (do_show("map", 1))
		    return AC_REDRAW;
		break;
	    }
	    if (do_map(initf))
		return AC_REDRAW;
	    break;
	}

	CASE("make") {
	    if (ARG(1, "map") && argv(2)) {
		switch (make_keymap(argv(2))) {
		    case -1:
			init_message("map %s already exists", argv(2));
			break;
		    case -2:
			init_message("cannot make %s: too many maps", argv(2));
			break;
		}
		break;
	    }
	    print_command("invalid make command");
	    break;
	}

	CASE("cd") {
	    if (change_dir(argv(1), in_init))
		init_message("chdir %s FAILED", argv(1));

	    break;
	}

	CASE("clear") {
	    clrdisp();
	    break;
	}

	if (in_init) {

	    CASE("load") {
		if (argv(1))
		    load_init_file(argv(1), (FILE **) NULL, 0);
		break;
	    }

	    CASE("on") {
		parse_on_to_end(initf);
		break;
	    }

	    CASE("else") {
		ARGTAIL = "#";	/* skip to end */
		parse_on_to_end(initf);
		break;
	    }

	    CASE("end") {
		break;
	    }

	    CASE("echo") {
		tprintf("\r%s\n\r", ARGTAIL);
		break;
	    }

	    CASE("error") {
		nn_exitmsg(1, "%s\n", ARGTAIL);
	    }

	    CASE("exit") {
		nn_exit(ARGTAIL != NULL ? atoi(ARGTAIL) : 0);
	    }

	    CASE("chain") {
		return CHAIN_FILE;
	    }

	    CASE("stop") {
		return STOP_FILE;
	    }

	    CASE("sequence") {
		return START_SEQUENCE;
	    }

	    CASE("save-files") {
		parse_save_files(initf);
		break;
	    }

	    print_command("unknown command");
	    break;
	}

	/*
	 * commands only available from : command line
	 */

	if (ok_val != AC_REDRAW) {

	    alt_cmd_key = lookup_command(sw_string,
				  in_menu_mode ? K_ONLY_MENU : K_ONLY_MORE);
	    if (alt_cmd_key > K_INVALID && alt_cmd_key != K_HELP) {
		if (alt_cmd_key == K_MACRO) {
		    if (ARGTAIL == NULL)
			break;
		    alt_cmd_key |= atoi(ARGTAIL);
		}
		return AC_KEYCMD;
	    }
	}
	CASE("load") {
	    clrdisp();
	    in_init = 1;
	    init_err = 0;
	    load_init_file(argv(1) ? argv(1) : "init", (FILE **) NULL, 0);
	    in_init = 0;
	    if (init_err)
		any_key(0);
	    return AC_REDRAW;
	}

	CASE("q") {
	    break;
	}

	CASE("Q") {
	    return AC_QUIT;
	}

	CASE("q!") {
	    if (restore_bak())
		return AC_QUIT;
	    break;
	}

	CASE("x") {
	    update_rc_all(current_group, 0);
	    return AC_QUIT;
	}

	CASE("help") {
	    if (argv(1) == NULL)
		display_help("help");
	    else
		display_help(argv(1));
	    return AC_REDRAW;
	}

	CASE("man") {
	    char           *manual;
	    group_header   *orig_group;

	    manual = relative(help_directory, "Manual");
	    if (!file_exist(manual, "fr")) {
		manual = relative(lib_directory, "Manual");
		if (!file_exist(manual, "fr")) {
		    msg("Online manual is not available");
		    break;
		}
	    }
	    orig_group = current_group;
	    folder_menu(manual, 1);
	    init_group(orig_group);

	    return AC_REDRAW;
	}

	CASE("motd") {
	    if (display_motd(0))
		return AC_REDRAW;
	    msg("no motd file");
	    break;
	}

	CASE("sort") {
	    if (argv(1) == NULL)
		sort_articles(-1);
	    else if (ARG(1, "no") || ARG(1, "arrival"))
		sort_articles(0);
	    else if ARG
		(1, "subject")
		    sort_articles(1);
	    else if ARG
		(1, "lexical")
		    sort_articles(2);
	    else if (ARG(1, "date") || ARG(1, "age"))
		sort_articles(3);
	    else if (ARG(1, "sender") || ARG(1, "from"))
		sort_articles(4);
	    else {
		msg("Unknown sort mode '%s'", argv(1));
		break;
	    }

	    return AC_REORDER;
	}

	CASE("unread") {
	    group_header   *gh;
	    int             ix;

	    if (argv(1) && (gh = lookup(argv(1))) != NULL)
		ix = 2;
	    else {
		ix = 1;
		gh = current_group;
	    }

	    if (gh == current_group)
		return AC_REENTER_GROUP;

	    if (argv(ix)) {
		if (!restore_rc(gh, gh->last_db_article - (article_number) ARGVAL(ix)))
		    break;
	    } else if (!restore_unread(gh))
		break;
	    break;
	}

	CASE("dump") {
	    if (do_show(argv(1), 2))
		return AC_REDRAW;
	    break;
	}

	CASE("show") {
	    if (do_show(argv(1), 2))
		return AC_REDRAW;
	    break;
	}

	CASE("compile") {
	    clrdisp();
	    rm_kill_file();
	    free_kill_entries();
	    do_kill_handling = init_kill() && do_kill_handling;
	    return AC_REDRAW;
	}

	CASE("print-variables") {
	    FILE           *p;
	    if ((p = popen(ARGTAIL ? ARGTAIL : printer, "w"))) {
		print_variable_config(p, 1);
		pclose(p);
		msg("Variables printed");
	    }
	    break;
	}

	CASE("pwd") {
	    FILE           *p = popen("exec pwd", "r");
	    char            dir[FILENAME];
	    if (p) {
		if (fgets(dir, FILENAME, p)) {
		    dir[strlen(dir) - 1] = NUL;
		    msg("%s", dir);
		}
		pclose(p);
	    }
	    break;
	}

	CASE("rmail") {
	    group_header   *orig_group;
	    int             rv;

	    if (mail_box == NULL) {
		msg("'mail' path not defined");
		break;
	    }
	    orig_group = current_group;
	    rv = folder_menu(mail_box, 2);
	    init_group(orig_group);

	    return rv == ME_NO_REDRAW ? ok_val : AC_REDRAW;
	}

	CASE("mkdir") {
	    char           *dir;
	    char            name_buf[FILENAME];

	    if ((dir = run_mkdir(argv(1), name_buf))) {
		prompt("Change to %s", dir);
		if (yes(0))
		    change_dir(dir, 0);
	    }
	    break;
	}

	CASE("sh") {
	    suspend_nn();
	    s_redraw = 0;
	    return AC_REDRAW;
	}

	CASE("admin") {
	    group_header   *cur_group;

	    cur_group = current_group;
	    no_raw();
	    clrdisp();
	    tprintf("\n\n\n\rADMINISTRATION MODE\r\n\n\n");
	    admin_mode((char *) NULL);
	    clrdisp();
	    nn_raw();
	    init_group(cur_group);
	    return AC_REDRAW;
	}

	CASE("cost") {

#ifdef ACCOUNTING
	    gotoxy(0, Lines - 1);
	    clrline();
	    account('C', 1);
#else
	    msg("No accounting");
#endif

	    break;
	}

	CASE("bug") {
	    if (answer((article_header *) NULL, K_BUG_REPORT, 0))
		return AC_REDRAW;
	    break;
	}

	CASE("debug") {
	    print_debug_info();
	    return AC_REDRAW;
	}

	CASE("coredump") {
	    unset_raw();
	    abort();
	}

	msg("unknown command: \"%s\"", argv(0));
    }

    return ok_val;

stx_err:
    print_command("syntax error");
    return ok_val;
}


void
display_help(char *subject)
{
    char            file[FILENAME];

    strcpy(file, "help.");
    strcpy(file + 5, subject);

    display_file(file, CLEAR_DISPLAY | CONFIRMATION);
}
