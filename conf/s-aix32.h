/*
 *	This version is for RS6000 AIX 3.2.x
 *	(also for AIX 4.1)
 *	From: tranle@intellicorp (Minh Tran-Le)
 */

/*
 *	Notice: AIX's PRINTER command is "lpr"  (see config.h)
 */

#include <s-sys5.h>

/*
 *	Define if your system has BSD like job control (SIGTSTP works)
 */

#define HAVE_JOBCONTROL			/* */

/*
 *	Define HAVE_MULTIGROUP if system has simultaneous multiple group
 *	membership capability (BSD style).
 */

#define HAVE_MULTIGROUP

/*
 *	Define HAVE_GETHOSTNAME if your system provides a BSD like
 *	gethostname routine.
 */

#undef  HAVE_UNAME
#define	HAVE_GETHOSTNAME

/*
 *	Define if your system has a 4.3BSD like ualarm call.
 */

#define HAVE_UALARM

/*
 *	Define if your system has a 4.3BSD like syslog library.
 */

#define HAVE_SYSLOG

/*
 * 	Define RESIZING to make nn understand dynamic window-resizing.
 * 	(It uses the TIOCGWINSZ ioctl found on most 4.3BSD systems)
 */

#define RESIZING

/*
 *	Setup to use the XLC compiler
 */
#undef COMPILER
#define COMPILER xlc -qroconst -ma
#define COMPILER_FLAGS -D_ALL_SOURCE
