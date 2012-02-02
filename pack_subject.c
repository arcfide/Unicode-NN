/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Pack subject by eliminating RE prefixes and - (nf) suffixes.
 *	Also collapse multiple blanks into single blanks.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"

int
pack_subject(register char *dest, register char *src, int *re_counter_ptr, int max_length)
{
    register char  *max_dest;
    int             re = 0;
    char           *start_dest = dest;

    if (src) {
	max_dest = dest + max_length;

	while (*src) {
	    if (isspace(*src)) {
		src++;
		continue;
	    }
	    /* count and remove 'Re: Re: ...' */

	    if (*src != 'R' && *src != 'r')
		break;
	    *dest++ = *src++;

	    if (*src != 'e' && *src != 'E')
		break;
	    *dest++ = *src++;

	    if (*src == ':' || *src == ' ') {
		src++;
		dest = start_dest;
		re++;
		continue;
	    }
	    if (*src != '^')
		break;

	    src++;
	    dest = start_dest;

	    while (isdigit(*src))
		*dest++ = *src++;
	    if (dest == start_dest)
		re++;
	    else {
		*dest = NUL;
		dest = start_dest;
		re += atoi(dest);
	    }
	    if (*src == ':')
		src++;
	}

	while (*src && dest < max_dest) {
	    if (*src == '-' && strncmp("- (nf)", src, 5) == 0)
		break;
	    if (isascii(*src) && isspace(*src)) {
		do
		    src++;
		while (isascii(*src) && isspace(*src));
		*dest++ = SP;
	    } else
		*dest++ = *src++;
	}
    }
    *dest = NUL;
    *re_counter_ptr = (char) re;

    return dest - start_dest;
}
