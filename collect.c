/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Collect and save article information in database.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "global.h"
#include "db.h"
#include "digest.h"
#include "master.h"
#include "news.h"
#include "nntp.h"
#include "pack_date.h"
#include "pack_name.h"
#include "pack_subject.h"

/* collect.c */

static void     do_auto_archive(group_header * gh, register FILE * f, article_number num);
static void     build_hdr(int type);
static int      collect_article(register group_header * gh, article_number art_num);
static long     collect_group(register group_header * gh);



#define COUNT_RE_REFERENCES	/* no of >>> depends on Reference: line */

int             ignore_bad_articles = 1;	/* no Newsgroups: line */
int             remove_bad_articles = 0;
time_t          max_article_age = 0;

extern int      trace, debug_mode;

#ifdef NNTP
extern int      nntp_failed;
#endif

static long     bad_count;

static FILE    *ix, *data;

static void
do_auto_archive(group_header * gh, register FILE * f, article_number num)
{
    char            line[200];
    article_number  last;
    register FILE  *arc;
    register int    c;
    long            start;
    static char    *arc_header = "Archived-Last: ";
    /* Header format: Archived-Last: 88888888 group.name */
    /* Fixed constants length == 15 and offset == 24 are used below */

    arc = open_file(gh->archive_file, OPEN_READ);
    last = 0;
    start = 0;
    if (arc != NULL) {
	while (fgets(line, 200, arc) != NULL) {
	    if (strncmp(line, arc_header, 15)) {
		log_entry('E', "%s not archive for %s\n",
			  gh->archive_file, gh->group_name);
		gh->master_flag &= ~M_AUTO_ARCHIVE;
		fclose(arc);
		return;
	    }
	    if (strncmp(line + 24, gh->group_name, gh->group_name_length)) {
		start = ftell(arc);
		continue;
	    }
	    last = atol(line + 15);
	    break;
	}
	fclose(arc);
    }
    if (last >= num)
	return;

    arc = open_file(gh->archive_file, last > 0 ? OPEN_UPDATE : OPEN_CREATE);
    if (arc == NULL) {
	log_entry('E', "Cannot create archive file: %s\n", gh->archive_file);
	gh->master_flag &= ~M_AUTO_ARCHIVE;
	return;
    }
    fseek(arc, start, 0);
    fprintf(arc, "%s%8ld %s\n", arc_header, (long) num, gh->group_name);
    fseek(arc, 0, 2);

    fseek(f, 0, 0);
    while ((c = getc(f)) != EOF)
	putc(c, arc);
    putc(NL, arc);
    fclose(arc);
}

static void
build_hdr(int type)
{
    register char  *name, *subj;
    int             re;

    db_data.dh_type = type;

    if (type == DH_SUB_DIGEST) {

	name = digest.dg_from;
	subj = digest.dg_subj;

	db_hdr.dh_lines = digest.dg_lines;

	db_hdr.dh_hpos = digest.dg_hpos;
	db_hdr.dh_fpos = (int16) (digest.dg_fpos - db_hdr.dh_hpos);
	db_hdr.dh_lpos = digest.dg_lpos;

	db_hdr.dh_date = pack_date(digest.dg_date ? digest.dg_date : news.ng_date);
    } else {

	if (!news.ng_from)
	    news.ng_from = news.ng_reply;

	name = news.ng_from;
	subj = news.ng_subj;

	db_hdr.dh_lines = news.ng_lines;

	db_hdr.dh_hpos = 0;
	db_hdr.dh_fpos = (int16) (news.ng_fpos);
	db_hdr.dh_lpos = news.ng_lpos;

	db_hdr.dh_date = pack_date(news.ng_date);
    }

    if (name) {
	db_hdr.dh_sender_length = pack_name(db_data.dh_sender, name, NAME_LENGTH);
    } else
	db_hdr.dh_sender_length = 0;

    if (type == DH_DIGEST_HEADER) {
	db_hdr.dh_subject_length = 1;
	db_data.dh_subject[0] = '@';
    } else
	db_hdr.dh_subject_length = 0;

    db_hdr.dh_subject_length +=
	pack_subject(db_data.dh_subject + db_hdr.dh_subject_length, subj, &re,
		     DBUF_SIZE);

#ifdef COUNT_RE_REFERENCES
    if (re)
	re = 0x80;
    if (news.ng_ref) {
	for (name = news.ng_ref; *name; name++) {
	    if ((re & 0x7f) == 0x7f)
		break;
	    if (*name == '<')
		re++;
	}
    }
#endif

    db_hdr.dh_replies = re;

    if (db_write_art(data) < 0)
	write_error();
}


static int
collect_article(register group_header * gh, article_number art_num)
{
    FILE           *art_file;
    news_header_buffer nhbuf, dgbuf;
    article_header  art_hdr;
    int             mode, count;
    cross_post_number *cp_ptr;
    long            age;

    count = 0;

    db_hdr.dh_number = art_num;

    /* get article header */

    art_hdr.a_number = art_num;
    art_hdr.hpos = 0;
    art_hdr.lpos = (off_t) 0;
    art_hdr.flag = 0;

    mode = FILL_NEWS_HEADER | FILL_OFFSETS | SKIP_HEADER;
    if ((gh->master_flag & (M_CONTROL | M_NEVER_DIGEST | M_ALWAYS_DIGEST)) == 0)
	mode |= DIGEST_CHECK;

#ifdef NNTP
    if ((gh->master_flag & M_ALWAYS_DIGEST) == 0)
	mode |= LAZY_BODY;
#endif

    if ((art_file = open_news_article(&art_hdr, mode, nhbuf, (char *) NULL)) == NULL) {

#ifdef NNTP
	if (nntp_failed) {

	    /*
	     * connection to nntp_server is broken stop collection of
	     * articles immediately
	     */
	    return -1;
	}
#endif

	/*
	 * it is not really necessary to save anything in the data file we
	 * simply use the index file to get the *first* available article
	 */
	return 0;
    }
    if (art_file == (FILE *) 1) {	/* empty file */
	if (!ignore_bad_articles)
	    return 0;
	news.ng_groups = NULL;
	art_file = NULL;
    } else if (max_article_age &&	/* == 0 if use_nntp */
	       (gh->master_flag & M_INCLUDE_OLD) == 0 &&
	       (age = m_time(art_file)) < max_article_age) {

	if (remove_bad_articles)
	    unlink(group_path_name);

	log_entry('O', "%sold article (%ld days): %s/%ld",
		  remove_bad_articles ? "removed " : "",
		  (cur_time() - age) / (24 * 60 * 60),
		  current_group->group_name, (long) art_num);
	bad_count++;
	fclose(art_file);
	return 0;
    }
    if (ignore_bad_articles && news.ng_groups == NULL) {
	char           *rem = "";

	if (!use_nntp && remove_bad_articles) {
	    unlink(group_path_name);
	    rem = "removed ";
	}
	log_entry('B', "%sbad article: %s/%ld", rem,
		  current_group->group_name, (long) art_num);
	if (art_file != NULL)
	    fclose(art_file);
	bad_count++;
	return 0;
    }
    /* map cross-postings into a list of group numbers */

    db_hdr.dh_cross_postings = 0;

    if (gh->master_flag & M_CONTROL) {
	/* we cannot trust the Newsgroups: line in the control group */
	/* so we simply ignore it (i.e. use "Newsgroups: control") */
	goto dont_digest;
    }
    if (news.ng_groups) {
	char           *curg, *nextg;
	group_header   *gh1;

	for (nextg = news.ng_groups, cp_ptr = db_data.dh_cross; *nextg;) {
	    curg = nextg;

	    if ((nextg = strchr(curg, ',')))
		*nextg++ = NUL;
	    else
		nextg = "";

	    if (strcmp(gh->group_name, curg) == 0)
		gh1 = gh;
	    else if ((gh1 = lookup(curg)) == NULL)
		continue;

	    *cp_ptr++ = NETW_CROSS_EXT(gh1->group_num);
	    if (++db_hdr.dh_cross_postings == DBUF_SIZE)
		break;
	}
    }
    if (db_hdr.dh_cross_postings == 1)
	db_hdr.dh_cross_postings = 0;	/* only current group */

    if (gh->master_flag & M_NEVER_DIGEST)
	goto dont_digest;

    /* split digest */

    if ((gh->master_flag & M_ALWAYS_DIGEST) || (news.ng_flag & N_DIGEST)) {
	int             any = 0, cont = 1;

	skip_digest_body(art_file);

	while (cont && (cont = get_digest_article(art_file, dgbuf)) >= 0) {

	    if (any == 0) {
		build_hdr(DH_DIGEST_HEADER);	/* write DIGEST_HEADER */
		count++;
		db_hdr.dh_cross_postings = 0;	/* no cross post in sub */
		any++;
	    }
	    build_hdr(DH_SUB_DIGEST);	/* write SUB_DIGEST */
	    count++;
	}

	if (any)
	    goto finish;
    }
    /* not a digest */

dont_digest:

    build_hdr(DH_NORMAL);	/* normal article */
    count++;

finish:

    if (gh->master_flag & M_AUTO_ARCHIVE) {

#ifdef NNTP
	FILE           *f;
	f = nntp_get_article(art_num, 0);
	do_auto_archive(gh, f, art_num);
	fclose(f);
#else
	do_auto_archive(gh, art_file, art_num);
#endif				/* NNTP */
    }
    fclose(art_file);

    return count;
}


/*
 *	Collect unread articles in current group
 *
 *	On entry, init_group has been called to setup the proper environment
 */

static long
collect_group(register group_header * gh)
{
    long            article_count, temp, obad;
    article_number  start_collect;

    if (gh->last_db_article == 0) {
	gh->first_db_article = gh->first_a_article;
	gh->last_db_article = gh->first_db_article - 1;
    }
    if (gh->last_db_article >= gh->last_a_article)
	return 0;

    if (gh->index_write_offset) {
	ix = open_data_file(gh, 'x', OPEN_UPDATE | MUST_EXIST);
	fseek(ix, gh->index_write_offset, 0);
    } else
	ix = open_data_file(gh, 'x', OPEN_CREATE | MUST_EXIST);

    if (gh->data_write_offset) {
	data = open_data_file(gh, 'd', OPEN_UPDATE | MUST_EXIST);
	fseek(data, gh->data_write_offset, 0);
    } else
	data = open_data_file(gh, 'd', OPEN_CREATE | MUST_EXIST);

    article_count = 0;
    start_collect = gh->last_db_article + 1;

    if (debug_mode) {
	printf("\t\t%s (%ld..%ld)\r",
	       gh->group_name, start_collect, gh->last_a_article);
	fl;
    }
    bad_count = obad = 0;

    while (gh->last_db_article < gh->last_a_article) {
	if (s_hangup)
	    break;
	gh->last_db_article++;
	if (debug_mode) {
	    printf("\r%ld", gh->last_db_article);
	    if (obad != bad_count)
		printf("\t%ld", bad_count);
	    obad = bad_count;
	    fl;
	}
	gh->data_write_offset = ftell(data);

#ifdef NNTP
	gh->index_write_offset = ftell(ix);
#endif

	temp = collect_article(gh, gh->last_db_article);

#ifdef NNTP
	if (temp < 0) {
	    /* connection failed, current article is not collected */
	    gh->last_db_article--;
	    article_count = -1;
	    goto out;
	}
#endif

#ifndef RENUMBER_DANGER
	if (temp == 0 && gh->data_write_offset == 0) {
	    gh->first_db_article = gh->last_db_article + 1;
	    continue;
	}
#endif

	if (!db_write_offset(ix, &(gh->data_write_offset)))
	    write_error();
	article_count += temp;
    }

    if (start_collect < gh->first_db_article)
	start_collect = gh->first_db_article;

    if (trace && start_collect <= gh->last_db_article)
	log_entry('T', "Col %s (%d to %d) %d",
		  gh->group_name,
		  start_collect, gh->last_db_article,
		  article_count);

    if (debug_mode)
	printf("\nCol %s (%ld to %ld) %ld",
	       gh->group_name,
	       start_collect, gh->last_db_article,
	       article_count);

    gh->data_write_offset = ftell(data);
    gh->index_write_offset = ftell(ix);

#ifdef NNTP
out:
#endif

    fclose(data);
    fclose(ix);

    if (debug_mode)
	putchar(NL);

    return article_count;
}

int
do_collect(void)
{
    register group_header *gh;
    long            col_article_count, temp;
    int             col_group_count;
    time_t          start_time;

    start_time = cur_time();
    col_article_count = col_group_count = 0;
    current_group = NULL;	/* for init_group */
    temp = 0;

    Loop_Groups_Header(gh) {
	if (s_hangup) {
	    temp = -1;
	    break;
	}
	if (gh->master_flag & M_IGNORE_GROUP)
	    continue;

	if (gh->master_flag & M_MUST_CLEAN)
	    clean_group(gh);

	if (gh->last_db_article == gh->last_a_article) {
	    if (gh->master_flag & M_BLOCKED)
		goto unblock_group;
	    continue;
	}
	if (!init_group(gh)) {
	    if ((gh->master_flag & M_NO_DIRECTORY) == 0) {
		log_entry('R', "%s: no directory", gh->group_name);
		gh->master_flag |= M_NO_DIRECTORY;
	    }
	    gh->last_db_article = gh->last_a_article;
	    gh->first_db_article = gh->last_a_article;	/* OBS: not first */
	    gh->master_flag &= ~(M_EXPIRE | M_BLOCKED);
	    db_write_group(gh);
	    continue;
	}
	if (gh->master_flag & M_NO_DIRECTORY) {
	    /* The directory has been created now */
	    gh->master_flag &= ~M_NO_DIRECTORY;
	    clean_group(gh);
	}
	temp = collect_group(gh);

#ifdef NNTP
	if (temp < 0) {
	    /* connection broken */
	    gh->master_flag &= ~M_EXPIRE;	/* remains blocked */
	    db_write_group(gh);
	    break;
	}
#endif

	if (temp > 0) {
	    col_article_count += temp;
	    col_group_count++;
	}
unblock_group:
	gh->master_flag &= ~(M_EXPIRE | M_BLOCKED);
	db_write_group(gh);
    }

    if (col_article_count > 0)
	log_entry('C', "Collect: %ld art, %d gr, %ld s",
		  col_article_count, col_group_count,
		  cur_time() - start_time);

    return temp > 0;		/* return true IF we got articles */
}
