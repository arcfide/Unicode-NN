/*
 *	For SCO UNIX System V/386
 *	From: Curtis Galloway <curtisg@sco.com>
 *	Uses system V libraries and system calls.
 * 	This version uses "terminfo".
 */

#include "s-sys5.h"

#define AVOID_SHELL_EXEC

/* Modified for SCO 3.2.4 by Peter Wemm */
/* #undef SIGNAL_HANDLERS_ARE_VOID  removed - Peter */

/*
 *	Define standard compiler flags here:
 */

#define COMPILER_FLAGS

/*
 *	If your system requires other libraries when linking nn
 *	specify them here:
 */

#define EXTRA_LIB	-lmalloc -lx
#define NNTP_EXTRA_LIB	-lsocket

/* Added for SCO 3.2.4 */
#define HAVE_JOBCONTROL
#undef  DETACH_TERMINAL
#define DETACH_TERMINAL setsid();
#define RESIZING
#define SYSV_RESIZING
