/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	String/Subject matching routines.
 */

#include <ctype.h>
#include "config.h"
#include "global.h"
#include "regexp.h"

int             case_fold_search = 1;

#define MAXFOLD	256		/* max length of any string */

/*
 *	Systems which have a tolower(c) function which is defined for
 *	all characters, but no _tolower(c) macro which works for
 *	isupper(c) only, may define HAVE_GENERIC_TOLOWER -- it
 *	may give a slight speed-up, but is not mandatory.
 */

#ifndef HAVE_GENERIC_TOLOWER

#ifndef _tolower
#define _tolower(c) tolower(c)
#endif

#endif

void
fold_string(register char *mask)
{				/* convert mask to lower-case */
    register char   c;

    for (; (c = *mask); mask++) {

#ifdef _tolower
	if (!isascii(c) || !isupper(c))
	    continue;
	*mask = _tolower(c);
#else
	*mask = tolower(c);
#endif
    }
}

static int
streq_fold(register char *mask, register char *str)
{				/* mask is prefix of str - FOLD */
    register char   c = 0, d;

    while ((d = *mask++)) {
	if ((c = *str++) == NUL)
	    return 0;
	if (c == d)
	    continue;

#ifdef _tolower
	if (!isascii(c) || !isupper(c) || _tolower(c) != (unsigned) d)
	    return 0;
#else
	if (tolower(c) != d)
	    return 0;
#endif
    }
    return c == NUL ? 1 : 2;
}

int
strmatch_fold(char *mask, register char *str)
{				/* mask occurs anywhere in str - FOLD */
    register char   c, m1 = *mask++;

    for (;;) {
	while ((c = *str++)) {	/* find first occ. of mask[0] in str. */
	    if (c == m1)
		break;

#ifdef _tolower
	    if (!isascii(c) || !isupper(c))
		continue;
	    if (_tolower(c) == (unsigned) m1)
		break;
#else
	    if (tolower(c) == m1)
		break;
#endif
	}
	if (c == NUL)
	    return 0;
	if (streq_fold(mask, str))
	    return 1;
    }
}

int
strmatch(char *mask, register char *str)
{				/* mask occurs anywhere in str - CASE */
    register char  *q, *m;
    register char   m1 = *mask;

    for (; *str; str++) {
	if (*str != m1)
	    continue;

	q = str;
	m = mask;
	do
	    if (*++m == NUL)
		return 1;
	while (*++q == *m);
    }
    return 0;
}

int
strmatch_cf(char *mask, char *str)
{				/* fold if case_fold_search is set */
    if (case_fold_search)
	return strmatch_fold(mask, str);

    return strmatch(mask, str);
}

/*
 *	case insensitive regexp matching
 */

int
regexec_fold(register regexp * prog, char *string)
{
    char            buf[256];
    register char   c, *bp, *str, *maxb;

    bp = buf, maxb = &buf[255];
    str = string;
    while (bp < maxb && (c = *str++) != NUL)

#ifdef _tolower
	*bp++ = (!isascii(c) || !isupper(c)) ? c : _tolower(c);
#else
	*bp++ = tolower(c);
#endif

    *bp = NUL;

    if (!regexec(prog, buf))
	return 0;

    prog->startp[0] = string + (prog->startp[0] - buf);
    prog->endp[0] = string + (prog->endp[0] - buf);
    return 1;
}

int
regexec_cf(register regexp * prog, char *string)
{
    if (case_fold_search)
	return regexec_fold(prog, string);

    return regexec(prog, string);
}
