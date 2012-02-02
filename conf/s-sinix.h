/*
 *	For Siemens SINIX system on MX300 (use with m-mx300.h)
 *	From: Bill Wohler <wohler@sap-ag.de>
 */

#include <sys/types.h>
#include <sys/stat.h>

#define TERMLIB	-ltermcap
#define HAVE_STRCHR			/* */
#define	RESET_SIGNAL_WHEN_CAUGHT	/* */

/*
 *	Define if your system has BSD like job control (SIGTSTP works)
 *	SINIX has job control but in our configuration, doesn't define
 *	it.
 */
#ifndef SIGSTOP
#define SIGSTOP		17
#endif
#ifndef SIGTSTP
#define SIGTSTP   	18
#endif
#define HAVE_JOBCONTROL			/* */

#define	HAVE_DIRECTORY			/* */
#include <dir.h>				/* BSD */
typedef struct direct Direntry;		/* BSD */

#define	HAVE_MKDIR			/* */

#define HAVE_GETHOSTNAME			/* BSD systems */

#define HAVE_MULTIGROUP	/* BSD */

#define SHELL		"/bin/sh"

#define	MAILX	"/usr/ucb/Mail"		/* BSD */

#define	FILENAME 	1280

/*
 *	NNTP support requires tcp/ip with socket interface.
 *
 *	Define NO_RENAME if the rename() system call is not available.
 *	Define EXCELAN if the tcp/ip package is EXCELAN based.
 *	Define NNTP_EXTRA_LIB to any libraries required only for nntp.
 */

/* #define NO_RENAME			/* */
/* #define EXCELAN			/* */
/* #define NNTP_EXTRA_LIB -lsocket	/* */

/*
 *	Define standard compiler flags here:
 */

#define COMPILER_FLAGS		-tp -W0

