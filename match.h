/*
 *      Copyright (c) 2003 Michael T Pins.  All rights reserved.
 */

#include "regexp.h"

void            fold_string(register char *);
int             strmatch_fold(char *, register char *);
int             strmatch(char *, register char *);
int             strmatch_cf(char *, char *);
int             regexec_fold(register regexp *, char *);
int             regexec_cf(register regexp *, char *);
