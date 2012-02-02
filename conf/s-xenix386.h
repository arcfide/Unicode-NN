/*
 *  Thu Jun 29 18:55:47 1989 - Chip Rosenthal <chip@vector.Dallas.TX.US>
 *	Generated SCO XENIX/386 version from "s-template.h".  XENIX has
 *	both "termcap" and "terminfo".  This version reflects the local
 *	preference for "termcap".
 */

#include "s-sys5-tcap.h"

#undef SIGNAL_HANDLERS_ARE_VOID

#define	HAVE_DIRECTORY			/* */
#include <sys/ndir.h>			/* SCO XENIX */
typedef struct direct Direntry;		/* BSD and SCO XENIX */

#undef HAVE_MKDIR

#undef	DETACH_TERMINAL

/*
 *	Define AVOID_SHELL_EXEC if the system gets confused by
 *		#!/bin/sh
 *	lines in shell scripts, e.g. only reads #! and thinks it
 *	is a csh script.
 */

#define AVOID_SHELL_EXEC		/* */

/*
 *	Specify the default mailer
 */

#undef	MAILX
#define	MAILX	"/usr/bin/mail"		/* SCO XENIX */

/*
 *	If your system requires other libraries when linking nn
 *	specify them here:
 */

#define EXTRA_LIB	-lx
