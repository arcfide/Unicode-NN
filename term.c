/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Terminal interface.
 */
#define raw __curses__raw__
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "execute.h"
#include "folder.h"
#include "keymap.h"
#include "macro.h"
#include "nn.h"
#include "regexp.h"
#include "nn_term.h"

#if !defined(__FreeBSD__) && !(__NetBSD__) && !defined(NeXT)
#include <stropts.h>
#else
#include <sys/ioctl.h>
#endif

#ifdef RESIZING
#include <sys/ioctl.h>		/* for TIOCGWINSZ */

extern int      s_resized;
#endif				/* RESIZING */

#ifdef FAKE_INTERRUPT
#include <setjmp.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef USE_TERMINFO
#include <curses.h>

#ifndef VINTR
#include <termio.h>		/* some systems don't include this in
				 * curses.h */
#endif				/* VINTR */

#ifndef auto_left_margin
#include <term.h>
#endif				/* !auto_left_margin */

#else
#define USE_TERMCAP

#if !defined(SUNOS4) && !defined(NeXT)
#include <termcap.h>
#endif				/* SUNOS4 */

#endif

#ifdef HAVE_TERMIO_H

#ifdef USE_TERMCAP
#include <termio.h>
#endif				/* USE_TERMCAP */

#else

#ifndef __FreeBSD__
#include <sgtty.h>
#endif				/* __FreeBSD__ */

#endif

#ifdef SYSV_RESIZING
#include <sys/stream.h>
#include <sys/ptem.h>
#endif				/* SYSV_RESIZING */

/* SYSV curses.h clash */
#undef raw

/* AIX term.h clash */

#ifdef _AIX
#undef Lines
#undef Columns
#endif				/* _AIX */

#ifdef __osf__
void            cfmakeraw(struct termios * t);
#endif

/* term.c */

#ifdef HAVE_TERMIOS_H
static int      outc(int c);
#else
static int      outc(char c);
#endif

static void     set_term_speed(register unsigned long sp);
static void     raw_not_ok_error(void);
static sig_type rd_timeout(int n);
static int      read_char_kbd(int tmo);
static void     unread_char(int c);
static int      nnstandout(int);

static sig_type catch_winch(int n);

void            visual_on(void);
void            xterm_mouse_on(void);
void            xterm_mouse_off(void);

extern int      data_bits;
extern int      batch_mode;
extern char    *help_directory;

extern void     ding();
extern void     clrmsg();
extern void     gotoxy();

struct msg_list {
    char           *buf;
    struct msg_list *prev;
};

static struct msg_list *msg_stack = NULL, *msg_ptr = NULL;

int             message_history = 15;
char           *term_name = NULL;
int             show_current_time = 1;
int             conf_dont_sleep = 0;
int             prompt_length;
int             terminal_speed = 0;
int             slow_speed = 1200;
int             any_message = 0;
int             flow_control = 1;
int             use_visible_bell = 1;	/* if supported by terminal */
int             ignore_xon_xoff = 1;
int             multi_key_guard_time = 2;	/* tenths of a second */
int             guard_double_slash = 0;	/* need /// or //+ to clear line */
char           *shade_on_attr = NULL;
char           *shade_off_attr = NULL;
int             mouse_state;	/* if xterm is in mouse state */
int             mouse_usage;	/* 0 don't set mouse, 1 only if xterm, 2 set
				 * mouse */

key_type        help_key = '?';
key_type        comp1_key = SP;
key_type        comp2_key = TAB;
key_type        erase_key, kill_key;
key_type        delword_key = CONTROL_('W');

static char     bell_str[256] = "\007";
static int      appl_keypad_mode = 0;

#ifdef USE_TERMINFO
#define HAS_CAP(str) (str && *str)

extern char    *tgoto();	/* some systems don't have this in term.h */

#else

char           *tgoto();
char            PC;
char           *BC, *UP;
char            nnspeed;

static char     XBC[64], XUP[64];
static char     enter_ca_mode[64], exit_ca_mode[64];
static char     cursor_home[64];
static char     cursor_address[128];
static char     clear_screen[64];
static char     clr_eol[64];
static char     clr_eos[64];
static char     enter_standout_mode[64], exit_standout_mode[64];
static char     enter_underline_mode[64], exit_underline_mode[64];
static char     key_down[64], key_up[64], key_right[64], key_left[64];
static char     keypad_local[64], keypad_xmit[64];

int             magic_cookie_glitch;	/* magic cookie size */
int             ceol_standout_glitch;	/* hp brain damage! */
int             auto_right_margin;	/* automatic right margin */
int             eat_newline_glitch;	/* newline ignored at right margin */

#define putp(str)	tputs(str, 1, outc)

#define HAS_CAP(str)	(*str)
#endif				/* USE_TERMCAP */

static char     key_mouse_d1[64] = "\33[M ";
static char     key_mouse_d2[64] = "\33[M!";
static char     key_mouse_d3[64] = "\33[M\"";
static char     key_mouse_u1[64] = "\33[M#";

/*
 * Compute the greatest multiple of p not greater than x.
 * p must be a power of 2.
 * x must be nonnegative, for portability to non-2's-complement hosts.
 */
#define DISCARD_REMAINDER(x,p) ((x) & ~((p)-1))

#ifdef HAVE_TERMIOS_H
static int
outc(int c)
#else
static int
outc(char c)
#endif
{
    putchar(c);
    return 0;			/* XXX What is it supposed to return? */
}

#define putpc(str, cnt) tputs(str, cnt, outc)

int             Lines, Columns;	/* screen size */
int             Name_Length;	/* length of displayed name */
int             cookie_size;	/* size of magic cookie */
static int      two_cookies;	/* space needed to enter&exit standout mode */
int             STANDOUT;	/* terminal got standout mode */

static int      curxy_c = 0, curxy_l = -1, savxy_c = 0, savxy_l = -1;
static int      just_sent_cr = 0;	/* just sent \r to avoid 'xn' term
					 * attribute */

static int     *nonsp;		/* number of non-space characters on line */

#ifdef TERM_DEBUG
static char    *term_debug = NULL;
extern char    *getenv();
#define curxy_nonsp	(curxy_l < 0 ? -1 : nonsp[curxy_l])
#endif

#ifdef FAKE_INTERRUPT
extern jmp_buf  fake_keyb_sig;
extern int      arm_fake_keyb_sig;
extern char     fake_keyb_siglist[];
#endif				/* FAKE_INTERRUPT */

#if defined(HAVE_TERMIO_H) || defined(HAVE_TERMIOS_H)
/* This used to be 50, but there are some rather complex bugs in the SYSV */
/* TERMIO driver... */
#define KEY_BURST 2		/* read bursts of 1 char (or timeout after
				 * 100 ms) */

#undef CBREAK

#ifdef HAVE_TERMIOS_H
static struct termios norm_tty, raw_tty;
#else
static struct termio norm_tty, raw_tty;
#endif

#define	IntrC	((key_type) norm_tty.c_cc[VINTR])
#define	EraseC	((key_type) norm_tty.c_cc[VERASE])
#define KillC	((key_type) norm_tty.c_cc[VKILL])

#ifdef HAVE_TERMIOS_H
#define SuspC	((key_type) norm_tty.c_cc[VSUSP])
#else
#define SuspC	((key_type) CONTROL_('Z'))	/* norm_tty.c_cc[SWTCH] */
#endif

#else				/* V7/BSD TTY DRIVER */

static struct sgttyb norm_tty, raw_tty;
static struct tchars norm_chars;

#define	IntrC	((key_type) norm_chars.t_intrc)
#define	EraseC	((key_type) norm_tty.sg_erase)
#define KillC	((key_type) norm_tty.sg_kill)

#ifdef TIOCGLTC
static struct ltchars spec_chars;
#define SuspC	((key_type) spec_chars.t_suspc)
#else
#define	SuspC	((key_type) CONTROL_('Z'))
#endif

#endif

#ifdef USE_TERMCAP
static int
opt_cap(char *cap, char *buf)
{
    char           *tgetstr();

    *buf = NUL;
    return tgetstr(cap, &buf) != NULL;
}

static void
get_cap(char *cap, char *buf)
{
    if (!opt_cap(cap, buf))
	nn_exitmsg(1, "TERMCAP entry for %s has no '%s' capability",
		   term_name, cap);
}

#endif				/* USE_TERMCAP */

/*
 * timeout in n/10 seconds via SIGALRM
 */

static void
micro_alarm(int n)
{

#ifdef HAVE_UALARM
    ualarm(n <= 1 ? 100000 : n * 100000, 0);	/* 4.3 BSD ualarm() */
#else

#ifdef MICRO_ALARM
    if (n <= 0)
	n = 1;
    MICRO_ALARM(n);		/* System specific timeout */
#else
    alarm(n <= 10 ? 1 : (n + 9) / 10);	/* Standard alarm() call */
#endif				/* MICRO_ALARM */

#endif				/* HAVE_UALARM */
}

static int      multi_keys = 0;

static struct multi_key {
    key_type       *cur_key;
    key_type       *keys;
    key_type        code;
}               multi_key_list[MULTI_KEYS];

void
enter_multi_key(int code, key_type * keys)
{
    register int    i;

    if (strlen((char *) keys) == 1)
	/* will ignore arrow keys overlaying these keys */
	if (*keys == NL || *keys == CR ||
	    *keys == erase_key || *keys == kill_key ||
	    *keys == IntrC)
	    return;

    /* lookup code to see if it is already defined... */
    for (i = 0; i < multi_keys; i++)
	if (multi_key_list[i].code == (key_type) code)
	    goto replace_key;

    i = multi_keys++;

    /* now i points to matching or empty slot */
    if (i >= MULTI_KEYS) {
	/* should never happen */
	log_entry('E', "too many multi keys");
	return;
    }
replace_key:

    multi_key_list[i].keys = keys;
    multi_key_list[i].code = code;
}

void
dump_multi_keys(void)
{
    register int    i;
    register key_type *cp;

    clrdisp();
    pg_init(0, 1);

    for (i = 0; i < multi_keys; i++) {
	if (pg_next() < 0)
	    break;
	tprintf("%d\t%s\t", i, key_name(multi_key_list[i].code));
	for (cp = multi_key_list[i].keys; *cp; cp++)
	    tprintf(" %s", key_name(*cp));
    }

    pg_end();
}


#ifdef RESIZING
static          sig_type
catch_winch(int n)
{
    struct winsize  winsize;
    int             i;

    signal(SIGWINCH, catch_winch);
    if (ioctl(0, TIOCGWINSZ, &winsize) >= 0
	&& (winsize.ws_row != Lines || winsize.ws_col != Columns)) {
	nonsp = resizeobj(nonsp, int, winsize.ws_row);
	if ((int) winsize.ws_row > Lines)
	    for (i = Lines; i < (int) winsize.ws_row; i++)
		nonsp[i] = winsize.ws_col;
	if ((int) winsize.ws_col < Columns)
	    for (i = 0; i < (int) winsize.ws_row; i++)
		if (nonsp[i] > (int) winsize.ws_col)
		    nonsp[i] = winsize.ws_col;
	Lines = winsize.ws_row;
	Columns = winsize.ws_col;
	Name_Length = Columns / 5;
	if (Name_Length < NAME_LENGTH)
	    Name_Length = NAME_LENGTH;
	curxy_l = -1;
	s_redraw = 1;
	s_resized = 1;
    }

#ifdef FAKE_INTERRUPT
    if (fake_keyb_siglist[n] && arm_fake_keyb_sig)
	longjmp(fake_keyb_sig, 1);
#endif				/* FAKE_INTERRUPT */
}

#endif				/* RESIZING */

#ifdef SV_INTERRUPT

#ifdef NO_SIGINTERRUPT
static int
siginterrupt(signo, on)
{
    struct sigvec   sv;
    sv.sv_handler = signal(signo, SIG_DFL);
    sv.sv_mask = 0;
    sv.sv_flags = on ? SV_INTERRUPT : 0;
    sigvec(signo, &sv, 0);
}

#endif				/* NO_SIGINTERRUPT */

#endif				/* SV_INTERRUPT */

#ifdef FAKE_INTERRUPT
#define SV_INTERRUPT
static int
siginterrupt(signo, on)
{
    fake_keyb_siglist[signo] = on;
}

#endif				/* FAKE_INTERRUPT */

static unsigned sp_table[] = {
    B9600, 960,

#ifdef B19200
    B19200, 1920,
#else

#ifdef EXTA
    EXTA, 1920,
#endif				/* EXTA */

#endif				/* B19200 */

#ifdef B38400
    B38400, 3840,
#else

#ifdef EXTB
    EXTB, 3840,
#endif				/* EXTB */

#endif				/* B38400 */

    B1200, 120,
    B2400, 240,
    B4800, 480,
    B300, 30,
    0, 0
};

static void
set_term_speed(register unsigned long sp)
{
    register unsigned *tp;

    for (tp = sp_table; *tp; tp += 2)
	if (*tp == sp) {
	    terminal_speed = tp[1];
	    return;
	}
    terminal_speed = 30;
}

static void
raw_not_ok_error(void)
{
    if (batch_mode)
	return;
    nn_exitmsg(1, "Not prepared for terminal i/o");
    /* NOTREACHED */
}


#define	RAW_CHECK	if (terminal_speed == 0) {raw_not_ok_error(); return 0;}
#define	RAW_CHECK_V	if (terminal_speed == 0) {raw_not_ok_error(); return;}
#define BATCH_CHECK	if (terminal_speed == 0) return 0
#define BATCH_CHECK_V	if (terminal_speed == 0) return

void
init_term(int full)
{

#ifdef USE_TERMCAP
    char            tbuf[1024];
#endif

    int             i;

#ifdef TERM_DEBUG
    term_debug = getenv("TERM_DEBUG");
    if (term_debug)
	fprintf(stderr, "init_term(%d)\n", full);
#endif

    if (batch_mode) {
	term_name = "batch";
	close(0);
	open("/dev/null", 0);
	STANDOUT = 0;
	cookie_size = 1;
	return;
    }
    if ((term_name = getenv("TERM")) == NULL) {
	if (full)
	    nn_exitmsg(1, "No TERM variable in environment");
	else
	    term_name = "unknown";
    }
    if (!full)
	return;

#ifdef HAVE_TERMIO_H
    ioctl(0, TCGETA, &norm_tty);
#else

#ifdef HAVE_TERMIOS_H
    tcgetattr(0, &norm_tty);
#else
    ioctl(0, TIOCGETP, &norm_tty);
#endif				/* HAVE_TERMIOS_H */

#endif				/* HAVE_TERMIO_H */

#ifdef USE_TERMINFO
    setupterm((char *) NULL, 1, (int *) NULL);
    Columns = columns;
    Lines = lines;
    if (use_visible_bell && HAS_CAP(flash_screen))
	strcpy(bell_str, flash_screen);
    else if (HAS_CAP(bell))
	strcpy(bell_str, bell);
    if (!HAS_CAP(cursor_home))
	cursor_home = copy_str(tgoto(cursor_address, 0, 0));
#else

    if (tgetent(tbuf, term_name) <= 0)
	nn_exitmsg(1, "Unknown terminal type: %s", term_name);

    if (opt_cap("bc", XBC))
	BC = XBC;
    if (opt_cap("up", XUP))
	UP = XUP;
    opt_cap("pc", cursor_address);	/* temp. usage */
    PC = cursor_address[0];

    get_cap("cm", cursor_address);
    if (!opt_cap("ho", cursor_home))
	strcpy(cursor_home, tgoto(cursor_address, 0, 0));

    get_cap("cl", clear_screen);
    opt_cap("ce", clr_eol);
    opt_cap("cd", clr_eos);

    Lines = tgetnum("li");
    Columns = tgetnum("co");

    opt_cap("so", enter_standout_mode);
    opt_cap("se", exit_standout_mode);

    opt_cap("us", enter_underline_mode);
    opt_cap("ue", exit_underline_mode);

    opt_cap("kd", key_down);
    opt_cap("ku", key_up);
    opt_cap("kr", key_right);
    opt_cap("kl", key_left);

    magic_cookie_glitch = tgetnum("sg");

    ceol_standout_glitch = tgetflag("xs");
    auto_right_margin = tgetflag("am");
    eat_newline_glitch = tgetflag("xn");

    opt_cap("ti", enter_ca_mode);
    opt_cap("te", exit_ca_mode);
    opt_cap("ks", keypad_xmit);	/* used to turn "application cursor */
    opt_cap("ke", keypad_local);/* key" mode on and off (sometimes) */

    if (!use_visible_bell || !opt_cap("vb", bell_str))
	if (!opt_cap("bl", bell_str))
	    strcpy(bell_str, "\007");
#endif				/* USE_TERMINFO */

#ifdef RESIZING
    {
	struct winsize  winsize;

	if (ioctl(0, TIOCGWINSZ, &winsize) >= 0
	    && winsize.ws_row != 0 && winsize.ws_col != 0) {
	    Lines = winsize.ws_row;
	    Columns = winsize.ws_col;
	    signal(SIGWINCH, catch_winch);

#ifdef SV_INTERRUPT
	    siginterrupt(SIGWINCH, 1);	/* make read from tty interruptable */
#endif				/* SV_INTERRUPT */
	}
    }
#endif				/* RESIZING */

    /* Stop NN from blowing up if on a *really* dumb terminal, like "dumb" */
    if (Lines < 1)
	Lines = 24;
    if (Columns < 1)
	Columns = 80;

    nonsp = newobj(int, Lines);
    for (i = 0; i < Lines; i++)
	nonsp[i] = Columns;

    STANDOUT = HAS_CAP(enter_standout_mode);
    cookie_size = magic_cookie_glitch;
    if (STANDOUT) {
	if (cookie_size < 0)
	    cookie_size = 0;
	two_cookies = 2 * cookie_size;
    } else
	cookie_size = two_cookies = 0;

    raw_tty = norm_tty;

#ifdef HAVE_TERMIO_H
    raw_tty.c_iflag &= ~(BRKINT | INLCR | ICRNL | IGNCR | ISTRIP);
    raw_tty.c_iflag |= IGNBRK | IGNPAR;
    raw_tty.c_oflag &= ~OPOST;
    raw_tty.c_lflag &= ~(ISIG | ICANON | XCASE | ECHO | NOFLSH);

    /* read a maximum of 10 characters in one burst; timeout in 1-200 ms */
    raw_tty.c_cc[VMIN] = KEY_BURST;
    raw_tty.c_cc[VTIME] = ((int) (raw_tty.c_cflag & CBAUD) > B1200) ? 1 : 2;
    set_term_speed((unsigned long) (raw_tty.c_cflag & CBAUD));
#else

#ifdef HAVE_TERMIOS_H
    cfmakeraw(&raw_tty);
    /* read a maximum of 10 characters in one burst; timeout in 1-200 ms */
    raw_tty.c_cc[VMIN] = KEY_BURST;

#if 0
    raw_tty.c_cc[VTIME] = (cfgetispeed(&raw_tty) > B1200) ? 1 : 2;
#else
    raw_tty.c_cc[VTIME] = 1;
#endif				/* 0 */

    set_term_speed((unsigned long) cfgetospeed(&raw_tty));

#ifdef SV_INTERRUPT
    siginterrupt(SIGTSTP, 1);
    siginterrupt(SIGALRM, 1);
#endif				/* SV_INTERRUPT */

#else
    ioctl(0, TIOCGETC, &norm_chars);

#ifdef TIOCGLTC
    ioctl(0, TIOCGLTC, &spec_chars);
#endif				/* TIOCGLTC */

    nnspeed = norm_tty.sg_ospeed;
    set_term_speed((unsigned long) nnspeed);

    raw_tty.sg_flags &= ~(ECHO | CRMOD);

#ifdef CBREAK

#ifdef SV_INTERRUPT		/* make read from tty interruptable */
    siginterrupt(SIGTSTP, 1);	/* this is necessary to redraw screen */
#endif				/* SV_INTERRUPT */

    raw_tty.sg_flags |= CBREAK;
#else
    raw_tty.sg_flags |= RAW;
#endif				/* CBREAK */

#ifdef SV_INTERRUPT
    siginterrupt(SIGALRM, 1);	/* make read from tty interruptable */
#endif				/* SV_INTERRUPT */

#endif				/* HAVE_TERMIOS_H */

#endif				/* HAVE_TERMIO_H */

    Name_Length = Columns / 5;
    if (Name_Length < NAME_LENGTH)
	Name_Length = NAME_LENGTH;

    erase_key = EraseC;
    kill_key = KillC;

    if (HAS_CAP(key_down))
	enter_multi_key(K_down_arrow, (key_type *) key_down);
    if (HAS_CAP(key_up))
	enter_multi_key(K_up_arrow, (key_type *) key_up);
    if (HAS_CAP(key_right))
	enter_multi_key(K_right_arrow, (key_type *) key_right);
    if (HAS_CAP(key_left))
	enter_multi_key(K_left_arrow, (key_type *) key_left);

    enter_multi_key(K_m_d1, (key_type *) key_mouse_d1);
    enter_multi_key(K_m_d2, (key_type *) key_mouse_d2);
    enter_multi_key(K_m_d3, (key_type *) key_mouse_d3);
    enter_multi_key(K_m_u1, (key_type *) key_mouse_u1);

    appl_keypad_mode = (HAS_CAP(keypad_xmit) && HAS_CAP(keypad_local));
    if (!HAS_CAP(key_up))
	appl_keypad_mode = 0;	/* no cursor keys */
    if (appl_keypad_mode) {
	/* Use of ks/ke isn't consistent, so we must guess what to do. */
	/* If termcap expects keys to send ESC [, don't switch */
	appl_keypad_mode = (key_up[0] != '\033' || key_up[1] != '[');
    }
    if ((mouse_usage == 2) || ((mouse_usage == 1)
			       && !strncmp("xterm", term_name, 5))) {
	mouse_state = 1;
	flow_control = 0;
    } else {
	mouse_state = 0;
    }
    visual_on();
}

void
home(void)
{
    BATCH_CHECK_V;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "home\n");
#endif

    putp(cursor_home);
    curxy_c = curxy_l = 0;
}

void
save_xy(void)
{
    savxy_c = curxy_c;
    savxy_l = curxy_l;
}

void
restore_xy(void)
{
    if (savxy_l < 0)
	return;
    gotoxy(savxy_c, savxy_l);
    fl;
}

void
gotoxy(int c, int l)
{
    BATCH_CHECK_V;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "gotoxy %d %d -> %d %d\n", curxy_c, curxy_l, c, l);
#endif

    if (Columns <= (unsigned) c || Lines <= (unsigned) l)
	return;

    if (!(c | l))
	putp(cursor_home);
    else
	putp(tgoto(cursor_address, c, l));
    curxy_c = c;
    curxy_l = l;
}

void
clrdisp(void)
{
    int             i;

    BATCH_CHECK_V;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "clrdisp\n");
#endif

    putpc(clear_screen, Lines);
    curxy_c = curxy_l = 0;
    savxy_l = -1;
    for (i = 0; i < Lines; i++)
	nonsp[i] = 0;
    msg_ptr = msg_stack;
}

void            clrline_noflush();

void
clrline(void)
{
    BATCH_CHECK_V;

    /* If we moved the cursor left to avoid weird effects, don't clear. */
    if (just_sent_cr)
	return;

    clrline_noflush();
    fl;
}

void
clrline_noflush(void)
{
    int             oldxy_c, oldxy_l, spcnt;

    BATCH_CHECK_V;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "clrline %d %d [%d]", curxy_c, curxy_l, curxy_nonsp);
#endif

    if (curxy_l < 0)
	return;

    /* remainder of line already blank? */

    if (curxy_c >= nonsp[curxy_l]) {

#ifdef TERM_DEBUG
	if (term_debug)
	    fprintf(stderr, " ignored\n");
#endif

	return;
    }
    if (HAS_CAP(clr_eol)) {

#ifdef TERM_DEBUG
	if (term_debug)
	    fprintf(stderr, "\n");
#endif

	putp(clr_eol);
    } else {
	oldxy_c = curxy_c;
	oldxy_l = curxy_l;

	spcnt = nonsp[curxy_l] - curxy_c;

	/* guard against scroll */

	if (auto_right_margin && curxy_l == Lines - 1 && curxy_c + spcnt == Columns)
	    spcnt--;

#ifdef TERM_DEBUG
	if (term_debug)
	    fprintf(stderr, " %d\n", spcnt);
#endif				/* TERM_DEBUG */

#ifdef TERM_DEBUG
	if (term_debug && *term_debug) {
	    while (spcnt--)
		tputc(*term_debug);
	    spcnt = 0;
	}
#endif

	/* clear out line */

	while (spcnt--)
	    tputc(SP);

	if (curxy_c != oldxy_c || curxy_l != oldxy_l)
	    gotoxy(oldxy_c, oldxy_l);
    }
    nonsp[curxy_l] = curxy_c;
}

void
clrpage(void)
{
    int             oldxy_c, oldxy_l, i;

    BATCH_CHECK_V;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "clrpage %d %d [%d]\n", curxy_c, curxy_l, curxy_nonsp);
#endif

    if (curxy_l < 0)
	return;

    oldxy_c = curxy_c;
    oldxy_l = curxy_l;

    /* code below only handles curxy_c == 0 */

    if (curxy_c != 0) {
	clrline();
	if (curxy_l < Lines - 1)
	    gotoxy(0, curxy_l + 1);
    }
    /* clear to end of screen */

    if (curxy_c == 0) {
	if (HAS_CAP(clr_eos)) {
	    putpc(clr_eos, Lines - curxy_l);
	    for (i = curxy_l; i < Lines; i++)
		nonsp[i] = 0;
	} else if (curxy_l == 0) {
	    putpc(clear_screen, Lines);
	    for (i = 0; i < Lines; i++)
		nonsp[i] = 0;
	} else {
	    for (i = curxy_l; i < Lines; i++) {
		if (nonsp[i] != 0) {
		    gotoxy(0, i);
		    clrline_noflush();
		}
	    }
	}
    }
    if (curxy_c != oldxy_c || curxy_l != oldxy_l)
	gotoxy(oldxy_c, oldxy_l);

    fl;
    msg_ptr = msg_stack;
}

static char     buf[512];

static void
tvprintf(char *fmt, va_list ap)
{
    char           *str;

    vsprintf(buf, fmt, ap);
    for (str = buf; *str; str++)
	tputc(*str);
}

void
tprintf(char *fmt,...)
{
    va_list         ap;

    va_start(ap, fmt);
    tvprintf(fmt, ap);
    va_end(ap);
}

void
tputc(int c)
{
    int             i;

#ifdef TERM_DEBUG
    if (term_debug) {
	fprintf(stderr, "tputc %d %d [%d] ", curxy_c, curxy_l, curxy_nonsp);
	if (c < ' ' || c > '~')
	    switch (c) {
		case 0:
		    fprintf(stderr, "'NUL'");
		    break;
		case 1:
		    fprintf(stderr, "'SOH'");
		    break;
		case 2:
		    fprintf(stderr, "'STX'");
		    break;
		case 3:
		    fprintf(stderr, "'ETX'");
		    break;
		case 4:
		    fprintf(stderr, "'EOT'");
		    break;
		case 5:
		    fprintf(stderr, "'ENQ'");
		    break;
		case 6:
		    fprintf(stderr, "'ACK'");
		    break;
		case 7:
		    fprintf(stderr, "'BEL'");
		    break;
		case 8:
		    fprintf(stderr, "'BS'");
		    break;
		case 9:
		    fprintf(stderr, "'HT'");
		    break;
		case 10:
		    fprintf(stderr, "'NL'");
		    break;
		case 11:
		    fprintf(stderr, "'VT'");
		    break;
		case 12:
		    fprintf(stderr, "'NP'");
		    break;
		case 13:
		    fprintf(stderr, "'CR'");
		    break;
		case 14:
		    fprintf(stderr, "'SP'");
		    break;
		case 15:
		    fprintf(stderr, "'SI'");
		    break;
		case 16:
		    fprintf(stderr, "'DLE'");
		    break;
		case 17:
		    fprintf(stderr, "'DC1'");
		    break;
		case 18:
		    fprintf(stderr, "'DC2'");
		    break;
		case 19:
		    fprintf(stderr, "'DC3'");
		    break;
		case 20:
		    fprintf(stderr, "'DC4'");
		    break;
		case 21:
		    fprintf(stderr, "'NAK'");
		    break;
		case 22:
		    fprintf(stderr, "'SYN'");
		    break;
		case 23:
		    fprintf(stderr, "'ETB'");
		    break;
		case 24:
		    fprintf(stderr, "'CAN'");
		    break;
		case 25:
		    fprintf(stderr, "'EM'");
		    break;
		case 26:
		    fprintf(stderr, "'SUB'");
		    break;
		case 27:
		    fprintf(stderr, "'ESC'");
		    break;
		case 28:
		    fprintf(stderr, "'FS'");
		    break;
		case 29:
		    fprintf(stderr, "'GS'");
		    break;
		case 30:
		    fprintf(stderr, "'RS'");
		    break;
		case 31:
		    fprintf(stderr, "'US'");
		    break;
		case 32:
		    fprintf(stderr, "'SP'");
		    break;
		default:
		    fprintf(stderr, "out of range: ???");
		    break;
	    }
	else
	    fprintf(stderr, "'%c'", c);
    }
#endif

    just_sent_cr = 0;

    putchar(c);

    switch (c) {

	case '\n':
	    curxy_c = 0;
	    if (curxy_l >= 0)
		curxy_l++;
	    break;

	case '\r':
	    curxy_c = 0;
	    break;

	case '\t':
	    curxy_c = DISCARD_REMAINDER(curxy_c + 8, 8);
	    break;

	case '\b':
	    curxy_c--;
	    break;

	case ' ':
	    curxy_c++;
	    if (curxy_l >= 0 && nonsp[curxy_l] == curxy_c)
		nonsp[curxy_l]--;
	    break;

	default:
	    curxy_c++;
	    if (curxy_l >= 0 && nonsp[curxy_l] < curxy_c)
		nonsp[curxy_l] = curxy_c;
	    break;

    }

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, " -> %d %d [%d]", curxy_c, curxy_l, curxy_nonsp);
#endif

    /* account for right margin */

    if (curxy_c == Columns) {

#ifdef TERM_DEBUG
	if (term_debug)
	    fprintf(stderr, " margin");
#endif

	if (auto_right_margin) {
	    if (eat_newline_glitch) {
		putchar(CR);
		just_sent_cr = 1;
		curxy_c = 0;
	    } else {
		curxy_c = 0;
		if (curxy_l >= 0)
		    curxy_l++;
	    }
	} else
	    curxy_c--;
    }
    /* account for vertical scroll */

    if (curxy_l == Lines) {

#ifdef TERM_DEBUG
	if (term_debug)
	    fprintf(stderr, " scroll");
#endif

	for (i = 1; i < Lines; i++)
	    nonsp[i - 1] = nonsp[i];
	nonsp[--curxy_l] = 0;
    }

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "\n");
#endif
}

static char     so_buf[512], *so_p;
static int      so_c, so_l, so_b, so_active = 0;

int
so_gotoxy(int c, int l, int blank)
{
    if (!STANDOUT && c >= 0) {
	if (c >= 0 && l >= 0)
	    gotoxy(c, l);
	return 0;
    }
    so_active++;
    so_c = c;
    so_l = l;
    so_b = blank;
    so_p = so_buf;
    *so_p = NUL;

    return 1;			/* not really true if not standout & c < 0 */
}

static void
so_vprintf(char *fmt, va_list ap)
{
    if (!so_active) {
	if (ceol_standout_glitch)
	    highlight(0);	/* xxx why? */
	tvprintf(fmt, ap);
	return;
    }
    vsprintf(so_p, fmt, ap);
    while (*so_p)
	so_p++;
}

void
so_printf(char *fmt,...)
{
    va_list         ap;

    va_start(ap, fmt);
    so_vprintf(fmt, ap);
    va_end(ap);
}

void
so_end(void)
{
    int             len;

    if (!so_active)
	return;

    if (so_l >= 0) {

	len = so_p - so_buf + two_cookies;

	if (so_c < 0)
	    so_c = Columns - len - 2;
	if (so_c < 0)
	    so_c = 0;

	if (len + so_c >= Columns) {
	    len = Columns - so_c - two_cookies;
	    so_buf[len] = NUL;
	}
	if (cookie_size) {
	    gotoxy(so_c + len - cookie_size, so_l);
	    nnstandout(0);
	}
	gotoxy(so_c, so_l);

    }
    if ((so_b & 1) && (!STANDOUT || !cookie_size))
	tputc(SP);

    if (STANDOUT) {
	if (ceol_standout_glitch)
	    clrline();
	nnstandout(1);
    }
    tprintf("%s", so_buf);

    if (STANDOUT)
	nnstandout(0);

    if ((so_b & 2) && (!STANDOUT || !cookie_size))
	tputc(SP);

    so_active = 0;
}


void
so_printxy(int c, int l, char *fmt,...)
{
    va_list         ap;

    va_start(ap, fmt);
    so_gotoxy(c, l, 0);
    so_vprintf(fmt, ap);
    so_end();
    va_end(ap);
}


int
underline(int on)
{
    if (cookie_size)
	return 0;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "underline %d %d [%d] %d", curxy_c, curxy_l, curxy_nonsp, on);
#endif

    if (!HAS_CAP(enter_underline_mode))
	return 0;
    putp(on ? enter_underline_mode : exit_underline_mode);
    return 1;
}

int
highlight(int on)
{
    if (cookie_size)
	return 0;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "highlight %d %d [%d] %d", curxy_c, curxy_l, curxy_nonsp, on);
#endif

    if (!HAS_CAP(enter_standout_mode))
	return 0;
    putp(on ? enter_standout_mode : exit_standout_mode);
    return 1;
}

int
shadeline(int on)
{
    if (cookie_size)
	return 0;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "shadeline %d %d [%d] %d", curxy_c, curxy_l, curxy_nonsp, on);
#endif

    if (shade_on_attr && shade_off_attr) {
	putp(on ? shade_on_attr : shade_off_attr);
	return 1;
    } else
	return underline(1);
}

static int
nnstandout(int on)
{

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "standout %d %d [%d] %d", curxy_c, curxy_l, curxy_nonsp, on);
#endif

    if (!HAS_CAP(enter_standout_mode))
	return 0;
    putp(on ? enter_standout_mode : exit_standout_mode);
    curxy_c += cookie_size;
    if (curxy_l >= 0 && curxy_c > nonsp[curxy_l])
	nonsp[curxy_l] = curxy_c;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, " -> %d %d [%d]\n", curxy_c, curxy_l, curxy_nonsp);
#endif

    return 1;
}

static int      is_visual = 0;
static int      is_raw = 0;

#ifdef HAVE_TERMIO_H
#define RAW_MODE_ON    ioctl(0, TCSETAW, &raw_tty)
#define RAW_MODE_OFF   ioctl(0, TCSETAW, &norm_tty)
#else

#ifdef HAVE_TERMIOS_H
#define RAW_MODE_ON    tcsetattr(0, TCSADRAIN, &raw_tty)
#define RAW_MODE_OFF   tcsetattr(0, TCSADRAIN, &norm_tty)
#else
#define RAW_MODE_ON    ioctl(0, TIOCSETP, &raw_tty)
#define RAW_MODE_OFF   ioctl(0, TIOCSETP, &norm_tty)
#endif				/* HAVE_TERMIOS_H */

#endif				/* HAVE_TERMIO_H */

void
xterm_mouse_on(void)
{
    putp("\33[?1000h");
}

void
xterm_mouse_off(void)
{
    putp("\33[?1000l");
}

void
visual_on(void)
{
    BATCH_CHECK_V;

    if (terminal_speed == 0)
	return;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "visual_on\n");
#endif

    if (!is_visual) {
	if (HAS_CAP(enter_ca_mode))
	    putp(enter_ca_mode);
	if (appl_keypad_mode)
	    putp(keypad_xmit);
	if (mouse_state)
	    xterm_mouse_on();
	fl;
    }
    is_visual = 1;
}

int
visual_off(void)
{
    int             was_raw = is_raw;

    if (terminal_speed == 0)
	return 0;

#ifdef TERM_DEBUG
    if (term_debug)
	fprintf(stderr, "visual_off\n");
#endif

    if (is_visual) {
	if (appl_keypad_mode)
	    putp(keypad_local);
	if (HAS_CAP(exit_ca_mode))
	    putp(exit_ca_mode);
	if (mouse_state)
	    xterm_mouse_off();
	fl;
    }
    is_visual = 0;

    is_raw = 1;
    unset_raw();

    return was_raw;
}


#ifdef CBREAK
void
nn_raw(void)
{
    RAW_CHECK_V;

    if (is_raw == 1)
	return;
    is_raw = 1;
    RAW_MODE_ON;
}

int
no_raw(void)
{
    return 0;
}

int
unset_raw(void)
{
    if (is_raw == 0)
	return 0;
    RAW_CHECK;
    RAW_MODE_OFF;
    is_raw = 0;
    return 1;
}

#else				/* not CBREAK */
static int      must_set_raw = 1;

#undef raw
void
nn_raw(void)
{
    RAW_CHECK_V;

    if (!flow_control) {
	if (!must_set_raw)
	    return;
	must_set_raw = 0;
    }
    if (is_raw)
	return;

    RAW_MODE_ON;
    is_raw++;
}

int
no_raw(void)
{
    if (!flow_control)
	return 0;

    if (!is_raw)
	return 0;

    RAW_CHECK;

    RAW_MODE_OFF;

    is_raw = 0;

    return 1;
}

int
unset_raw(void)
{
    int             was_raw = is_raw;

    if (is_raw) {
	RAW_CHECK;
	RAW_MODE_OFF;
	is_raw = 0;
    }
    if (!flow_control)
	must_set_raw = 1;
    return was_raw;
}

#endif				/* CBREAK */

#ifndef KEY_BURST
#define KEY_BURST 32
#endif				/* KEY_BURST */

#define RD_PUSHBACK 10
static char     rd_buffer[KEY_BURST + RD_PUSHBACK];	/* Holds stuff from read */
static char    *rd_ptr;
static int      rd_count = 0, rd_alarm = 0;

#ifdef FAKE_INTERRUPT
static jmp_buf  fake_alarm_sig;
#endif				/* FAKE_INTERRUPT */

static          sig_type
rd_timeout(int n)
{
    rd_alarm = 1;

#ifdef FAKE_INTERRUPT
    longjmp(fake_alarm_sig, 1);
#endif				/* FAKE_INTERRUPT */
}

#define RD_TIMEOUT	0x1000
#define RD_INTERRUPT	0x1001

static int
read_char_kbd(int tmo)
 /* tmo	 timeout if no input arrives */
{
    if (rd_count <= 0) {
	if (tmo) {

#ifdef FAKE_INTERRUPT
	    if (setjmp(fake_alarm_sig))
		goto tmout;
#endif				/* FAKE_INTERRUPT */

	    rd_alarm = 0;
	    signal(SIGALRM, rd_timeout);
	    micro_alarm(multi_key_guard_time);
	}
	rd_ptr = rd_buffer + RD_PUSHBACK;
	rd_count = read(0, rd_ptr, KEY_BURST);
	if (tmo) {
	    if (rd_alarm)
		goto tmout;
	    alarm(0);
	}
	if (rd_count < 0) {
	    if (errno != EINTR)
		s_hangup++;
	    return RD_INTERRUPT;
	}
    }
    --rd_count;
    return *rd_ptr++;

tmout:
    rd_count = 0;
    return RD_TIMEOUT;
}

static void
unread_char(int c)
{
    if (rd_ptr == rd_buffer)
	return;
    rd_count++;
    *--rd_ptr = c;
}

void
flush_input(void)
{

#ifndef HAVE_TERMIO_H

#if !defined(HAVE_TERMIOS_H) && defined(FREAD)
    int             arg;
#endif

#endif

    BATCH_CHECK_V;

#ifdef HAVE_TERMIO_H
    ioctl(0, TCFLSH, 0);
#else

#ifdef HAVE_TERMIOS_H
    ioctl(0, TCIFLUSH);
#else

#ifdef FREAD
    arg = FREAD;
    ioctl(0, TIOCFLUSH, &arg);
#else
    ioctl(0, TIOCFLUSH, 0);
#endif				/* FREAD */

#endif				/* HAVE_TERMIOS_H */

#endif				/* HAVE_TERMIO_H */

    rd_count = 0;
}

int             enable_stop = 1;

static int      do_macro_processing = 1;
int             mouse_y = -1;
static int      mouse_last = 0;

int
get_c(void)
{
    key_type        c, first_key;
    int             key_cnt, mc, n;
    register struct multi_key *mk, *multi_match;
    register int    i;

    multi_match = NULL;
    first_key = 0;
    mouse_y = -1;

next_key:
    if (s_hangup)
	return K_interrupt;

    if (do_macro_processing)
	switch (m_getc(&mc)) {
	    case 0:
		break;
	    case 1:
		return mc;
	    case 2:
		return K_interrupt;
	}

#ifdef RESIZING
    if (s_resized)
	goto redraw;
#endif				/* RESIZING */

    if (batch_mode)
	nn_exitmsg(1, "Attempt to read keyboard input in batch mode");

    for (i = multi_keys, mk = multi_key_list; --i >= 0; mk++)
	mk->cur_key = mk->keys;
    key_cnt = 0;

#ifdef FAKE_INTERRUPT
    if (setjmp(fake_keyb_sig))
	goto intr;
    arm_fake_keyb_sig = 1;
#endif				/* FAKE_INTERRUPT */

multi:
    switch (n = read_char_kbd(key_cnt)) {

	case RD_INTERRUPT:

#ifdef CBREAK
	    if (s_redraw)
		goto redraw;
#endif				/* CBREAK */

#ifdef RESIZING
	    if (s_resized)
		goto redraw;
#endif				/* RESIZING */

	    goto intr;

	case RD_TIMEOUT:
	    while (--key_cnt > 0)
		unread_char(multi_match->keys[key_cnt]);
	    c = first_key;
	    goto got_char;

	default:
	    c = (key_type) n;
	    if (data_bits < 8)
		c &= 0x7f;
	    if (ignore_xon_xoff)
		if (c == CONTROL_('Q') || c == CONTROL_('S'))
		    goto multi;
	    break;
    }

    multi_match = NULL;
    for (i = multi_keys, mk = multi_key_list; --i >= 0; mk++) {
	if (mk->cur_key == NUL)
	    continue;
	if (*(mk->cur_key)++ != c) {
	    mk->cur_key = NUL;
	    continue;
	}
	if (*(mk->cur_key) == NUL) {
	    c = mk->code;

	    /*
	     * xterm mouse specific code, translate cursor position into
	     * static variables, synthesize key up events
	     */
	    if ((c >= K_m_d1) && (c <= K_m_u1)) {
		mouse_y = read_char_kbd(key_cnt) - '!';
		if (c == K_m_u1) {
		    if (mouse_last == K_m_d2) {
			c = K_m_u2;
		    } else if (mouse_last == K_m_d3) {
			c = K_m_u3;
		    }
		}
		mouse_last = c;
	    }
	    goto got_char;
	}
	multi_match = mk;
    }

    if (multi_match) {
	if (key_cnt == 0)
	    first_key = c;
	key_cnt++;
	goto multi;
    }
    if (key_cnt) {
	if (key_cnt == 1 && first_key == 033) {
	    unread_char(c);
	    c = 033;
	    goto got_char;
	}
	ding();
	flush_input();
	goto next_key;
    }
got_char:

#ifdef FAKE_INTERRUPT
    arm_fake_keyb_sig = 0;
#endif				/* FAKE_INTERRUPT */

    c = global_key_map[c];

#ifndef CBREAK
    if (c == IntrC)
	return K_interrupt;	/* don't flush */
    if (c == SuspC) {
	if (enable_stop && suspend_nn())
	    goto redraw;
	goto next_key;
    }
#endif				/* CBREAK */

    return (int) c;

intr:

#ifdef FAKE_INTERRUPT
    arm_fake_keyb_sig = 0;
#endif				/* FAKE_INTERRUPT */

    rd_count = 0;
    return K_interrupt;

redraw:

#ifdef RESIZING
    s_resized = 0;
#endif				/* RESIZING */

    s_redraw = 0;
    return GETC_COMMAND | K_REDRAW;
}


/*
 * read string with completion, pre-filling, and break on first char
 *
 *	dflt		is a string that will be use as default value if the
 *			space bar is hit as the first character.
 *
 *	prefill		pre-fill the buffer with .... and print it
 *
 *	break_chars	return immediately if one of these characters
 *			is entered as the first character.
 *
 *	completion	is a function that will fill the buffer with a value
 *			see the group_completion and file_completion routines
 *			for examples.
 */

char           *
get_s(char *dflt, char *prefill, char *break_chars, fct_type completion)
{
    static key_type lbuf[GET_S_BUFFER];
    register char  *cp;
    register int    i, c, lastc;
    char           *ret_val = (char *) lbuf;
    int             comp_used, comp_len;
    int             ostop, max, did_help;
    int             hit_count;

    switch (m_gets((char *) lbuf)) {
	case 0:
	    break;
	case 1:
	    return (char *) lbuf;
	case 2:
	    return NULL;
    }

    ostop = enable_stop;
    enable_stop = 0;
    do_macro_processing = 0;
    hit_count = 0;

    max = Columns - prompt_length;

    if (max >= FILENAME)
	max = FILENAME - 1;

    i = comp_len = comp_used = did_help = 0;

    if (prefill && prefill[0]) {
	while ((c = *prefill++)) {
	    if (i == max)
		break;

	    tputc(c);
	    lbuf[i] = c;
	    i++;
	}
	fl;
    }
    if (dflt && *dflt == NUL)
	dflt = NULL;

    if (break_chars && *break_chars == NUL)
	break_chars = NULL;

    c = NUL;
    for (;;) {
	lastc = c;
	c = get_c();
	if (c & GETC_COMMAND)
	    continue;

kill_prefill_hack:

	hit_count++;

	if (i == 0) {
	    if (c == comp1_key && dflt) {
		while ((c = *dflt++) != NUL && i < max) {
		    tputc(c);
		    lbuf[i] = c;
		    i++;
		}
		fl;
		dflt = NULL;
		continue;
	    }
	    if ((cp = break_chars)) {	/* Stupid ^@$# GCC!!! */
		while (*cp)
		    if (*cp++ == c) {
			lbuf[0] = c;
			lbuf[1] = NUL;
			goto out;
		    }
	    }
	}
	if (completion != NULL_FCT) {
	    if (comp_used && c == erase_key) {
		if (comp_len) {
		    i -= comp_len;
		    while (--comp_len >= 0)
			tputc(BS);
		    clrline();
		}
		if (!CALL(completion) (lbuf, -(i + 1)) && did_help)
		    clrmsg(i);
		did_help = 0;
		comp_len = comp_used = 0;
		if (lastc == help_key)
		    goto no_completion;
		continue;
	    }
	    if (c == comp1_key || c == comp2_key || c == help_key) {
		if (!comp_used || c == comp2_key ||
		    (c == help_key && lastc != c)) {
		    lbuf[i] = NUL;
		    if ((comp_used = CALL(completion) (lbuf, i)) == 0) {
			ding();
			continue;
		    }
		    if (comp_used < 0) {
			comp_used = 0;
			goto no_completion;
		    }
		    comp_len = 0;
		}
		if (c == help_key) {
		    if (CALL(completion) ((char *) NULL, 1)) {
			gotoxy(prompt_length + i, prompt_line);
			fl;
			did_help = 1;
		    }
		    continue;
		}
		if (comp_len) {
		    i -= comp_len;
		    while (--comp_len >= 0)
			tputc(BS);
		    clrline();
		    comp_len = 0;
		}
		switch (CALL(completion) ((char *) NULL, 0)) {

		    case 0:	/* no possible completion */
			comp_used = 0;
			ding();
			continue;

		    case 2:	/* whole new alternative */
			while (--i >= 0)
			    tputc(BS);
			clrline();

			/* FALLTHRU */
		    case 1:	/* completion */
			comp_len = i;
			while ((c = lbuf[i])) {
			    if (i == max)
				break;
			    tputc(c);
			    i++;
			}
			fl;
			comp_len = i - comp_len;
			continue;
		}
	    }
	    if (comp_used) {
		if (!CALL(completion) (lbuf, -(i + 1)) && did_help)
		    clrmsg(i);
		did_help = 0;
		comp_len = comp_used = 0;
	    }
	}
no_completion:

	if (c == CR || c == NL) {
	    lbuf[i] = NUL;
	    break;
	}
	if (c == erase_key) {
	    if (i <= 0)
		continue;
	    i--;
	    tputc(BS);
	    tputc(' ');
	    tputc(BS);
	    fl;
	    continue;
	}
	if (c == delword_key) {
	    if (i <= 0)
		continue;
	    lbuf[i - 1] = 'X';
	    while (i > 0 && isalnum(lbuf[i - 1])) {
		tputc(BS);
		i--;
	    }
	    clrline();
	    continue;
	}
	if (c == kill_key) {
	    while (i > 0) {
		tputc(BS);
		i--;
	    }
	    clrline();
	    if (hit_count == 1 && dflt) {
		c = comp1_key;
		goto kill_prefill_hack;
	    }
	    continue;
	}
	if (c == K_interrupt) {
	    ret_val = NULL;
	    break;
	}
	if (data_bits == 8) {
	    if (!iso8859(c))
		continue;
	} else if (!isascii(c) || !isprint(c))
	    continue;

	if (i == max)
	    continue;

	if (i > 0 && lbuf[i - 1] == '/' && (c == '/' || c == '+')) {
	    if (c != '/' || !guard_double_slash || (i > 1 && lbuf[i - 2] == '/')) {
		if (completion == file_completion) {
		    while (i > 0) {
			tputc(BS);
			i--;
		    }
		    clrline();
		}
	    }
	}
	tputc(c);
	fl;

	lbuf[i] = c;
	i++;
    }
out:
    enable_stop = ostop;
    do_macro_processing = 1;
    return ret_val;
}

int             list_offset = 0;

int
list_completion(char *str)
{
    static int      cols, line;

    if (str == NULL) {
	cols = Columns;
	line = prompt_line + 1;
	if (line == Lines - 1)
	    cols--;

	gotoxy(0, line);
	clrpage();
	return 1;
    }
    str += list_offset;

    for (;;) {
	cols -= strlen(str);
	if (cols >= 0) {
	    tprintf("%s%s", str, cols > 0 ? " " : "");
	    cols--;
	    return 1;
	}
	if (line >= Lines - 1)
	    return 0;
	line++;
	cols = Columns;
	gotoxy(0, line);
	if (line == Lines - 1)
	    cols--;
    }
}

int
yes(int must_answer)
{
    int             c, help = 1, in_macro = 0;

    switch (m_yes()) {
	case 0:
	    break;
	case 1:
	    return 0;
	case 2:
	    return 1;
	case 3:
	    do_macro_processing = 0;
	    in_macro++;
	    break;
    }
    fl;

    for (;;) {
	if (!is_raw) {
	    nn_raw();
	    c = get_c();
	    unset_raw();
	} else
	    c = get_c();

	if (c == 'y' || c == 'Y') {
	    c = 1;
	    break;
	}
	if (must_answer == 0 && (c == SP || c == CR || c == NL)) {
	    c = 1;
	    break;
	}
	if (c == 'n' || c == 'N') {
	    c = 0;
	    break;
	}
	if (c == K_interrupt) {
	    c = -1;
	    break;
	}
	if (help) {
	    tprintf(" y=YES n=NO");
	    fl;
	    prompt_length += 11;
	    help = 0;
	}
    }

    if (in_macro) {
	if (c < 0)
	    m_break();
	do_macro_processing = 1;
    }
    return c;
}

void
ding(void)
{
    BATCH_CHECK_V;

    putp(bell_str);
    fl;
}


void
display_file(char *name, int modes)
{
    FILE           *f;
    register int    c, stand_on;
    int             linecnt, headln_cnt, hdline, no_conf;
    char            headline[128];

    headline[0] = 0;
    hdline = 0;
    no_conf = 0;

    headln_cnt = -1;

    if (modes & CLEAR_DISPLAY) {
	gotoxy(0, 0);
	clrdisp();
    }
    linecnt = Lines - 1;

chain:

    if (*name != '/')
	name = relative(help_directory, name);
    f = open_file(name, OPEN_READ);
    if (f == NULL)
	tprintf("\r\n\nFile %s is not available\n\n", name);
    else {
	stand_on = 0;

	while ((c = getc(f)) != EOF) {

#ifdef HAVE_JOBCONTROL
	    if (s_redraw) {
		no_conf = 1;
		break;
	    }
#endif				/* HAVE_JOBCONTROL */

	    no_conf = 0;
	    if (c == '\1') {
		if (STANDOUT) {
		    putp(stand_on ? exit_standout_mode : enter_standout_mode);
		    curxy_c += cookie_size;
		    stand_on = !stand_on;
		}
		continue;
	    }
	    if (c == '\2') {
		headln_cnt = 0;
		continue;
	    }
	    if (c == '\3') {
		headln_cnt = 0;
		while ((c = getc(f)) != EOF && c != NL)
		    headline[headln_cnt++] = c;
		headline[headln_cnt++] = NUL;
		name = headline;
		fclose(f);
		goto chain;
	    }
	    if (c == '\4') {
		tprintf("%s", version_id);
		continue;
	    }
	    if (headln_cnt >= 0)
		headline[headln_cnt++] = c;

	    if (hdline) {
		tprintf("%s\r", headline);
		hdline = 0;
		linecnt--;
	    }
	    tputc(c);
	    if (c == NL) {
		tputc(CR);
		if (headln_cnt >= 0) {
		    headline[--headln_cnt] = 0;
		    headln_cnt = -1;
		}
		if (--linecnt == 0) {
		    no_conf = 1;
		    if (any_key(0) == K_interrupt)
			break;
		    linecnt = Lines - 1;
		    if (modes & CLEAR_DISPLAY) {
			gotoxy(0, 0);
			clrdisp();
		    }
		    hdline = headline[0];
		}
	    }
	}

	if (stand_on) {
	    putp(exit_standout_mode);
	    curxy_c += cookie_size;
	}
	fclose(f);
    }

    prompt_line = Lines - 1;	/* move prompt to last line */

    if (!no_conf && (modes & CONFIRMATION))
	any_key(prompt_line);
}


void
nn_exitmsg(int n, char *fmt,...)
{
    va_list         ap;

    if (terminal_speed != 0) {
	clrdisp();
	visual_off();
    }
    va_start(ap, fmt);
    vprintf(fmt, ap);
    putchar(NL);
    va_end(ap);

    nn_exit(n);
    /* NOTREACHED */
}

static void
push_msg(char *str)
{
    register struct msg_list *mp, *newmsg;
    static int      slots = 0;

    if (str != NULL) {
	if (slots > message_history) {
	    for (mp = newmsg = msg_stack; mp->prev != NULL; mp = mp->prev)
		newmsg = mp;
	    if (newmsg == mp)
		msg_stack = NULL;
	    else {
		newmsg->prev = NULL;
		newmsg = mp;
	    }
	    freeobj(newmsg->buf);
	} else {
	    slots++;
	    newmsg = newobj(struct msg_list, 1);
	}
	newmsg->buf = copy_str(str);
	newmsg->prev = msg_stack;
	msg_stack = newmsg;
    }
    msg_ptr = msg_stack;
}

void
vmsg(char *fmt, va_list ap)
{
    char           *errmsg;

    if (fmt) {
	char            lbuf[512];

	vsprintf(lbuf, fmt, ap);
	push_msg(lbuf);
    }
    if (msg_ptr) {
	errmsg = msg_ptr->buf;
	msg_ptr = msg_ptr->prev;
    } else {
	errmsg = "(no more messages)";
	msg_ptr = msg_stack;
    }

    if (terminal_speed == 0) {
	tprintf("%s\n", errmsg);
	fl;
	return;
    }
    gotoxy(0, Lines - 1);
    tprintf("%s", errmsg);
    clrline();
    any_message = 1;

    if (prompt_line != Lines - 1)
	gotoxy(prompt_length, prompt_line);
    fl;
}

void
msg(char *fmt,...)
{
    va_list         ap;

    va_start(ap, fmt);
    vmsg(fmt, ap);
    va_end(ap);
}

void
clrmsg(int col)
{
    BATCH_CHECK_V;

    gotoxy(0, prompt_line + 1);
    clrpage();
    if (col >= 0)
	gotoxy(prompt_length + col, prompt_line);
    fl;
    any_message = 0;
}


void
prompt(char *fmt,...)
{
    va_list         ap;
    register char  *cp;
    int             stand_on;
    static char     cur_p[FILENAME];
    static char     saved_p[FILENAME];

    BATCH_CHECK_V;

    va_start(ap, fmt);

    if (fmt == P_VERSION) {
	gotoxy(0, prompt_line + 1);
	tprintf("Release %s ", version_id);
	clrline();
	any_message++;

	if (prompt_line >= 0)
	    gotoxy(prompt_length, prompt_line);
	goto out;
    }
    if (fmt == P_SAVE) {
	strcpy(saved_p, cur_p);
	goto out;
    }
    if (fmt == P_RESTORE)
	strcpy(cur_p, saved_p);

    if (prompt_line >= 0)
	gotoxy(0, prompt_line);

    if (fmt == P_MOVE) {
	clrline();
	goto out;
    }
    if (fmt != P_REDRAW && fmt != P_RESTORE)
	vsprintf(cur_p, fmt, ap);

    tputc(CR);

    for (cp = cur_p, stand_on = 0, prompt_length = 0; *cp; cp++) {
	if (*cp == '\1') {
	    if (cp[1] != '\1') {
		if (STANDOUT) {
		    stand_on = !stand_on;
		    nnstandout(stand_on);
		    prompt_length += cookie_size;
		}
		continue;
	    }
	    cp++;
	} else if (*cp == '\2') {
	    time_t          t;
	    char           *timestr;

#ifdef STATISTICS
	    t = tick_usage();
#else
	    t = cur_time();
#endif

	    if (show_current_time) {
		timestr = ctime(&t) + 11;
		timestr[5] = NUL;

		tprintf("-- %s ", timestr);
		prompt_length += 9;
	    }
	    if (unread_mail(t)) {
		tprintf("Mail ");
		prompt_length += 5;
	    }
	    continue;
	}
	tputc(*cp);
	prompt_length++;
    }
    if (stand_on) {
	nnstandout(0);
	prompt_length += cookie_size;
    }
    clrline();

    if (fmt == P_RESTORE)
	restore_xy();

#if notdef
    else
	curxy_c = -1;
#endif

out:
    va_end(ap);
}


int
any_key(int line)
{
    int             was_raw, c, dmp;

    BATCH_CHECK;

    was_raw = is_raw;
    if (!is_raw)
	nn_raw();
    if (line == 0)
	line = -1;
    else if (line < 0)
	line = Lines + line;

    if (line != 10000)
	so_printxy(0, line, "Hit any key to continue");

    clrline();

    dmp = do_macro_processing;
    do_macro_processing = 0;
    c = get_c();
    if (c == 'q' || c == 'Q')
	c = K_interrupt;
    do_macro_processing = dmp;

    if (!was_raw)
	unset_raw();

    return c;
}


static int      pg_fline, pg_width, pg_maxw, pg_line, pg_col, pg_quit;
regexp         *pg_regexp = NULL;
int             pg_new_regexp = 0;

void
pg_init(int first_line, int cols)
{
    if (pg_regexp) {
	freeobj(pg_regexp);
	pg_regexp = NULL;
    }
    pg_new_regexp = 0;

    pg_fline = first_line;
    pg_line = pg_fline - 1;
    pg_quit = pg_col = 0;
    pg_width = Columns / cols;
    pg_maxw = pg_width * (cols - 1);
}

int
pg_scroll(int n)
{
    pg_line += n;
    if (pg_line >= (Lines - 1)) {
	pg_line = 0;
	if (any_key(0) == K_interrupt)
	    return 1;
	tputc(CR);
	clrline();
    }
    return 0;
}

int
pg_next(void)
{
    int             c;

    if (batch_mode) {
	putchar(NL);
	return 0;
    }
    pg_line++;
    if (pg_line < Lines) {
	gotoxy(pg_col, pg_line);
	if (pg_line == Lines - 1 && pg_col == pg_maxw) {
	    c = any_key(0);
	    if (c == '/') {
		char           *expr;
		tputc(CR);
		tputc('/');
		clrline();
		expr = get_s((char *) NULL, (char *) NULL, (char *) NULL, NULL_FCT);
		if (expr != NULL && *expr != NUL) {
		    freeobj(pg_regexp);
		    pg_regexp = regcomp(expr);
		    pg_new_regexp = 1;
		}
	    }
	    gotoxy(0, pg_fline);
	    clrpage();
	    pg_col = 0;
	    pg_line = pg_fline;
	    if (c == K_interrupt) {
		pg_quit = 1;
		return -1;
	    }
	    return 1;
	}
    } else {
	pg_line = pg_fline;
	pg_col += pg_width;
	gotoxy(pg_col, pg_line);
    }
    return 0;
}

void
pg_indent(int pos)
{
    BATCH_CHECK_V;

    gotoxy(pg_col + pos, pg_line);
}

int
pg_end(void)
{
    int             c;

    if (pg_quit == 0 && pg_next() == 0)
	c = any_key(0);
    else
	c = K_interrupt;

    if (pg_regexp) {
	freeobj(pg_regexp);
	pg_regexp = NULL;
    }
    return c == K_interrupt ? -1 : 0;
}

void
user_delay(int ticks)
{
    BATCH_CHECK_V;

    if (ticks <= 0 || conf_dont_sleep) {
	tprintf(" <>");
	any_key(10000);
    } else {
	fl;
	sleep((unsigned) ticks);
    }
}
