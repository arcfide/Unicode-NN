/*
 *	For SunOS 5.x (which is a component of Solaris 2.x)
 *	From: Richard M. Mathews (Richard.Mathews@West.Sun.COM)  1/7/93
 *		Mike Sullivan (Mike.Sullivan@Eng.Sun.COM)
 *	Works with both Solaris for SPARC and Solaris for x86.
 *
 *      Do *not* compile with /usr/ucb/cc or any of the BSD compatibility
 *      stuff in /usr/ucblib.
 */

/*
 *	Sys V plus some
 */

/* moved generic SVR4 stuff to s-sys5-4.h */

#include "s-sys5-4.h"

/* add and Solaris/SunOS5 *specific* stuff here */

#define USEGETPASSPHRASE
#define COMPILER_FLAGS -D_MSE_INT_H -D__EXTENSIONS__
