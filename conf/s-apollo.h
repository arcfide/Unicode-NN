/*
 *	This version is for Apollo Domain/OS systems running BSD 4.3.
 *	From: mmitchel@digi.lonestar.org (Mitch Mitchell)
 *
 *	Notice: If you use the ANSI compiler and have installed the SR10.3
 *	ANSI stuff, you will have to #define SIGNAL_HANDLERS_ARE_VOID below.
 *	(You should really do it in config.h though for portability! ++Kim)
 *	From: rog@speech.kth.se (Roger Lindell)
 */

#include "s-bsd4-3.h"

#define	SIGNAL_HANDLERS_ARE_VOID	/* ANSI compiler */
#define RESIZING

/*
 *	special sleep function that terminates following
 *      the receipt of a signal  (needed for BSD Unix).
 */

#define HAVE_HARD_SLEEP

/*
 *	Specify where the Korne Shell is. (instead of Borne Shell)
 *	The Bourne Shell is somewhat unreliable (at SR10.2).
 */

#ifdef  SHELL
#undef  SHELL
#endif
#define SHELL		"/bin/ksh"

/*
 *	Allow specification of file names to begin with a double
 *      slash "//" so that Apollo node names of form "//node" can
 *      occur in folder and file specifications.
 */

#define ALLOW_LEADING_DOUBLE_SLASH

/*
 *	Cause nnmaster to keep a CLIENT copy of the MASTER file to overcome
 *	problems with network write access.
 */

#define APOLLO_DOMAIN_OS

#undef NO_MEMMOVE
