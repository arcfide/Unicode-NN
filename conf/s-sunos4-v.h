/*
 *	This version is for SUNOS 4.1 SysV environment
 *	From: "Gary Mills" <mills@ccu.umanitoba.ca>
 */

#include "s-sys5.h"

/*
 *      Has a 4.3BSD like syslog library.
 */

#define HAVE_SYSLOG

/*
 *      Have gethostname().
 */

#undef HAVE_UNAME
#define HAVE_GETHOSTNAME

/*
 *      Have multiple group membership.
 */

#define HAVE_MULTIGROUP

/*
 *      Specify the default mailer
 */

#undef MAILX
#define MAILX        "/usr/ucb/mail"

/*
 *      Maximum length of a pathname.
 */

#undef FILENAME
#define FILENAME   1024

