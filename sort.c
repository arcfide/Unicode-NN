/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Article sorting.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "articles.h"

/* sort.c */

static int      order_subj_date(article_header ** ah1, article_header ** ah2);
static int      order_date_subj_date(article_header ** ah1, article_header ** ah2);
static int      order_arrival(article_header ** a, article_header ** b);
static int      order_date(register article_header ** ah1, register article_header ** ah2);
static int      order_from_date(register article_header ** ah1, register article_header ** ah2);
static flag_type article_equal(article_header ** ah1, article_header ** ah2);

static int      order_arrival();

#ifdef BAD_ORDER_SUBJ_DATE
/* If one article's subject is identical to the first part of another
 article, the two subjects will still be considered identical if the
 length of the shorter subject is at least the limit set by this variable. */
int             subject_match_limit = 20;	/* "strncmp" limit for
						 * subjects */
#else
/* Subjects are considered identical if their first s-m-l characters match */
int             subject_match_limit = 256;	/* "strncmp" limit for
						 * subjects */
#endif

int             match_skip_prefix = 0;	/* skip first N bytes in matches */
int             match_parts_equal = 0;	/* match digits as equal */
int             match_parts_begin = 4;	/* require N characters for part
					 * match */

int             sort_mode = 1;	/* default sort mode */

extern int      bypass_consolidation;

/*
 *	String matching macroes.
 *
 * 	MATCH_DROP(t, a) and MATCH_DROP(t, b) must both be proven false
 *	before MATCH_?? (t, a, b) is used.
 */

#define	MATCH_DROP(table, c) ( c & 0200 || table[c] == 0 )
#define MATCH_EQ(table, a, b) ( a == b || table[a] == table[b] )
#define MATCH_LS_EQ(table, a, b) ( a <= b || table[a] <= table[b] )
#define MATCH_LS(table, a, b) ( a < b || table[a] < table[b] )
#define	MATCH_CMP(table, a, b) (table[a] - table[b])

static char     match_subject[128] = {

/*  NUL SOH STX ETX EOT ENQ ACK BEL BS  TAB NL  VT  FF  CR  SO  SI  */
    00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,

/*  DLE DC1 DC2 DC3 DC4 NAK SYN ETB CAN EM  SUB ESC FS  GS  RS  US  */
    00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,

/*  SP  !   "   #   $   %   &   '   (   )   *   +   ,   -   .   /   */
    00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 99, 00, 00, 00, 00,
/*                                              ^^                  */

/*  0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ?   */
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 00, 00, 00, 00, 00, 00,

/*  @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O   */
    00, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,

/*  P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _   */
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 00, 00,

/*  `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o   */
    00, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,

/*  p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~   DEL */
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 00, 00
};


static int
order_subj_date(article_header ** ah1, article_header ** ah2)
{
    register char  *a = (**ah1).subject, *b = (**ah2).subject;
    register char   ca, cb;
    register int    p, len;

    if (match_skip_prefix) {
	if ((int) (**ah1).subj_length > match_skip_prefix) {
	    if ((int) (**ah2).subj_length > match_skip_prefix) {
		a += match_skip_prefix;
		b += match_skip_prefix;
	    } else
		return 1;
	} else {
	    if ((int) (**ah2).subj_length > match_skip_prefix) {
		return -1;
	    }
	}
    }

#ifdef BAD_ORDER_SUBJ_DATE
    for (len = 0;; len++, a++, b++) {
#else
    for (len = subject_match_limit; --len >= 0; a++, b++) {
#endif

	while ((ca = *a) && MATCH_DROP(match_subject, ((int8) ca)))
	    a++;
	while ((cb = *b) && MATCH_DROP(match_subject, ((int8) cb)))
	    b++;

	if (ca == NUL) {
	    if (cb == NUL)
		break;

#ifdef BAD_ORDER_SUBJ_DATE
	    if (len >= subject_match_limit)
		break;
#endif

	    return -1;
	}
	if (cb == NUL) {

#ifdef BAD_ORDER_SUBJ_DATE
	    if (len >= subject_match_limit)
		break;
#endif

	    return 1;
	}
	if ((p = MATCH_CMP(match_subject, (int8) ca, (int8) cb)))
	    return p;
    }

    if ((**ah1).t_stamp > (**ah2).t_stamp)
	return 1;
    if ((**ah1).t_stamp < (**ah2).t_stamp)
	return -1;
    return order_arrival(ah1, ah2);
}

/* data_subj_date can only be used after root_t_stamp is set */

static int
order_date_subj_date(article_header ** ah1, article_header ** ah2)
{
    register time_stamp t1 = (**ah1).root_t_stamp;
    register time_stamp t2 = (**ah2).root_t_stamp;

    if (t1 > t2)
	return 1;
    if (t1 < t2)
	return -1;
    return order_subj_date(ah1, ah2);	/* get original order */
}


static int
order_arrival(article_header ** a, article_header ** b)
{
    register long   i;

    if ((i = ((*a)->a_number - (*b)->a_number)) == 0)
	i = (*a)->fpos - (*b)->fpos;

    return (i > 0) ? 1 : (i < 0) ? -1 : 0;
}

static int
order_date(article_header ** ah1, article_header ** ah2)
{
    if ((**ah1).t_stamp > (**ah2).t_stamp)
	return 1;
    if ((**ah1).t_stamp < (**ah2).t_stamp)
	return -1;
    return order_arrival(ah1, ah2);
}

static int
order_from_date(register article_header ** ah1, register article_header ** ah2)
{
    register int    i;

    if ((i = strcmp((**ah1).sender, (**ah2).sender)))
	return i;
    return order_date(ah1, ah2);
}

static          flag_type
article_equal(article_header ** ah1, article_header ** ah2)
 /* ah1.hdr == ah2.hdr */
{
    register char  *a = (**ah1).subject, *b = (**ah2).subject;
    register int    len;

    if (match_skip_prefix) {
	if ((int) (**ah1).subj_length > match_skip_prefix) {
	    if ((int) (**ah2).subj_length > match_skip_prefix) {
		a += match_skip_prefix;
		b += match_skip_prefix;
	    }
	}
    }
    for (len = 0;; len++, a++, b++) {
	while (*a && MATCH_DROP(match_subject, (int8) * a))
	    a++;
	while (*b && MATCH_DROP(match_subject, (int8) * b))
	    b++;

	if (*a == NUL) {
	    if (*b == NUL)
		break;
	    if (len >= subject_match_limit)
		return A_ALMOST_SAME;
	    return 0;
	}
	if (*b == NUL) {
	    if (len >= subject_match_limit)
		return A_ALMOST_SAME;
	    return 0;
	}
	if (!MATCH_EQ(match_subject, (int8) * a, (int8) * b)) {
	    if (len >= subject_match_limit)
		return A_ALMOST_SAME;
	    if (len >= match_parts_begin && match_parts_equal &&
		isdigit(*a) && isdigit(*b))
		return A_ALMOST_SAME;
	    return 0;
	}
    }

    return A_SAME;
}

void
sort_articles(int mode)
{
    register article_header **app;
    register long   n;
    register flag_type same;
    fct_type        cmp;

    if (n_articles <= 0)
	return;

    for (n = n_articles; --n >= 0;)
	articles[n]->flag &= ~(A_SAME | A_ALMOST_SAME | A_NEXT_SAME | A_ROOT_ART);

    if (n_articles == 1) {
	articles[0]->flag |= A_ROOT_ART;
	return;
    }
    if (mode == -1)
	mode = sort_mode;

    switch (mode) {
	case -2:		/* restore original ordering for update */
	    cmp = order_arrival;
	    break;
	default:
	case 0:		/* arrival (no sort) */
	    cmp = order_arrival;
	    bypass_consolidation = 1;
	    break;
	case 1:		/* date-subject-date */
	case 2:		/* subject-date */
	    cmp = order_subj_date;
	    break;
	case 3:		/* date only */
	    cmp = order_date;
	    bypass_consolidation = 1;
	    break;
	case 4:		/* sender-date */
	    cmp = order_from_date;
	    bypass_consolidation = 1;
	    break;
    }

    quicksort(articles, n_articles, article_header *, cmp);

    articles[0]->root_t_stamp = articles[0]->t_stamp;
    articles[0]->flag |= A_ROOT_ART;
    for (n = n_articles - 1, app = articles + 1; --n >= 0; app++) {
	if ((same = article_equal(app, app - 1))) {
	    app[0]->root_t_stamp = app[-1]->root_t_stamp;
	    app[0]->flag |= same;
	    app[-1]->flag |= A_NEXT_SAME;
	} else {
	    app[0]->root_t_stamp = app[0]->t_stamp;
	    app[0]->flag |= A_ROOT_ART;
	}
    }

    if (mode == 1)
	quicksort(articles, n_articles, article_header *, order_date_subj_date);
}

/*
 *	If articles are not sorted via sort_articles, they must still be
 *	marked with proper attributes (e.g. A_ROOT_ART) via no_sort_articles.
 */

void
no_sort_articles(void)
{
    register article_number n;
    register article_header *ah;

    for (n = n_articles; --n >= 0;) {
	ah = articles[n];
	ah->flag &= ~(A_SAME | A_ALMOST_SAME | A_NEXT_SAME | A_ROOT_ART);
	ah->flag |= A_ROOT_ART;
    }
    bypass_consolidation = 1;
}

/*
 * Eliminate articles with the A_KILL flag set preserving the present ordering.
 * This will only release the last entries in the articles array.
 * Neither strings nor articles headers are released.
 */

int
elim_articles(register article_number * list, int list_lgt)
{
    register article_header **srca, **desta;
    register article_number n, count;
    register flag_type same;
    int             changed, llen;

    count = 0;
    changed = 0, llen = 0;
    for (n = 0, srca = desta = articles; n < n_articles; n++, srca++) {
	if ((*srca)->attr == A_KILL) {
	    if (list_lgt > 0) {
		if (n < *list) {
		    if (llen)
			changed = 1;
		} else if (n == *list) {
		    if (llen) {
			llen++;
			list_lgt--;
			*list++ = -1;
		    } else
			++(*list);
		    changed = 1;
		}
	    }
	    continue;
	}
	if (list_lgt > 0 && n == *list) {
	    *list++ = count;
	    list_lgt--;
	    llen++;
	}
	count++;
	*desta++ = *srca;
    }
    if (list_lgt > 0) {
	if (!llen)
	    *list = 0;
	changed = 1;
    }
    n_articles = count;

    if (changed && n_articles > 0) {
	srca = articles;
	srca[0]->flag &= ~(A_SAME | A_ALMOST_SAME | A_NEXT_SAME);
	srca[0]->flag |= A_ROOT_ART;
	for (n = n_articles - 1, srca++; --n >= 0; srca++) {
	    srca[0]->flag &= ~(A_SAME | A_ALMOST_SAME | A_NEXT_SAME | A_ROOT_ART);
	    if ((same = article_equal(srca, srca - 1))) {
		srca[0]->flag |= same;
		srca[-1]->flag |= A_NEXT_SAME;
	    } else
		srca[0]->flag |= A_ROOT_ART;
	}
    }
    return changed;
}
