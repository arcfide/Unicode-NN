/*
 *	This version is for Microport UNIX V.2 machines.
 */

#include "s-sys5.h"

#undef SIGNAL_HANDLERS_ARE_VOID
#undef HAVE_MKDIR

/*
 *	Specify where the Bourne Shell is.
 */

#define AVOID_SHELL_EXEC

#undef	SHELL
#define	SHELL		"/bin/ksh"

/*
 *	Use P.D. malloc library for the 286.
 *	This may not be needed if you don't have it...
 */

#define EXTRA_LIB	 -lm286
