/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Macro parsing and execution.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "db.h"
#include "init.h"
#include "keymap.h"
#include "macro.h"
#include "menu.h"
#include "nn_term.h"
#include "variable.h"

/* macro.c */

static void     m_error(char *fmt, char *arg);
static void     m_new(int t);
static int      parse_word(char *w);
static int      parse_line(char *lp);
static struct macro *m_call(int who);
static void     macro_dbg(void);


int             in_menu_mode = 0;
int             get_from_macro = 0;
int             macro_debug = 0;
char           *dflt_enter_macro = NULL;
char           *start_up_macro = NULL;

#ifdef M_BREAK
#undef M_BREAK
#endif

#define M_DUMMY		0	/* do nothing (end of branch)	 */

#define	M_COMMAND	1	/* a command (get_c()) 		 */
#define M_KEY		2	/* a key stroke (get_c()) 	 */
#define M_STRING	3	/* a string (get_s())		 */

#define M_INPUT		4	/* take input from keyboard (get_c/get_s) */

#define M_YES		5	/* answer yes to confirmation	 */
#define M_NO		6	/* answer no to confirmation and break */
 /* -- if neither are present, take input */

#define M_PUTS		7	/* puts "..."			 */
#define M_PROMPT	8	/* prompt(...)	 		 */
#define M_ECHO		9	/* msg(...)			 */

#define M_IS_MENU	10	/* in menu mode ?		 */
#define M_IS_SHOW	11	/* in reading mode ?		 */
#define M_IS_GROUP	12	/* are we in a news group ?	 */
#define M_IS_FOLDER	13	/* are we in a folder ?		 */
#define M_CONFIRM	14	/* ask for confirmation to procede	 */
#define M_REJECT	15	/* ask for !confirmation to procede	 */
#define M_VARTEST	16	/* test value of variable	 */
#define M_BREAK		17	/* exit from macroes		 */
#define	M_RETURN	18	/* return from this macro	 */

#define	M_SET_COMMAND	19	/* set/unset command		 */


struct macro {
    int             m_type;	/* entry type */
    union {
	int             mu_int;	/* command or char */
	char           *mu_string;	/* string for get_s */
	struct macro   *mu_branch;	/* false conditional */
    }               m_value;
    struct macro   *m_next;	/* next macro element */
};

#define m_int		m_value.mu_int
#define m_string	m_value.mu_string
#define m_branch	m_value.mu_branch

#define NUM_MACRO 101		/* numbered macros */
#define ANON_MACRO 100		/* anonymous macroes */
#define NMACRO (NUM_MACRO + ANON_MACRO)

#define MSTACK 5		/* max nesting level */

static struct macro *macro[NMACRO + 1];	/* macro table */
 /* the extra slot is for entry macroes */

static struct macro *mstack[MSTACK];	/* macro stack */
static int      cstack[MSTACK + 1];
static int      m_level = 0;

static struct macro *m = NULL;	/* current macro */
static int      no_advance = 0;
static int      changed_prompt = 0;

static int      cur_m;

#define MERROR ((struct macro *)1)

static void
m_error(char *fmt, char *arg)
{
    char            buf[80];

    if (arg) {
	sprintf(buf, fmt, arg);
	fmt = buf;
    }
    init_message("Error in macro %d: %s", cur_m, fmt);
}

void
init_macro(void)
{
    int             n;

    for (n = 0; n <= NMACRO; n++)
	macro[n] = NULL;
}

static void
m_new(int t)
{
    struct macro   *m1;

    m1 = newobj(struct macro, 1);

    if (m == NULL)
	m = macro[cur_m] = m1;
    else {
	m->m_next = m1;
	m = m1;
    }
    m->m_type = t;
    m->m_next = NULL;
}


/*
 *	Define macro "id" reading from file f until "end"
 *
 *	Macro definition syntax:
 *		define <id>
 *		   <body>
 *		end
 *
 *	Id string interpretation:
 *	NULL 		use next free numbered macro
 *			Return: pointer to macro
 *	"nnn" nnn>=0	Numbered macro nnn
 *			Return: pointer to macro
 *	"-1"		entry macro
 *			Return: pointer to macro
 *	"-2"		anonymous macro
 *			Return: K_MACRO code or K_UNBOUND on error
 */

static int      initial_set_commands;

static int
parse_word(char *w)
{
    int             cmd;
    register struct macro *m1;

    if (m && m->m_type == M_COMMAND && m->m_int == (GETC_COMMAND | K_MACRO)) {
	if (isdigit(*w)) {
	    m->m_int |= atoi(w);
	    goto ok;
	}
	m_error("macro number missing", (char *) NULL);
	return 1;
    }
    if (*w == '"') {
	if (m == NULL || (m->m_type != M_PROMPT && m->m_type != M_ECHO && m->m_type != M_PUTS))
	    m_new(M_STRING);
	m->m_string = copy_str(w + 1);
	goto ok;
    }
    if (*w == '\'') {
	m_new(M_KEY);
	m->m_int = parse_key(w + 1);
	goto ok;
    }
    if (*w == '?') {
	if (strchr(w, '=')) {
	    m->m_type = M_VARTEST;
	    m1 = m;
	    m_new(M_DUMMY);
	    m->m_branch = m1->m_branch;
	    m1->m_string = copy_str(w + 1);
	    goto ok;
	}
	switch (w[1]) {
	    case 'f':		/* ?folder */
		cmd = M_IS_FOLDER;
		break;
	    case 'g':		/* ?group */
		cmd = M_IS_GROUP;
		break;
	    case 'm':		/* ?menu */
		cmd = M_IS_MENU;
		break;
	    case 'n':		/* ?no */
		cmd = M_REJECT;
		break;
	    case 's':		/* ?show */
		cmd = M_IS_SHOW;
		break;
	    case 'y':		/* ?yes */
		cmd = M_CONFIRM;
		break;
	    default:
		m_error("unknown conditional %s", w - 1);
		return 1;
	}
	m->m_type = cmd;
	goto ok;
    }
    if ((cmd = lookup_command(w, (K_ONLY_MENU | K_ONLY_MORE))) > K_INVALID) {
	m_new(M_COMMAND);
	m->m_int = GETC_COMMAND | cmd;
	goto ok;
    }
    if (strcmp(w, "prompt") == 0) {
	m_new(M_PROMPT);
	m->m_string = "?";
	goto ok;
    }
    if (strcmp(w, "echo") == 0) {
	m_new(M_ECHO);
	m->m_string = "ups";
	goto ok;
    }
    if (strcmp(w, "puts") == 0) {
	m_new(M_PUTS);
	m->m_string = "";
	goto ok;
    }
    if (strcmp(w, "input") == 0) {
	m_new(M_INPUT);
	goto ok;
    }
    if (strcmp(w, "yes") == 0) {
	m_new(M_YES);
	goto ok;
    }
    if (strcmp(w, "no") == 0) {
	m_new(M_NO);
	goto ok;
    }
    if (strcmp(w, "break") == 0) {
	m_new(M_BREAK);
	goto ok;
    }
    if (strcmp(w, "return") == 0) {
	m_new(M_RETURN);
	goto ok;
    }
    m_error("Unknown word >>%s<<", w);
    return 1;

ok:
    return 0;
}

static int
parse_line(char *lp)
{
    char           *word;
    struct macro   *m1, *branch = NULL;

    while (*lp) {
	if (*lp == '#')
	    break;

	if (*lp == ':') {
	    lp++;
	    if (initial_set_commands) {

#ifdef REL_640_COMPAT
		if (strncmp(lp, "local", 5) == 0 ||
		    strncmp(lp, "set", 3) == 0 ||
		    strncmp(lp, "unset", 5) == 0) {
#else
		if (lp[0] == ':')
		    lp++;
		else {
#endif

		    m_new(M_SET_COMMAND);
		    m->m_string = copy_str(lp);
		    break;
		}
		initial_set_commands = 0;
	    }
	    m_new(M_COMMAND);
	    m->m_int = GETC_COMMAND | K_EXTENDED_CMD;
	    m_new(M_STRING);
	    m->m_string = copy_str(lp);
	    break;
	}
	initial_set_commands = 0;

	if (*lp == '?') {
	    m_new(M_IS_MENU);
	    if (branch == NULL) {
		m1 = m;
		m_new(M_DUMMY);
		branch = m;
		m = m1;
	    }
	    m->m_branch = branch;
	}
	word = lp;
	if (*lp == '"') {
	    do
		lp++;
	    while (*lp && *lp != '"');
	} else if (*lp == '\'') {
	    do
		lp++;
	    while (*lp && *lp != '\'');
	} else
	    while (*lp && !isspace(*lp))
		lp++;
	if (*lp) {
	    *lp++ = NUL;
	    while (*lp && isspace(*lp))
		lp++;
	}
	if (parse_word(word))
	    return 1;
    }

    if (branch) {
	m->m_next = branch;
	m = branch;
    }
    return 0;
}

char           *
m_define(char *id, FILE * f)
{
    char            line[1024], *lp, skip;
    int             type = 0;

    if (id) {
	cur_m = atoi(id);
	if (cur_m == -1) {
	    cur_m = NMACRO;	/* special slot for this purpose */
	} else if (cur_m == -2) {
	    for (cur_m = NUM_MACRO; cur_m < NMACRO; cur_m++)
		if (macro[cur_m] == NULL)
		    break;
	    if (cur_m == NMACRO) {
		init_message("No unused macro slots");
		return (char *) K_UNBOUND;
	    }
	    type = 1;
	} else if (cur_m < 0 || cur_m >= NUM_MACRO) {
	    m_error("macro number out of range\n", id);
	    return (char *) 0;
	}
    } else {
	for (cur_m = 0; cur_m < NUM_MACRO; cur_m++)
	    if (macro[cur_m] == NULL)
		break;
	if (cur_m == NUM_MACRO) {
	    init_message("No unused macro numbers");
	    return (char *) 0;
	}
    }

    if (f == NULL) {
	clrdisp();
	tprintf("DEFINE %sMACRO %d -- END WITH 'end'\n\n\r",
		cur_m >= NUM_MACRO ? "ANONYMOUS " : "",
		cur_m >= NUM_MACRO ? cur_m - NUM_MACRO : cur_m);
	unset_raw();
	f = stdin;
    }
    m = NULL;
    skip = 0;
    initial_set_commands = (cur_m == NMACRO);

    while (fgets_multi(line, 1024, f)) {
	for (lp = line; *lp && isspace(*lp); lp++);
	if (*lp == NUL)
	    continue;
	if (*lp == ')' || strncmp(lp, "end", 3) == 0)
	    goto out;
	if (!skip && parse_line(lp)) {
	    macro[cur_m] = NULL;
	    skip++;
	}
    }

    if (f != stdin)
	m_error("end missing", (char *) NULL);

out:
    if (f == stdin)
	nn_raw();
    m = NULL;
    return type == 0 ? (char *) macro[cur_m] : (char *) (K_MACRO | cur_m);
}

static char    *
m_get_macro(char *id)
{
    if (id) {
	cur_m = atoi(id);
	if (cur_m < 0 || cur_m >= NMACRO) {
	    m_error("macro number out of range\n", id);
	    return (char *) 0;
	}
    }
    return (char *) macro[cur_m];
}

char           *
parse_enter_macro(FILE * f, register int c)
{
    register char  *gp;
    char            other[FILENAME];
    group_header   *gh;
    static char    *last_defined = NULL;

    while (c != EOF && c != NL && (!isascii(c) || isspace(c)))
	c = getc(f);

    if (c == ')')
	return last_defined;

    if (c == EOF)
	return (char *) NULL;

    if (c == NL)
	return last_defined = m_define("-1", f);

    gp = other;
    do {
	*gp++ = c;
	c = getc(f);
    } while (c != EOF && c != ')' && isascii(c) && !isspace(c));

    *gp = NUL;
    if ((gh = lookup(other)))
	return gh->enter_macro;

    return m_get_macro(other);
}

/*
 *	Invoke macro # N
 */

void
m_invoke(int n)
{
    if (n == -2) {
	n = NMACRO;
	if ((macro[n] = (struct macro *) start_up_macro) == NULL)
	    return;
    } else if (n < 0) {
	n = NMACRO;
	if ((macro[n] = (struct macro *) (current_group->enter_macro)) == NULL)
	    if ((macro[n] = (struct macro *) dflt_enter_macro) == NULL)
		return;
    } else if (n >= NMACRO || macro[n] == NULL) {
	msg("undefined macro %d", n);
	return;
    }
    if (m_level == 0)
	no_advance = 0;
    else if (m == NULL)
	m_level--;
    else {
	if (m_level > MSTACK) {
	    msg("Macro stack overflow");
	    m_break();
	    return;
	}
	mstack[m_level] = m;
	cstack[m_level] = cur_m;
    }
    m_level++;

    cur_m = n;
    m = macro[cur_m];
    while (m && m->m_type == M_SET_COMMAND) {
	char            buffer[128];
	strcpy(buffer, m->m_string);
	if (macro_debug) {
	    msg(":%s", buffer);
	    user_delay(1);
	}
	parse_command(buffer, AC_UNCHANGED, (FILE *) NULL);
	m = m->m_next;
    }
}

void
m_startinput(void)
{
    no_advance = 1;
}

void
m_endinput(void)
{
    if (no_advance) {
	no_advance = 0;
	if (m && m->m_type == M_INPUT)
	    m = m->m_next;
    }
}

void
m_advinput(void)
{
    if (m && m->m_type == M_INPUT)
	m = m->m_next;
}

static struct macro *
m_call(int who)
{
    struct macro   *m1;

    for (;;) {
	while (m == NULL && m_level > 1) {
	    m_level--;
	    m = mstack[m_level];
	    cur_m = cstack[m_level];
	}
	if (m == NULL) {
	    if (macro_debug)
		msg("end");
	    m_break();
	    return NULL;
	}
	if (macro_debug)
	    macro_dbg();

	if (who == 3) {
	    if (m->m_type == M_YES || m->m_type == M_NO)
		goto out;
	    return NULL;
	}
	switch (m->m_type) {
	    case M_COMMAND:
		if (m->m_int == (GETC_COMMAND | K_REDRAW))
		    changed_prompt = 0;

		/* FALLTHRU */
	    case M_KEY:
		if (who == 1)
		    goto out;
		goto err;

	    case M_STRING:
		if (who == 2)
		    goto out;
		goto err;

	    case M_INPUT:
		if (no_advance)
		    return m;
		goto out;

	    case M_YES:
	    case M_NO:
	    case M_DUMMY:
		break;

	    case M_PUTS:
		tprintf("%s", m->m_string);
		fl;
		break;

	    case M_PROMPT:
		if (m->m_string[0] == NUL) {
		    changed_prompt = 0;
		    break;
		}
		if (!changed_prompt)
		    prompt(P_SAVE);
		changed_prompt = 1;
		prompt("\1%s\1 ", m->m_string);
		break;

	    case M_ECHO:
		msg(m->m_string);
		restore_xy();
		break;

	    case M_IS_MENU:
		if (!in_menu_mode)
		    m = m->m_branch;
		break;
	    case M_IS_SHOW:
		if (in_menu_mode)
		    m = m->m_branch;
		break;
	    case M_IS_GROUP:
		if (current_group->group_flag & G_FOLDER)
		    m = m->m_branch;
		break;
	    case M_IS_FOLDER:
		if ((current_group->group_flag & G_FOLDER) == 0)
		    m = m->m_branch;
		break;
	    case M_CONFIRM:
		if (yes(0) == 0)
		    m = m->m_branch;
		break;
	    case M_REJECT:
		if (yes(0) == 1)
		    m = m->m_branch;
		break;

	    case M_VARTEST:
		m1 = m;
		m = m->m_next;

		switch (test_variable(m1->m_string)) {
		    case 0:
			m = m->m_branch;
			break;
		    case -1:
			goto err1;
		}
		break;

	    case M_RETURN:
		m = NULL;
		continue;

	    case M_BREAK:
		goto term;
	}

	if (m)
	    m = m->m_next;
    }

out:
    m1 = m;
    m = m->m_next;
    no_advance = 0;
    return m1;

err:
    msg("Error in macro %d", cur_m);
err1:
    user_delay(1);
    m_break();
    return MERROR;

term:
    m_break();
    return NULL;
}

void
m_break_entry(void)
{
    if (current_group->enter_macro || dflt_enter_macro)
	m = NULL;
}

void
m_break(void)
{
    if (changed_prompt)
	prompt(P_RESTORE);
    changed_prompt = 0;
    m = NULL;
    m_level = 0;
}

static void
macro_dbg(void)
{
    char           *name = NULL;

    switch (m->m_type) {
	case M_COMMAND:
	    msg("COMMAND: %s", command_name(m->m_int));
	    goto delay;

	case M_KEY:
	    msg("KEY: %s", key_name((key_type) (m->m_int)));
	    goto delay;

	case M_STRING:
	    msg("STRING: %s", m->m_string);
	    goto delay;

	case M_INPUT:
	    name = "input";
	    break;

	case M_YES:
	    name = "yes";
	    break;

	case M_NO:
	    name = "no";
	    break;

	case M_DUMMY:
	    name = "dummy";
	    break;

	case M_PROMPT:
	    msg("PROMPT: %s", m->m_string);
	    goto delay;

	case M_ECHO:
	    msg("ECHO: %s", m->m_string);
	    goto delay;

	case M_IS_MENU:
	    msg("?menu => %d", in_menu_mode);
	    goto delay;

	case M_IS_SHOW:
	    msg("?show => %d", !in_menu_mode);
	    goto delay;

	case M_IS_GROUP:
	    msg("?group => %d", (current_group->group_flag & G_FOLDER) == 0);
	    goto delay;

	case M_IS_FOLDER:
	    msg("?group => %d", (current_group->group_flag & G_FOLDER));
	    goto delay;

	case M_CONFIRM:
	    name = "?yes";
	    break;

	case M_REJECT:
	    name = "?no";
	    break;

	case M_VARTEST:
	    msg("?%s => %d", m->m_string, test_variable(m->m_string));
	    goto delay;

	case M_RETURN:
	    name = "return";
	    break;

	case M_BREAK:
	    name = "break";
	    break;
    }
    msg(name);

delay:
    user_delay(1);
}

/*
 *	Macro processing for get_c()
 */

int
m_getc(int *cp)
{
    struct macro   *m1;

    get_from_macro = 0;
    if (m_level && (m1 = m_call(1))) {
	if (m1 == MERROR)
	    return 2;
	if (m1->m_type == M_INPUT)
	    return 0;
	*cp = m1->m_int;
	get_from_macro = 1;
	return 1;
    }
    return 0;
}

/*
 *	Macro processing for get_s()
 */

int
m_gets(char *s)
{
    struct macro   *m1;

    get_from_macro = 0;
    if (m_level && (m1 = m_call(2))) {
	if (m1 == MERROR)
	    return 2;
	if (m1->m_type == M_INPUT)
	    return 0;
	strcpy(s, m1->m_string);
	get_from_macro = 1;
	return 1;
    }
    return 0;
}

/*
 *	Macro processing for yes()
 */

int
m_yes(void)
{
    struct macro   *m1;

    if (m)
	if (m->m_type == M_CONFIRM || m->m_type == M_REJECT)
	    return 3;

    if (m_level) {
	if ((m1 = m_call(3))) {
	    if (m1->m_type == M_NO)
		return 1;
	    else
		return 2;
	} else {
	    return 3;
	}
    }
    return 0;
}
