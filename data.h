/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Internal representation of the master, group, and article
 *	information read from the database.
 *
 *	For each article read from the database, an article_header
 *	structure is initialized.
 */

#ifndef _NN_DATA_H
#define _NN_DATA_H 1

/*
 *	The magic number allows us to change the format of the database
 *	and tell the users about it
 */

#define NNDB_MAGIC	0x4e4e2302	/* NN#2 */

/*
 *	global master data
 */

#define	DB_LOCK_MESSAGE		80

typedef struct {
    int32           db_magic;
    char            db_lock[DB_LOCK_MESSAGE];
    time_t          db_created;	/* when database was last built */
    time_t          last_scan;	/* age of active file at last scan */
    off_t           last_size;	/* size of active file at last scan */
    group_number    number_of_groups;
    int32           free_groups;
}               master_header;

/*
 *	group information
 */

typedef struct group_header {

    /* this part of the header is read from the MASTER file */

    flag_type       master_flag;/* master's flags */

#define M_VALID		FLAG(1)	/* group present in active file */
#define M_CONTROL	FLAG(2)	/* group is control group */
#define M_NO_DIRECTORY	FLAG(3)	/* group directory not found */
#define M_ALWAYS_DIGEST	FLAG(4)	/* D in GROUPS */
#define M_NEVER_DIGEST	FLAG(5)	/* N in GROUPS */
#define M_EXPIRE		FLAG(6)	/* expire in progress or pending */
#define M_BLOCKED	FLAG(7)	/* don't trust this entry */
#define M_MUST_CLEAN	FLAG(8)	/* group should be cleaned */
#define M_MODERATED	FLAG(9)	/* group is moderated */
#define M_ALIASED	FLAG(10)/* =xxx in active file */
#define M_NOPOST		FLAG(11)	/* 'n' in active file */
#define M_AUTO_RECOLLECT	FLAG(12)	/* R in GROUPS */
#define M_AUTO_ARCHIVE	FLAG(13)/* >file in GROUPS */
#define M_INCLUDE_OLD	FLAG(14)/* O in GROUPS */
#define M_IGNORE_G	FLAG(15)/* ignore this group (GROUPS X) */
#define M_IGNORE_A	FLAG(16)/* ignore this group (ACTIVE x) */
#define M_IGNORE_GROUP	(M_IGNORE_G | M_IGNORE_A)

    article_number  first_db_article;	/* min article in db */
    article_number  last_db_article;	/* max article in db */

    article_number  first_a_article;	/* min article in active */
    article_number  last_a_article;	/* max article in active */

    long            index_write_offset;
    long            data_write_offset;

    time_t          creation_time;	/* when group was created */

    int             group_name_length;

    /* this part is initialized during reading of the GROUPS file */

    /* DO NOT CHANGE THE POSITION OF group_flag AS THE FIRST FIELD */
    /* AFTER THE PART WHICH IS SAVED IN THE MASTER FILE */

    flag_type       group_flag;	/* client's flags */

#define G_UNSUBSCRIBED	FLAG(1)	/* ! in .newsrc */
#define G_READ		FLAG(2)	/* group has been visited */
#define G_FAKED		FLAG(3)	/* faked group - not in .newsrc */
#define G_NEW		FLAG(5)	/* new group */
#define G_FOLDER		FLAG(6)	/* "group" is a folder file */
#define G_DIRECTORY	FLAG(7)	/* "group" is directory */
#define G_SEQUENCE	FLAG(8)	/* in presentation sequence */
#define G_DONE		FLAG(9)	/* sequence is done with group */
#define G_MAILBOX	FLAG(10)/* user's mail box file */
#define G_MERGE_HEAD	FLAG(11)/* merged group head */
#define G_MERGE_SUB	FLAG(12)/* merged group sub element */
#define G_MERGED 	(G_MERGE_HEAD | G_MERGE_SUB)
#define G_COUNTED	FLAG(13)/* counted in unread_count */

    group_number    group_num;
    group_number    preseq_index;

    char           *group_name;

    char           *archive_file;

    article_number  first_article;
    article_number  last_article;

    article_number  current_first;	/* first article currently read in */

    struct group_header *next_group;	/* group sequence */
    struct group_header *prev_group;

    struct group_header *merge_with;	/* merged groups */

    char           *kill_list;
    char           *save_file;	/* default save file from init */
    char           *enter_macro;

    struct group_header *newsrc_seq;	/* .newsrc sequence		 */
    char           *newsrc_line;/* .newsrc line for this group */
    char           *newsrc_orig;/* original newsrc_line if changed */
    char           *select_line;/* .nn/select line for this group */
    char           *select_orig;/* original select_line if changed */

    article_number  unread_count;	/* number of unread articles */
}               group_header;


/* size of the part of the group header placed on backing storage */


#define SAVED_GROUP_HEADER_SIZE(group) \
    (((char *)(&((group).group_flag))) - ((char *)(&(group))))

/*
 *	Internal article header information.
 */

typedef struct {
    union {
	article_number  au_number;	/* article number in the group	 */
	char           *au_string;
    }               au_union;
    group_header   *a_group;	/* if merged article menu	 */

    /* indexes to header line text	 */
    long            hpos;	/* first byte of header		 */
    long            fpos;	/* first byte in article text	 */
    off_t           lpos;	/* last pos of article		 */

    time_stamp      t_stamp;	/* encoded time_stamp		 */
    time_stamp      root_t_stamp;	/* subject's time_stamp		 */

    char           *sender;	/* sender's name		 */
    char           *subject;	/* subject (w/o Re:)		 */

    int16           lines;	/* no of lines		 */
    int8            replies;	/* no of Re:			 */

    int8            subj_length;/* length of subject		 */
    int8            name_length;/* length of sender		 */

    int8            menu_line;	/* current line on menu		 */

    attr_type       attr;	/* attributes: 			 */
    attr_type       disp_attr;	/* currently displayed attr.	 */
    /* warning: notice relation between A_SELECT and A_AUTO_SELECT */

#define A_READ		0x01	/* article has been read	 */
#define A_SEEN		0x02	/* article presented on menu	 */
#define A_LEAVE		0x03	/* marked for later activity	 */
#define A_LEAVE_NEXT	0x04	/* marked for next invokation	 */
#define A_CANCEL		0x05	/* folder entry cancelled	 */
#define A_KILL		0x06	/* eliminate article		 */
#define A_SELECT   	0x08	/* article has been selected	 */
#define A_AUTO_SELECT	0x09	/* will match & A_SELECT	 */

#define A_NOT_DISPLAYED	0x7f	/* not currently on menu	 */

    flag_type       flag;	/* attributes and flags:	 */

#define A_SAME	    	FLAG(1)	/* same subject as prev. article */
#define A_ALMOST_SAME	FLAG(2)	/* A_SAME (match-limit)		 */
#define A_ROOT_ART	FLAG(3)	/* root article in subject	 */
#define A_NEXT_SAME	FLAG(4)	/* next is same subject		 */
#define A_DIGEST   	FLAG(5)	/* digest sub article		 */
#define A_FULL_DIGEST	FLAG(6)	/* full digest			 */
#define A_FOLDER		FLAG(7)	/* article file = "folder_path"	 */
#define A_FAKED    	FLAG(8)	/* only 'number' is valid	 */
#define A_ST_FILED	FLAG(10)/* articles is saved		 */
#define A_ST_REPLY	FLAG(11)/* sent reply to article	 */
#define A_ST_FOLLOW	FLAG(12)/* sent followup to article	 */
#define A_CLOSED		FLAG(13)	/* subject closed on menu	 */
#define A_HIDE		FLAG(14)/* hide subject on menu	 */

}               article_header;


#define	a_number	au_union.au_number
#define a_string	au_union.au_string
#endif				/* _NN_DATA_H */
