/*
 *	This version is for BSD 4.2 systems
 */


/*
 *	Include header files containing the following definitions:
 *
 * 		off_t, time_t, struct stat
 */

#include <sys/types.h>
#include <sys/stat.h>


/*
 *	Define if your system doesn't have memmove()
 */

#define NO_MEMMOVE

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

#define TERMLIB	-ltermlib

/*
 *	Define HAVE_STRCHR if strchr() and strrchr() are available
 */

#undef HAVE_STRCHR			/* */

/*
 *	Define if a signal handler has type void (see signal.h)
 */

#undef	SIGNAL_HANDLERS_ARE_VOID	/* */

/*
 *	Define if your system has BSD like job control (SIGTSTP works)
 */

#define HAVE_JOBCONTROL			/* */

/*
 *	Define if your system provides the "directory(3X)" access routines
 *
 *	If true, include the header file(s) required by the package below
 *	(remember that <sys/types.h> or equivalent is included above)
 *	Also typedef Direntry to the proper struct type.
 */

#define	HAVE_DIRECTORY			/* */

#include <sys/dir.h>			/* BSD */

typedef struct direct Direntry;		/* BSD */

/*
 *	Define if your system has a mkdir() library routine
 */

#define	HAVE_MKDIR			/* */

/*
 *	Define if your system provides a BSD like gethostname routine.
 *	Otherwise, define HAVE_UNAME if uname() is avaiable.
 */

#define	HAVE_GETHOSTNAME	/* BSD systems */

/*
 *	Define HAVE_MULTIGROUP if system has simultaneous multiple group
 *	membership capability (BSD style).
 */

#define HAVE_MULTIGROUP

/*
 *	Define DETACH_TERMINAL to be a command sequence which
 *	will detatch a process from the control terminal
 *	Also include system files needed to perform this HERE.
 *	If not possible, just define it (empty)
 */

#include <sys/ioctl.h>	/* for TIOCNOTTY */

#define	DETACH_TERMINAL \
    { int t = open("/dev/tty", O_RDONLY); \
	  if (t >= 0) ioctl(t, TIOCNOTTY, (int *)0), close(t); }


/*
 *	Specify where the Bourne Shell is.
 */

#define SHELL		"/bin/sh"

/*
 *	Specify the default mailer
 */

#define	MAILX	"/usr/ucb/Mail"		/* BSD */

/*
 *	Define the maximum length of any pathname that may occur
 */

#define	FILENAME 	256
