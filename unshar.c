/****************************************************************
 * unshar.c: Unpackage one or more shell archive files
 *
 * Description:	unshar is a filter which removes the front part
 *		of a file and passes the rest to the 'sh' command.
 *		It understands phrases like "cut here", and also
 *		knows about shell comment characters and the Unix
 *		commands "echo", "cat", and "sed".
 *
 * HISTORY
 *  27-July-88  Kim F. Storm (storm@texas.dk) Texas Instruments, Denmark
 *	Adapted to :unshar command in nn
 *	Added static to function declarations
 *	Removed all unused functions, i.e. not useful as stand-alone pgm.
 * 29-Jan-85  Michael Mauldin (mlm) at Carnegie-Mellon University
 *	Created.
 ****************************************************************/

#include "config.h"
#include "global.h"
#include "nn_term.h"

/* unshar.c */

static int      stlmatch(char *big, char *small);
static int      smatch(register char *dat, register char *pat, register char **res);

extern long     ftell();


/*****************************************************************
 * stlmatch  --  match leftmost part of string
 *
 * Usage:  i = stlmatch (big,small)
 *	int i;
 *	char *small, *big;
 *
 * Returns 1 iff initial characters of big match small exactly;
 * else 0.
 *
 * HISTORY
 * 18-May-82 Michael Mauldin (mlm) at Carnegie-Mellon University
 *      Ripped out of CMU lib for Rog-O-Matic portability
 * 20-Nov-79  Steven Shafer (sas) at Carnegie-Mellon University
 *	Rewritten for VAX from Ken Greer's routine.
 *
 *  Originally from klg (Ken Greer) on IUS/SUS UNIX
 *****************************************************************/

static int
stlmatch(char *big, char *small)
{
    register char  *s, *b;
    s = small;
    b = big;
    do {
	if (*s == '\0')
	    return (1);
    }
    while (*s++ == *b++);
    return (0);
}

/*****************************************************************
 * smatch: Given a data string and a pattern containing one or
 * more embedded stars (*) (which match any number of characters)
 * return true if the match succeeds, and set res[i] to the
 * characters matched by the 'i'th *.
 *****************************************************************/

static int
smatch(register char *dat, register char *pat, register char **res)
{
    register char  *star = 0, *starend = 0, *resp = 0;
    int             nres = 0;

    while (1) {
	if (*pat == '*') {
	    star = ++pat;	/* Pattern after * */
	    starend = dat;	/* Data after * match */
	    resp = res[nres++];	/* Result string */
	    *resp = '\0';	/* Initially null */
	} else if (*dat == *pat) {	/* Characters match */
	    if (*pat == '\0')	/* Pattern matches */
		return (1);
	    pat++;		/* Try next position */
	    dat++;
	} else {
	    if (*dat == '\0')	/* Pattern fails - no more */
		return (0);	/* data */
	    if (star == 0)	/* Pattern fails - no * to */
		return (0);	/* adjust */
	    pat = star;		/* Restart pattern after * */
	    *resp++ = *starend;	/* Copy character to result */
	    *resp = '\0';	/* null terminate */
	    dat = ++starend;	/* Rescan after copied char */
	}
    }
}

/****************************************************************
 * position: position 'fil' at the start of the shell command
 * portion of a shell archive file.
 * Kim Storm: removed static variables
 ****************************************************************/

int
unshar_position(FILE * fil)
{
    char            buf[BUFSIZ];
    long            pos;

    /* Results from star matcher */
    char            res1[BUFSIZ], res2[BUFSIZ], res3[BUFSIZ], res4[BUFSIZ];
    char           *result[4];

    result[0] = res1, result[1] = res2, result[2] = res3, result[3] = res4;

    /* rewind (fil);  */

    while (1) {
	/* Record position of the start of this line */
	pos = ftell(fil);

	/* Read next line, fail if no more */
	if (fgets(buf, BUFSIZ, fil) == NULL) {
	    msg("no shell commands in file");
	    return (0);
	}
	/* Bail out if we see C preprocessor commands or C comments */
	if (stlmatch(buf, "#include") || stlmatch(buf, "# include") ||
	    stlmatch(buf, "#define") || stlmatch(buf, "# define") ||
	    stlmatch(buf, "#ifdef") || stlmatch(buf, "# ifdef") ||
	    stlmatch(buf, "#ifndef") || stlmatch(buf, "# ifndef") ||
	    stlmatch(buf, "/*")) {
	    msg("file looks like raw C code, not a shar file");
	    return (0);
	}
	/* Does this line start with a shell command or comment */
	if (stlmatch(buf, "#") ||
	    stlmatch(buf, ":") ||
	    stlmatch(buf, "echo ") ||
	    stlmatch(buf, "sed ") ||
	    stlmatch(buf, "cat ")) {
	    fseek(fil, pos, 0);
	    return (1);
	}
	/* Does this line say "Cut here" */
	if (smatch(buf, "*CUT*HERE*", result) ||
	    smatch(buf, "*cut*here*", result) ||
	    smatch(buf, "*TEAR*HERE*", result) ||
	    smatch(buf, "*tear*here*", result) ||
	    smatch(buf, "*CUT*CUT*", result) ||
	    smatch(buf, "*cut*cut*", result)) {
	    /* Read next line after "cut here", skipping blank lines */
	    while (1) {
		pos = ftell(fil);

		if (fgets(buf, BUFSIZ, fil) == NULL) {
		    msg("no shell commands after 'cut'");
		    return (0);
		}
		if (*buf != '\n')
		    break;
	    }

	    /*
	     * Win if line starts with a comment character of lower case
	     * letter
	     */
	    if (*buf == '#' ||
		*buf == ':' ||
		(('a' <= *buf) && ('z' >= *buf))) {
		fseek(fil, pos, 0);
		return (1);
	    }
	    /* Cut here message lied to us */
	    msg("invalid data after CUT line");
	    return (0);
	}
    }
}
