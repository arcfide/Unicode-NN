/*******************WARNING*********************

This is a *MODIFIED* version of Geoff Coller's proof-of-concept NOV
implementation.

It has been modified to support threading directly from a file handle
to a NNTP server without a temporary file.

This is not a complete distribution.  We have only distributed enough
to support NN's needs.

The original version came from world.std.com:/src/news/nov.dist.tar.Z
and was dated 11 Aug 1993.

In any case, bugs you find here are probably my fault, as I've trimmed
a fair bit of unused code.

-Peter Wemm  <peter@DIALix.oz.au>
*/

/*
 * Copyright (c) Geoffrey Collyer 1992, 1993.
 * All rights reserved.
 * Written by Geoffrey Collyer.
 * Thanks to UUNET Communications Services Inc for financial support.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company, the Regents of the University of California, or
 * the Free Software Foundation.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The authors are not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */


/*
 * library to access news history adjunct data
 */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "config.h"
#include "global.h"
#include "awksplit.h"
#include "digest.h"
#include "hash.h"
#include "newsoverview.h"
#include "nntp.h"
#include "split.h"

#ifndef NEWS_DIRECTORY
#define NEWS_DIRECTORY	"/usr/spool/news"
#endif

#ifndef OVFILENAME
#define OVFILENAME ".overview"
#endif

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

/* imports */
static char    *newsarts = NEWS_DIRECTORY;	/* news spool */
static char    *overviewfiles = OVFILENAME;	/* overview */
static int      prsoverview(register struct novgroup *, register article_number, register article_number);

#ifdef DO_NOV_DIGEST
static void     de_digest(struct novgroup *, struct novart *);
#endif

void
novartdir(char *dir)
{
    newsarts = (dir == NULL ? NEWS_DIRECTORY : dir);
}

void
novfilename(char *name)
{
    overviewfiles = (name == NULL ? OVFILENAME : name);
}

static struct novgroup *	/* malloced */
novnew(void)
{
    register struct novgroup *gp = (struct novgroup *) malloc(sizeof *gp);

    if (gp != NULL) {
	gp->g_first = gp->g_curr = NULL;
	gp->g_msgids = gp->g_roots = NULL;
	gp->g_dir = NULL;
	gp->g_stream = NULL;
    }
    return gp;
}

struct novgroup *		/* malloced cookie */
novopen(char *grp)
{				/* change to group grp */
    register struct novgroup *gp = novnew();
    register char  *sgrp;
    register char  *s;

    if (gp == NULL)
	return NULL;
    sgrp = strsave(grp);
    if (sgrp == NULL) {
	free((char *) gp);
	return NULL;
    }
    for (s = sgrp; *s != '\0'; s++)
	if (*s == '.')
	    *s = '/';
    gp->g_dir = str3save(newsarts, "/", sgrp);
    free(sgrp);
    return gp;
}

struct novgroup *
novstream(register FILE * fp)
{
    register struct novgroup *gp = novnew();

    if (gp != NULL)
	gp->g_stream = fp;
    return gp;
}

/*
 * novseek()
 *	For local overview file, use binary search to find first line
 *	which is at artnum or before.
 *	Ripped off from inn1.4/nnrpd/newnews.c
 */
static int
novseek(register FILE * fp, register article_number artnum)
{
    char           *line;
    long            upper;
    long            lower;
    long            middle;

    /* Read first line -- is it in our range? */
    (void) fseek(fp, 0L, 0);
    if ((line = fgetstr(fp)) == NULL)
	return 0;
    if (atol(line) >= artnum) {
	(void) fseek(fp, 0L, 0);
	return 1;
    }
    /* Set search ranges and go. */
    lower = 0;
    (void) fseek(fp, 0L, 2);
    upper = ftell(fp);
    for (;;) {
	/* Seek to middle line. */
	middle = (upper + lower) / 2;
	(void) fseek(fp, middle, 0);
	while (++middle <= upper && getc(fp) != '\n')
	    continue;

	if (middle >= upper)
	    break;

	if ((line = fgetstr(fp)) != NULL && atol(line) > artnum)
	    upper = middle;
	else if (lower == middle)
	    break;
	else
	    lower = middle;
    }

    /* Move to lower bound; we know this will always be the start of a line. */
    (void) fseek(fp, lower, 0);
    while ((line = fgetstr(fp)) != NULL)
	if (atol(line) >= artnum) {
	    (void) fseek(fp, lower, 0);
	    return 1;
	}
    return 0;
}


struct novart  *
novall(register struct novgroup * gp, register article_number first, register article_number last)
{
    if (gp->g_first == NULL)	/* new group? */
	(void) prsoverview(gp, first, last);
    return gp->g_first;
}

struct novart  *
novnext(register struct novgroup * gp)
 /* gp		cookie from novopen */
{
    register struct novart *thisart;

    if (gp->g_first == NULL)	/* new group? */
	(void) prsoverview(gp, 1, 201);
    thisart = gp->g_curr;
    if (thisart != NULL)
	gp->g_curr = thisart->a_nxtnum;
    return thisart;
}

static void
freeart(register struct novart * art)
{
    if (art->a_refs != NULL)
	free(art->a_refs);
    if (art->a_parent != NULL)
	free(art->a_parent);
    if (art->a_num != NULL)
	free(art->a_num);	/* the original input line, chopped */
    free((char *) art);
}

#define MAXFIELDS 9		/* last field is "other" fields */
#define DEFREFS 20

#define PRSFAIL 0		/* disaster (out of memory, etc.) */
#define PRSOKAY 1
#define PRSBAD  2		/* bad syntax */

static int
prsovline(register char *line, register struct novgroup * gp, register struct novart * art, register struct novart * prevart)
 /* line		malloced; will be chopped up */
{
    register int    nf, nrefs, len;
    char           *fields[MAXFIELDS], *refs[DEFREFS];
    char          **refsp = refs;
    static struct novart zart;

    *art = zart;		/* make freeart safe if we bail out early */
    len = strlen(line);
    if (len > 0 && line[len - 1] == '\n')
	line[len - 1] = '\0';	/* make field count straightforward */
    nf = split(line, fields, MAXFIELDS, "\t");
    if (nf < MAXFIELDS - 1)	/* only "others" fields are optional */
	return PRSBAD;		/* skip this line */
    while (nf < MAXFIELDS)
	fields[nf++] = "";	/* fake missing fields */

    /*
     * duplicate message-ids would confuse the threading code and anyway
     * should not happen (now that relaynews suppresses multiple links within
     * a group for the same article), so ignore any entries for duplicate
     * message-ids.
     */
    if (hashfetch(gp->g_msgids, fields[4]) != NULL)
	return PRSBAD;

    art->a_parent = NULL;
    art->a_refs = strsave(fields[5]);	/* fields[5] will be split below */
    if (art->a_refs == NULL)
	return PRSFAIL;
    if (art->a_refs[0] != '\0') {	/* at least one ref? */
	nrefs = awksplit(fields[5], &refsp, DEFREFS, "");
	if (refsp == NULL)
	    return PRSFAIL;
	if (nrefs > 0) {	/* last ref is parent */
	    if (refsp[nrefs - 1] == NULL)
		return PRSFAIL;
	    art->a_parent = strsave(refsp[nrefs - 1]);
	    if (art->a_parent == NULL)
		return PRSFAIL;
	    if (refsp != refs)
		free((char *) refsp);
	}
    }
    art->a_num = fields[0];	/* line */
    art->a_subj = fields[1];
    art->a_from = fields[2];
    art->a_date = fields[3];
    art->a_msgid = fields[4];
    /* see above for fields[5] */
    art->a_bytes = fields[6];
    art->a_lines = fields[7];
    art->a_others = fields[8];
    art->a_nxtnum = NULL;

    if (!hashstore(gp->g_msgids, art->a_msgid, (char *) art))
	return PRSFAIL;
    if (gp->g_first == NULL)
	gp->g_first = art;
    if (prevart != NULL)
	prevart->a_nxtnum = art;
    return PRSOKAY;
}

static int
prsoverview(register struct novgroup * gp, register article_number first, register article_number last)
 /* gp			cookie from novopen */
{
    register struct novart *art, *prevart = NULL;
    register int    prssts;
    unsigned        hsize;
    char           *line;

    gp->g_curr = gp->g_first = NULL;
    if (gp->g_dir == NULL && gp->g_stream == NULL)
	return 0;
    if (gp->g_stream == NULL) {
	line = str3save(gp->g_dir, "/", overviewfiles);
	if (line == NULL)
	    return 0;
	gp->g_stream = fopen(line, "r");
	free(line);
	if (gp->g_stream == NULL)
	    return 0;
    }
    /* parse input and store in gp->g_msgids for later traversal */
    hsize = (last - first) | 0x7f;
    gp->g_msgids = hashcreate(hsize, (unsigned (*) ()) NULL);
    if (gp->g_msgids == NULL) {
	if (gp->g_dir != NULL)	/* we opened the stream? */
	    (void) fclose(gp->g_stream);
	return 0;
    }
    if (!use_nntp) {
	if (!novseek(gp->g_stream, first))
	    goto done;
    }
    while ((line = fgetstr(gp->g_stream)) != NULL) {
	if (strcmp(line, ".") == 0)	/* EOF on a NNTP stream */
	    break;
	art = (struct novart *) malloc(sizeof *art);
	if (art == NULL || (prssts = prsovline(strsave(line), gp, art, prevart)) == PRSFAIL) {
	    if (gp->g_dir != NULL)	/* we opened the stream? */
		(void) fclose(gp->g_stream);
	    if (art != NULL)
		freeart(art);
	    return 0;
	}
	if (prssts == PRSOKAY)
	    prevart = art;
	else
	    freeart(art);
    }
done:
    if (gp->g_dir != NULL)	/* we opened the stream? */
	(void) fclose(gp->g_stream);
    gp->g_curr = gp->g_first;

#ifdef DO_NOV_DIGEST

    /*
     * This is really horrible.  NOV doesn't break down digests (I don't
     * think it should), but NN wants all the information up front. We have
     * to find any digest and break it apart.
     */
    for (art = gp->g_first; art; art = art->a_nxtnum) {
	if (is_digest(art->a_subj))
	    de_digest(gp, art);
    }
#endif

    return 1;
}

#ifdef DO_NOV_DIGEST
static char    *build_nov_line(struct novart *, struct digest_header *, int);
static char    *detab_cp(register char *, register char *);

static void
de_digest(struct novgroup * gp, struct novart * ap)
 /* gp			cookie from novopen */
{
    register struct novart *art, *prevart;
    news_header_buffer dgbuf;
    int             cont, seq;
    FILE           *fp;
    char           *line;

#ifdef NNTP
    if (use_nntp) {
	if (atol(ap->a_num) == 0)
	    return;
	fp = nntp_get_article(atol(ap->a_num), 0);
    } else
#endif				/* NNTP */

	fp = open_file(ap->a_num, OPEN_READ);

    if (fp == NULL)
	return;

    cont = 1;
    prevart = ap;
    seq = 0;

    skip_digest_body(fp);
    while (cont && (cont = get_digest_article(fp, dgbuf)) >= 0) {
	if (seq == 0) {

#ifndef NO_MEMMOVE
	    memmove(ap->a_num + 1, ap->a_num,
		    ap->a_bytes - ap->a_num);
#else
	    bcopy(ap->a_num, ap->a_num + 1,
		  ap->a_bytes - ap->a_num);
#endif				/* NO_MEMMOVE */

	    ap->a_num[0] = '-';
	    ap->a_subj++;
	    ap->a_from++;
	    ap->a_date++;
	    ap->a_msgid++;
	} else {
	    if ((art = (struct novart *) malloc(sizeof *art)) == NULL)
		break;
	    if ((line = build_nov_line(ap, &digest, seq)) == NULL) {
		free(art);
		break;
	    }
	    if (prsovline(line, gp, art, (struct novart *) NULL) != PRSOKAY) {
		if (art->a_num != line)
		    free(line);
		freeart(art);
		continue;
	    }
	    art->a_nxtnum = prevart->a_nxtnum;
	    prevart->a_nxtnum = art;
	    prevart = art;
	}
	seq++;
    }
    fclose(fp);
}

static char    *
build_nov_line(struct novart * ap, struct digest_header * dp, int seq)
{
    char           *cp, *bp;
    int             len, i;
    char           *flds[10];

    flds[0] = dp->dg_subj;
    flds[1] = dp->dg_from;

    if (dp->dg_date)
	flds[2] = dp->dg_date;
    else
	flds[2] = ap->a_date;

    flds[3] = ap->a_msgid;
    flds[4] = ap->a_refs;
    flds[5] = ap->a_bytes;
    flds[6] = ap->a_others;

    len = 64;
    for (i = 0; i <= 6; i++) {
	if (flds[i])
	    len += strlen(flds[i]);
	else
	    flds[i] = "";
    }
    if ((bp = malloc(len)) == NULL)
	return (bp);

    cp = bp;
    *cp++ = '0';
    *cp++ = '\t';
    cp = detab_cp(cp, flds[0]);
    *cp++ = '\t';
    cp = detab_cp(cp, flds[1]);
    *cp++ = '\t';
    cp = detab_cp(cp, flds[2]);
    *cp++ = '\t';

    cp = detab_cp(cp, flds[3]);	/* need unique msgid */
    sprintf(cp, ".%d\t", seq);
    cp += strlen(cp);

    cp = detab_cp(cp, flds[4]);
    *cp++ = '\t';

    cp = detab_cp(cp, flds[5]);	/* add position data to byte count */
    sprintf(cp, ":%ld:%ld:%ld\t", (long) dp->dg_hpos,
	    (long) dp->dg_fpos - (long) dp->dg_hpos, (long) dp->dg_lpos);
    cp += strlen(cp);

    sprintf(cp, "%d\t", --dp->dg_lines);
    cp += strlen(cp);

    detab_cp(cp, flds[6]);

    return (bp);
}

static char    *
detab_cp(register char *dst, register char *src)
{
    while ((*dst = *src++)) {
	if (*dst == '\t')
	    *dst = ' ';
	dst++;
    }
    return (dst);
}

#endif				/* DO_NOV_DIGEST */

#ifdef THREAD
/*
 * if this article has no parent, enter it in the roots hash table.
 * if it has a parent, make this article the parent's first child,
 * even it means making the existing first child our first sibling.
 */
/* ARGSUSED */
static int
numvisit(char *key, char *data, char *hook)
{
    register struct novart *art = (struct novart *) data, *parent = NULL;
    register char  *msgid;
    register struct novgroup *gp = (struct novgroup *) hook;

    if (gp->g_roots == NULL) {
	gp->g_roots = hashcreate(500, (unsigned (*) ()) NULL);
	if (gp->g_roots == NULL)/* better not happen */
	    return;
    }
    msgid = art->a_msgid;
    if (art->a_parent != NULL)
	parent = (struct novart *) hashfetch(gp->g_msgids, art->a_parent);
    if (parent != NULL) {
	if (parent->a_child1 != NULL) {
	    if (art->a_sibling != NULL)
		return;		/* sibling in use; better not happen */
	    art->a_sibling = parent->a_child1;
	}
	parent->a_child1 = msgid;
    } else {			/* no parent - must be a root */
	art->a_parent = NULL;
	if (!hashstore(gp->g_roots, msgid, (char *) art))
	    return;		/* better not happen */
    }
}


static void
novthread(register struct novgroup * gp)
{
    if (gp->g_first == NULL)	/* new group? */
	(void) prsoverview(gp, 1, 201);
    /* build trees */
    if (gp->g_first != NULL)
	hashwalk(gp->g_msgids, numvisit, (char *) gp);
}

#endif

void
novclose(register struct novgroup * gp)
{
    register struct novart *art, *next;

    hashdestroy(gp->g_msgids);
    hashdestroy(gp->g_roots);
    if (gp->g_dir != NULL)
	free(gp->g_dir);
    for (art = gp->g_first; art != NULL; art = next) {
	next = art->a_nxtnum;
	freeart(art);
    }
}
