/*
 *	This file is for Fortune 32:16 systems.
 *	Use with m-m680x0.h file, but requires STRCSPN in config.h
 *	The Fortune may have problems with the nested #ifdef:s in
 *	regexp.c (STRCSPN + __STDC__).  If so remove the __STDC__ ifdef.
 *
 *	From thewev!root, July 9, 1989.
 */


/*
 *	Include header files containing the following definitions:
 *
 * 		off_t, time_t, struct stat
 */

#include <sys/types.h>
#include <stat.h>


/*
 *	Define if your system has system V like ioctls
 */

/* #define	HAVE_TERMIO_H			/* */

/*
 *	Define to use terminfo database.
 *	Otherwise, termcap is used
 */

/* #define	USE_TERMINFO			/* */

/*
 *	Specify the library (or libraries) containing the termcap/terminfo
 *	routines.
 *
 *	Notice:  nn only uses the low-level terminal access routines
 *	(i.e. it does not use curses).
 */

#define TERMLIB	-ltermlib

/*
 *	Define HAVE_STRCHR if strchr() and strrchr() are available
 */

/* #define HAVE_STRCHR			/* */

/*
 *	Define if a signal handler has type void (see signal.h)
 */

/* #define	SIGNAL_HANDLERS_ARE_VOID	/* */

/*
 *	Define if signals must be set again after they are caught
 */

#define	RESET_SIGNAL_WHEN_CAUGHT	/* */

/*
 *	Define if your system has BSD like job control (SIGTSTP works)
 */

/* #define HAVE_JOBCONTROL			/* */


/*
 *	Define if your system has a 4.3BSD like syslog library.
 */

#undef HAVE_SYSLOG

/*
 *	Define if your system provides the "directory(3X)" access routines
 *
 *	If true, include the header file(s) required by the package below
 *	(remember that <sys/types.h> or equivalent is included above)
 *	Also typedef Direntry to the proper struct type.
 */

/* #define	HAVE_DIRECTORY			/* */

/* #include <dirent.h>			/* System V */
#include <sys/dir.h>				/* BSD */

/* typedef struct dirent Direntry;		/* System V */
typedef struct direct Direntry;		/* BSD */

/*
 *	Define if your system has a mkdir() library routine
 */

#define	HAVE_MKDIR			/* */


/*
 *	Define HAVE_GETHOSTNAME if your system provides a BSD like
 *	gethostname routine.
 *	Otherwise, define HAVE_UNAME if uname() is avaiable.
 *	As a final resort, define HOSTNAME to the name of your system
 *	(in config.h).
 */

/* #define	HAVE_GETHOSTNAME	/* BSD systems */

/* #define	HAVE_UNAME			/* System V */

/*
 *	Define DETACH_TERMINAL to be a command sequence which
 *	will detatch a process from the control terminal
 *	Also include system files needed to perform this HERE.
 *	If not possible, just define it (empty)
 */

/* #include "...." */

#define	DETACH_TERMINAL /* setpgrp(); */


/*
 *	Specify where the Bourne Shell is.
 */

#define SHELL		"/bin/sh"

/*
 *	Specify the default mailer
 */

#define	MAILX		"/bin/mail"	/* FOR:PRO */
/* #define	MAILX	"/usr/ucb/Mail"		/* BSD */

/*
 *	Define the maximum length of any pathname that may occur
 */

#define	FILENAME 	128


/*
 *	Define standard compiler flags here:
 */

#define COMPILER_FLAGS -O

/*
 *	If your system requires other libraries when linking nn
 *	specify them here:
 */

#define EXTRA_LIB
