/*
 *	This file is for a Silicon Graphics 4D series machines
 *	running IRIX 3.1 or 3.2.
 *	HAVE_JOBCONTROL should only be defined if you have 3.2.
 */

#include "s-sys5.h"

/*
 *	Define if your system has BSD like job control (SIGTSTP works)
 */

#define HAVE_JOBCONTROL			/* */

/*
 *	Define if your system has a 4.3BSD like syslog library.
 */

#define HAVE_SYSLOG

/*
 *	Define HAVE_GETHOSTNAME if your system provides a BSD like
 *	gethostname routine.
 */

#undef	HAVE_UNAME
#define	HAVE_GETHOSTNAME			/* System V */

/*
 *	Specify the default mailer
 */

#undef	MAILX
#define	MAILX		"/usr/sbin/Mail"

/*
 *	Define standard compiler flags here:
 */

#define COMPILER_FLAGS -O -I/usr/include/bsd


/*
 *	Define the maximum length of any pathname that may occur
 */

#undef	FILENAME
#define	FILENAME 	1024	/* really should be from limits.h */

/*
 *	If your system requires other libraries when linking nn
 *	specify them here:  (use shared C library for reduced
 *	size and portability across releases).
 */

#define EXTRA_LIB -lbsd -lc_s
