/*
 *	This is for Dynix/PTX.
 *	From: Jaap Vermeulen <sequent!jaap>
 */

#include "s-sys5.h"

#undef TERMLIB
#define TERMLIB	-ltermlib

#define HAVE_JOBCONTROL

#define USE_MALLOC_H

#define MALLOC_GRAIN            sizeof(double)          /* M_GRAIN */
#define MALLOC_MAXFAST  	(MALLOC_GRAIN*4)        /* M_MXFAST */
#define MALLOC_FASTBLOCKS       100                     /* M_NLBLKS */

#define EXTRA_LIB -lmalloc
