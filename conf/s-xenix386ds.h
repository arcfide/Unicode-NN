/*
 *	For XENIX386 release 2.3.2 development system.
 *	Uses system V libraries and system calls.
 * 	This version uses "termcap", but XENIX also has "terminfo".
 */

#include "s-sys5-tcap.h"

#undef	SIGNAL_HANDLERS_ARE_VOID

#define AVOID_SHELL_EXEC

#undef	MAILX
#define	MAILX	"/usr/bin/mail"		/* SCO XENIX */

/*
 *	Define standard compiler flags here:
 */

#define COMPILER_FLAGS	-O

/*
 *	If your system requires other libraries when linking nn
 *	specify them here:
 */

#define EXTRA_LIB	-ldir
