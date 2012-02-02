/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Basic article access and management
 */

#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "global.h"
#include "articles.h"
#include "db.h"
#include "kill.h"
#include "match.h"
#include "news.h"
#include "newsrc.h"
#include "regexp.h"
#include "sort.h"
#include "nn_term.h"

/* articles.c */

static void     new_thunk(thunk * t, char *ptr, long size);

#ifdef ART_GREP
static int      grep_article(group_header * gh, article_header * ah, char *pattern, int (*fcn) ());
#endif				/* ART_GREP */

int             seq_cross_filtering = 1;
int             select_leave_next = 0;	/* ask to select left over art. */
int             ignore_re = 0;
int             body_search_header = 0;
int             cross_post_limit = 0;

extern int      ignore_fancy_select;
extern int      killed_articles;

extern attr_type test_article();

/*
 * memory management
 */

static          thunk
                dummy_str_t = {
    NULL,
    NULL,
    0L
},

                dummy_art_t = {
    NULL,
    NULL,
    0L
};


static thunk   *first_str_t = &dummy_str_t;
static thunk   *current_str_t = &dummy_str_t;
static thunk   *first_art_t = &dummy_art_t;
static thunk   *current_art_t = &dummy_art_t;
static long     cur_str_size = 0, cur_art_size = 0;
static char    *next_str;
static article_header *next_art;
static article_header **art_array = NULL;

static article_number max_articles = 0, mem_offset = 0;

/*
 * allocate one article header
 */

#ifndef ART_THUNK_SIZE
#define ART_THUNK_SIZE	127
#endif

static void
new_thunk(thunk * t, char *ptr, long size)
{
    thunk          *new;

    new = newobj(thunk, 1);

    new->next_thunk = t->next_thunk;
    t->next_thunk = new;

    new->this_thunk = ptr;
    new->thunk_size = size;
}


article_header *
alloc_art(void)
{
    if (cur_art_size == 0) {
	if (current_art_t->next_thunk == NULL)
	    new_thunk(current_art_t,
		      (char *) newobj(article_header, ART_THUNK_SIZE),
		      (long) ART_THUNK_SIZE);

	current_art_t = current_art_t->next_thunk;
	next_art = (article_header *) current_art_t->this_thunk;
	cur_art_size = current_art_t->thunk_size;
    }
    cur_art_size--;
    return next_art++;
}

/*
 * allocate a string of length 'len'
 */

#ifndef STR_THUNK_SIZE
#define STR_THUNK_SIZE	((1<<14) - 32)	/* leave room for malloc header */
#endif

char           *
alloc_str(int len)
{
    /* allow space for '\0', and align on word boundary */
    int             size = (len + sizeof(int)) & ~(sizeof(int) - 1);
    char           *ret;

    if (cur_str_size < size) {
	if (current_str_t->next_thunk == NULL)
	    new_thunk(current_str_t,
		      newstr(STR_THUNK_SIZE), (long) STR_THUNK_SIZE);

	current_str_t = current_str_t->next_thunk;
	next_str = current_str_t->this_thunk;
	cur_str_size = current_str_t->thunk_size;
    }
    /* XXX else should do something reasonable */

    ret = next_str;
    cur_str_size -= size;
    next_str += size;

    ret[len] = NUL;		/* ensure string is null terminated */
    return ret;
}

/*
 * "free" the allocated memory
 */

void
free_memory(void)
{
    current_str_t = first_str_t;
    current_art_t = first_art_t;
    cur_str_size = 0;
    cur_art_size = 0;
    n_articles = 0;
}


/*
 * mark/release memory
 */


void
mark_str(string_marker * str_marker)
{
    str_marker->sm_cur_t = current_str_t;
    str_marker->sm_size = cur_str_size;
    str_marker->sm_next = next_str;
}

void
release_str(string_marker * str_marker)
{
    current_str_t = str_marker->sm_cur_t;
    cur_str_size = str_marker->sm_size;
    next_str = str_marker->sm_next;
}


void
mark_memory(memory_marker * mem_marker)
{
    mark_str(&(mem_marker->mm_string));

    mem_marker->mm_cur_t = current_art_t;
    mem_marker->mm_size = cur_art_size;
    mem_marker->mm_next = next_art;

    mem_marker->mm_nart = n_articles;
    mem_offset += n_articles;

    n_articles = 0;
    articles = art_array + mem_offset;
}

void
release_memory(memory_marker * mem_marker)
{
    release_str(&(mem_marker->mm_string));

    current_art_t = mem_marker->mm_cur_t;
    cur_art_size = mem_marker->mm_size;
    next_art = mem_marker->mm_next;

    n_articles = mem_marker->mm_nart;

    mem_offset -= n_articles;
    articles = art_array + mem_offset;
}

/*
 * merge all memory chunks into one.
 */

void
merge_memory(void)
{
    n_articles += mem_offset;
    mem_offset = 0;
    articles = art_array;
}


/*
 * save article header in 'articles' array
 * 'articles' is enlarged if too small
 */

#define	FIRST_ART_ARRAY_SIZE	500	/* malloc header */
#define	NEXT_ART_ARRAY_SIZE	512

void
add_article(article_header * art)
{
    if ((n_articles + mem_offset) == max_articles) {
	/* must increase size of 'articles' */

	if (max_articles == 0) {
	    /* allocate initial 'articles' array */
	    max_articles = FIRST_ART_ARRAY_SIZE;
	} else {
	    max_articles += NEXT_ART_ARRAY_SIZE;
	}
	art_array = resizeobj(art_array, article_header *, max_articles);
	articles = art_array + mem_offset;
    }
    articles[n_articles] = art;
    n_articles++;
}


int
access_group(register group_header * gh, article_number first_article, article_number last_article, register flag_type flags, char *mask)
{
    register article_header *ah;
    int             skip_digest, n;
    group_header   *cpgh;

#ifdef NOV
    int             dbstatus;
#else				/* NOV */
    FILE           *data;
    long            data_offset, data_size;
#endif				/* NOV */

    cross_post_number cross_post;
    attr_type       leave_attr;
    memory_marker   mem_marker;
    static regexp  *rexp = NULL;
    static char     subptext[80];

    if (first_article < gh->first_db_article)
	first_article = gh->first_db_article;

    if (last_article > gh->last_db_article)
	last_article = gh->last_db_article;

    if (last_article == 0 || first_article > last_article)
	return 0;

#ifdef NOV
    db_read_group(gh, first_article, last_article);
#else
    data = open_data_file(gh, 'd', OPEN_READ);
    if (data == NULL)
	return -10;

    if ((data_offset = get_data_offset(gh, first_article)) == -1)
	return -11;
#endif				/* NOV */

    if (mask == NULL) {
	if (rexp != NULL) {
	    freeobj(rexp);
	    rexp = NULL;
	}
    } else {
	if (*mask == '/') {
	    mask++;
	    if (rexp != NULL) {
		if (strncmp(mask, subptext, 80) != 0) {
		    freeobj(rexp);
		    rexp = NULL;
		}
	    }
	    if (rexp == NULL) {
		strncpy(subptext, mask, 80);
		rexp = regcomp(mask);
		if (rexp == NULL)
		    return -1;
	    }
	    /* notice mask is still non-NULL */
	}
    }

    if ((flags & (ACC_ALSO_READ_ARTICLES | ACC_ONLY_READ_ARTICLES)))
	leave_attr = 0;
    else if (select_leave_next)
	leave_attr = A_READ;	/* will prompt */
    else
	leave_attr = A_LEAVE_NEXT;

    if (!(flags & ACC_SPEW_MODE))
	use_newsrc(gh, (flags & ACC_MERGED_NEWSRC) ? 2 :
		   (flags & ACC_ORIG_NEWSRC) ? 1 : 0);

    if ((flags & ACC_EXTRA_ARTICLES) == 0)
	mark_memory(&mem_marker);

    ah = alloc_art();

    skip_digest = 0;

#ifdef NOV
    /* XXX: db_read_art takes a FILE * in nnmaster version */
    while ((dbstatus = db_read_art((FILE *) - 1)) > 0) {

#ifdef ART_GREP
	if (s_keyboard || s_hangup)
	    break;		/* add to allow interrupt of GREP */
#endif

	if (db_hdr.dh_number < first_article)
	    continue;
#else
    fseek(data, data_offset, 0);

    while (data_offset < gh->data_write_offset) {

#ifdef ART_GREP
	if (s_keyboard || s_hangup)
	    break;		/* add to allow interrupt of GREP */
#endif

	data_size = db_read_art(data);
	if (data_size <= 0) {
	    fclose(data);
	    if ((flags & ACC_EXTRA_ARTICLES) == 0)
		release_memory(&mem_marker);
	    return -2;
	}
	data_offset += data_size;
#endif				/* NOV */

	if (db_hdr.dh_lpos == (off_t) 0)
	    continue;		/* article not accessible */

	if (db_hdr.dh_number > gh->last_db_article
	    || db_hdr.dh_number < gh->first_db_article)
	    goto data_error;

	if (skip_digest && db_data.dh_type == DH_SUB_DIGEST)
	    continue;

	skip_digest = 0;

	if (cross_post_limit && db_hdr.dh_cross_postings >= cross_post_limit)
	    continue;

	if (db_hdr.dh_cross_postings && !(flags & ACC_ALSO_CROSS_POSTINGS)) {
	    for (n = 0; n < (int) db_hdr.dh_cross_postings; n++) {
		cross_post = NETW_CROSS_INT(db_data.dh_cross[n]);
		if (cross_post < 0 || cross_post >= master.number_of_groups)
		    continue;
		cpgh = ACTIVE_GROUP(cross_post);
		if (cpgh == gh) {
		    if (seq_cross_filtering)
			continue;
		    n = db_hdr.dh_cross_postings;
		    break;
		}
		if (!(flags & ACC_ALSO_UNSUB_GROUPS))
		    if (cpgh->group_flag & G_UNSUBSCRIBED)
			continue;

		if (!seq_cross_filtering)
		    break;
		if (cpgh->preseq_index > 0 &&
		    cpgh->preseq_index < gh->preseq_index)
		    break;
	    }

	    if (n < (int) db_hdr.dh_cross_postings) {
		if (db_data.dh_type == DH_DIGEST_HEADER)
		    skip_digest++;
		continue;
	    }
	}
	ah->flag = 0;

	switch (db_data.dh_type) {
	    case DH_DIGEST_HEADER:
		if (flags & ACC_DONT_SPLIT_DIGESTS)
		    skip_digest++;
		else if ((flags & ACC_ALSO_FULL_DIGEST) == 0)
		    continue;	/* don't want the full digest when split */
		ah->flag |= A_FULL_DIGEST;
		break;
	    case DH_SUB_DIGEST:
		ah->flag |= A_DIGEST;
		break;
	}

	ah->a_number = db_hdr.dh_number;
	if (ah->a_number > last_article)
	    break;

	if (flags & ACC_SPEW_MODE) {
	    fold_string(db_data.dh_subject);
	    tprintf("%x:%s\n", (int) (gh->group_num), db_data.dh_subject);
	    continue;
	}
	ah->hpos = db_hdr.dh_hpos;
	ah->fpos = ah->hpos + (off_t) (db_hdr.dh_fpos);
	ah->lpos = db_hdr.dh_lpos;

	ah->attr = test_article(ah);

	if (flags & ACC_MERGED_NEWSRC) {
	    if (ah->attr == A_KILL)
		continue;
	} else if (ah->attr != A_READ && (flags & ACC_ONLY_READ_ARTICLES))
	    continue;

#ifdef ART_GREP
	if ((flags & ACC_ON_GREP_UNREAD) && (ah->attr == A_READ))
	    continue;
#endif				/* ART_GREP */

	if (rexp != NULL) {
	    if (flags & ACC_ON_SUBJECT)
		if (regexec_cf(rexp, db_data.dh_subject))
		    goto match_ok;
	    if (flags & ACC_ON_SENDER)
		if (regexec_cf(rexp, db_data.dh_sender))
		    goto match_ok;

#ifdef ART_GREP
	    if (flags & (ACC_ON_GREP_UNREAD | ACC_ON_GREP_ALL))
		if (grep_article(gh, ah, (char *) rexp, regexec_cf))
		    goto match_ok;
#endif				/* ART_GREP */

	    continue;
	} else if (mask != NULL) {
	    if (flags & ACC_ON_SUBJECT)
		if (strmatch_cf(mask, db_data.dh_subject))
		    goto match_ok;
	    if (flags & ACC_ON_SENDER)
		if (strmatch_cf(mask, db_data.dh_sender))
		    goto match_ok;

#ifdef ART_GREP
	    if (flags & (ACC_ON_GREP_UNREAD | ACC_ON_GREP_ALL))
		if (grep_article(gh, ah, mask, strmatch_cf))
		    goto match_ok;
#endif				/* ART_GREP */

	    continue;
	}
match_ok:

attr_again:

	switch (ah->attr) {
	    case A_LEAVE:
		if (mask) {
		    ah->attr = 0;
		    break;
		}
		if (leave_attr == A_READ) {
		    clrdisp();
		    prompt("Select left over articles in group %s? ", gh->group_name);
		    leave_attr = yes(0) > 0 ? A_SELECT : A_LEAVE_NEXT;
		}
		ah->attr = leave_attr;
		goto attr_again;

	    case A_SELECT:
		if (mask)
		    ah->attr = 0;
		break;

	    case A_READ:
		if (flags & ACC_MERGED_NEWSRC)
		    break;

		if (!(flags & (ACC_ALSO_READ_ARTICLES | ACC_ONLY_READ_ARTICLES)))
		    if (ah->a_number > gh->last_article)
			continue;

		/* FALLTHROUGH */
	    case A_SEEN:
	    case 0:
		if (flags & ACC_DO_KILL) {
		    ah->replies = db_hdr.dh_replies;
		    ah->sender = db_data.dh_sender;
		    ah->subject = db_data.dh_subject;
		    if (kill_article(ah))
			continue;
		}
		/* The 'P' command to a read group must show articles as read */
		if (gh->unread_count <= 0)
		    ah->attr = A_READ;
		break;

	    default:
		break;
	}

	if (db_hdr.dh_sender_length) {
	    ah->name_length = db_hdr.dh_sender_length;
	    ah->sender = alloc_str((int) db_hdr.dh_sender_length);
	    strcpy(ah->sender, db_data.dh_sender);
	} else
	    ah->sender = "";

	if (db_hdr.dh_subject_length) {
	    ah->subj_length = db_hdr.dh_subject_length;
	    ah->subject = alloc_str((int) db_hdr.dh_subject_length);
	    strcpy(ah->subject, db_data.dh_subject);
	} else
	    ah->subject = "";

	ah->replies = db_hdr.dh_replies;
	ah->lines = db_hdr.dh_lines;
	ah->t_stamp = db_hdr.dh_date;

	ah->a_group = (flags & ACC_MERGED_MENU) ? current_group : NULL;

	add_article(ah);
	ah = alloc_art();
    }

#ifdef NOV
    if (dbstatus < 0) {		/* Error reading DB? */
	if ((flags & ACC_EXTRA_ARTICLES) == 0)
	    release_memory(&mem_marker);
	return -2;
    }
    /* else EOF reading DB */
#else
    fclose(data);
#endif				/* NOV */

    if ((flags & ACC_DONT_SORT_ARTICLES) == 0)
	sort_articles(-1);
    else
	no_sort_articles();

    if (ignore_re && !ignore_fancy_select && !(flags & ACC_ALSO_READ_ARTICLES)) {
	int             nexta, roota, killa, newa;
	for (nexta = killa = newa = roota = 0; nexta < n_articles; nexta++) {
	    ah = articles[nexta];
	    if (ah->flag & A_ROOT_ART)
		roota = ah->replies & 127;
	    if (roota && ah->replies && !auto_select_article(ah, 1))
		killa++;
	    else
		articles[newa++] = ah;
	}
	n_articles -= killa;
	killed_articles += killa;
    }
    return n_articles > 0 ? 1 : 0;

data_error:
    log_entry('E', "%s: data inconsistency", gh->group_name);

#ifndef NOV
    fclose(data);
#endif				/* NOV */

    if ((flags & ACC_EXTRA_ARTICLES) == 0)
	release_memory(&mem_marker);
    return -12;
}

#ifdef ART_GREP
/* GREP ARTICLE -- search the article for the pattern using the specified function
   Return 1 if pattern occurs in the article else 0
   Also return 0 if any malloc or file open or read error occurs
   Global data_header db_hdr is set by caller
todo:
   doesn't work for folders (hangs)
*/

static int
grep_article(group_header * gh, article_header * ah, char *pattern, int (*fcn) ())
{
    char           *line, *end;
    FILE           *art;
    news_header_buffer buffer1, buffer2;
    static char    *buf;
    static int      bufsize, count;
    size_t          size;

    count++;
    art = open_news_article(ah, FILL_OFFSETS |
		  (body_search_header ? 0 : SKIP_HEADER), buffer1, buffer2);
    if (!art) {
	/* msg("Cannot open article"); */
	return 0;
    }
    size = ah->lpos - ftell(art) + 1;
    if (bufsize < size) {
	if (buf)
	    free(buf);
	buf = (char *) malloc(size + 10);
	if (!buf) {
	    msg("Cannot malloc %d bytes", size);
	    bufsize = 0;
	    return 0;
	}
	bufsize = size;
    }
    if (fread(buf, size - 1, 1, art) != 1) {
	fclose(art);
	/* msg("Cannot read article"); */
	return 0;
    }
    fclose(art);
    /* print status message every so often */
    if (!(count % 27))
	msg("Searching %d of %d",
	    ah->a_number - gh->first_db_article,
	    gh->last_db_article - gh->first_db_article);

    buf[size] = 0;		/* make buf a giant string so strchr below
				 * terminates */
    line = buf;
    while ((end = strchr(line, '\n'))) {
	*end++ = 0;		/* make the line a nul terminated string */
	if ((*fcn) (pattern, line))
	    return 1;
	line = end;
    }
    return 0;
}

#endif				/* ART_GREP */
