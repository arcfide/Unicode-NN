/*
 *	This is a file for Mips Risc/os, version 4.0 or later, for
 *	compiling in the bsd43 (instead of sys v) environment.
 *	From: beldar@mips.com (Gardner Cohen)
 */


/*
 *	Include header files containing the following definitions:
 *
 * 		off_t, time_t, struct stat
 */

#include <sys/types.h>
#include <sys/stat.h>


/*
 *	Define if your system has system V like ioctls
 */

#undef	HAVE_TERMIO_H			/* */

/*
 *	Define to use terminfo database.
 *	Otherwise, termcap is used
 */

#undef	USE_TERMINFO			/* */

/*
 *	Specify the library (or libraries) containing the termcap/terminfo
 *	routines.
 *
 *	Notice:  nn only uses the low-level terminal access routines
 *	(i.e. it does not use curses).
 */

#define TERMLIB	-ltermcap

/*
 *	Define HAVE_STRCHR if strchr() and strrchr() are available
 */

#define HAVE_STRCHR			/* */

/*
 *	Define if a signal handler has type void (see signal.h)
 */

#undef	SIGNAL_HANDLERS_ARE_VOID	/* */

/*
 *	Define if signals must be set again after they are caught
 */

#undef	RESET_SIGNAL_WHEN_CAUGHT	/* */

/*
 *	Define if your system has a 4.3BSD like ualarm call.
 */

#define HAVE_UALARM

/*
 *	Define if your system has BSD like job control (SIGTSTP works)
 */

#define HAVE_JOBCONTROL			/* */


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

#define	HAVE_DIRECTORY			/* */

#include <sys/dir.h>				/* BSD */

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

#define	HAVE_GETHOSTNAME	/* BSD systems */

/*
 *	Define HAVE_MULTIGROUP if system has simultaneous multiple group
 *	membership capability (BSD style).
 */

#define HAVE_MULTIGROUP

/* #define	HAVE_UNAME			/* System V */

/*
 *	Define DETACH_TERMINAL to be a command sequence which
 *	will detatch a process from the control terminal
 *	Also include system files needed to perform this HERE.
 *	If not possible, just define it (empty)
 */


#include <sys/file.h>	/* for O_RDONLY */
#include <sys/ioctl.h>	/* for TIOCNOTTY */

#define	DETACH_TERMINAL \
    { int t = open("/dev/tty", O_RDONLY); \
	  if (t >= 0) ioctl(t, TIOCNOTTY, (int *)0), close(t); }



/*
 *	Specify where the Bourne Shell is.
 */

#define SHELL		"/bin/sh"

/*
 *	Define AVOID_SHELL_EXEC if the system gets confused by
 *		#!/bin/sh
 *	lines in shell scripts, e.g. only reads #! and thinks it
 *	is a csh script.
 */

/* #define AVOID_SHELL_EXEC		/* */

/*
 *	Specify the default mailer
 */

#define	MAILX		"/usr/bin/mailx"	/* SV */
/* #define	MAILX	"/usr/ucb/Mail"		/* BSD */


/*
 *	Specify the default pager & options.
 */

#define	PAGER		"less"


/*
 *	Define the maximum length of any pathname that may occur
 */

#define	FILENAME 	1024


/*
 *	Define standard compiler flags here:
 */
#undef COMPILER_FLAGS
#undef COMPILER
#undef CDEBUG
#define COMPILER_FLAGS -O2 -Olimit 2000
#define COMPILER cc -systype bsd43
#define CDEBUG -g3

/*
 *	If your system requires other libraries when linking nn
 *	specify them here:
 */

#define EXTRA_LIB

