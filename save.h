/*
 *      Copyright (c) 2003-2005 Michael T Pins.  All rights reserved.
 */

char           *init_save(int, char **);
int             save(article_header *);
int             end_save(void);
void            store_header(article_header *, FILE *, char *, char *);
int             mailbox_format(FILE *, int);
char           *run_mkdir(char *, char *);
int             change_dir(char *, int);
