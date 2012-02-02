/*
 *      Copyright (c) 2003 Michael T Pins.  All rights reserved.
 */

int             group_completion(char *, int);
int             group_menu(register group_header *, article_number, register flag_type, char *, fct_type);
group_header   *lookup_regexp(char *, char *, int);
int             goto_group(int, article_header *, flag_type);
void            merge_and_read(flag_type, char *);
int             unsubscribe(group_header *);
void            group_overview(int);
