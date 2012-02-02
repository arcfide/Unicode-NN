/*
 *	This version is for A/UX 3.0.x.
 *
 *	jim@jagubox.gsfc.nasa.gov
 *
 *	Modified by Jim Jagielski to fold in some of stuff and make the
 *	file a bit more universal. I also made sure that it would
 *	work with 6.5... I have _only_ tested it with 'gcc' and there's
 *	one hint... Use "make CC=gcc CPP='gcc -E'" to get things working
 *	OK.
 *
 *	This file relies on the s-sys5.h file. Any items that I override have
 *	comments taked from the prototype s- file, and then comments of mine
 *	about the change.
 *
 *	Note that the s-aux1-1.h file included with nn 6.4.16 and prior
 * 	versions is now badly out of date. It may still run, but it's severely
 *	suboptimal.
 *
 *	The original version was written by Alexis Rosen.
 */

/*
 */

#include "s-sys5.h"

/*
 *	Define if signals must be set again after they are caught
 */

/*	A/UX NO CHANGE
 *	This is probably not necessary with BSD compatibility enabled.
 *	If you want to try undef'ing it, be my guest.
 */

#define	RESET_SIGNAL_WHEN_CAUGHT	/* */

/*
 *	Define if your system has BSD like job control (SIGTSTP works)
 */

/*	A/UX CHANGE
 *	A/UX supports job control.
 */

#define HAVE_JOBCONTROL			/* */

/*
 *	Define if your system has a 4.3BSD like syslog library.
 */

/*	A/UX CHANGE
 *	A/UX has syslog.
 */

#define HAVE_SYSLOG

/*
 *	Pick one:
 *	Define HAVE_GETHOSTNAME if you have a BSD like gethostname routine.
 *	Define HAVE_UNAME if a system V compatible uname() is available.
 *	Define HOSTNAME_FILE "...." to a file containing the hostname.
 *	Define HOSTNAME_WHOAMI if sysname is defined in <whoami.h>.
 *
 *	As a final resort, define HOSTNAME to the name of your system
 *	(in config.h).
 */

/*	A/UX CHANGE
 *	A/UX supports gethostname and uname. Use gethostname.
 *	[and undef uname]
 */

#undef HAVE_UNAME				/*  */
#define HAVE_GETHOSTNAME			/* BSD systems */

/*
 *	Define HAVE_MULTIGROUP if system has simultaneous multiple group
 *	membership capability (BSD style).
 *	Also define NGROUPS or include the proper .h file if NGROUPS is
 *	not defined in <sys/param.h>.
 */

/*	A/UX CHANGE
 *	A/UX supports multiple groups with the appropriate setcompat call.
 *	Oddly enough, NN seems to like this even without the setcompat call.
 *	I haven't looked at this so I don't know why. On the other hand the
 *	A/UX man pages for getgroups(2) doesn't indicate any need for calling
 *	setcompat, so maybe that's why it works. Or maybe the bsd library sets
 *	the compat bit for multigroup.
 */

#define HAVE_MULTIGROUP	/* BSD */

/*
 *	Define DETACH_TERMINAL to be a command sequence which
 *	will detatch a process from the control terminal
 *	Also include system files needed to perform this HERE.
 *	If not possible, just define it (empty)
 */

/*	A/UX CHANGE
 *
 *	The default (for Sys5) setpgrp() call does kinda work, but it carries
 *	extra baggage... why bother?
 */
#include <sys/file.h>   /* for O_RDONLY */
#include <sys/ioctl.h>  /* for TIOCNOTTY */

#undef DETACH_TERMINAL	/* eliminate obnoxious compiler warnings */
#define DETACH_TERMINAL \
    { int t = open("/dev/tty", O_RDONLY); \
          if (t >= 0) ioctl(t, TIOCNOTTY, (int *)0), close(t); }


/*
 *	Define USE_MALLOC_H if the faster malloc() in -lmalloc should be used.
 *	This requires that -lmalloc is added to EXTRA_LIB below.
 *
 *	You can tune the malloc package through the following definitions
 *	according to the descriptions in malloc(3X):
 */

/*	A/UX CHANGE
 *	Use the malloc library. Don't fiddle with the tuning stuff though.
 */

#define USE_MALLOC_H		/* */

#define MALLOC_GRAIN		sizeof(double)		/* M_GRAIN */
#define MALLOC_MAXFAST	(MALLOC_GRAIN*4)	/* M_MXFAST */
#define MALLOC_FASTBLOCKS	100			/* M_NLBLKS */

/*
 *	Define standard compiler flags here:
 */

/*	A/UX CHANGE
 *	Use "-O2" for gcc. If you have an old gcc or just Apple C, use "-O".
 *	A/UX 3.0 with gcc requires "-DUSG" because stdio doesn't define
 *	L_ctermid, and stdio.h uses the existance of L_ctermid to decide
 *	whether the code is V7 or USG. Without USG, termio doesn't get included
 *	correctly. So force "USG" in, and everything works again.
 *
 *	The trouble is that gcc, by default, defines STDC (as 1) so
 *	L_ctermid isn't defined in stdio.h which then doesn't make
 *	curses.h load stuff correctly. We can wrap this in a header.
 */

#ifdef __GNUC__
#  if (__STDC__ == 1)
#    define COMPILER_FLAGS -O2 -DUSG
#  else
#    define COMPILER_FLAGS -O2
#  endif
#else   /* remember that c89 exists too */
#  if (__STDC__ == 1)
#    define COMPILER_FLAGS -O -DUSG
#  else
#    define COMPILER_FLAGS -O
#  endif
#endif

/*
 *	Define standard loader flags here: Force BSD compatibility stuff
 */


#define LOADER_FLAGS -lbsd

/*
 *	If your system requires other libraries when linking nn
 *	specify them here:
 */

/*	A/UX CHANGE
 *	Use -lmalloc to support the use of malloc configured above.
 */

#define EXTRA_LIB -lmalloc

/*
 *	Define NO_SIGINTERRUPT on BSD based systems which don't have
 *	a siginterrupt() function, but provides an SV_INTERRUPT flag
 *	in <signal.h>.
 */

#define NO_SIGINTERRUPT      /* */

/*
 *	Depending on whether compiled with cc, c89 or gcc, the type of
 *	signal handlers differ... wrap it
 */

#undef SIGNAL_HANDLERS_ARE_VOID
#ifdef __STDC__
#define SIGNAL_HANDLERS_ARE_VOID
#endif

/*
 * Misc stuff: A/UX has truncate() and can resize... Before RESIZING
 * was defined in config.h but it's here now...
 */

#define RESIZING
#define HAVE_TRUNCATE
