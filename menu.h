/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2003 Michael T Pins.  All rights reserved.
 *
 *	Various module return codes.
 */

#ifndef _NN_MENU_H
#define _NN_MENU_H 1


/* menu commands */

#define ME_QUIT		0	/* quit nn */
#define ME_NEXT		1	/* continue to next group */
#define ME_PREV		3	/* previous group */
#define ME_NO_ARTICLES	4	/* no articles in group */
#define ME_REDRAW	5	/* redraw screen after return */
#define ME_NO_REDRAW	6	/* screen is not corrupted */
#define ME_REENTER_GROUP 7	/* reenter after .newsrc update */


/* more commands */

#define MC_QUIT		0	/* quit nn */
#define	MC_NEXT		1	/* next article */
#define MC_MENU		2	/* return to menu */
#define MC_PREV		3	/* previous article */
#define MC_NEXTSUBJ	4	/* show next subject */
#define MC_ALLSUBJ	5	/* show all with same subject */
#define MC_NEXTGROUP	6	/* next group, no read */
#define	MC_READGROUP	7	/* next group, mark as read */
#define MC_PREVIEW_NEXT	8	/* preview next article */
#define MC_PREVIEW_OTHER 9	/* preview another article */
#define MC_REDRAW	10	/* redraw screen after return */
#define MC_NO_REDRAW	11	/* screen is not corrupted */
#define	MC_BACK_ART	12	/* back one article (don't deselect cur) */
#define MC_FORW_ART	13	/* forward one article (deselect cur) */
#define MC_DO_KILL	14	/* did kill-select kill */
#define MC_DO_SELECT	15	/* did kill-select selection */
#define MC_REENTER_GROUP 16	/* reenter after .newsrc update */

/* more modes */

#define	MM_NORMAL		0x0000	/* show article */
#define MM_DIGEST		0x0001	/* show full digest */
#define MM_PREVIOUS		0x0010	/* previous article exists */
#define MM_LAST_SELECTED	0x0020	/* last selected article in group */
#define MM_LAST_GROUP		0x0040	/* last group */
#define MM_PREVIEW		0x0080	/* preview mode flag */
#define MM_FIRST_ARTICLE 	0x0100	/* first article in group */
#define MM_LAST_ARTICLE		0x0200	/* last article in group */

/* alt_command return values */

#define	AC_QUIT		0	/* quit nn */
#define	AC_PROMPT	1	/* just redraw prompt line */
#define	AC_REDRAW	2	/* redraw screen */
#define AC_REORDER	3	/* articles have been reordered */
#define	AC_REENTER_GROUP 4	/* reenter group after .newsrc update */
#define AC_KEYCMD	5	/* alt_cmd_key contains command */
#define AC_UNCHANGED	6	/* no display changes */

char           *pct(long, long, long, long);
int             menu(fct_type);
article_header *get_menu_article(void);
int             alt_command(void);
int             prt_replies(int);
#endif				/* _NN_MENU_H */
