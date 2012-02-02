/*
 *	This version is for SCO Xenix Sys V Release 2.2.1 (286)
 *	Original from: Paul Shields, shields@nccn.yorku.ca
 *	Uses terminfo!
 */

#include "s-sys5.h"

#undef SIGNAL_HANDLERS_ARE_VOID

#define	HAVE_DIRECTORY			/* */
#include <sys/ndir.h>			/* Xenix System V */
typedef struct direct Direntry;		/* Xenix System V */

#undef HAVE_MKDIR
#undef HAVE_UNAME
#undef DETACH_TERMINAL

#define AVOID_SHELL_EXEC

#undef MAILX
#define	MAILX		"/usr/bin/mail" 	/* SV */

#undef FILENAME
#define	FILENAME 	128

#define COMPILER_FLAGS	-LARGE -Ml2t128 -DM_TERMINFO

/* Default stack space is only 4KB.  Increase is needed but	*/
/* must be limited to 20KB (-F 5000) for nn itself to load.	*/
#define EXTRA_LIB	-ltinfo -lx -F 5000
