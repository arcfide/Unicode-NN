/*
 *	This version is for PYRAMID (almost BSD 4.2) systems
 *	(also known as Nixdorf Targon 35).
 */

#include "s-bsd4-2.h"

extern FILE *popen();

/* The sysV shell has test built-in */
#undef SHELL
#define SHELL "/.attbin/sh"

#undef	MAILX
#define	MAILX	"/usr/.ucbucb/Mail"		/* BSD */
