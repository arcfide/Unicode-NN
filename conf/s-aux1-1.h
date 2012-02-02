/*
 *	This version is for A/UX 1.1 (approx. System V Release 3.1)
 */

#include "s-sys5.h"

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

#include <sys/dir.h>			/* A/UX */

typedef struct direct Direntry;		/* System V */

/*
 *	Define HAVE_MULTIGROUP if system has simultaneous multiple group
 *	membership capability (BSD style).
 */

#define HAVE_MULTIGROUP

