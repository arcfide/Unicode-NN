/*
 *      Copyright (c) 2003 Michael T Pins.  All rights reserved.
 */

void            novartdir(char *);
void            novfilename(char *);
struct novgroup *novopen(char *);
struct novgroup *novstream(register FILE *);
struct novart  *novall(register struct novgroup *, register article_number, register article_number);
struct novart  *novnext(register struct novgroup *);
void            novclose(register struct novgroup *);
