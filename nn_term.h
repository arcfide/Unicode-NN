/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2004 Michael T Pins.  All rights reserved.
 *
 *	Terminal interface definitions.
 */

#ifndef _NN_TERM_H
#define _NN_TERM_H 1

#include <stdarg.h>

extern int      Lines, Columns, Name_Length;
extern int      cookie_size;
extern int      STANDOUT;

#define	NONE		(char *)NULL	/* no default string etc. */

#define	GET_S_BUFFER	256	/* if caller want to reuse get_s buffer */

/* special keys returned by get_c() */

#define	K_interrupt	CONTROL_('G')

#define	K_up_arrow	0x0081
#define	K_down_arrow	0x0082
#define K_left_arrow	0x0083
#define K_right_arrow	0x0084

#define	K_function(n)	(0x0085 + n)
#define K_m_d1		0x008f
#define K_m_d2		0x0090
#define K_m_d3		0x0091
#define K_m_u1		0x0092
#define K_m_u2		0x0093
#define K_m_u3		0x0094

#define	GETC_COMMAND	0x4000	/* bit set by get_c to return a command */


/*
 *	prompt_line = ...
 *	prompt( [P_COMMAND], ] [ format [, arg1 ... , arg4] ] );
 *
 *	P_MOVE:		just move to prompt line
 *	P_REDRAW:	redraw prompt
 *      P_VERSION:	print version on prompt line
 */


int             prompt_line;	/* prompt line */

#define	P_MOVE		(char *)1
#define P_REDRAW	(char *)5
#define	P_VERSION	(char *)9
#define P_SAVE		(char *)13
#define P_RESTORE	(char *)17

#define	CLEAR_DISPLAY	0x01
#define	CONFIRMATION	0x02

#include "keymap.h"

void            enter_multi_key(int, key_type *);
void            dump_multi_keys(void);
void            init_term(int);
void            home(void);
void            save_xy(void);
void            restore_xy(void);
void            gotoxy(int, int);
void            clrdisp(void);
void            clrline(void);
void            clrline_noflush(void);
void            clrpage(void);
void            tprintf(char *,...);
void            tputc(int);
int             so_gotoxy(int, int, int);
void            so_printf(char *,...);
void            so_end(void);
void            so_printxy(int, int, char *,...);
int             underline(int);
int             highlight(int);
int             shadeline(int);
void            xterm_mouse_on(void);
void            xterm_mouse_off(void);
void            visual_on(void);
int             visual_off(void);
void            nn_raw(void);
int             no_raw(void);
int             unset_raw(void);
void            flush_input(void);
int             get_c(void);
char           *get_s(char *, char *, char *, fct_type);
int             list_completion(char *);
int             yes(int);
void            ding(void);
void            display_file(char *, int);
void            nn_exitmsg(int, char *,...);
void            vmsg(char *, va_list);
void            msg(char *,...);
void            clrmsg(int);
void            prompt(char *,...);
int             any_key(int);
void            pg_init(int, int);
int             pg_scroll(int);
int             pg_next(void);
void            pg_indent(int);
int             pg_end(void);
void            user_delay(int);
#endif				/* _NN_TERM_H */
