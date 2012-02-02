/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2003 Michael T Pins.  All rights reserved.
 *
 *	Keyboard (re)mapping
 */

#ifndef _NN_KEYMAP_H
#define _NN_KEYMAP_H 1

#define K_INVALID		0x0000	/* unknown command (for lookup) */

#define K_UNBOUND		0x0001	/* unbound command key 		 */

#define K_REDRAW		0x0002	/* redraw 			 */
#define K_CONTINUE		0x0003	/* continue with next ... 	 */
#define K_LAST_MESSAGE		0x0004	/* repeat last message 		 */
#define K_HELP			0x0005	/* online help 			 */
#define K_SHELL			0x0006	/* shell escape 		 */
#define K_VERSION		0x0007	/* print version 		 */
#define K_EXTENDED_CMD		0x0008	/* extended commands		 */

#define K_QUIT			0x0009	/* quit 			 */

#define	K_BUG_REPORT		0x000a	/* send bug report */

#define K_SAVE_HEADER_ONLY	0x0010	/* save header only		 */
#define K_SAVE_NO_HEADER 	0x0011	/* save articles without header */
#define K_SAVE_SHORT_HEADER 	0x0012	/* save article with short header */
#define K_SAVE_FULL_HEADER 	0x0013	/* save articles with full header */

#define K_PRINT			0x0014	/* print ariticle 		 */

#define K_UNSHAR		0x0015	/* unshar article		 */

#define K_REPLY			0x0016	/* reply to article 		 */
#define K_FOLLOW_UP		0x0017	/* follow up to article 	 */
#define K_POST			0x0018	/* post an article		 */
#define K_MAIL_OR_FORWARD 	0x0019	/* mail (forward article) 	 */
#define K_CANCEL		0x001a	/* cancel article 		 */
#define K_UNSUBSCRIBE		0x001b	/* (un)subscribe to group 	 */
#define K_GROUP_OVERVIEW 	0x001c	/* group overview 		 */
#define K_PATCH			0x001d	/* pipe article to patch         */
#define	K_UUDECODE		0x001e	/* uudecode articles		 */

#define K_GOTO_GROUP		0x001f	/* goto named group/folder	 */

#define K_KILL_HANDLING		0x0020	/* enter kill menu		 */

 /* scrolling/menu movement */

#define K_CONTINUE_NO_MARK	0x0021	/* as continue but don't mark seen */
#define K_JUNK_ARTICLES		0x0022	/* convert given attr to read	 */
#define K_SKIP_LINES		0x0023	/* skip lines of same type	 */
#define K_NEXT_PAGE		0x0024	/* next page 			 */
#define K_NEXT_HALF_PAGE 	0x0025	/* next half page		 */
#define K_NEXT_LINE		0x0026	/* next line			 */
#define K_PREV_PAGE		0x0027	/* previous page 		 */
#define K_PREV_HALF_PAGE 	0x0028	/* previous half page		 */
#define K_PREV_LINE		0x0029	/* previous line		 */

#define K_HEADER_PAGE		0x002a	/* first page incl. header	 */
#define K_FIRST_PAGE		0x002b	/* first page 			 */
#define K_LAST_PAGE		0x002c	/* last page 			 */

#define K_GOTO_LINE		0x002d	/* goto specific line		 */
#define K_GOTO_PAGE		0x002e	/* goto specific page		 */
#define K_GOTO_MATCH		0x002f	/* goto line matching regexp	 */
#define K_NEXT_MATCH		0x0030	/* find next match		 */

#define K_PREVIOUS		0x0031	/* goto prev group or article	 */
 /* (no update is performed)	 */

 /* more() SPECIFIC COMMANDS */

#define K_LEAVE_ARTICLE		0x0032	/* goto next article, mark current */
#define K_LEAVE_NEXT		0x0033	/* mark current for next time	 */
#define K_NEXT_ARTICLE		0x0034	/* goto next article	 	 */
#define K_NEXT_SUBJECT		0x0035	/* goto next subject		 */
#define K_FULL_DIGEST		0x0036	/* show full digest		 */
#define K_ROT13			0x0037	/* do rot13 			 */
#define K_COMPRESS		0x0038	/* compress spaces		 */
#define K_BACK_TO_MENU		0x0039	/* return to menu */
#define	K_BACK_ARTICLE		0x003a	/* back one article		 */
#define	K_FORW_ARTICLE		0x003b	/* forward one article		 */

 /* menu() SPECIFIC COMMANDS	 */

#define K_SELECT		0x0041	/* select current, move down 	 */
#define K_SELECT_INVERT		0x0042	/* invert all selections 	 */
#define K_SELECT_SUBJECT 	0x0043	/* select all with same subject */
#define K_SELECT_RANGE		0x0044	/* select range 		 */
#define K_AUTO_SELECT		0x0045	/* auto select from kill file	 */
#define K_UNSELECT_ALL		0x0046	/* undo all selections		 */

#define K_LAYOUT		0x0049	/* change menu layout 		 */

#define K_NEXT_GROUP_NO_UPDATE 	0x004a	/* goto next group, no update 	 */
#define K_READ_GROUP_UPDATE 	0x004b	/* read selected, then next group */
#define K_READ_GROUP_THEN_SAME	0x004c	/* read selected, then same group */

#define K_ADVANCE_GROUP		0x004d	/* advance one group in sequence */
#define K_BACK_GROUP		0x004e	/* back-up one group in sequence */

#define K_PREVIEW		0x004f	/* preview article 		 */

#define K_OPEN_SUBJECT		0x0050	/* open subject on menu		 */
#define K_CLOSE_SUBJECT		0x0051	/* close subject on menu		 */

#define K_M_TOGGLE		0x0060	/* page with mouse       	 */
#define K_M_CONTINUE		0x0061	/* page with mouse       	 */
#define K_M_SELECT		0x0062	/* select with mouse       	 */
#define K_M_SELECT_SUBJECT	0x0063	/* select subject        	 */
#define K_M_SELECT_RANGE	0x0064	/* used for draging a range     	 */
#define K_M_PREVIEW		0x0065	/* preview articles	     	 */

#define	K_EQUAL_KEY		0x0070	/* map command special symbol	 */

#define	K_MACRO			0x0100	/* call macro			 */
#define	K_ARTICLE_ID		0x0200	/* article id in lower part	 */
#define K_PREFIX_KEY		0x0400	/* key map number in lower part	 */

/* keymap definitions from keymap.c */

#define MULTI_KEYS	21

/* restrictions */

#define K_ONLY_MENU	0x0001
#define K_ONLY_MORE	0x0002
#define	K_BOTH_MAPS	0x0004	/* map flag: for "both" */
#define	K_BIND_ORIG	0x0008	/* map flag: must maintain orig_menu_map */
#define K_GLOBAL_KEY_MAP	0x0010	/* "key" */
#define	K_MULTI_KEY_MAP		0x0020	/* "#..." */

typedef unsigned char key_type;

extern int      menu_key_map[];
extern int      more_key_map[];
extern int      orig_menu_map[];

extern key_type global_key_map[];

struct key_map_def {
    char           *km_name;	/* key map name */
    int            *km_map;	/* key map table */

    int             km_flag;	/* flags */
};

extern struct key_map_def keymaps[];

void            init_key_map(void);
int             lookup_command(char *, int);
int             cmd_completion(char *, int);
char           *command_name(int);
key_type        parse_key(char *);
char           *key_name(key_type);
void            dump_global_map(void);
int             dump_key_map(char *);
int             lookup_keymap(char *);
int             make_keymap(char *);
int             keymap_completion(char *, int);
#endif				/* _NN_KEYMAP_H */
