/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2003 Michael T Pins.  All rights reserved.
 *
 *	Memory handling
 */

#ifndef _NN_ARTICLES_H
#define _NN_ARTICLES_H 1

/* article headers */

article_number  n_articles;
article_header **articles;


typedef struct thunk {
    char           *this_thunk;
    struct thunk   *next_thunk;
    long            thunk_size;
}               thunk;


typedef struct {
    thunk          *sm_cur_t;
    int             sm_size;
    char           *sm_next;
}               string_marker;


typedef struct {
    string_marker   mm_string;
    thunk          *mm_cur_t;
    int             mm_size;
    article_header *mm_next;
    long            mm_nart;
}               memory_marker;


article_header *alloc_art(void);
char           *alloc_str(int);
void            free_memory(void);
void            mark_str(string_marker *);
void            release_str(string_marker *);
void            mark_memory(memory_marker *);
void            release_memory(memory_marker *);
void            merge_memory(void);
void            add_article(article_header *);
int             access_group(register group_header *, article_number, article_number, register flag_type, char *);


/* flags to access_group */

#define	ACC_ALSO_CROSS_POSTINGS	FLAG(1)	/* */
#define	ACC_DONT_SORT_ARTICLES	FLAG(2)	/* */
#define	ACC_DONT_SPLIT_DIGESTS	FLAG(3)	/* only full digest */
#define	ACC_ALSO_FULL_DIGEST	FLAG(4)	/* also full digest */
#define ACC_EXTRA_ARTICLES	FLAG(5)	/* add to current menu */
#define ACC_ALSO_READ_ARTICLES	FLAG(6)	/* */
#define ACC_ONLY_READ_ARTICLES	FLAG(7)	/* unread are already collected */
#define ACC_MERGED_MENU		FLAG(8)	/* set a_group field */
#define ACC_ORIG_NEWSRC		FLAG(9)	/* get previously unread articles */
#define ACC_VALIDATE_ONLY	FLAG(10)	/* don't save articles */
#define ACC_SPEW_MODE		FLAG(11)	/* */
#define ACC_ON_SENDER		FLAG(12)	/* match on sender (only) */
#define ACC_ON_SUBJECT		FLAG(13)	/* match on subject (also) */
#define ACC_DO_KILL		FLAG(14)	/* do auto-kill/select */
#define	ACC_PARSE_VARIABLES	FLAG(15)	/* kill, split, etc. */
#define ACC_MERGED_NEWSRC	FLAG(16)	/* merge orig and cur .newsrc */
#define ACC_ALSO_UNSUB_GROUPS	FLAG(17)	/* kill x-posts based on
						 * unsub also */

#ifdef ART_GREP
#define ACC_ON_GREP_UNREAD	FLAG(18)	/* grep article body, unread
						 * articles */
#define ACC_ON_GREP_ALL		FLAG(19)	/* grep article body, all
						 * articles */
#endif				/* ART_GREP */

#endif				/* _NN_ARTICLES_H */
