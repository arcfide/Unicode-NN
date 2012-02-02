/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	"generic" gethostname() emulation:
 *
 *	Possibilities are used in following order:
 *
 *	HAVE_GETHOSTNAME		-- use gethostname()
 *	HAVE_UNAME			-- use sysV uname().nodename
 *	HOSTNAME_FILE "/.../..."	-- read hostname from file
 *	HOSTNAME_WHOAMI			-- use sysname from whoami.h
 *	HOSTNAME "host"			-- hard-coded hostname
 *	You lose!
 */

#include <unistd.h>
#include <string.h>
#include "config.h"

#undef DONE

#ifdef HAVE_GETHOSTNAME
/*
 *	Easy -- we already got it
 */
void
nn_gethostname(char *name, int length)
{
    gethostname(name, length);
}

#define DONE
#endif

#ifndef DONE

#ifdef HAVE_UNAME
/*
 *	System V uname() is available.	Use nodename field.
 */

#include <sys/utsname.h>

void
nn_gethostname(char *name, int length)
{
    struct utsname  un;

    uname(&un);
    strncpy(name, un.nodename, length);
}

#define DONE
#endif

#endif

#ifndef DONE

#ifdef HOSTNAME_FILE
/*
 *	Hostname is available in a file.
 *	The name of the file is defined in HOSTNAME_FILE.
 *	This is not intended to fail (or exit would have been via nn_exit)
 */

void
nn_gethostname(char *name, int length)
{
    FILE           *f;
    register char  *p;

    f = fopen(HOSTNAME_FILE, "r");	/* Generic code -- don't use
					 * open_file */
    if (f == NULL)
	goto err;
    if (fgets(name, length, f) == NULL)
	goto err;
    if ((p = strchr(name, NL)) != NULL)
	*p = NUL;
    fclose(f);
    return;

err:
    fprintf(stderr, "HOSTNAME NOT FOUND IN %s\n", HOSTNAME_FILE);
    exit(77);
}

#define DONE
#endif

#endif

#ifndef DONE

#ifdef HOSTNAME_WHOAMI
/*
 *	Hostname is found in whoami.h
 */

void
nn_gethostname(char *name, int length)
{
    FILE           *f;
    char            buf[512];
    register char  *p, *q;

    f = fopen("/usr/include/whoami.h", "r");
    if (f == NULL)
	goto err;

    while (fgets(buf, 512, f) != NULL) {
	if (buf[0] != '#')
	    continue;
	if ((p = strchr(buf, '"')) == NULL)
	    continue;
	*p++ = NUL;
	if (strncmp(buf, "#define sysname", 15))
	    continue;
	if ((q = strchr(p, '"')) == NULL)
	    break;
	*q = NUL;
	strncpy(name, p, length);
	return;
    }

err:
    fprintf(stderr, "HOSTNAME (sysname) NOT DEFINED IN whoami.h\n");
    exit(77);
}

#define DONE
#endif

#endif

#ifndef DONE

#ifdef HOSTNAME
void
nn_gethostname(char *name, int length)
{
    strncpy(name, HOSTNAME, length);
}

#define DONE
#endif

#endif

#ifndef DONE
YOU LOSE ON     GETHOSTNAME-- DEFINE HOSTNAME IN CONFIG.H
#endif
