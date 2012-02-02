/*
 *	For HP-UX 8.0  
 *	Derived from HP-UX 7.0 by Bart Muyzer, 08 Jan 1992.
 *
 *	7.0 version from: Rick Low, MEL Defence Systems Ltd, Ottawa,
 *                        Canada, 11 Jun 90  
 *
 *	Uses setsid() instead of setpgrp() as described in 7.0 release
 *	notes page 5-1.
 */

#include "s-hpux.h"

#define	SIGNAL_HANDLERS_ARE_VOID	/* */

#define EXTRA_LIB	-lBSD

#undef DETACH_TERMINAL
extern pid_t setsid();
#define DETACH_TERMINAL setsid();
