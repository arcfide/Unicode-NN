/*
 *	System V based system
 *	TERMCAP is used instead of TERMINFO
 */

#include "s-sys5.h"

#undef USE_TERMINFO
#undef TERMLIB

#define TERMLIB -ltermlib
