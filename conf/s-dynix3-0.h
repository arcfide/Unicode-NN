/*
 *	This version is for Dynix 3.0 systems
 */

#include "s-bsd4-2.h"

#undef HAVE_MULTIGROUP
FILE *popen ();
#define FAKE_INTERRUPT
