/*
 *	Definitions for generic SVR4/386
 */

#include "s-sys5.h"

#define HAVE_JOBCONTROL				/* */
#undef	DETACH_TERMINAL 			/* */
#define DETACH_TERMINAL setsid();		/* */
#define HAVE_TRUNCATE				/* */
#define HAVE_SYSLOG				/* */
#define RESIZING				/* */
#define SYSV_RESIZING				/* */
#define NNTP_EXTRA_LIB	-lsocket -lnsl


#include <limits.h>
#ifdef NGROUPS_MAX
# define NGROUPS NGROUPS_MAX
# define HAVE_MULTIGROUP				/* */
#endif
