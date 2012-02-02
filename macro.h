/*
 *      Copyright (c) 2003 Michael T Pins.  All rights reserved.
 */

void            init_macro(void);
char           *m_define(char *, FILE *);
char           *parse_enter_macro(FILE *, register int);
void            m_invoke(int);
void            m_startinput(void);
void            m_endinput(void);
void            m_advinput(void);
void            m_break_entry(void);
void            m_break(void);
int             m_getc(int *);
int             m_gets(char *);
int             m_yes(void);
