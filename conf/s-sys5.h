/*
 *	This is a generic System V Release 2 & 3 file.
 *
 *	If you need to modify anything, make a new s- file,
 *	#include this file, and #define or #undef a few symbols.
 */


/*
 *	Include header files containing the following definitions:
 *
 * 		off_t, time_t, struct stat
 */

#ifdef lint
struct proc {	/* sys/types.h references this on many systems */
    int keep_lint_happy;
};
#endif

#include <sys/types.h>
#include <sys/stat.h>

/*
 *	Define if your system has system V like ioctls
 */

#define	HAVE_TERMIO_H			/* */

/*
 *	Define to use terminfo database.
 *	Otherwise, termcap is used
 */

#define	USE_TERMINFO			/* */

/*
 *	Specify the library containing the termcap/terminfo access routines.
 *	Notice:  nn does not use curses.
 *	Notice:  You must also specify whether termcap or terminfo is
 *		 used when you edit config.h (see below).
 */

#define	TERMLIB -lcurses

/*
 *	Define HAVE_STRCHR if strchr() and strrchr() are available
 */

#define HAVE_STRCHR			/* */

/*
 *	Define if a signal handler has type void (see signal.h)
 */

#define	SIGNAL_HANDLERS_ARE_VOID	/* */

/*
 *	Define if signals must be set again after they are caught
 */

#define	RESET_SIGNAL_WHEN_CAUGHT	/* */

/*
 *	Define if your system has BSD like job control (SIGTSTP works)
 */

/* #define HAVE_JOBCONTROL			*/

/*
 *	Define if your system provides the "directory(3X)" access routines
 *
 *	If true, include the header file(s) required by the package below
 *	(remember that <sys/types.h> or equivalent is included above)
 *	Also typedef Direntry to the proper struct type.
 */

#define	HAVE_DIRECTORY			/* */

#include <dirent.h>			/* System V */
typedef struct dirent Direntry;		/* System V */

/*
 *	Define if your system has a mkdir() library routine
 */

#define	HAVE_MKDIR			/* */

/*
 *	Define HAVE_GETHOSTNAME if your system provides a BSD like
 *	gethostname routine.
 *	Otherwise, define HAVE_UNAME if uname() is avaiable.
 *	As a final resort, define HOSTNAME to the name of your system.
 */

#define	HAVE_UNAME			/* System V */

/*
 *	Define DETACH_TERMINAL to be a command sequence which
 *	will detatch a process from the control terminal
 *	Also include files needed to perform this HERE.
 *	If not possible, just define it (empty)
 */

#define	DETACH_TERMINAL setpgrp();	/* System V */

/*
 *	Specify where the Bourne Shell is.
 */

#define SHELL		"/bin/sh"

/*
 *	Specify the default mailer
 */

#define	MAILX		"/usr/bin/mailx"	/* SV */

/*
 *	Define the maximum length of any pathname that may occur
 */

#define	FILENAME 	256
