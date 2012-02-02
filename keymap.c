/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Keyboard (re)mapping
 */

#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "init.h"
#include "keymap.h"
#include "nn_term.h"

extern int      data_bits;

/*
 * KEY MAP LAYOUT:
 *	128 normal ASCII chars
 *	0200 -- unused
 *	4 arrow keys #up/down/left/right
 *	10 multi keys #0-#9
 *	17 spare keys
 *	95 national 8-bit characters (8859/x)
 *	0377 is unused (since we must be able to test c<KEY_MAP_SIZE)
 *
 *	The encoding of the keymap arrays are performed in
 *	keymap.c (initialization + key_name/parse_key)
 */

/* in keymap.h, MULTI_KEYS include ARROW_KEYS for term.c */
#undef MULTI_KEYS

#define NORMAL_KEYS	129	/* include 0200 for convenience */
#define ARROW_KEYS	4
#define MULTI_KEYS	16
#define SPARE_KEYS	11
#define NATIONAL_KEYS	96
#define KEY_MAP_SIZE	255

/*
 * standard keyboard mapping for more()
 *
 *	redraw			^L, ^R
 *	continue		space
 *	repeat message		^P
 *	help			?
 *	shell escape		!
 *	version			V
 *	extended command	:
 *	quit			Q
 *
 *	save			S, O
 *	save, no header		W
 *	reply			R
 *	follow up		F
 *	mail (forward)		M
 *	cancel			C
 *	unsubscribe		U
 *	group overview		Y
 *	print article		P
 *	kill handling		K
 *
 *	update, goto next group	X
 *	no update, next group	q, Z
 *	return to menu		=
 *	prev article		p
 *	goto group		G
 *
 *	line forward		CR/NL
 *	half page forward	d/^D
 *	half page back		u/^U
 *	full page back		BS, DEL, (up arrow)
 *	goto line		g
 *	goto match		/
 *	next match		.
 *
 *	select subject		N, *
 *
 *	header			h
 *	digest header		H
 *	top			t
 *	last page		$
 *	leave article		l
 *	leave article to next	L
 *	next article		n
 *	kill subject		k
 *
 *	rot13			D
 *	compress		c
 */

int             more_key_map[KEY_MAP_SIZE] = {

     /* NUL ^@ */ K_UNBOUND,
     /* SOH ^A */ K_UNBOUND,
     /* STX ^B */ K_UNBOUND,
     /* ETX ^C */ K_UNBOUND,
     /* EOT ^D */ K_NEXT_HALF_PAGE,
     /* ENQ ^E */ K_UNBOUND,
     /* ACK ^F */ K_UNBOUND,
     /* BEL ^G */ K_INVALID,
     /* BS  ^H */ K_PREV_PAGE,
     /* TAB ^I */ K_SKIP_LINES,
     /* NL  ^J */ K_NEXT_LINE,
     /* VT  ^K */ K_UNBOUND,
     /* FF  ^L */ K_REDRAW,
     /* CR  ^M */ K_NEXT_LINE,
     /* SO  ^N */ K_UNBOUND,
     /* SI  ^O */ K_UNBOUND,
     /* DLE ^P */ K_LAST_MESSAGE,
     /* DC1 ^Q */ K_UNBOUND,
     /* DC2 ^R */ K_REDRAW,
     /* DC3 ^S */ K_UNBOUND,
     /* DC4 ^T */ K_UNBOUND,
     /* NAK ^U */ K_PREV_HALF_PAGE,
     /* SYN ^V */ K_NEXT_PAGE,
     /* ETB ^W */ K_UNBOUND,
     /* CAN ^X */ K_UNBOUND,
     /* EM  ^Y */ K_UNBOUND,
     /* SUB ^Z */ K_UNBOUND,
     /* ESC ^[ */ K_UNBOUND,
     /* FS  ^\ */ K_UNBOUND,
     /* GS  ^] */ K_UNBOUND,
     /* RS  ^^ */ K_UNBOUND,
     /* US  ^_ */ K_UNBOUND,
     /* SP  */ K_CONTINUE,
     /* !   */ K_SHELL,
     /* "   */ K_UNBOUND,
     /* #   */ K_UNBOUND,
     /* $   */ K_LAST_PAGE,
     /* %   */ K_PREVIEW,
     /* &   */ K_UNBOUND,
     /* '   */ K_UNBOUND,
     /* (   */ K_UNBOUND,
     /* )   */ K_UNBOUND,
     /* *   */ K_SELECT_SUBJECT,
     /* +   */ K_UNBOUND,
     /* ,   */ K_UNBOUND,
     /* -   */ K_UNBOUND,
     /* .   */ K_NEXT_MATCH,
     /* /   */ K_GOTO_MATCH,
     /* 0   */ K_UNBOUND,
     /* 1   */ K_UNBOUND,
     /* 2   */ K_UNBOUND,
     /* 3   */ K_UNBOUND,
     /* 4   */ K_UNBOUND,
     /* 5   */ K_UNBOUND,
     /* 6   */ K_UNBOUND,
     /* 7   */ K_UNBOUND,
     /* 8   */ K_UNBOUND,
     /* 9   */ K_UNBOUND,
     /* :   */ K_EXTENDED_CMD,
     /* ;   */ K_UNBOUND,
     /* <   */ K_UNBOUND,
     /* =   */ K_BACK_TO_MENU,
     /* >   */ K_UNBOUND,
     /* ?   */ K_HELP,
     /* @   */ K_UNBOUND,
     /* A   */ K_UNBOUND,
     /* B   */ K_UNBOUND,
     /* C   */ K_CANCEL,
     /* D   */ K_ROT13,
     /* E   */ K_SAVE_HEADER_ONLY,
     /* F   */ K_FOLLOW_UP,
     /* G   */ K_GOTO_GROUP,
     /* H   */ K_FULL_DIGEST,
     /* I   */ K_UNBOUND,
     /* J   */ K_UNBOUND,
     /* K   */ K_KILL_HANDLING,
     /* L   */ K_LEAVE_NEXT,
     /* M   */ K_MAIL_OR_FORWARD,
     /* N   */ K_NEXT_GROUP_NO_UPDATE,
     /* O   */ K_SAVE_SHORT_HEADER,
     /* P   */ K_PRINT,
     /* Q   */ K_QUIT,
     /* R   */ K_REPLY,
     /* S   */ K_SAVE_FULL_HEADER,
     /* T   */ K_UNBOUND,
     /* U   */ K_UNSUBSCRIBE,
     /* V   */ K_VERSION,
     /* W   */ K_SAVE_NO_HEADER,
     /* X   */ K_READ_GROUP_UPDATE,
     /* Y   */ K_GROUP_OVERVIEW,
     /* Z   */ K_BACK_TO_MENU,
     /* [   */ K_UNBOUND,
     /* \   */ K_UNBOUND,
     /* ]   */ K_UNBOUND,
     /* ^   */ K_FIRST_PAGE,
     /* _   */ K_UNBOUND,
     /* `   */ K_UNBOUND,
     /* a   */ K_FORW_ARTICLE,
     /* b   */ K_BACK_ARTICLE,
     /* c   */ K_COMPRESS,
     /* d   */ K_NEXT_HALF_PAGE,
     /* e   */ K_SAVE_HEADER_ONLY,
     /* f   */ K_FOLLOW_UP,
     /* g   */ K_GOTO_LINE,
     /* h   */ K_HEADER_PAGE,
     /* i   */ K_UNBOUND,
     /* j   */ K_UNBOUND,
     /* k   */ K_NEXT_SUBJECT,
     /* l   */ K_LEAVE_ARTICLE,
     /* m   */ K_MAIL_OR_FORWARD,
     /* n   */ K_NEXT_ARTICLE,
     /* o   */ K_SAVE_SHORT_HEADER,
     /* p   */ K_PREVIOUS /* article */ ,
     /* q   */ K_NEXT_GROUP_NO_UPDATE,
     /* r   */ K_REPLY,
     /* s   */ K_SAVE_FULL_HEADER,
     /* t   */ K_FIRST_PAGE,
     /* u   */ K_PREV_HALF_PAGE,
     /* v   */ K_UNBOUND,
     /* w   */ K_SAVE_NO_HEADER,
     /* x   */ K_UNBOUND,
     /* y   */ K_UNBOUND,
     /* z   */ K_UNBOUND,
     /* {   */ K_UNBOUND,
     /* |   */ K_UNBOUND,
     /* }   */ K_UNBOUND,
     /* ~   */ K_UNBOUND,
     /* DEL */ K_PREV_PAGE,
     /* 200 */ K_UNBOUND,
     /* up  */ K_PREV_PAGE,
     /* down */ K_NEXT_PAGE,
     /* left */ K_UNBOUND,
     /* right */ K_UNBOUND,
     /* #0  */ K_UNBOUND,
     /* #1  */ K_UNBOUND,
     /* #2  */ K_UNBOUND,
     /* #3  */ K_UNBOUND,
     /* #4  */ K_UNBOUND,
     /* #5  */ K_UNBOUND,
     /* #6  */ K_UNBOUND,
     /* #7  */ K_UNBOUND,
     /* #8  */ K_UNBOUND,
     /* #9  */ K_UNBOUND,
     /* mouse d1 */ K_M_CONTINUE,
     /* mouse d2 */ K_NEXT_SUBJECT,
     /* mouse d3 */ K_BACK_TO_MENU,
     /* mouse u1 */ K_INVALID,
     /* mouse u2 */ K_INVALID,
     /* mouse u3 */ K_INVALID
};



/*
 * standard keyboard mappings for menu()
 *
 *	illegal command
 *	redraw			^L, ^R
 *	continue		space
 *	continue no mark	return, newline
 *	repeat message		^P
 *	help			?
 *	shell escape		!
 *	version			V
 *	alternative commands	:
 *	quit			Q
 *
 *	save			S, O
 *	save, no header		W
 *	reply			R
 *	follow up		F
 *	mail (forward)		M
 *	cancel			C
 *	unsubscribe		U
 *	group overview		Y
 *	kill handling		K
 *	junk articles		J
 *
 *	read, then next		X
 *	read, then same		Z
 *	no update, next		N
 *	prev group		P
 *	goto group		G
 *	advance group		A
 *	back group		B
 *
 *	article identifier	a-z 0-9
 *	inverse			@
 *	select current, next	.
 *	next			, (down arrow)
 *	prev			/ (up arrow)
 *	select subject		*
 *	range			-
 *	auto select 		+
 *
 *	next page		>
 *	prev page		<
 *	first page		^
 *	last page		$
 *
 *	pre-view article	%
 *
 *	layout			L
 */


int             menu_key_map[KEY_MAP_SIZE] = {

     /* NUL ^@ */ K_UNBOUND,
     /* SOH ^A */ K_UNBOUND,
     /* STX ^B */ K_UNBOUND,
     /* ETX ^C */ K_UNBOUND,
     /* EOT ^D */ K_UNBOUND,
     /* ENQ ^E */ K_UNBOUND,
     /* ACK ^F */ K_UNBOUND,
     /* BEL ^G */ K_INVALID,
     /* BS  ^H */ K_PREV_LINE,
     /* TAB ^I */ K_UNBOUND,
     /* NL  ^J */ K_CONTINUE_NO_MARK,
     /* VT  ^K */ K_UNBOUND,
     /* FF  ^L */ K_REDRAW,
     /* CR  ^M */ K_CONTINUE_NO_MARK,
     /* SO  ^N */ K_UNBOUND,
     /* SI  ^O */ K_UNBOUND,
     /* DLE ^P */ K_LAST_MESSAGE,
     /* DC1 ^Q */ K_UNBOUND,
     /* DC2 ^R */ K_REDRAW,
     /* DC3 ^S */ K_UNBOUND,
     /* DC4 ^T */ K_UNBOUND,
     /* NAK ^U */ K_UNBOUND,
     /* SYN ^V */ K_NEXT_PAGE,
     /* ETB ^W */ K_UNBOUND,
     /* CAN ^X */ K_UNBOUND,
     /* EM  ^Y */ K_UNBOUND,
     /* SUB ^Z */ K_UNBOUND,
     /* ESC ^[ */ K_UNBOUND,
     /* FS  ^\ */ K_UNBOUND,
     /* GS  ^] */ K_UNBOUND,
     /* RS  ^^ */ K_UNBOUND,
     /* US  ^_ */ K_UNBOUND,
     /* SP  */ K_CONTINUE,
     /* !   */ K_SHELL,
     /* "   */ K_LAYOUT,
     /* #   */ K_UNBOUND,
     /* $   */ K_LAST_PAGE,
     /* %   */ K_PREVIEW,
     /* &   */ K_UNBOUND,
     /* '   */ K_UNBOUND,
     /* (   */ K_OPEN_SUBJECT,
     /* )   */ K_CLOSE_SUBJECT,
     /* *   */ K_SELECT_SUBJECT,
     /* +   */ K_AUTO_SELECT,
     /* ,   */ K_NEXT_LINE,
     /* -   */ K_SELECT_RANGE,
     /* .   */ K_SELECT,
     /* /   */ K_PREV_LINE,
     /* 0   */ K_ARTICLE_ID + 26,
     /* 1   */ K_ARTICLE_ID + 27,
     /* 2   */ K_ARTICLE_ID + 28,
     /* 3   */ K_ARTICLE_ID + 29,
     /* 4   */ K_ARTICLE_ID + 30,
     /* 5   */ K_ARTICLE_ID + 31,
     /* 6   */ K_ARTICLE_ID + 32,
     /* 7   */ K_ARTICLE_ID + 33,
     /* 8   */ K_ARTICLE_ID + 34,
     /* 9   */ K_ARTICLE_ID + 35,
     /* :   */ K_EXTENDED_CMD,
     /* ;   */ K_UNBOUND,
     /* <   */ K_PREV_PAGE,
     /* =   */ K_GOTO_MATCH,
     /* >   */ K_NEXT_PAGE,
     /* ?   */ K_HELP,
     /* @   */ K_SELECT_INVERT,
     /* A   */ K_ADVANCE_GROUP,
     /* B   */ K_BACK_GROUP,
     /* C   */ K_CANCEL,
     /* D   */ K_UNBOUND,
     /* E   */ K_SAVE_HEADER_ONLY,
     /* F   */ K_FOLLOW_UP,
     /* G   */ K_GOTO_GROUP,
     /* H   */ K_UNBOUND,
     /* I   */ K_UNBOUND,
     /* J   */ K_JUNK_ARTICLES,
     /* K   */ K_KILL_HANDLING,
     /* L   */ K_LEAVE_NEXT,
     /* M   */ K_MAIL_OR_FORWARD,
     /* N   */ K_NEXT_GROUP_NO_UPDATE,
     /* O   */ K_SAVE_SHORT_HEADER,
     /* P   */ K_PREVIOUS /* group */ ,
     /* Q   */ K_QUIT,
     /* R   */ K_REPLY,
     /* S   */ K_SAVE_FULL_HEADER,
     /* T   */ K_UNBOUND,
     /* U   */ K_UNSUBSCRIBE,
     /* V   */ K_VERSION,
     /* W   */ K_SAVE_NO_HEADER,
     /* X   */ K_READ_GROUP_UPDATE,
     /* Y   */ K_GROUP_OVERVIEW,
     /* Z   */ K_READ_GROUP_THEN_SAME,
     /* [   */ K_UNBOUND,
     /* \   */ K_UNBOUND,
     /* ]   */ K_UNBOUND,
     /* ^   */ K_FIRST_PAGE,
     /* _   */ K_UNBOUND,
     /* `   */ K_UNBOUND,
     /* a   */ K_ARTICLE_ID + 0,
     /* b   */ K_ARTICLE_ID + 1,
     /* c   */ K_ARTICLE_ID + 2,
     /* d   */ K_ARTICLE_ID + 3,
     /* e   */ K_ARTICLE_ID + 4,
     /* f   */ K_ARTICLE_ID + 5,
     /* g   */ K_ARTICLE_ID + 6,
     /* h   */ K_ARTICLE_ID + 7,
     /* i   */ K_ARTICLE_ID + 8,
     /* j   */ K_ARTICLE_ID + 9,
     /* k   */ K_ARTICLE_ID + 10,
     /* l   */ K_ARTICLE_ID + 11,
     /* m   */ K_ARTICLE_ID + 12,
     /* n   */ K_ARTICLE_ID + 13,
     /* o   */ K_ARTICLE_ID + 14,
     /* p   */ K_ARTICLE_ID + 15,
     /* q   */ K_ARTICLE_ID + 16,
     /* r   */ K_ARTICLE_ID + 17,
     /* s   */ K_ARTICLE_ID + 18,
     /* t   */ K_ARTICLE_ID + 19,
     /* u   */ K_ARTICLE_ID + 20,
     /* v   */ K_ARTICLE_ID + 21,
     /* w   */ K_ARTICLE_ID + 22,
     /* x   */ K_ARTICLE_ID + 23,
     /* y   */ K_ARTICLE_ID + 24,
     /* z   */ K_ARTICLE_ID + 25,
     /* {   */ K_UNBOUND,
     /* |   */ K_UNBOUND,
     /* }   */ K_UNBOUND,
     /* ~   */ K_UNSELECT_ALL,
     /* DEL */ K_PREV_LINE,
     /* 200 */ K_UNBOUND,
     /* up  */ K_PREV_LINE,
     /* down */ K_NEXT_LINE,
     /* left */ K_UNBOUND,
     /* right */ K_UNBOUND,
     /* #0  */ K_UNBOUND,
     /* #1  */ K_UNBOUND,
     /* #2  */ K_UNBOUND,
     /* #3  */ K_UNBOUND,
     /* #4  */ K_UNBOUND,
     /* #5  */ K_UNBOUND,
     /* #6  */ K_UNBOUND,
     /* #7  */ K_UNBOUND,
     /* #8  */ K_UNBOUND,
     /* #9  */ K_UNBOUND,
     /* mouse d1 */ K_M_SELECT,
     /* mouse d2 */ K_M_PREVIEW,
     /* mouse d3 */ K_NEXT_GROUP_NO_UPDATE,
     /* mouse u1 */ K_M_SELECT_RANGE,
     /* mouse u2 */ K_INVALID,
     /* mouse u3 */ K_INVALID
};

int             orig_menu_map[KEY_MAP_SIZE];	/* initially empty */


static struct command_name_map {
    char           *cmd_name;
    int             cmd_code;
    int             cmd_restriction;
}               command_name_map[] = {

    "advance-article", K_FORW_ARTICLE, K_ONLY_MORE,
    "advance-group", K_ADVANCE_GROUP, 0,
    "article", K_ARTICLE_ID, K_ONLY_MENU,
    "as", K_EQUAL_KEY, 0,

    "back-article", K_BACK_ARTICLE, K_ONLY_MORE,
    "back-group", K_BACK_GROUP, 0,

    "cancel", K_CANCEL, 0,
    "close-subject", K_CLOSE_SUBJECT, K_ONLY_MENU,
    "command", K_EXTENDED_CMD, 0,
    "compress", K_COMPRESS, K_ONLY_MORE,
    "continue", K_CONTINUE, 0,
    "continue-no-mark", K_CONTINUE_NO_MARK, K_ONLY_MENU,

    "decode", K_UUDECODE, 0,

    "find", K_GOTO_MATCH, 0,
    "find-next", K_NEXT_MATCH, K_ONLY_MORE,
    "follow", K_FOLLOW_UP, 0,
    "full-digest", K_FULL_DIGEST, K_ONLY_MORE,

    "goto-group", K_GOTO_GROUP, 0,
    "goto-menu", K_BACK_TO_MENU, K_ONLY_MORE,

    "help", K_HELP, 0,

    "junk-articles", K_JUNK_ARTICLES, K_ONLY_MENU,

    "kill-select", K_KILL_HANDLING, 0,

    "layout", K_LAYOUT, K_ONLY_MENU,
    "leave-article", K_LEAVE_ARTICLE, K_ONLY_MORE,
    "leave-next", K_LEAVE_NEXT, K_ONLY_MORE,
    "line+1", K_NEXT_LINE, 0,
    "line-1", K_PREV_LINE, K_ONLY_MENU,
    "line=@", K_GOTO_LINE, K_ONLY_MORE,

    "macro", K_MACRO, 0,
    "mail", K_MAIL_OR_FORWARD, 0,
    "message", K_LAST_MESSAGE, 0,
    "mouse-continue", K_M_CONTINUE, K_ONLY_MORE,
    "mouse-preview", K_M_PREVIEW, K_ONLY_MENU,
    "mouse-select", K_M_SELECT, K_ONLY_MENU,
    "mouse-select-range", K_M_SELECT_RANGE, K_ONLY_MENU,
    "mouse-select-subject", K_M_SELECT_SUBJECT, K_ONLY_MENU,
    "mouse-toggle", K_M_TOGGLE, 0,

    "next-article", K_NEXT_ARTICLE, K_ONLY_MORE,
    "next-group", K_NEXT_GROUP_NO_UPDATE, 0,
    "next-subject", K_NEXT_SUBJECT, K_ONLY_MORE,
    "nil", K_UNBOUND, 0,

    "open-subject", K_OPEN_SUBJECT, K_ONLY_MENU,
    "overview", K_GROUP_OVERVIEW, 0,

    "page+1", K_NEXT_PAGE, 0,
    "page+1/2", K_NEXT_HALF_PAGE, K_ONLY_MORE,
    "page-1", K_PREV_PAGE, 0,
    "page-1/2", K_PREV_HALF_PAGE, K_ONLY_MORE,
    "page=$", K_LAST_PAGE, 0,
    "page=0", K_HEADER_PAGE, 0,
    "page=1", K_FIRST_PAGE, 0,
    "page=@", K_GOTO_PAGE, K_ONLY_MORE,

    "patch", K_PATCH, 0,
    "post", K_POST, 0,
    "prefix", K_PREFIX_KEY, 0,
    "preview", K_PREVIEW, 0,
    "previous", K_PREVIOUS, 0,
    "print", K_PRINT, 0,

    "quit", K_QUIT, 0,

    "read-return", K_READ_GROUP_THEN_SAME, K_ONLY_MENU,
    "read-skip", K_READ_GROUP_UPDATE, 0,
    "redraw", K_REDRAW, 0,
    "reply", K_REPLY, 0,
    "rot13", K_ROT13, K_ONLY_MORE,

    "save-body", K_SAVE_NO_HEADER, 0,
    "save-full", K_SAVE_FULL_HEADER, 0,
    "save-short", K_SAVE_SHORT_HEADER, 0,
    "save-hdr-only", K_SAVE_HEADER_ONLY, 0,
    "select", K_SELECT, K_ONLY_MENU,
    "select-auto", K_AUTO_SELECT, K_ONLY_MENU,
    "select-invert", K_SELECT_INVERT, K_ONLY_MENU,
    "select-range", K_SELECT_RANGE, K_ONLY_MENU,
    "select-subject", K_SELECT_SUBJECT, 0,
    "shell", K_SHELL, 0,
    "skip-lines", K_SKIP_LINES, 0,

    "unselect-all", K_UNSELECT_ALL, K_ONLY_MENU,
    "unshar", K_UNSHAR, 0,
    "unsub", K_UNSUBSCRIBE, 0,

    "version", K_VERSION, 0,

    (char *) NULL, 0, 0
};

static int      name_map_size;
#define max_cmd_name_length 16	/* recalculate if table is changed */

key_type        global_key_map[KEY_MAP_SIZE];


void
init_key_map(void)
{
    register int    c;
    register struct command_name_map *cnmp;

    for (c = 0; c < KEY_MAP_SIZE; c++)
	global_key_map[c] = c;
    for (c = NORMAL_KEYS + ARROW_KEYS + MULTI_KEYS; c < KEY_MAP_SIZE; c++) {
	menu_key_map[c] = K_UNBOUND;
	more_key_map[c] = K_UNBOUND;
    }

    for (cnmp = command_name_map; cnmp->cmd_name; cnmp++);
    name_map_size = cnmp - command_name_map;
}


int
lookup_command(char *command, int restriction)
{
    register struct command_name_map *cnmp;
    register int    i, j, k, t;

    i = 0;
    j = name_map_size - 1;

    while (i <= j) {
	k = (i + j) / 2;
	cnmp = &command_name_map[k];

	if ((t = strcmp(command, cnmp->cmd_name)) > 0)
	    i = k + 1;
	else if (t < 0)
	    j = k - 1;
	else {
	    switch (restriction) {
		case K_ONLY_MENU:
		case K_ONLY_MORE:
		    if (cnmp->cmd_restriction == restriction)
			break;
		    /* FALLTHRU */
		case 0:
		    if (cnmp->cmd_restriction == 0)
			break;
		    return K_INVALID - 1;
		default:
		    break;
	    }
	    return cnmp->cmd_code;
	}
    }

    return K_INVALID;
}


int
cmd_completion(char *path, int index)
{
    static char    *head, *tail = NULL;
    static int      len;
    static struct command_name_map *cmd, *help_cmd;

    if (index < 0)
	return 0;

    if (path) {
	head = path;
	tail = path + index;
	while (*head && isspace(*head))
	    head++;
	help_cmd = cmd = command_name_map;
	len = tail - head;

	return 1;
    }
    if (index) {
	list_completion((char *) NULL);

	if (help_cmd->cmd_name == NULL)
	    help_cmd = command_name_map;

	for (; help_cmd->cmd_name; help_cmd++) {
	    index = strncmp(help_cmd->cmd_name, head, len);
	    if (index < 0)
		continue;
	    if (index > 0) {
		help_cmd = command_name_map;
		break;
	    }
	    if (list_completion(help_cmd->cmd_name) == 0)
		break;
	}
	fl;
	return 1;
    }
    for (; cmd->cmd_name; cmd++) {
	if (len == 0)
	    index = 0;
	else
	    index = strncmp(cmd->cmd_name, head, len);
	if (index < 0)
	    continue;
	if (index > 0)
	    break;
	if (cmd->cmd_code == K_MACRO ||
	    cmd->cmd_code == K_ARTICLE_ID ||
	    cmd->cmd_code == K_PREFIX_KEY ||
	    cmd->cmd_code == K_EQUAL_KEY)
	    sprintf(tail, "%s ", cmd->cmd_name + len);
	else
	    strcpy(tail, cmd->cmd_name + len);
	cmd++;
	return 1;
    }
    return 0;
}


char           *
command_name(int cmd)
{
    register struct command_name_map *cnmp;

    cmd &= ~GETC_COMMAND;

    for (cnmp = command_name_map; cnmp->cmd_name; cnmp++)
	if (cnmp->cmd_code == cmd)
	    return cnmp->cmd_name;

    return "unknown";
}


/*
 * convert key name into ascii code
 *
 *	key names are:
 *		c	character c
 *		^C	control-C
 *		0xNN	hex value (0..0x7f)
 *		0NNN	octal value (0..0177)
 *		NNN	decimal value (0..127)
 *		up, down, left, rigth	arrow keys
 *		#0..#9			function keys (initially undefined)
 */

key_type
parse_key(char *str)
{
    int             x;

    if (str[1] == NUL)
	return (data_bits < 8) ? (str[0] & 0177) : str[0];

    if (str[0] == '^') {
	if (str[1] == '?')
	    return 0177;
	else
	    return CONTROL_(str[1]);
    }

    if (isdigit(str[0])) {
	if (str[0] == '0') {
	    if (str[1] == 'x')
		sscanf(str + 2, "%d", &x);
	    else
		sscanf(str + 1, "%d", &x);
	} else {
	    sscanf(str, "%d", &x);
	}

	return x;
    }
    if (str[0] == '#' && isdigit(str[1]))
	return K_function(str[1] - '0');

    if (str[0] == '#')
	str++;

    if (strcmp(str, "up") == 0)
	return K_up_arrow;

    if (strcmp(str, "down") == 0)
	return K_down_arrow;

    if (strcmp(str, "left") == 0)
	return K_left_arrow;

    if (strcmp(str, "right") == 0)
	return K_right_arrow;

    if (strcmp(str, "mouse_d1") == 0)	/* thp */
	return K_m_d1;
    if (strcmp(str, "mouse_d2") == 0)	/* thp */
	return K_m_d2;
    if (strcmp(str, "mouse_d3") == 0)	/* thp */
	return K_m_d3;
    if (strcmp(str, "mouse_u1") == 0)	/* thp */
	return K_m_u1;
    if (strcmp(str, "mouse_u2") == 0)	/* thp */
	return K_m_u2;
    if (strcmp(str, "mouse_u3") == 0)	/* thp */
	return K_m_u3;

    init_message("unknown key: %s", str);

    return 0200;
}

char           *
key_name(key_type c)
{
    static char     buf[10];

    if (c >= NORMAL_KEYS && c <= (key_type) (KEY_MAP_SIZE - NATIONAL_KEYS)) {
	switch (c) {
	    case K_up_arrow:
		return "up";
	    case K_down_arrow:
		return "down";
	    case K_left_arrow:
		return "left";
	    case K_right_arrow:
		return "right";
	    case K_m_d1:
		return "mouse_d1";
	    case K_m_d2:
		return "mouse_d2";
	    case K_m_d3:
		return "mouse_d3";
	    case K_m_u1:
		return "mouse_u1";
	    case K_m_u2:
		return "mouse_u2";
	    case K_m_u3:
		return "mouse_u3";
	    default:
		buf[0] = '#';
		buf[1] = (c - K_function(0))
		    + (c >= (key_type) K_function(MULTI_KEYS) ? 'A' - K_function(MULTI_KEYS) : '0');
		buf[2] = NUL;
		goto out;
	}
    }
    if (c == SP)
	return "space";

    if (c < SP) {
	buf[0] = '^';
	buf[1] = c + '@';
	buf[2] = NUL;
	goto out;
    }
    if (c == 0177) {
	strcpy(buf, "^?");
	goto out;
    }
    if (data_bits < 8 && c >= NORMAL_KEYS) {
	sprintf(buf, "0x%02x", (uint)c);
	goto out;
    }
    buf[0] = c;
    buf[1] = NUL;

out:
    return buf;
}


void
dump_global_map(void)
{
    register key_type c;

    clrdisp();
    so_printf("\1REMAPPED KEYS\1\n\n");
    pg_init(2, 4);

    for (c = 0; c < KEY_MAP_SIZE; c++)
	if (c != global_key_map[c]) {
	    if (pg_next() < 0)
		break;
	    tprintf("%s", key_name(c));
	    pg_indent(6);
	    tprintf("-> %s", key_name(global_key_map[c]));
	}
    pg_end();
}

int
dump_key_map(char *where)
{
    register struct command_name_map *cnmp;
    register key_type c;
    register int    code, first_prt;
    int            *map, restriction;

    if ((code = lookup_keymap(where)) < 0)
	return -1;
    map = keymaps[code].km_map;
    restriction = keymaps[code].km_flag & (K_ONLY_MENU | K_ONLY_MORE);

    clrdisp();
    so_printf("\1KEY BINDINGS (%s)\1\n\n", where);

    if (restriction == K_ONLY_MENU) {
	tprintf("\rarticle:  ");
	for (c = 0; c < KEY_MAP_SIZE; c++)
	    if (map[c] & K_ARTICLE_ID)
		tprintf("%s", key_name(c));
    }
    pg_init(4, 2);

    for (cnmp = command_name_map; cnmp->cmd_name; cnmp++) {
	if (cnmp->cmd_restriction && cnmp->cmd_restriction != restriction)
	    continue;
	if (cnmp->cmd_code == K_UNBOUND)
	    continue;
	if (cnmp->cmd_code == K_MACRO)
	    continue;

	code = cnmp->cmd_code;
	first_prt = 1;

	for (c = 0; c < KEY_MAP_SIZE; c++)
	    if (map[c] == code) {
		if (first_prt) {
		    if (pg_next() < 0)
			goto out;
		    tprintf("%s", cnmp->cmd_name);
		    pg_indent(max_cmd_name_length);
		    first_prt = 0;
		}
		tprintf(" %s", key_name(c));
	    }
    }

    for (c = 0; c < KEY_MAP_SIZE; c++)
	if (map[c] & K_MACRO) {
	    if (pg_next() < 0)
		goto out;
	    tprintf("macro %d: %s", (map[c] & ~K_MACRO), key_name(c));
	    if (map == menu_key_map && orig_menu_map[c] != K_UNBOUND)
		tprintf(" (%s)", command_name(orig_menu_map[c]));
	} else if (map[c] & K_PREFIX_KEY) {
	    if (pg_next() < 0)
		goto out;
	    tprintf("prefix %s: %s",
		    keymaps[(map[c] & ~K_PREFIX_KEY)].km_name, key_name(c));
	}
out:
    pg_end();
    return 0;
}

#define MAX_KEYMAPS	17

struct key_map_def keymaps[MAX_KEYMAPS + 1] = {
    "#", NULL, K_MULTI_KEY_MAP,
    "key", NULL, K_GLOBAL_KEY_MAP,
    "menu", menu_key_map, K_BIND_ORIG | K_ONLY_MENU,
    "show", more_key_map, K_ONLY_MORE,
    "both", menu_key_map, K_BOTH_MAPS | K_BIND_ORIG,
    "more", more_key_map, K_ONLY_MORE,
    "read", more_key_map, K_ONLY_MORE,
    NULL,
};

int
lookup_keymap(char *name)
{
    register struct key_map_def *m;

    if (name[0] == '#')
	return 0;

    for (m = keymaps; m->km_name; m++) {
	if (strcmp(name, m->km_name) == 0)
	    return m - keymaps;
    }
    return keymaps - m;
}

int
make_keymap(char *name)
{
    register struct key_map_def *m;
    register int   *kp;
    int             ix;

    ix = lookup_keymap(name);
    if (ix >= 0)
	return -1;
    ix = -ix;
    if (ix == MAX_KEYMAPS)
	return -2;

    m = &keymaps[ix];
    m->km_name = copy_str(name);
    m->km_map = newobj(int, KEY_MAP_SIZE);
    m->km_flag = K_ONLY_MORE | K_ONLY_MENU;
    for (kp = m->km_map; kp < &(m->km_map)[KEY_MAP_SIZE];)
	*kp++ = K_UNBOUND;

    return ix;
}

int
keymap_completion(char *buf, int ix)
{
    static char    *head, *tail = NULL;
    static int      len;
    static struct key_map_def *map, *help_map;

    if (ix < 0)
	return 0;

    if (buf) {
	head = buf;
	tail = buf + ix;
	while (*head && isspace(*head))
	    head++;
	help_map = map = keymaps;
	len = tail - head;

	return 1;
    }
    if (ix) {
	list_completion((char *) NULL);

	if (help_map->km_name == NULL)
	    help_map = keymaps;

	for (; help_map->km_name; help_map++) {
	    if (strncmp(help_map->km_name, head, len))
		continue;
	    if (list_completion(help_map->km_name) == 0)
		break;
	}
	fl;
	return 1;
    }
    for (; map->km_name; map++) {
	if (len && strncmp(map->km_name, head, len))
	    continue;
	strcpy(tail, map->km_name + len);
	if (map != keymaps)
	    strcat(tail, " ");
	map++;
	return 1;
    }
    return 0;
}
