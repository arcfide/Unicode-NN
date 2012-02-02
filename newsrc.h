/*
 *      Copyright (c) 2003-2005 Michael T Pins.  All rights reserved.
 */

void            visit_rc_file(void);
void            use_newsrc(register group_header *, int);
attr_type       test_article(article_header *);
void            flush_newsrc(void);
int             restore_bak(void);
void            update_rc(register group_header *);
void            update_rc_all(register group_header *, int);
void            add_to_newsrc(group_header *);
article_number  restore_rc(register group_header *, article_number);
int             restore_unread(register group_header *);
void            count_unread_articles(void);
void            prt_unread(register char *);
article_number  add_unread(group_header *, int);
void            opt_nngrep(int, char *[]);
void            do_grep(char **);
int             opt_nntidy(int, char *[]);
void            do_tidy_newsrc(void);
int             opt_nngoback(int, char ***);
void            do_goback(void);

#ifdef NOV
group_header   *add_new_group(char *);
#endif
