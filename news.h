/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2003 Michael T Pins.  All rights reserved.
 *
 *	Interface for decoding article headers.
 */

#ifndef _NN_NEWS_H
#define _NN_NEWS_H 1

struct news_header {

    int             ng_flag;	/* flags:			 */
#define N_DIGEST 1		/* article is part of a digest */
#define N_MODERATED 2		/* group is moderated		 */

    off_t           ng_fpos;	/* position of article text	 */
    off_t           ng_lpos;	/* last text offset		 */
    /* header lines:		 */
    char           *ng_from;	/* from			 */
    char           *ng_name;	/* senders name		 */
    char           *ng_subj;	/* subject			 */
    char           *ng_groups;	/* newsgroups			 */
    char           *ng_path;	/* path			 */
    char           *ng_reply;	/* reply-to			 */
    char           *ng_ident;	/* message id			 */
    char           *ng_follow;	/* followup to		 */
    char           *ng_ref;	/* references			 */
    char           *ng_keyw;	/* keywords			 */
    char           *ng_dist;	/* distibution		 */
    char           *ng_org;	/* organization		 */
    char           *ng_appr;	/* approved			 */
    char           *ng_summ;	/* summary			 */
    char           *ng_control;	/* control			 */
    char           *ng_sender;	/* sender			 */

    char           *ng_xref;	/* xref			 */
    char           *ng_date;	/* date			 */

    char           *ng_rdate;	/* date-received (News 3)	 */
    char           *ng_bref;	/* back-references (News 3)	 */
    char           *ng_origr;	/* originator			 */

    char           *ng_xlines;	/* lines (from header)	 */
    int             ng_lines;	/* lines (decoded)		 */
    char           *ng_comment;	/* comment-to (rfmail)	 */
}               news;


/*
 * digest article subheader
 */

struct digest_header {
    off_t           dg_hpos;	/* position of article header	 */
    off_t           dg_fpos;	/* position of article text	 */
    off_t           dg_lpos;	/* last text position		 */
    /* header lines:		 */
    char           *dg_date;	/* date			 */
    char           *dg_from;	/* from			 */
    char           *dg_subj;	/* subject			 */
    char           *dg_to;	/* to				 */

    int             dg_lines;	/* lines (pseudo field)	 */
}               digest;


#define	NEWS_HEADER_BUFFER	4096

typedef char    news_header_buffer[NEWS_HEADER_BUFFER];

char           *parse_header(FILE *, char **(*) (), int, news_header_buffer);
int             is_header_line(char *);
FILE           *open_news_article(article_header *, int, news_header_buffer, news_header_buffer);


/* modes */

#define	FILL_NEWS_HEADER	0x0001	/* parse first header -> buffer1 */
#define	FILL_DIGEST_HEADER	0x0002	/* parse second header -> buffer[12] */


#define	GET_ALL_FIELDS		0x0010	/* get all fields (otherwise only   */
 /* name, subj, groups, lines	    */

#define	GET_DATE_ONLY		0x0020	/* get Date field		    */

#define	FILL_OFFSETS		0x0080	/* fill ng_[hfl]pos */


#define	DIGEST_CHECK		0x0100	/* set N_DIGEST if "digest" in subj */
 /* only valid with FILL_NEWS_HEADER */
#define LAZY_BODY		0x0200	/* nntp: get body only for digests */


#define	SKIP_HEADER		0x1000	/* position after (sub) header */
#endif				/* _NN_NEWS_H */
