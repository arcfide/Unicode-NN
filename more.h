/*
 *      Copyright (c) 2003 Michael T Pins.  All rights reserved.
 */

void            scan_header_fields(char *, article_header *);
int             next_header_field(char **, char **, fct_type *);
int             more(article_header *, int, int);
void            rot13_line(register char *);

#ifdef NOV
void            setpos(register article_header *, register FILE *);
#endif
