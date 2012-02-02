/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Digest article handling
 *
 *	The code to do the selective parsing of mail and mmdf formats,
 *	mail from lines and determining folder types is based on patches
 *	contributed by Bernd Wechner (bernd@bhpcpd.kembla.oz.au).
 */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "debug.h"
#include "digest.h"
#include "news.h"
#include "pack_name.h"
#include "nn_term.h"

/* digest.c */

static char   **dg_hdr_field(register char *lp, int all);

int             strict_from_parse = 2;

#ifdef DG_TEST
#define TEST0(fmt)
#define TEST1(fmt, x)    if (Debug & DG_TEST) printf(fmt, x)
#define TEST2(fmt, x, y) if (Debug & DG_TEST) printf(fmt, x, y)

#else

#define TEST0(fmt, x)
#define TEST1(fmt, x)
#define TEST2(fmt, x, y)
#endif

#define UNIFY 040

static char     digest_pattern[] = "igest";

void
init_digest_parsing(void)
{
    register char  *m;

    for (m = digest_pattern; *m; m++)
	*m |= UNIFY;
}

int
is_digest(register char *subject)
{
    register char   c, *q, *m;

    if (subject == NULL)
	return 0;

    while ((c = *subject++)) {
	if ((c | UNIFY) != ('d' | UNIFY))
	    continue;

	q = subject;
	m = digest_pattern;
	while ((c = *m++) && (*q++ | UNIFY) == c);
	if (c == NUL)
	    return 1;
    }
    return 0;
}


/*
 * is_mail_from_line - Is this a legal unix mail "From " line?
 *
 * Given a line of input will check to see if it matches the standard
 * unix mail "from " header format. Returns 0 if it does and <0 if not.
 *
 * The check may be very lax or very strict depending upon
 * the value of "strict-mail-from-parse":
 *
 * 0 - Lax, checks only for the string "From ".
 * 1 - Strict, checks that the correct number of fields are present.
 * 2 - Very strict, also checks that each field contains a legal value.
 *
 * Assumptions: Not having the definitive unix mailbox reference I have
 * assumed that unix mailbox headers follow this format:
 *
 * From <person> <date> <garbage>
 *
 * Where <person> is the address of the sender, being an ordinary
 * string with no white space imbedded in it, and <date> is the date of
 * posting, in ctime(3C) format.
 *
 * This would, on the face of it, seem valid. I (Bernd) have yet to find a
 * unix mailbox header which doesn't follow this format.
 *
 * From: Bernd Wechner (bernd@bhpcpd.kembla.oz.au)
 * Obfuscated by: KFS (as usual)
 */

#define MAX_FIELDS 10

static char     legal_day[] = "SunMonTueWedThuFriSat";
static char     legal_month[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
static int      legal_numbers[] = {1, 31, 0, 23, 0, 59, 0, 60, 1969, 2199};

static int
is_mail_from_line(char *line, char *namebuf)
 /* line		Line of text to be checked */
 /* namebuf	Optional buffer to place packed sender info */
{
    char           *fields[MAX_FIELDS];
    char           *sender_tail = NULL;
    register char  *lp, **fp;
    register int    n, i;

    if (strncmp(line, "From ", 5))
	return -100;
    if (strict_from_parse == 0)
	return 0;

    lp = line + 5;
    /* sender day mon dd hh:mm:ss year */
    for (n = 0, fp = fields; n < MAX_FIELDS; n++) {
	while (*lp && *lp != NL && isascii(*lp) && isspace(*lp))
	    lp++;
	if (*lp == NUL || *lp == NL)
	    break;
	*fp++ = lp;
	while (*lp && isascii(*lp) && !isspace(*lp))
	    if (*lp++ == ':' && (n == 4 || n == 5))
		break;
	if (n == 0)
	    sender_tail = lp;
    }

    if (n < 8)
	return -200 - n;

    fp = fields;

    if (n > 8 && !isdigit(fp[7][0]))
	fp[7] = fp[8];		/* ... TZ year */
    if (n > 9 && !isdigit(fp[7][0]))
	fp[7] = fp[9];		/* ... TZ DST year */

    if (namebuf != NULL) {
	char            x = *sender_tail;
	*sender_tail = NUL;
	pack_name(namebuf, *fp, Name_Length);
	*sender_tail = x;
    }
    if (strict_from_parse == 1)
	return 0;

    fp++;
    for (i = 0; i < 21; i += 3)
	if (strncmp(*fp, &legal_day[i], 3) == 0)
	    break;
    if (i == 21)
	return -1;

    fp++;
    for (i = 0; i < 36; i += 3)
	if (strncmp(*fp, &legal_month[i], 3) == 0)
	    break;
    if (i == 36)
	return -2;

    for (i = 0; i < 10; i += 2) {
	lp = *++fp;
	if (!isdigit(*lp))
	    return -20 - i;
	n = atoi(lp);
	if (n < legal_numbers[i] || legal_numbers[i + 1] < n)
	    return -10 - i;
    }
    return 0;
}

/*
 * expect that f is positioned at header of an article
 */

static int      is_mmdf_folder = 0;
static int      is_mail_folder = 0;

/*
 * get_folder_type
 *
 * Given a file descriptor f, will check what type of folder it is.
 * Must be called at zero offset and caller must reposition if necessary.
 * Side-effects: sets is_mail_folder, is_mmdf_folder, and current_folder_type.
 * Return values:
 *	-1: folder is empty,
 *	 0: normal digest,
 *	 1: UNIX mail format
 *	 2: MMDF format
 */

int             current_folder_type;

int
get_folder_type(FILE * f)
{
    char            line[1024];

    is_mail_folder = 0;
    is_mmdf_folder = 0;

    if (fgets(line, 1024, f) == NULL)
	return current_folder_type = -1;

    if (strncmp(line, "\001\001\001\001\n", 5) == 0) {
	is_mmdf_folder = 1;
	return current_folder_type = 2;
    }
    if (is_mail_from_line(line, (char *) NULL) == 0) {
	is_mail_folder = 1;
	return current_folder_type = 1;
    }
    return current_folder_type = 0;
}

int
get_digest_article(FILE * f, news_header_buffer hdrbuf)
{
    int             cont;

    digest.dg_hpos = ftell(f);
    TEST1("GET DIGEST hp=%ld\n", (long) digest.dg_hpos);

    do {
	if (!parse_digest_header(f, 0, hdrbuf))
	    return -1;
	digest.dg_fpos = ftell(f);
	TEST2("END HEADER hp=%ld fp=%ld\n", (long) digest.dg_hpos, (long) digest.dg_fpos);
    } while ((cont = skip_digest_body(f)) < 0);

    TEST2("END BODY lp=%ld next=%ld\n", (long) digest.dg_lpos, ftell(f));

    return cont;
}

#define BACKUP_LINES	 50	/* remember class + offset for parsed lines */

#define	LN_BLANK	0x01	/* blank line */
#define	LN_DASHED	0x02	/* dash line */
#define	LN_HEADER	0x04	/* (possible) header line */
#define	LN_ASTERISK	0x08	/* asterisk line (near end) */
#define	LN_END_OF	0x10	/* End of ... line */
#define	LN_TEXT		0x20	/* unclassified line */


/*
 * skip until 'Subject: ' (or End of digest) line is found
 * then backup till start of header
 */

/*
 * Tuning parameters:
 *
 *	MIN_HEADER_LINES:	number of known header lines that must
 *				be found in a block to identify a new
 *				header
 *
 *	MAX_BLANKS_DASH		max no of blanks on a 'dash line'
 *
 *	MIN_DASHES		min no of dashes on a 'dash line'
 *
 *	MAX_BLANKS_ASTERISKS	max no of blanks on an 'asterisk line'
 *
 *	MIN_ASTERISKS		min no of asterisks on an 'asterisk line'
 *
 *	MAX_BLANKS_END_OF	max no of blanks before "End of "
 */

#define	MIN_HEADER_LINES	2
#define	MAX_BLANKS_DASH		3
#define	MIN_DASHES		16
#define	MAX_BLANKS_ASTERISK	1
#define	MIN_ASTERISKS		10
#define	MAX_BLANKS_END_OF	1

int
skip_digest_body(register FILE * f)
{
    long            backup_p[BACKUP_LINES];
    int             line_type[BACKUP_LINES];
    register int    backup_index, backup_count;
    int             more_header_lines, end_or_asterisks, blanks;
    int             colon_lines;
    char            line[1024];
    register char  *cp;

#define	decrease_index()	\
    if (--backup_index < 0) backup_index = BACKUP_LINES - 1

    backup_index = -1;
    backup_count = 0;
    end_or_asterisks = 0;

    digest.dg_lines = 0;


next_line:
    more_header_lines = 0;
    colon_lines = 0;

next_possible_header_line:
    digest.dg_lines++;

    if (++backup_index == BACKUP_LINES)
	backup_index = 0;
    if (backup_count < BACKUP_LINES)
	backup_count++;

    backup_p[backup_index] = ftell(f);
    line_type[backup_index] = LN_TEXT;

    if (fgets(line, 1024, f) == NULL) {
	TEST2("end_of_file, bc=%d, lines=%d\n", backup_count, digest.dg_lines);

	if (is_mmdf_folder) {
	    digest.dg_lpos = backup_p[backup_index];
	    is_mmdf_folder = 0;
	    return 0;
	}
	/* end of file => look for "****" or "End of" line */

	if (end_or_asterisks)
	    while (--backup_count >= 0) {
		--digest.dg_lines;
		decrease_index();
		if (line_type[backup_index] & (LN_ASTERISK | LN_END_OF))
		    break;
	    }

	digest.dg_lpos = backup_p[backup_index];

	if (digest.dg_lines == 0)
	    return 0;

	while (--backup_count >= 0) {
	    --digest.dg_lines;
	    digest.dg_lpos = backup_p[backup_index];
	    decrease_index();
	    if ((line_type[backup_index] &
		 (LN_ASTERISK | LN_END_OF | LN_BLANK | LN_DASHED)) == 0)
		break;
	}

	return 0;		/* no article follows */
    }
    TEST1("\n>>%-.50s ==>>", line);

    if (is_mmdf_folder) {
	/* in an mmdf folder we simply look for the next ^A^A^A^A line */
	if (line[0] != '\001' || strcmp(line, "\001\001\001\001\n"))
	    goto next_line;

	digest.dg_lpos = backup_p[backup_index];
	--digest.dg_lines;
	return (digest.dg_lines <= 0) ? -1 : 1;
    }
    for (cp = line; *cp && isascii(*cp) && isspace(*cp); cp++);

    if (*cp == NUL) {
	TEST0("BLANK");
	line_type[backup_index] = LN_BLANK;
	goto next_line;
    }
    if (is_mail_folder) {
	/* in a mail folder we simply look for the next "From " line */
	if (line[0] != 'F' || is_mail_from_line(line, (char *) NULL) < 0)
	    goto next_line;

	line_type[backup_index] = LN_HEADER;
	fseek(f, backup_p[backup_index], 0);
	goto found_mail_header;
    }
    blanks = cp - line;

    if (*cp == '-') {
	if (blanks > MAX_BLANKS_DASH)
	    goto next_line;

	while (*cp == '-')
	    cp++;
	if (cp - line - blanks > MIN_DASHES) {
	    while (*cp && (*cp == '-' || (isascii(*cp) && isspace(*cp))))
		cp++;
	    if (*cp == NUL) {
		TEST0("DASHED");

		line_type[backup_index] = LN_DASHED;
	    }
	}
	goto next_line;
    }
    if (*cp == '*') {
	if (blanks > MAX_BLANKS_ASTERISK)
	    goto next_line;

	while (*cp == '*')
	    cp++;
	if (cp - line - blanks > MIN_ASTERISKS) {
	    while (*cp && (*cp == '*' || (isascii(*cp) && isspace(*cp))))
		cp++;
	    if (*cp == NUL) {
		TEST0("ASTERISK");
		line_type[backup_index] = LN_ASTERISK;
		end_or_asterisks++;
	    }
	}
	goto next_line;
    }
    if (blanks <= MAX_BLANKS_END_OF &&
	*cp == 'E' && strncmp(cp, "End of ", 7) == 0) {
	TEST0("END_OF_");
	line_type[backup_index] = LN_END_OF;
	end_or_asterisks++;
	goto next_line;
    }
    if (blanks)
	goto next_possible_header_line;
/* must be able to handle continued lines in sub-digest headers...
	goto next_line;
*/

    if (!dg_hdr_field(line, 0)) {
	char           *colon;
	if ((colon = strchr(line, ':'))) {
	    for (cp = line; cp < colon; cp++)
		if (!isascii(*cp) || isspace(*cp))
		    break;
	    if (cp == colon) {
		TEST0("COLON");
		colon_lines++;
		line_type[backup_index] = LN_HEADER;
		goto next_possible_header_line;
	    }
	}
	if (is_mail_from_line(line, (char *) NULL) == 0) {
	    TEST0("FROM_");
	    colon_lines++;
	    line_type[backup_index] = LN_HEADER;
	}
	goto next_possible_header_line;
    }
    TEST0("HEADER");

    line_type[backup_index] = LN_HEADER;
    if (++more_header_lines < MIN_HEADER_LINES)
	goto next_possible_header_line;

    /* found block with MIN_HEADER_LINES */

    TEST0("\nSearch for start of header\n");

    colon_lines += more_header_lines;
    for (;;) {
	fseek(f, backup_p[backup_index], 0);
	if (line_type[backup_index] == LN_HEADER)
	    if (--colon_lines <= 0)
		break;
	--digest.dg_lines;
	if (--backup_count == 0)
	    break;
	decrease_index();
	if ((line_type[backup_index] & (LN_HEADER | LN_TEXT)) == 0)
	    break;
    }

    if (digest.dg_lines == 0) {
	TEST0("Skipped empty article\n");
	return -1;
    }
found_mail_header:

    for (;;) {
	digest.dg_lpos = backup_p[backup_index];
	if (--backup_count < 0)
	    break;
	decrease_index();
	if ((line_type[backup_index] & (LN_BLANK | LN_DASHED)) == 0)
	    break;
	--digest.dg_lines;
    }

    return (digest.dg_lines == 0) ? -1 : 1;
}

int
parse_digest_header(FILE * f, int all, news_header_buffer hdrbuf)
{
    digest.dg_date = digest.dg_from = digest.dg_subj = digest.dg_to = NULL;

    parse_header(f, dg_hdr_field, all, hdrbuf);

    return digest.dg_from || digest.dg_subj;
}


static char   **
dg_hdr_field(register char *lp, int all)
{
    static char    *dummy;
    static char     namebuf[33];

#define check(name, lgt, field) \
    if (isascii(lp[lgt]) && isspace(lp[lgt]) \
	&& strncasecmp(name, lp, lgt) == 0) {\
	TEST0("MATCH: " #field " "); \
	return &digest.field; \
    }

    TEST1("\nPARSE[%.20s] ==>> ", lp);

    switch (*lp++) {

	case '\001':
	    /* In an mmdf folder ^A^A^A^A is skipped at beginning of header */
	    if (!is_mmdf_folder)
		break;
	    if (strncmp(lp, "\001\001\001\n", 4))
		break;
	    digest.dg_hpos += 5;
	    return NULL;

	case 'D':
	case 'd':
	    check("ate:", 4, dg_date);
	    break;

	case 'F':
	case 'f':
	    check("rom:", 4, dg_from);
	    if (!is_mail_folder)
		break;
	    if (*--lp != 'F')
		break;
	    if (is_mail_from_line(lp, namebuf) < 0)
		break;
	    /* Store packed sender in dg_from here and return dummy to parser */
	    if (digest.dg_from == NULL)
		digest.dg_from = namebuf;
	    return &dummy;

	case 'R':
	case 'r':
	    if (!all)
		break;
	    check("e:", 2, dg_subj);
	    break;

	case 'S':
	case 's':
	    check("ubject:", 7, dg_subj);
	    check("ubject", 6, dg_subj);
	    break;

	case 'T':
	case 't':
	    check("itle:", 5, dg_subj);
	    if (!all)
		break;
	    check("o:", 2, dg_to);
	    break;
    }

#undef check
    TEST0("NOT MATCHED ");

    return NULL;
}
