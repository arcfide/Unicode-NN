/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Reply address rewriting.
 */

#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"


int
reroute(char *route, char *address)
{
    char           *name, *atpos;
    register char  *sp;
    register int    c;

    if ((atpos = strchr(address, '@'))) {
	name = atpos;

	while (--name >= address)
	    if (isspace(*name) || *name == '<') {
		name++;
		break;
	    }
	if (name < address)
	    name++;

	for (sp = atpos; (c = *sp); sp++)
	    if (isspace(c) || c == '>')
		break;

	*sp = NUL;
	strcpy(route, name);
	*sp = c;
    } else
	strcpy(route, address);
    return 1;
}
