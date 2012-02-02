/*
 *      Copyright (c) 2003-2005 Michael T Pins.  All rights reserved.
 */

FILE           *open_purpose_file(void);
int             answer(article_header *, int, int);
int             cancel(article_header *);
void            do_nnpost(int, char *[]);
int             post_menu(void);
