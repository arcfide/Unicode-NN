/*
 * Decode one or more uuencoded article back to binary form.
 *
 * UNIX/NN VERSION
 *	This version cannot be used as a stand-alone uud!
 * 	This version is made: 16 June 1989.
 *
 * From the Berkeley original, modified by MSD, RDR, JPHD & WLS.
 */

#include <unistd.h>
#include <string.h>
#include "config.h"
#include "global.h"
#include "save.h"
#include "nn_term.h"

/* decode.c */

static int      strncmp_skip(char *buf, char *str, int n);
static int      decode_line(char *buf, register int len, int dont_write);
static void     inittbls(void);
static int      gettable(FILE * in);
static void     new_file(void);

 /* #define DEC_DEBUG *//* never define this */

char           *decode_header_file = "Decode.Headers";
int             decode_skip_prefix = 2;
int             decode_keep = 0;

#define MAXCHAR 256
#define LINELEN 256
#define NORMLEN 60		/* allows for 80 encoded chars per line */

#define SEQMAX 'z'
#define SEQMIN 'a'

static char     seqc, partn;
static int      first, secnd, check;

#define MAX_PREFIX 10
static int      prefix_lgt, set_prefix;
static char     prefix_str[MAX_PREFIX];

static FILE    *out;
static char    *target;
static char     blank;
static int      chtbl[MAXCHAR], cdlen[NORMLEN + 3];
static char     ofname[FILENAME], arcname[FILENAME];
static int      state, arcpart;

#define	NO_ADVANCE		0x10

#define	DECODE_TEXT			1
#define	FIND_BEGIN			2
#define	FIND_BEGIN_AFTER_ERROR		3
#define FIND_BEGIN_AFTER_INCLUDE	4
#define NEW_BEGIN			(5 | NO_ADVANCE)
#define	FOUND_END	       		(6 | NO_ADVANCE)
#define FOUND_INCLUDE			(7 | NO_ADVANCE)
#define	SKIP_LEADING			8
#define	SKIP_TRAILING			(9 | NO_ADVANCE)
#define DECODE_ERROR			(10 | NO_ADVANCE)
#define OTHER_ERROR			(11 | NO_ADVANCE)

#define MIN_DECODE_LEADING 8	/* lines to decode ok when doing skip-leading */

#ifdef DEC_DEBUG
char           *state_tbl[] = {
    "-", "decode", "find begin", "find a/error", "find a/include",
    "new begin", "found end", "found include", "skip leading",
    "skip trail", "error", "other error"
};

#endif

/*
 * decode one line, write on out file
 */

static int
strncmp_skip(char *buf, char *str, int n)
{
    register int    i;
    register char  *line = buf;

    if (!set_prefix)
	return strncmp(line, str, n);

    if (decode_skip_prefix > MAX_PREFIX)
	decode_skip_prefix = MAX_PREFIX;

    for (i = 0; i <= decode_skip_prefix; i++, line++) {
	if (*line == NUL)
	    break;
	if (*line == '#' || *line == ':')
	    break;
	if (strncmp(line, str, n))
	    continue;
	prefix_lgt = i;
	if (i)
	    strncpy(prefix_str, buf, i);
	set_prefix = 0;

#ifdef DEC_DEBUG
	msg("match %s", str);
	user_delay(1);
#endif

	return 0;
    }

    return 1;
}

static int
decode_line(char *buf, register int len, int dont_write)
 /* len		actual input line length */
 /* dont_write	doing leading check */
{
    char            outl[LINELEN];
    register char  *bp, *ut;
    register int   *trtbl = chtbl;
    register int    n;
    register int    blen;	/* binary length (from decoded file) */
    register int    rlen;	/* calculated input line length */

    /*
     * Get the binary line length.
     */
    if ((blen = trtbl[buf[0]]) < 0) {
	if (strncmp(buf, "begin", 5) == 0 || strncmp(buf, "table", 5) == 0)
	    return NEW_BEGIN;

	if (state == SKIP_LEADING)
	    return SKIP_LEADING;

	/*
	 * end of uuencoded file ?
	 */
	if (strncmp(buf, "end", 3) == 0)
	    return FOUND_END;

	/*
	 * end of current file ? : get next one.
	 */
	if (strncmp(buf, "include", 7) == 0)
	    return FOUND_INCLUDE;

	/*
	 * trailing garbage
	 */
	return SKIP_TRAILING;
    }

    /*
     * Some common endings that survive to this point. Could fail if a line
     * really started with one of these, but this is sufficiently unlikely,
     * plus it handles lots of news post formats.
     */
    if (state != SKIP_LEADING &&
	(strncmp(buf, "END", 3) == 0 ||
	 strncmp(buf, "CUT", 3) == 0))
	return SKIP_TRAILING;

    rlen = cdlen[blen];
    if (len < rlen)
	goto d_err;
    if (len > (rlen + 2))
	goto d_err;		/* line too long */

    /*
     * Is it the empty line before the end line ?
     */
    if (blen == 0)
	return state;

    /*
     * Pad with blanks.
     */
    for (bp = buf + len, n = rlen - len; --n >= 0;)
	*bp++ = blank;

    /*
     * Verify
     */
    for (n = rlen, bp = buf; --n >= 0; bp++)
	if (trtbl[*bp] < 0) {

#ifdef DEC_DEBUG
	    msg("%s - verify failed %d '%.30s'",
		state_tbl[state & 0xf], rlen - n, buf);
	    user_delay(2);
#endif

	    goto d_err;
	}

    /*
     * Check for uuencodes that append a 'z' to each line....
     */
    if (check) {
	if (secnd) {
	    secnd = 0;
	    if (buf[rlen] == SEQMAX) {
		check = 0;
	    }
	} else if (first) {
	    first = 0;
	    secnd = 1;
	    if (buf[rlen] != SEQMAX) {
		check = 0;
	    }
	}
    }

    /*
     * There we check.
     */
    if (check) {
	if (buf[rlen] != seqc) {

#ifdef DEC_DEBUG
	    msg("check failed %d != %d", buf[rlen], seqc);
	    user_delay(1);
#endif

	    goto d_err;
	}
	if (--seqc < SEQMIN)
	    seqc = SEQMAX;
    }
    if (dont_write)
	return DECODE_TEXT;

    /*
     * output a group of 3 bytes (4 input characters).
     */
    ut = outl;
    n = blen;
    bp = &buf[1];
    while (--n >= 0) {
	*(ut++) = trtbl[*bp] << 2 | trtbl[bp[1]] >> 4;
	if (n > 0) {
	    *(ut++) = (trtbl[bp[1]] << 4) | (trtbl[bp[2]] >> 2);
	    n--;
	}
	if (n > 0) {
	    *(ut++) = trtbl[bp[2]] << 6 | trtbl[bp[3]];
	    n--;
	}
	bp += 4;
    }
    if ((int) fwrite(outl, 1, blen, out) <= 0) {
	msg("Error on writing decoded file");
	return OTHER_ERROR;
    }
    return DECODE_TEXT;

d_err:
    if (state == SKIP_LEADING)
	return SKIP_LEADING;
    return DECODE_ERROR;

}



/*
 * Install the table in memory for later use.
 */
static void
inittbls(void)
{
    register int    i, j;

    /*
     * Set up the default translation table.
     */
    for (i = 0; i < ' '; i++)
	chtbl[i] = -1;
    for (i = ' ', j = 0; i < ' ' + 64; i++, j++)
	chtbl[i] = j;
    for (i = ' ' + 64; i < MAXCHAR; i++)
	chtbl[i] = -1;
    chtbl['`'] = chtbl[' '];	/* common mutation */
    chtbl['~'] = chtbl['^'];	/* an other common mutation */
    blank = ' ';

    /*
     * set up the line length table, to avoid computing lotsa * and / ...
     */
    cdlen[0] = 1;
    for (i = 1, j = 5; i <= NORMLEN; i += 3, j += 4)
	cdlen[i] = (cdlen[i + 1] = (cdlen[i + 2] = j));
}

static int
gettable(FILE * in)
{
    char            buf[LINELEN], *line;
    register int    c, n = 0;
    register char  *cpt;

    for (c = 0; c < MAXCHAR; c++)
	chtbl[c] = -1;

    for (;;) {
	if (fgets(buf, sizeof buf, in) == NULL) {
	    msg("EOF while in translation table.");
	    return -1;
	}
	line = buf + prefix_lgt;
	if ((prefix_lgt > 0 && strncmp(buf, prefix_str, prefix_lgt)) ||
	    strncmp(line, "begin", 5) == 0) {
	    msg("Incomplete translation table.");
	    return -1;
	}
	cpt = line + strlen(line) - 1;
	*cpt = ' ';
	while (*(cpt) == ' ') {
	    *cpt = 0;
	    cpt--;
	}
	cpt = line;
	while (*cpt) {
	    c = *cpt;
	    if (chtbl[c] != -1) {
		msg("Duplicate char in translation table.");
		return -1;
	    }
	    if (n == 0)
		blank = c;
	    chtbl[c] = n++;
	    if (n >= 64)
		return 0;
	    cpt++;
	}
    }
}

static void
new_file(void)
{
    out = NULL;
    seqc = SEQMAX;
    partn = 'a';
    check = 1;
    first = 1;
    secnd = 0;
    state = FIND_BEGIN;
    prefix_lgt = 0;
    set_prefix = decode_skip_prefix;
    arcpart = 0;
    inittbls();
}

void
uud_start(char *dir)
{
    target = dir;
    new_file();
}

void
uud_end(void)
{
    if (out != NULL) {
	fclose(out);
	if (decode_keep) {
	    msg("%s INCOMPLETE -- removed", arcname);
	    unlink(ofname);
	    user_delay(5);
	}
	out = NULL;
    }
}

int
uudecode(register article_header * ah, FILE * in)
{
    mode_t          mode;
    int             onedone, len, lead_check = 0;
    char            buf[LINELEN], part[2], *line;
    long            real_size, start_offset;
    long            expect_size;

    onedone = 0;

    /*
     * search for header or translation table line.
     */
    start_offset = ftell(in);

    for (;;) {
	if ((state & NO_ADVANCE) == 0) {
	    if (ftell(in) >= ah->lpos)
		break;
	    if (fgets(buf, sizeof buf, in) == NULL)
		break;
	}
	if (s_keyboard)
	    return -1;

	len = strlen(buf);
	if (len > 0 && buf[len - 1] == NL)
	    buf[--len] = NUL;

#ifdef DEC_DEBUG
	if (state != ostate) {
	    msg("%s->%s - '%.30s'", state_tbl[ostate & 0xf], state_tbl[state & 0xf], buf);
	    user_delay(2);
	    ostate = state;
	}
#endif

	switch (state) {

	    case NEW_BEGIN:
		if (out != NULL) {
		    uud_end();
		    user_delay(5);
		}
		new_file();

		/* FALLTHRU */
	    case FIND_BEGIN:
	    case FIND_BEGIN_AFTER_ERROR:
	    case FIND_BEGIN_AFTER_INCLUDE:
		set_prefix = decode_skip_prefix;
		if (strncmp_skip(buf, "table", 5) == 0) {
		    gettable(in);
		    continue;
		}
		if (strncmp_skip(buf, "begin", 5))
		    continue;

		line = buf + prefix_lgt;

		if (state == FIND_BEGIN_AFTER_INCLUDE) {
		    if (sscanf(line, "begin part %1s%s", part, arcname) != 2) {
			msg("Invalid 'begin' line after 'include'");
			continue;
		    }
		    partn++;
		    if (partn > 'z')
			partn = 'a';
		    if (part[0] != partn) {
			msg("PARTS NOT IN SEQUENCE: %s -- removed", arcname);
			user_delay(5);
			fclose(out);
			unlink(ofname);
			new_file();
			state = FIND_BEGIN_AFTER_ERROR;
			goto err;
		    }
		} else {
		    if (sscanf(line, "begin %o %s", (unsigned int *) &mode, arcname) != 2)
			continue;

		    if (target != NULL)
			sprintf(ofname, "%s%s", target, arcname);
		    else
			strcpy(ofname, arcname);

		    if ((out = open_file(ofname, OPEN_CREATE)) == NULL) {
			msg("Cannot create file: %s", ofname);
			goto err;
		    }
		    chmod(ofname, mode & 0666);

		    if (decode_header_file)
			store_header(ah, in, target, decode_header_file);
		}

		state = DECODE_TEXT;
		continue;

	    case SKIP_LEADING:
		if (len > prefix_lgt &&
		(!prefix_lgt || strncmp(buf, prefix_str, prefix_lgt) == 0)) {
		    state = decode_line(buf + prefix_lgt, len - prefix_lgt, 1);
		    if (state == DECODE_TEXT) {
			if (++lead_check == MIN_DECODE_LEADING) {
			    fseek(in, start_offset, 0);
			    continue;
			}
			state = SKIP_LEADING;
			continue;
		    }
		} else {
		    set_prefix = decode_skip_prefix;
		    if (strncmp_skip(buf, "begin", 5) == 0 ||
			strncmp_skip(buf, "table", 5) == 0)
			state = NEW_BEGIN;
		}

		lead_check = 0;
		start_offset = ftell(in);
		continue;

	    case DECODE_TEXT:
		if (len <= prefix_lgt ||
		 (prefix_lgt > 0 && strncmp(buf, prefix_str, prefix_lgt))) {
		    state = SKIP_TRAILING;
		    continue;
		}
		if (onedone == 0) {
		    msg("Decoding%s: %s (part %d)",
		      prefix_lgt ? " & Unsharing" : "", arcname, ++arcpart);

		    onedone = 1;
		}
		state = decode_line(buf + prefix_lgt, len - prefix_lgt, 0);
		continue;

	    case FOUND_END:
		real_size = ftell(out);
		fclose(out);

		if (ftell(in) >= ah->lpos || fgets(buf, sizeof buf, in) == NULL) {
		    new_file();
		    break;
		}
		if ((!prefix_lgt || strncmp(buf, prefix_str, prefix_lgt) == 0) &&
		    sscanf(buf + prefix_lgt, "size%ld", &expect_size) == 1 &&
		    real_size != expect_size) {

		    msg("%s decoded with wrong size %ld (exp. %ld)",
			arcname, real_size, expect_size);
		    user_delay(3);
		} else {
		    msg("%s complete", arcname);
		    user_delay(1);
		}

		new_file();
		state = NEW_BEGIN;
		continue;

	    case FOUND_INCLUDE:
		state = FIND_BEGIN_AFTER_INCLUDE;
		return 0;

	    case SKIP_TRAILING:
		state = SKIP_LEADING;
		return 0;

	    case DECODE_ERROR:
		state = SKIP_TRAILING;
		continue;

	    case OTHER_ERROR:
		fclose(out);
		new_file();
		state = FIND_BEGIN_AFTER_ERROR;
		goto err;
	}

	break;			/* break in switch => break in loop */
    }

    if (onedone) {
	if (state == DECODE_TEXT)
	    state = SKIP_LEADING;
	return 0;
    }
    if (state == FIND_BEGIN_AFTER_ERROR)
	return -1;
    msg("No 'begin' line");

err:
    user_delay(2);
    return -1;
}
