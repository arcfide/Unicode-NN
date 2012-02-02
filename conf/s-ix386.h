/*
 *	For ISC 386/ix 2.0.x (also Dell UNIX 1.x).
 *	With tweaks for Host Based TCP/IP (1.1, 1.2)
 *	From: Jeremy Chatfield (jdc@dell.dell.com) May 27, 90.
 */

/*
 *	Define if your system has a 4.3BSD like syslog library.
 *
 *	Host Based TCP/IP includes syslog (check /etc/syslog.conf for
 *	message routings). jdc.
 */

#define HAVE_SYSLOG

/*
 *	ISC 386/ix is extraordinarily like System 5 :-) jdc.
 */

#include "s-sys5.h"

/*
 *	If we have defined RESIZING in config.h, make sure that we
 *	use the SV resizing (to include ptem.h, etc). jdc.
 */

#define SYSV_RESIZING

/*
 *	Define AVOID_SHELL_EXEC if the system gets confused by
 *		#!/bin/sh
 *	lines in shell scripts, e.g. only reads #! and thinks it
 *	is a csh script.
 *
 *	I've seen some funnies , but it may be just be me... jdc.
 */

#define AVOID_SHELL_EXEC

/*
 *	Uses malloc.h
 *	Values tuned slightly for ix386
 */

#define USE_MALLOC_H
#define MALLOC_MAXFAST		(4 * sizeof(double))
#define MALLOC_FASTBLOCKS	128
#define MALLOC_GRAIN		sizeof(double)

/*
 *	If your system requires other libraries when linking nn
 *	specify them here:
 *
 *	-lmalloc	Adds better malloc
 *	-linet		Essential for HB TCP/IP use (NNTP requires use)
 *	-lc_s		Standard C shared library.  (Useful, not essential)
 */

#if defined(NNTP) || defined (HAVE_SYSLOG)
#define EXTRA_LIB	-linet -lmalloc -lc_s
#else
#define EXTRA_LIB	-lmalloc -lc_s
#endif

/*
 *	There is a rename() function in -linet, but it is broken.
 *
 *	If you don't have nn 6.4 patch level 4 or higher, NO_RENAME is not
 *	recognized.  Instead, you will need to fix nntp.c, around line 722:
 *	Change "if (rename..." 	to "if (unlink(news_active), rename..."
 *	But save a copy of the original nntp.c to be able to apply patch 4.
 */

#define NO_RENAME
