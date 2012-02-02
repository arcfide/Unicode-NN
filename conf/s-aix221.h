/*
 *	This version is for AIX 2.2.1
 *	From: marcel@duteca.tudelft.nl (Marcel J.E. Mol)
 */

/*
 *	Notice: AIX's PRINTER command is "print"  (see config.h)
 */

#include <s-sys5.h>

/*
 *    AIX does have void signal handlers, but
 *    a   typedef void   does not work
 */

#undef	SIGNAL_HANDLERS_ARE_VOID

/*
 *	Define HAVE_GETHOSTNAME if your system provides a BSD like
 *	gethostname routine.
 */

#undef  HAVE_UNAME
#define	HAVE_GETHOSTNAME

