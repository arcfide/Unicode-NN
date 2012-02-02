/*
 *      Copyright (c) 2003 Michael T Pins.  All rights reserved.
 */

int             kill_article(article_header *);
int             auto_select_article(article_header *, int);
void            enter_kill_file(group_header *, char *, register flag_type, int);
int             kill_menu(article_header *);
int             init_kill(void);
void            rm_kill_file(void);
void            free_kill_entries(void);
void            dump_kill_list(void);
