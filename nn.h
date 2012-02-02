/*
 *      Copyright (c) 2003-2005 Michael T Pins.  All rights reserved.
 */

flag_type       parse_access_flags(void);
int             unread_mail(time_t);
void            stop_usage(void);
time_t          tick_usage(void);
int             display_motd(int);
void            nn_exit(int);

#ifdef ACCOUNTING
int             account(char, int);
#endif
