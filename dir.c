/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Directory access.
 * 	read file names in directory 'dir' starting with 'prefix'
 */

#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "global.h"
#include "articles.h"
#include "dir.h"
#include "nn_term.h"


/* dir.c */

static int      sort_directory(register char **f1, register char **f2);


static char     dir_path[FILENAME], *dir_tail;

#ifdef HAVE_DIRECTORY
static string_marker str_mark;
static char   **completions = NULL;
static char   **comp_iterator;
static char   **comp_help;

/*
 * list_directory scans the directory twice; first time to find out how
 * many matches there are, and second time to save the names, after
 * sufficient memory have been allocated to store it all.
 */

static int
sort_directory(register char **f1, register char **f2)
{				/* Used by qsort */
    return strcmp(*f1, *f2);
}

int
list_directory(char *dir, char *prefix)
{
    DIR            *dirp;
    register Direntry *dp;
    register char  *cp;
    register char **comp = NULL;
    int             pflen = strlen(prefix);
    unsigned        count = 0, comp_length = 0;

    if ((dirp = opendir(dir)) == NULL)
	return 0;		/* tough luck */

    mark_str(&str_mark);

    while ((dp = readdir(dirp)) != NULL) {
	cp = dp->d_name;

#ifdef FAKED_DIRECTORY
	if (dp->d_ino == 0)
	    continue;
	cp[14] = NUL;
#endif

	if (*cp == '.' && (cp[1] == '\0' || (cp[1] == '.' && cp[2] == '\0')))
	    continue;
	if (pflen && strncmp(prefix, cp, pflen))
	    continue;
	if (count == comp_length) {
	    comp_length += 100;
	    completions = resizeobj(completions, char *, comp_length + 1);
	    comp = completions + count;
	}
	strcpy(*comp++ = alloc_str(strlen(cp)), cp);
	count++;
    }
    closedir(dirp);
    if (count == 0) {
	release_str(&str_mark);
	return 0;
    }
    quicksort(completions, count, char *, sort_directory);
    *comp = (char *) 0;
    comp_iterator = completions;
    comp_help = completions;

    dir_tail = dir_path;
    while ((*dir_tail++ = *dir++));
    dir_tail[-1] = '/';

    return 1;
}

int
next_directory(register char *buffer, int add_slash)
{
    if (*comp_iterator != NULL) {
	strcpy(buffer, *comp_iterator);

	if (add_slash) {
	    strcpy(dir_tail, *comp_iterator);
	    if (file_exist(dir_path, "d"))
		strcat(buffer, "/");
	}
	comp_iterator++;
	return 1;
    }
    close_directory();
    return 0;
}

int
compl_help_directory(void)
{
    list_completion((char *) NULL);

    if (*comp_help == NULL)
	comp_help = completions;
    while (*comp_help && list_completion(*comp_help))
	comp_help++;

    fl;
    return 1;
}

void
close_directory(void)
{
    if (completions) {
	release_str(&str_mark);
	freeobj(completions);
	completions = NULL;
    }
}

#else

static FILE    *dirf;
static int      prefix_lgt;

int
list_directory(char *dir, char *prefix)
{
    if (prefix[0])
	sprintf(dir_path, "cd %s && echo %s* 2>/dev/null", dir, prefix);
    else
	sprintf(dir_path, "cd %s && ls 2>/dev/null", dir);
    prefix_lgt = strlen(prefix);

    if ((dirf = popen(dir_path, "r")) == NULL)
	return 0;

    dir_tail = dir_path;
    while (*dir_tail++ = *dir++);
    dir_tail[-1] = '/';

    return 1;
}

next_directory(char *buffer, int add_slash)
{
    register char  *cp;
    register int    c;

    cp = buffer;
    while ((c = getc(dirf)) != EOF && (c != SP) && (c != NL))
	*cp++ = c;

    if (cp != buffer) {
	*cp = NUL;
	if (strcmp(buffer + prefix_lgt, "*")) {

	    if (!add_slash)
		return 1;

	    strcpy(dir_tail, buffer);
	    if (file_exist(dir_path, "d")) {
		*cp++ = '/';
		*cp = NUL;
	    }
	    return 1;
	}
    }
    close_directory();
    return 0;
}

int
compl_help_directory(void)
{
    return 0;
}

void
close_directory(void)
{
    if (dirf) {
	pclose(dirf);
	dirf = NULL;
    }
}

#endif				/* HAVE_DIRECTORY */
