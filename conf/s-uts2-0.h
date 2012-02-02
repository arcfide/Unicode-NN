/*
 *	This version is for Amdahl UTS 2.0
 *	From: neulynne@uts.uni-c.dk (Mogens Lynnerup)
 */

#include "s-sys5.h"

/*
 *	Define HAVE_GETHOSTNAME if your system provides a BSD like
 *	gethostname routine.
 *	Otherwise, define HAVE_UNAME if uname() is avaiable.
 *	As a final resort, define HOSTNAME to the name of your system.
 */

#undef	HAVE_UNAME			/* System V */
#define HAVE_GETHOSTNAME                /* UTS 2.0  */

#define EXTRA_LIB -lsocket              /* To get gethostname routine. */
