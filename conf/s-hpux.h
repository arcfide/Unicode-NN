/*
 *	This version is for Hewlett-Packard HP-UX
 */

#include "s-sys5.h"

#undef	SIGNAL_HANDLERS_ARE_VOID	/* */

/*
 *	Define if your system provides the "directory(3X)" access routines
 *
 *	If true, include the header file(s) required by the package below
 *	(remember that <sys/types.h> or equivalent is included above)
 *	Also typedef Direntry to the proper struct type.
 */

#define	HAVE_DIRECTORY			/* */

#include <ndir.h>			/* HP-UX */

typedef struct direct Direntry;		/* HP-UX */
