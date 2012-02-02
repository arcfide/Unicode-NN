/*
 *	This version is for dnix version 5.2 on DIAB DS90.
 */

#include "s-sys5.h"

#undef	SIGNAL_HANDLERS_ARE_VOID	/* not void on dnix */

/*
 *	Define AVOID_SHELL_EXEC if the system gets confused by
 *		#!/bin/sh
 *	lines in shell scripts, e.g. only reads #! and thinks it
 *	is a csh script.
 */

#define AVOID_SHELL_EXEC		/* */
