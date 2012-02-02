/*
 *	s-xenix68k.h
 *	for Tandy 6000's running Tandy 68000/Xenix Version 3.2
 *	(including the 3.2 development system)
 *	Created by paul@devon.lns.pa.us (Paul Sutcliffe Jr.)
 *	
 *	Assumes presense of D.Gwyn's directory(3) routines in -ldirent
 */

#include "s-sys5-tcap.h"

#undef	SIGNAL_HANDLERS_ARE_VOID	/* */
#undef	HAVE_MKDIR			/* */
#define AVOID_SHELL_EXEC		/* */

#undef MAILX
#define	MAILX		"/usr/bin/mail"

#define EXTRA_LIB	-lx -ldirent
