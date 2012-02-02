/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Pack sender's name into something sensible, return in packed:
 * 	pack_name(packed, name, length)
 *
 */

#include <string.h>
#include "config.h"
#include "global.h"

 /* #define NAME_TEST *//* Never define this ! */

#define	SEP_DOT		0	/* . */
#define	SEP_PERCENT	1	/* % */
#define	SEP_SCORE	2	/* _ */
#define	SEP_AMPERSAND	3	/* @ */
#define	SEP_BANG	4	/* ! */
#define	SEP_MAXIMUM	5


#define CL_OK		0x0100	/* letter or digit */
#define CL_SPACE	0x0200	/* cvt to space */
#define CL_IGNORE	0x0400	/* ignore */
#define CL_RANGE(c)	0x0800+c/* space range, end with c */
#define CL_HYPHEN	0x1000	/* convert to - */
#define CL_STOP		0x2000	/* discard rest of name */
#define	CL_SEP		| 0x4000 +	/* address separator */

#define	IS_OK(c)	(Class[c] & CL_OK)
#define IS_SPACE(c)	(Class[c] & CL_SPACE)
#define IGNORE(c)	(c & 0x80 || Class[c] & CL_IGNORE)
#define BEGIN_RANGE(c)	(Class[c] & CL_RANGE(0))
#define END_RANGE(c)	(Class[c] & 0xff)
#define IS_HYPHEN(c)	(Class[c] & CL_HYPHEN)
#define IS_STOP(c)	(Class[c] & CL_STOP)
#define	IS_SEPARATOR(c)	(Class[c] & (0 CL_SEP 0))

int             old_packname = 0;	/* Default to new behavior */

static short    Class[128] = {
     /* NUL */ CL_STOP,
     /* SOH */ CL_IGNORE,
     /* STX */ CL_IGNORE,
     /* ETX */ CL_IGNORE,
     /* EOT */ CL_IGNORE,
     /* ENQ */ CL_IGNORE,
     /* ACK */ CL_IGNORE,
     /* BEL */ CL_IGNORE,
     /* BS  */ CL_IGNORE,
     /* TAB */ CL_SPACE,
     /* NL  */ CL_IGNORE,
     /* VT  */ CL_IGNORE,
     /* FF  */ CL_IGNORE,
     /* CR  */ CL_IGNORE,
     /* SO  */ CL_IGNORE,
     /* SI  */ CL_IGNORE,
     /* DLE */ CL_IGNORE,
     /* DC1 */ CL_IGNORE,
     /* DC2 */ CL_IGNORE,
     /* DC3 */ CL_IGNORE,
     /* DC4 */ CL_IGNORE,
     /* NAK */ CL_IGNORE,
     /* SYN */ CL_IGNORE,
     /* ETB */ CL_IGNORE,
     /* CAN */ CL_IGNORE,
     /* EM  */ CL_IGNORE,
     /* SUB */ CL_IGNORE,
     /* ESC */ CL_IGNORE,
     /* FS  */ CL_IGNORE,
     /* GS  */ CL_IGNORE,
     /* RS  */ CL_IGNORE,
     /* US  */ CL_IGNORE,

     /* space */ CL_SPACE,
     /* !   */ CL_SPACE CL_SEP SEP_BANG,
     /* "   */ CL_RANGE('"'),
     /* #   */ CL_OK,
     /* $   */ CL_OK,
     /* %   */ CL_OK CL_SEP SEP_PERCENT,
     /* &   */ CL_OK,
     /* '   */ CL_OK,
     /* (   */ CL_OK,
     /* )   */ CL_OK,
     /* *   */ CL_HYPHEN,
     /* +   */ CL_HYPHEN,
     /* ,   */ CL_STOP,
     /* -   */ CL_HYPHEN,
     /* .   */ CL_SPACE CL_SEP SEP_DOT,
     /* /   */ CL_OK,
     /* 0   */ CL_OK,
     /* 1   */ CL_OK,
     /* 2   */ CL_OK,
     /* 3   */ CL_OK,
     /* 4   */ CL_OK,
     /* 5   */ CL_OK,
     /* 6   */ CL_OK,
     /* 7   */ CL_OK,
     /* 8   */ CL_OK,
     /* 9   */ CL_OK,
     /* :   */ CL_IGNORE,
     /* ;   */ CL_STOP,
     /* <   */ CL_IGNORE,
     /* =   */ CL_HYPHEN,
     /* >   */ CL_IGNORE,
     /* ?   */ CL_IGNORE,
     /* @   */ CL_OK CL_SEP SEP_AMPERSAND,
     /* A   */ CL_OK,
     /* B   */ CL_OK,
     /* C   */ CL_OK,
     /* D   */ CL_OK,
     /* E   */ CL_OK,
     /* F   */ CL_OK,
     /* G   */ CL_OK,
     /* H   */ CL_OK,
     /* I   */ CL_OK,
     /* J   */ CL_OK,
     /* K   */ CL_OK,
     /* L   */ CL_OK,
     /* M   */ CL_OK,
     /* N   */ CL_OK,
     /* O   */ CL_OK,
     /* P   */ CL_OK,
     /* Q   */ CL_OK,
     /* R   */ CL_OK,
     /* S   */ CL_OK,
     /* T   */ CL_OK,
     /* U   */ CL_OK,
     /* V   */ CL_OK,
     /* W   */ CL_OK,
     /* X   */ CL_OK,
     /* Y   */ CL_OK,
     /* Z   */ CL_OK,
     /* [   */ CL_OK,
     /* \   */ CL_OK,
     /* ]   */ CL_OK,
     /* ^   */ CL_IGNORE,
     /* _   */ CL_SPACE CL_SEP SEP_SCORE,
     /* `   */ CL_IGNORE,
     /* a   */ CL_OK,
     /* b   */ CL_OK,
     /* c   */ CL_OK,
     /* d   */ CL_OK,
     /* e   */ CL_OK,
     /* f   */ CL_OK,
     /* g   */ CL_OK,
     /* h   */ CL_OK,
     /* i   */ CL_OK,
     /* j   */ CL_OK,
     /* k   */ CL_OK,
     /* l   */ CL_OK,
     /* m   */ CL_OK,
     /* n   */ CL_OK,
     /* o   */ CL_OK,
     /* p   */ CL_OK,
     /* q   */ CL_OK,
     /* r   */ CL_OK,
     /* s   */ CL_OK,
     /* t   */ CL_OK,
     /* u   */ CL_OK,
     /* v   */ CL_OK,
     /* w   */ CL_OK,
     /* x   */ CL_OK,
     /* y   */ CL_OK,
     /* z   */ CL_OK,
     /* {   */ CL_OK,
     /* |   */ CL_OK,
     /* }   */ CL_OK,
     /* ~   */ CL_HYPHEN,
     /* DEL  */ CL_IGNORE
};


int
pack_name(char *dest, char *source, int length)
{
    register char  *p, *q, *r, c;
    register int    n;
    register int    i;
    register int    sep;
    register int    ignore_braces = 0;
    register int    ignore_comment = 0;
    register int    rfc_addr = 0;
    register char  *name;
    register int    lname;
    register int    drop_space, prev_space;
    char           *maxq;
    int             lfirst, lmiddle, llast;
    char            namebuf[129];
    char           *separator[SEP_MAXIMUM];

    dest[0] = NUL;

    if (source == NULL || source[0] == NUL)
	return 0;

    p = source;
    if (old_packname == 0) {
	while ((c = *p++)) {
	    if (c == '<') {
		ignore_comment = 1;
		rfc_addr = 1;
		break;
	    }
	}
    }
full_scan:
    p = source, q = namebuf, n = 0;
    maxq = namebuf + sizeof namebuf - 1;

new_partition:
    for (i = SEP_MAXIMUM; --i >= 0; separator[i] = NULL);

    while ((c = *p++)) {
	if (c == '<') {
	    while (q > namebuf && q[-1] == SP)
		q--;
	    if (q == namebuf)
		continue;
	    break;
	}
	if (c == '(' && ignore_comment)
	    break;
	if (!rfc_addr) {
	    if (c == ')') {
		if (--n == 0 && !ignore_braces)
		    break;
		continue;
	    }
	}
	if (IGNORE((int8) c))
	    continue;
	if (q == namebuf && IS_SPACE((int8) c))
	    continue;

	if (!rfc_addr) {
	    if (c == '(') {
		if (n++ == 0 && !ignore_braces) {
		    q = namebuf;
		    goto new_partition;
		}
		continue;
	    }
	}
	if (n > 0)
	    if (ignore_braces || n > 1)
		continue;
	if (q >= maxq)
	    break;
	*q++ = c;
	if (IS_SEPARATOR((int8) c)) {
	    switch ((sep = (Class[(int8) c] & 0xff))) {

		case SEP_DOT:
		    if (separator[SEP_AMPERSAND] && q - namebuf <= length)
			break;
		    continue;

		case SEP_BANG:
		    if (separator[SEP_AMPERSAND])
			continue;
		    break;

		default:
		    if (separator[sep])
			continue;
		    break;
	    }

	    separator[sep] = q - 1;
	}
    }

    *q = NUL;

    if (namebuf[0] == NUL && !ignore_braces) {
	ignore_braces = 1;
	goto full_scan;
    }
    if (namebuf[0] == NUL)
	return 0;

    name = namebuf;

    if (name[0] == '"') {
	name++;
	if (q[-1] == '"')
	    *--q = NUL;
    }
    if (q - name <= length)
	goto name_ok;

    /* sorry for the many goto's -- the 3B2 C compiler does not */
    /* make correct code for complicated logical expressions!!  */
    /* not even without -O					 */

    /* We must pack the name to make it fit */

    /* Name_of_person%... -> Name_of_person */

    if ((r = separator[SEP_PERCENT])) {
	if (!(q = separator[SEP_SCORE]) || q > r)
	    goto no_percent;
	if ((q = separator[SEP_AMPERSAND]) && q < r)
	    goto no_percent;
	if ((q = separator[SEP_BANG]) && q < r)
	    goto no_percent;
	*r = NUL;
	goto parse_name;
    }
no_percent:

    /* name@site.domain -> name@site */

    if ((r = separator[SEP_AMPERSAND])) {

	if ((q = separator[SEP_PERCENT]) && q < r) {
	    *r = NUL;
	    if (r - name <= length)
		goto name_ok;

	    *q = NUL;

	    if (((p = separator[SEP_BANG]) && p < q)
		|| ((p = strrchr(name, '!')) && p < q)) {
		name = p + 1;
	    }
	    if (strchr(name, '.'))
		goto parse_name;

	    goto name_ok;
	}
	if ((q = separator[SEP_DOT])) {
	    *q = NUL;
	    goto name_ok;
	}
	*r = NUL;
	if (r - name <= length)
	    goto name_ok;

	if ((q = separator[SEP_BANG]) && q < r) {
	    name = q + 1;
	    goto name_ok;
	}

#ifdef NOTDEF
	if (strchr(name, '!') == NULL)
	    goto parse_name;	/* take the chance ... */
#endif

	goto name_ok;		/* can't do it any better */
    }
    /* Phase 1: Normalization (remove superfluous characters) */

parse_name:

    for (p = name, lname = 0, prev_space = 0; (c = *p); p++) {

/*
	if (IGNORE(c)) {
	    *p = TAB;
	    if (p == name) name++;
	    continue;
	}
*/

	if (IS_OK((int8) c)) {
	    lname++;
	    prev_space = 0;
	    continue;
	}
	if (IS_HYPHEN((int8) c)) {
	    if (p == name) {
		name++;
		continue;
	    }
	    if (prev_space) {
		*p = NUL;
		break;		/* strip from " -" */
		/* *p = TAB; *//* do not strip from " -" */
	    } else {
		*p = '-';
		lname++;
	    }
	    continue;
	}
	if (BEGIN_RANGE((int8) c)) {

	    if (p == name) {
		name++;
		continue;
	    }
	    c = END_RANGE((int8) c);
	    for (q = p + 1; *q && *q != c; q++);
	    if (*q) {
		if (p[-1] != ' ')
		    lname++;
		while (p <= q)
		    *p++ = ' ';
		p--;
		prev_space++;
		continue;
	    }
	    c = ' ';
	}
	if (IS_SPACE((int8) c)) {
	    *p = ' ';
	    if (p == name)
		name++;
	    else if (!prev_space) {
		lname++;
		prev_space++;
	    }
	    continue;
	}
	if (IS_STOP((int8) c)) {
	    *p = NUL;
	    break;
	}
    }
drop_last_name:
    while (p > name && (*--p == ' ' || *p == TAB))
	*p = NUL;

    if (lname < length)
	goto name_ok;


    /* Phase 2: Reduce middle names */

    for (r = p, llast = 0; r > name && *r != ' '; r--)
	if (*r != TAB)
	    llast++;

    /* r points to space before last name */

    if (strncmp(r, " Jr", 3) == 0 || strncmp(r, " II", 3) == 0) {
	p = r + 1;
	lname -= llast;
	goto drop_last_name;
    }
    if (r == name)
	goto phase6;		/* only last name */

    for (q = name, lfirst = 0; *q && *q != ' '; q++)
	if (*q != TAB)
	    lfirst++;

    /* q points at space after first name */

    for (p = q, lmiddle = 0; p < r;) {
	/* find next middle name */
	while (p < r && (*p == ' ' || *p == TAB))
	    p++;

	if (p >= r)
	    break;		/* found last name */

	p++;			/* skip first char of middle name */
	for (; *p != ' '; p++) {/* remove rest */
	    if (*p == TAB)
		continue;
	    *p = TAB;
	    lname--;
	}
	lmiddle += 2;		/* initial + space */
    }

    if (lname < length)
	goto name_ok;

    /*
     * If removing middle names is not enough, but reducing first name
     * instead is, do it that way
     */

    if (lname - lmiddle >= length && lname - lfirst + 1 < length)
	goto phase4;


    /* Phase 3: Remove middle names */

    for (p = q; p < r; p++) {
	if (*p == TAB)
	    continue;
	if (*p == ' ')
	    continue;
	*p = TAB;
	lname -= 2;
    }

    if (lname < length)
	goto name_ok;


    /* Phase 4: Reduce first name */

phase4:
    for (p = name + 1; p < q; p++) {
	if (*p == TAB)
	    continue;
	if (*p == ' ')
	    continue;
	if (*p == '-' && (p + 1) < q) {
	    p++;
	    continue;
	}
	*p = TAB;
	lname--;
    }

    if (lname < length)
	goto name_ok;

    /* Phase 5: Remove first name */

    name = r + 1;
    lname--;

    if (lname < length)
	goto name_ok;

    /* Phase 6: Cut last name */
phase6:
    goto name_ok;

name_ok:

    q = dest;
    maxq = q + length;

    drop_space = 1;

    for (p = name; *p && q < maxq; p++) {
	if (*p == TAB)
	    continue;

	if (*p == ' ') {
	    if (!drop_space) {
		drop_space = 1;
		*q++ = ' ';
	    }
	    continue;
	}
	drop_space = 0;
	*q++ = *p;
    }

    *q = NUL;

    return strlen(dest);
}

#ifdef NAME_TEST
main()
{
    char            in[512], out[512];

    while (fgets(in, sizeof(in), stdin)) {
	printf("'%s' -> (%d) ", in, pack_name(out, in, Name_Length));
	puts(out);
    }
    exit(0);
}

#endif
