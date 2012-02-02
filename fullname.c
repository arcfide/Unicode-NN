/*
 * This file is derived from Bnews' fullname.c file.
 * Bnews is Copyright (c) 1986 by Rick Adams.
 *
 * NOTICE: THIS CODE HAS BEEN MODIFIED TO FIT THE NN ENVIRONMENT:
 *
 *	The full_name function has been rewritten entirely, although
 *	there are still some structural resemblence.
 *	Fullname checks $NAME before looking at /etc/passwd.
 *	The LOCALNAME alternative has been removed, because it would fit
 *	nn very poorly.
 *	The buildfname function is made static and moved before full_name.
 *
 * fullname.c - this file is made separate so that different local
 * conventions can be applied.  The stock version understands two
 * conventions:
 *
 * (a) Berkeley finger: the gecos field in /etc/passwd begins with
 *     the full name, terminated with comma, semicolon, or end of
 *     field.  & expands to the login name.
 * (b) BTL RJE: the gecos field looks like
 *	: junk - full name ( junk :
 *     where the "junk -" is optional.
 *
 * If you have a different local convention, modify this file accordingly.
 */

#ifdef SCCSID
static char    *SccsId = "@(#)fullname.c	1.13	11/4/87";
#endif				/* SCCSID */

#include <stdlib.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"


/* fullname.c */

static void     buildfname(register char *p, char *login, char *buf);



/*
**  BUILDFNAME -- build full name from gecos style entry.
**	(routine lifted from sendmail)
**
**	This routine interprets the strange entry that would appear
**	in the GECOS field of the password file.
**
**	Parameters:
**		p -- name to build.
**		login -- the login name of this user (for &).
**		buf -- place to put the result.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

static void
buildfname(register char *p, char *login, char *buf)
{
    register char  *bp = buf;

    if (*p == '*')
	p++;
    while (*p != '\0' && *p != ',' && *p != ';' && *p != ':' && *p != '(') {
	if (*p == '-' && (isdigit(p[-1]) || isspace(p[-1]))) {
	    bp = buf;
	    p++;
	} else if (*p == '&') {
	    strcpy(bp, login);
	    if ((bp == buf || !isalpha(bp[-1])) && islower(*bp))
		*bp = toupper(*bp);
	    while (*bp != '\0')
		bp++;
	    p++;
	} else
	    *bp++ = *p++;
    }
    *bp = '\0';
}

/*
 * Figure out who is sending the message and sign it.
 * We attempt to look up the user in the gecos field of /etc/passwd.
 */
char           *
full_name(void)
{
    static char    *fullname = NULL;
    char            inbuf[FILENAME];
    struct passwd  *pw;

    if (fullname == NULL) {
	if ((fullname = getenv("NAME")) != NULL)
	    return fullname;

	pw = getpwuid(user_id);
	if (pw == NULL)
	    return fullname = "?";

	buildfname(pw->pw_gecos, pw->pw_name, inbuf);
	if (inbuf[0] == 0)
	    strcpy(inbuf, pw->pw_name);

	fullname = copy_str(inbuf);
    }
    return fullname;
}
