/*
 *	These defs for GCC 1.34 on a UNIX-PC.  Written by Steve
 *	Simmons (scs@lokkur.dexter.mi.us).  Shoot me, not Kim.
 */

#include "s-sys5-tcap.h"

#undef	SIGNAL_HANDLERS_ARE_VOID

#undef	HAVE_MKDIR

#undef	DETACH_TERMINAL

#undef	MAILX
#define	MAILX		"/bin/mail"

#define COMPILER_FLAGS -g -O
