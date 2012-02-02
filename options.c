/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Generic option parsing
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "nn.h"
#include "options.h"
#include "variable.h"

/* options.c */

static void     dump_options(int type, char *tail);
static void     error(char *message, char option_letter);

static char   **save_argv, *usage_mesg;
static struct option_descr *save_optd;

extern int      no_update;


char           *
program_name(char **av)
{
    char           *cp;

    /* skip "/path/" part of program name */
    if ((cp = strrchr(*av, '/')))
	return cp + 1;
    else
	return *av;
}

static void
dump_options(int type, char *tail)
{
    register struct option_descr *od;
    int             any = 0;

    for (od = save_optd; od->option_letter; od++) {
	if (od->option_type != type)
	    continue;
	fprintf(stderr, any ? "%c" : " -%c", od->option_letter);
	any++;
    }

    if (any && tail && tail[0]) {
	fprintf(stderr, "%s", tail);
    }
}

static void
error(char *message, char option_letter)
{
    char           *prog_name = program_name(save_argv);

    fprintf(stderr, "%s: ", prog_name);
    fprintf(stderr, message, option_letter);
    fputc('\n', stderr);

    fprintf(stderr, "usage: %s", prog_name);

    dump_options(1, "");
    dump_options(2, " STR");
    dump_options(3, " [STR]");
    dump_options(4, " NUM");
    dump_options(5, " [NUM]");

    if (usage_mesg)
	fprintf(stderr, usage_mesg);
    fputc(NL, stderr);

    nn_exit(9);
}

/* parse variables set on command line seperately from everything
 * else, very early.  This is necessary because open_master is done
 * so early for nntp; don't alter the arg, so we can skip it later.
 */
void
parseargv_variables(char **av, int ac)
{
    char           *value;

    while (--ac > 0) {
	if (**++av != '-' && (value = strchr(*av, '='))) {
	    *value++ = NUL;
	    set_variable(*av, 1, value);
	    *--value = '=';	/* restore for later parsing to skip it */
	}
    }
}

int
parse_options(int ac, char **av, char *envname, struct option_descr options[], char *usage)
{
    register char  *cp, opt;
    register struct option_descr *od;
    int             files;
    char          **names;
    char           *envinit;

    save_argv = av;
    save_optd = options;

    if (options == NULL)
	return 0;

    usage_mesg = usage;

    --ac;
    names = ++av;
    files = 0;

    envinit = envname ? getenv(envname) : NULL;
    cp = envinit;

next_option:

    if (envinit) {
	while (*cp && isspace(*cp))
	    cp++;
	if (*cp == '-') {
	    cp++;
	    goto next_option;
	}
	if (*cp == NUL) {
	    envinit = NULL;
	    goto next_option;
	}
    } else if (cp == NULL || *cp == NUL) {
	if ((cp = *av++) == NULL) {
	    *names = NULL;
	    return files;
	}
	ac--;

	if (*cp != '-') {
	    if (!strchr(cp, '=')) {
		*names++ = cp;
		cp = NULL;
		files++;
	    }

	    /*
	     * else it's a variable that was handled in parseargv_variables()
	     * earlier, so nothing to do.
	     */
	    cp = NULL;
	    goto next_option;
	}
	cp++;			/* skip - */
    }
    opt = *cp++;

    for (od = options; od->option_letter; od++) {
	if (od->option_letter != opt)
	    continue;

	switch (od->option_type) {

	    case 1:		/* BOOL_OPTION */

		*((int *) (od->option_address)) = !*((int *) (od->option_address));
		goto next_option;

	    case 2:		/* STRING_OPTION */
	    case 3:		/* OPTIONAL_STRING */

		/* -oSTR or -o STR */

		while (*cp && isspace(*cp))
		    cp++;

		if (*cp == NUL) {
		    if (envinit || ac == 0) {
			if (od->option_type == 3)
			    goto opt_str;
			error("missing string argumet to -%c", opt);
		    }
		    cp = *av++;
		    ac--;
		}
		if (od->option_type == 3 && *cp == '-') {
		    cp++;
		    goto opt_str;
		}
		*(od->option_address) = cp;

		if (envinit) {
		    while (*cp && !isspace(*cp))
			cp++;
		    if (*cp)
			*cp++ = NUL;
		} else
		    cp = NULL;

		goto next_option;

	opt_str:
		*(od->option_address) = od->option_default;
		goto next_option;

	    case 4:
	    case 5:

		/* -oN or -o N */

		while (*cp && isspace(*cp))
		    cp++;

		if (*cp) {
		    if (!isdigit(*cp)) {
			if (od->option_type == 5)
			    goto opt_int;
			error("non-numeric argument to -%c", opt);
		    }
		} else {
		    if (envinit || ac == 0 || !isdigit(**av)) {
			if (od->option_type == 5)
			    goto opt_int;
			error("missing argument to -%c", opt);
		    }
		    cp = *av++;
		    ac--;
		}
		*((int *) (od->option_address)) = atoi(cp);
		while (isdigit(*cp))
		    cp++;
		goto next_option;

	opt_int:
		*((int *) (od->option_address)) = (int) (od->option_default);
		goto next_option;
	}
    }

    error("unknown option '-%c'", opt);
    /* NOTREACHED */
    return 0;
}
