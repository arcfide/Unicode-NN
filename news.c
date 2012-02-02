/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Article header parsing.
 */

#include <stdlib.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "digest.h"
#include "more.h"
#include "news.h"
#include "nntp.h"

#ifndef SUNOS4
#include <strings.h>
#else
#include <string.h>
#endif

/* news.c */

static char   **art_hdr_field(register char *lp, int all);


int             retry_on_error = 0;

char           *
parse_header(FILE * f, char **(*hdr_field) (), int modes, news_header_buffer hdrbuf)
{
    register char  *bp, *cp, **fptr;
    int             siz, all, date_only;
    long            pos;

    pos = ftell(f);

/* read first NEWS_HEADER_BUFFER bytes (should be more than enough) */

    all = modes & GET_ALL_FIELDS;
    date_only = modes & GET_DATE_ONLY;

    siz = fread(hdrbuf, sizeof(char), NEWS_HEADER_BUFFER, f);
    if (siz <= 0) {
	hdrbuf[0] = NUL;
	return hdrbuf;
    }
    bp = hdrbuf;
    bp[siz - 1] = NUL;

    /* decode subarticle header */
    while (*bp) {

	if (*bp == NL) {	/* empty line following header */
	    ++bp;
	    fseek(f, pos + (bp - hdrbuf), 0);
	    return bp;
	}
	if (bp[0] == SP && bp[1] == SP) {	/* An ugly hack so that NN
						 * can read */
	    bp += 2;		/* it's own FAQ... :-) (sorry Bill) */
	    continue;
	}
	if (date_only && *bp != 'D')
	    fptr = NULL;
	else if ((fptr = (*hdr_field) (bp, all))) {
	    while (*bp && *bp != ':' && isascii(*bp) && !isspace(*bp))
		bp++;
	    if (*bp)
		bp++;
	    while (*bp && isascii(*bp) && isspace(*bp) && *bp != NL)
		bp++;
	    *fptr = bp;
	}

#ifdef NO_HEADER_SEPARATION_HACK
	else {
	    for (cp = bp; *cp && *cp != ':'; cp++) {
		if (!isascii(*cp))
		    break;
		if (*cp == '_' || *cp == '-')
		    continue;
		if (isalnum(*cp))
		    continue;
		break;
	    }
	    if (*cp != ':') {
		*bp = NL;
		pos--;
		continue;
	    }
	}
#endif

	while (*bp && *bp != NL)
	    bp++;

	/* Assume that continued lines are never empty! */
	if (fptr && bp == *fptr)
	    *fptr = NULL;

	while (*bp) {		/* look for continued lines */
	    cp = bp + 1;

	    if (!(*cp && isascii(*cp) && isspace(*cp) && *cp != NL)) {
		/* next line is empty or not indented */
		*bp++ = NUL;
		break;
	    }
	    *bp = SP;		/* substitute NL with SPACE */
	    bp = cp;
	    while (*bp && *bp != NL)
		bp++;
	}
    }

    return bp;
}

static char   **
art_hdr_field(register char *lp, int all)
{

#define check(name, lgt, field) \
    if (isascii(lp[lgt]) && isspace(lp[lgt]) \
	&& strncasecmp(name, lp, lgt) == 0)\
	return &news.field

    switch (*lp++) {

	    case 'A':
	    case 'a':
	    if (!all)
		break;
	    check("pproved:", 8, ng_appr);
	    break;

	case 'B':
	case 'b':
	    check("ack-References:", 15, ng_bref);
	    break;

	case 'C':
	case 'c':
	    check("ontrol:", 7, ng_control);
	    check("omment-To:", 10, ng_comment);
	    break;

	case 'D':
	case 'd':
	    check("ate:", 4, ng_date);
	    if (!all)
		break;
	    check("ate-Received:", 13, ng_rdate);
	    check("istribution:", 12, ng_dist);
	    break;

	case 'F':
	case 'f':
	    check("rom:", 4, ng_from);
	    if (!all)
		break;
	    check("ollowup-To:", 11, ng_follow);
	    break;

	case 'K':
	case 'k':
	    if (!all)
		break;
	    check("eywords:", 8, ng_keyw);
	    break;

	case 'L':
	case 'l':
	    check("ines:", 5, ng_xlines);
	    break;

	case 'M':
	case 'm':
	    if (!all)
		break;
	    if (strncasecmp(lp, "essage-", 7))
		break;
	    lp += 7;
	    check("ID:", 3, ng_ident);
	    break;

	case 'N':
	case 'n':
	    check("ewsgroups:", 10, ng_groups);
	    break;

	case 'O':
	case 'o':
	    if (!all)
		break;
	    check("rganization:", 12, ng_org);
	    check("riginator:", 10, ng_origr);
	    break;

	case 'P':
	case 'p':
	    if (!all)
		break;
	    check("ath:", 4, ng_path);
	    break;

	case 'R':
	case 'r':
	    check("eferences:", 10, ng_ref);
	    check("eply-To:", 8, ng_reply);
	    break;

	case 'S':
	case 's':
	    check("ubject:", 7, ng_subj);
	    check("ender:", 6, ng_sender);
	    if (!all)
		break;
	    check("ummary:", 7, ng_summ);
	    break;

	case 'T':
	case 't':
	    check("itle:", 5, ng_subj);
	    break;

	case 'X':
	case 'x':
	    check("ref:", 4, ng_xref);
	    break;
    }

    return NULL;

#undef check
}

int
is_header_line(char *line)
{
    return art_hdr_field(line, 0) != (char **) NULL;
}


FILE           *
open_news_article(article_header * art, int modes, news_header_buffer buffer1, news_header_buffer buffer2)
{

    char           *digest_buffer;
    int             retry;
    FILE           *f;
    struct stat     statb;

#ifndef DONT_COUNT_LINES
    int             c;
    off_t           digest_artlen = 0;
#endif				/* DONT_COUNT_LINES */

#ifdef NNTP
    int             lazy = 0;
#endif				/* NNTP */

#ifndef DONT_COUNT_LINES
#ifdef NNTP
    long            fpos;
#endif				/* NNTP */
#endif				/* DONT_COUNT_LINES */

    if (art->flag & A_FOLDER) {
	f = open_file(group_path_name, OPEN_READ);
	if (f == NULL)
	    return NULL;
	fseek(f, art->hpos, 0);

#ifndef DONT_COUNT_LINES
	digest_artlen = art->lpos - art->fpos;
#endif				/* DONT_COUNT_LINES */
    }

#ifdef NNTP
    else if (use_nntp) {
	lazy = (current_group->master_flag & M_ALWAYS_DIGEST) == 0
	    && (modes & LAZY_BODY) ? 1 : 0;
	f = nntp_get_article(art->a_number, lazy);
	if (f == NULL)
	    return NULL;
    }
#endif				/* NNTP */

    else {
	sprintf(group_file_name, "%ld", art->a_number);

	retry = retry_on_error;
	while ((f = open_file(group_path_name, OPEN_READ)) == NULL)
	    if (--retry < 0)
		return NULL;

	/* necessary because empty files wreak havoc */
	if (fstat(fileno(f), &statb) < 0 ||

#ifdef NOV
	    (art->lpos = statb.st_size, statb.st_size <= (off_t) 0)) {
#else
	    statb.st_size < art->lpos || statb.st_size <= (off_t) 0) {
#endif				/* NOV */

	    fclose(f);
	    return who_am_i == I_AM_MASTER ? (FILE *) 1 : NULL;
	}
    }

    digest_buffer = buffer1;

    if (modes & FILL_NEWS_HEADER) {

	news.ng_from = NULL;
	news.ng_reply = NULL;
	news.ng_name = NULL;
	news.ng_subj = NULL;
	news.ng_groups = NULL;
	news.ng_ref = NULL;
	news.ng_bref = NULL;
	news.ng_sender = NULL;

	news.ng_xlines = NULL;
	news.ng_xref = NULL;

	if (modes & GET_ALL_FIELDS) {
	    news.ng_path = NULL;
	    news.ng_reply = NULL;
	    news.ng_ident = NULL;
	    news.ng_follow = NULL;
	    news.ng_keyw = NULL;
	    news.ng_dist = NULL;
	    news.ng_org = NULL;
	    news.ng_appr = NULL;
	    news.ng_summ = NULL;
	    news.ng_control = NULL;
	    news.ng_date = NULL;
	    news.ng_rdate = NULL;
	    news.ng_comment = NULL;
	    news.ng_origr = NULL;
	}
	if (modes & GET_DATE_ONLY)
	    news.ng_date = NULL;

	(void) parse_header(f, art_hdr_field, modes, buffer1);

	if (news.ng_from == NULL)
	    news.ng_from = news.ng_sender;

#ifdef NOV
	/* fill in article positions..  new style.. */
	if ((art->flag & (A_FOLDER | A_DIGEST)) == 0) {
	    setpos(art, f);
	    news.ng_fpos = art->fpos;
	}
#else				/* NOV */
	if (modes & FILL_OFFSETS)	/* used only by old DB code */
	    art->fpos = news.ng_fpos = ftell(f);
#endif				/* NOV */

	if (news.ng_xlines)
	    news.ng_lines = atoi(news.ng_xlines);
	else {

#ifndef DONT_COUNT_LINES

#ifdef NNTP
	    if (use_nntp && lazy && !(art->flag & (A_DIGEST | A_FOLDER))) {
		fpos = ftell(f);
		fclose(f);
		f = nntp_get_article(art->a_number, 2);
		if (f == NULL)
		    return NULL;
		lazy = 0;
		fseek(f, fpos, 0);
	    }
#endif				/* NNTP */

	    news.ng_lines = 0;
	    while ((c = getc(f)) != EOF) {
		if (c == '\n')
		    news.ng_lines++;
		if (digest_artlen && --digest_artlen == 0)
		    break;
	    }
#else				/* DONT_COUNT_LINES */
	    news.ng_lines = -1;
#endif				/* DONT_COUNT_LINES */
	}

	if (modes & FILL_OFFSETS) {
	    fseek(f, 0, 2);
	    news.ng_lpos = ftell(f);
	}

#ifdef NNTP
	else if (use_nntp && (art->flag & (A_DIGEST | A_FOLDER)) == 0) {
	    fseek(f, 0, 2);
	    art->lpos = ftell(f);
	}
#endif

	news.ng_flag = 0;

	if (news.ng_appr)
	    news.ng_flag |= N_MODERATED;

	if (modes & DIGEST_CHECK && is_digest(news.ng_subj))
	    news.ng_flag |= N_DIGEST;

#ifdef NNTP
	if (use_nntp && lazy && news.ng_flag & N_DIGEST) {
	    fclose(f);
	    f = nntp_get_article(art->a_number, 2);
	    if (f == NULL)
		return NULL;
	}
#endif

	digest_buffer = buffer2;
    }

#ifdef NNTP
    else if (use_nntp && (art->flag & (A_DIGEST | A_FOLDER)) == 0) {
	fseek(f, 0, 2);
	art->lpos = ftell(f);
    }
#endif

    if (modes & FILL_DIGEST_HEADER) {
	fseek(f, art->hpos, 0);
	parse_digest_header(f, modes & GET_ALL_FIELDS, digest_buffer);
    }

#ifdef NOV
    else {
	/* fill in article positions..  new style.. */
	if ((art->flag & (A_FOLDER | A_DIGEST)) == 0) {
	    setpos(art, f);
	    news.ng_fpos = art->fpos;
	}
    }
#endif				/* NOV */

    fseek(f, (modes & SKIP_HEADER) ? art->fpos : art->hpos, 0);

    return f;
}
