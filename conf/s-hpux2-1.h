/*
 *    This version is for HP-UX 2.1 (on HP9000 Series 800)
 */

#include "s-hpux.h"

/*
 *    Define if your system has BSD like job control (SIGTSTP works)
 */

#define HAVE_JOBCONTROL            /* */


/*
 *    Define if your system has a 4.3BSD like syslog library.
 */

#define HAVE_SYSLOG

/*
 *    Define HAVE_GETHOSTNAME if your system provides a BSD like
 *    gethostname routine.
 */

#undef	HAVE_UNAME
#define	HAVE_GETHOSTNAME

/*
 *    Define DETACH_TERMINAL to be a command sequence which
 *    will detatch a process from the control terminal
 */

#undef	DETACH_TERMINAL

/*
 *    Define standard compiler flags here:
 */

#define COMPILER_FLAGS -O -z
