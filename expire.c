/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Expire will remove all entries in the index and data files
 *	corresponding to the articles before the first article registered
 *	in the active file.  No attempt is made to eliminate other
 *	expired articles.
 */

#include <stdlib.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "db.h"
#include "master.h"
#include "nntp.h"
#include "nn_term.h"

/* expire.c */

static int      sort_art_list(register article_number * f1, register article_number * f2);
static article_number *get_article_list(char *dir);
static long     expire_in_database(register group_header * gh);
static long     expire_sliding(register group_header * gh);
static void     block_group(register group_header * gh);
static void     unblock_group(register group_header * gh);


extern int      trace, debug_mode;
extern int      nntp_failed;

/*
 *	Expire methods:
 *	1: read directory and reuse database info.
 *	2: "slide" index and datafiles (may leave unexpired art. in database)
 *	3: recollect group to expire (also if "min" still exists)
 */

int             expire_method = 1;	/* expire method */
int             recollect_method = 1;	/* recollection method -- see
					 * do_expire */
int             expire_level = 0;	/* automatic expiration detection */

#ifdef HAVE_DIRECTORY
static article_number *article_list = NULL;
static long     art_list_length = 0;

static int
sort_art_list(register article_number * f1, register article_number * f2)
{
    return (*f1 < *f2) ? -1 : (*f1 == *f2) ? 0 : 1;
}

static article_number *
get_article_list(char *dir)
{
    DIR            *dirp;
    register Direntry *dp;
    register char   c, *pp, *cp;
    register article_number *art;
    register long   count;	/* No. of completions plus one */

    if ((dirp = opendir(dir)) == NULL)
	return NULL;		/* tough luck */

    art = article_list;
    count = 0;

    while ((dp = readdir(dirp)) != NULL) {
	cp = dp->d_name;

#ifdef FAKED_DIRECTORY
	if (dp->d_ino == 0)
	    continue;
	cp[14] = NUL;
#endif

	for (pp = cp; (c = *pp++);)
	    if (!isascii(c) || !isdigit(c))
		break;
	if (c)
	    continue;

	if (count == art_list_length) {
	    art_list_length += 250;
	    article_list = resizeobj(article_list, article_number, art_list_length + 1);
	    art = article_list + count;
	}
	*art++ = atol(cp);
	count++;
    }
    closedir(dirp);

    if (article_list != NULL) {
	*art = 0;
	if (count > 1)
	    quicksort(article_list, count, article_number, sort_art_list);
    }
    return article_list;
}

static long
expire_in_database(register group_header * gh)
{
    FILE           *old, *data = NULL, *ix = NULL;
    off_t           old_max_offset;
    register article_number *list;
    article_number  old_last_article;
    long            count;

    if (gh->first_db_article > gh->last_db_article)
	return 0;

    if (gh->last_db_article == 0)
	return 0;

    if (!init_group(gh))
	return 0;

    if (debug_mode == 1) {
	printf("\t\tExp %s (%ld..%ld)\r",
	       gh->group_name, gh->first_db_article, gh->last_db_article);
	fl;
    }
    count = 0;

    /* get list of currently available articles in the group */

#ifdef NNTP
    if (use_nntp)
	list = nntp_get_article_list(gh);
    else
#endif

	list = get_article_list(".");

    if (list == NULL || *list == 0) {

#ifdef NNTP
	if (nntp_failed == 2) {
	    log_entry('N', "NNTP server supports neither LISTGROUP nor XHDR");
	    sys_error("Cannot use specified expire method (NNTP limitations)");
	}
	if (nntp_failed)
	    return -1;
#endif

	if (debug_mode == 1) {
	    printf("\rempty");
	    fl;
	}
	/* group is empty - clean it */
	count = gh->last_db_article - gh->first_db_article + 1;
	clean_group(gh);
	gh->last_db_article = gh->last_a_article;
	gh->first_db_article = gh->last_db_article + 1;
	db_write_group(gh);
	return count;
    }

    /*
     * Clean & block the group while expire is working on it.
     */

    gh->first_db_article = 0;
    old_last_article = gh->last_db_article;
    gh->last_db_article = 0;

    gh->index_write_offset = 0;
    old_max_offset = gh->data_write_offset;
    gh->data_write_offset = 0;

    gh->master_flag &= ~M_EXPIRE;
    gh->master_flag |= M_BLOCKED;

    db_write_group(gh);

    /*
     * We ignore the old index file, and we unlink the data file immediately
     * after open because we want to write a new.
     */

    (void) open_data_file(gh, 'x', -1);
    old = open_data_file(gh, 'd', OPEN_READ | OPEN_UNLINK);
    if (old == NULL)
	goto out;

    data = open_data_file(gh, 'd', OPEN_CREATE | MUST_EXIST);
    ix = open_data_file(gh, 'x', OPEN_CREATE | MUST_EXIST);

    while (ftell(old) < old_max_offset) {
	if (s_hangup) {
	    /* ok, this is what we got -- let collect get the rest */
	    old_last_article = gh->last_db_article;
	    break;
	}

	/*
	 * maybe not enough articles, or last one is incomplete we take what
	 * there is, and leave the rest to do_collect(void) It may actually
	 * be ok if the last articles have been expired/ cancelled!
	 */

	if (db_read_art(old) <= 0) {
	    break;
	}
	if (debug_mode == 1) {
	    printf("\r%ld", (long) db_hdr.dh_number);
	    fl;
	}
	/* check whether we want this article */

	while (*list && db_hdr.dh_number > *list) {
	    /* potentially, we have a problem here: there might be an */
	    /* article in the directory which is not in the database! */
	    /* For the moment we just ignore this - it might be a bad */
	    /* article which has been rejected!! */
	    list++;
	}

	if (*list == 0) {
	    /* no more articles in the directory - the rest must be */
	    /* expired.  So we ignore the rest of the data file */
	    break;
	}
	if (db_hdr.dh_number < *list) {
	    /* the current article from the data file isn't in the */
	    /* article list, so it must be expired! */
	    count++;
	    if (debug_mode == 1) {
		printf("\t%ld", count);
		fl;
	    }
	    continue;
	}
	if (gh->first_db_article == 0) {
	    gh->first_db_article = db_hdr.dh_number;
	    gh->last_db_article = db_hdr.dh_number - 1;
	}
	if (gh->last_db_article < db_hdr.dh_number) {
	    gh->data_write_offset = ftell(data);

	    /* must fill gab between last index and current article */
	    while (gh->last_db_article < db_hdr.dh_number) {
		if (!db_write_offset(ix, &(gh->data_write_offset)))
		    write_error();
		gh->last_db_article++;
	    }
	}
	if (db_write_art(data) < 0)
	    write_error();
    }

    if (gh->first_db_article == 0) {
	gh->first_db_article = old_last_article + 1;
	gh->last_db_article = old_last_article;
    } else {
	gh->data_write_offset = ftell(data);
	while (gh->last_db_article < old_last_article) {
	    /* must fill gab between last index and last article */
	    ++gh->last_db_article;
	    if (!db_write_offset(ix, &(gh->data_write_offset)))
		write_error();
	}
	gh->index_write_offset = ftell(ix);
    }

    gh->master_flag &= ~M_BLOCKED;

    db_write_group(gh);

out:
    if (old)
	fclose(old);
    if (data)
	fclose(data);
    if (ix)
	fclose(ix);

    if (debug_mode)
	putchar(NL);

    return count;
}

#else
#define expire_in_database expire_sliding
#endif

static long
expire_sliding(register group_header * gh)
{
    FILE           *old_x, *old_d;
    FILE           *new;
    long            data_offset, new_offset;
    long            count, expire_count, index_offset;
    char           *err_message;

#define expire_error(msg) { \
    err_message = msg; \
    goto error_handler; \
}

    if (!init_group(gh))
	return 0;

#ifdef RENUMBER_DANGER

    /*
     * check whether new first article is collected
     */

    if (!art_collected(gh, gh->first_a_article)) {
	expire_count = gh->first_db_article - gh->last_db_article + 1;
	err_message = NULL;
	goto error_handler;	/* renumbering, collect from start */
    }
#else
    if (gh->first_a_article <= gh->first_db_article)
	return 0;
#endif

    expire_count = gh->first_a_article - gh->first_db_article;

    new = NULL;

    /*
     * Open old files, unlink after open
     */

    old_x = open_data_file(gh, 'x', OPEN_READ | OPEN_UNLINK);
    old_d = open_data_file(gh, 'd', OPEN_READ | OPEN_UNLINK);

    if (old_x == NULL || old_d == NULL)
	expire_error("INDEX or DATA file missing");

    /*
     * Create new index file; copy from old
     */

    new = open_data_file(gh, 'x', OPEN_CREATE);
    if (new == NULL)
	expire_error("INDEX: cannot create");

    /*
     * index_offset is the offset into the old index file for the first entry
     * in the new index file
     */

    index_offset = get_index_offset(gh, gh->first_a_article);

    /*
     * adjust the group's index write offset (the next free entry)
     */

    gh->index_write_offset -= index_offset;

    /*
     * calculate the number of entries to copy
     */

    count = gh->index_write_offset / sizeof(long);

    /*
     * data offset is the offset into the old data file for the first byte in
     * the new data file; it is initialized in the loop below, by reading the
     * entry in the old index file at offset 'index_offset'.
     */

    data_offset = (off_t) 0;

    /*
     * read 'count' entries from the old index file starting from
     * index_offset, subtract the 'data_offset', and output the new offset to
     * the new index file.
     */

    fseek(old_x, index_offset, 0);

    while (--count >= 0) {
	if (!db_read_offset(old_x, &new_offset))
	    expire_error("INDEX: too short");

	if (data_offset == 0)
	    data_offset = new_offset;

	new_offset -= data_offset;
	if (!db_write_offset(new, &new_offset))
	    expire_error("NEW INDEX: cannot write");
    }

    fclose(new);
    fclose(old_x);
    old_x = NULL;

    /*
     * copy from old data file to new data file
     */

    new = open_data_file(gh, 'd', OPEN_CREATE);
    if (new == NULL)
	expire_error("DATA: cannot create");

    /*
     * calculate offset for next free entry in the new data file
     */

    gh->data_write_offset -= data_offset;

    /*
     * calculate number of bytes to copy (piece of cake)
     */

    count = gh->data_write_offset;

    /*
     * copy 'count' bytes from the old data file, starting at offset
     * 'data_offset', to the new data file
     */

    fseek(old_d, data_offset, 0);
    while (count > 0) {
	char            block[1024];
	int             count1;

	count1 = fread(block, sizeof(char), 1024, old_d);
	if (count1 <= 0)
	    expire_error("DATA: read error");

	if (fwrite(block, sizeof(char), count1, new) != count1)
	    expire_error("DATA: write error");

	count -= count1;
    }

    fclose(new);
    fclose(old_d);

    /*
     * Update group entry
     */

    gh->first_db_article = gh->first_a_article;

    /*
     * Return number of expired articles
     */

    return expire_count;

    /*
     * Errors end up here. We simply recollect the whole group once more.
     */

error_handler:

    if (new)
	fclose(new);
    if (old_x)
	fclose(old_x);
    if (old_d)
	fclose(old_d);

    if (err_message)
	log_entry('E', "Expire Error (%s): %s", gh->group_name, err_message);

    clean_group(gh);

    /* will be saved & unblocked later */

    /*
     * We cannot say whether any articles actually had to be expired, but
     * then we must guess...
     */

    return expire_count;
}

static void
block_group(register group_header * gh)
{
    if ((gh->master_flag & M_BLOCKED) == 0) {
	gh->master_flag |= M_BLOCKED;
	db_write_group(gh);
    }
}

static void
unblock_group(register group_header * gh)
{
    if (gh->master_flag & M_BLOCKED) {
	gh->master_flag &= ~(M_BLOCKED | M_EXPIRE);
	db_write_group(gh);
    }
}

int
do_expire(void)
{
    register group_header *gh;
    long            exp_article_count, temp;
    int             exp_group_count, must_expire;
    time_t          start_time;

    must_expire = 0;

    Loop_Groups_Header(gh) {
	if (s_hangup)
	    break;
	if (gh->master_flag & M_IGNORE_GROUP)
	    continue;

	if ((gh->master_flag & M_VALID) == 0) {
	    log_entry('X', "Group %s removed", gh->group_name);
	    gh->master_flag |= M_IGNORE_A;
	    clean_group(gh);
	    continue;
	}

#ifdef RENUMBER_DANGER
	if (gh->last_db_article > gh->last_a_article ||
	    gh->first_db_article > gh->first_a_article) {
	    log_entry('X', "group %s renumbered", gh->group_name);
	    clean_group(gh);
	    continue;
	}
#endif

	if (gh->master_flag & M_AUTO_RECOLLECT) {
	    switch (recollect_method) {
		case 1:	/* expire when new articles arrive */
		    if (gh->last_a_article <= gh->last_db_article)
			break;
		    /* FALLTHRU */
		case 2:	/* expire unconditionally */
		    gh->master_flag |= M_EXPIRE;
		    must_expire = 1;
		    continue;

		case 3:	/* clean when new articles arrive */
		    if (gh->last_a_article <= gh->last_db_article)
			break;
		    /* FALLTHRU */
		case 4:	/* clean unconditionally */
		    gh->master_flag |= M_MUST_CLEAN;
		    continue;
		default:	/* ignore auto-recollect */
		    break;
	    }
	}
	if (gh->index_write_offset > 0) {
	    if (gh->first_a_article > gh->last_db_article) {
		if (trace)
		    log_entry('T', "%s expire void", gh->group_name);
		if (debug_mode)
		    printf("%s expire void\n", gh->group_name);
		clean_group(gh);
		continue;
	    }
	}
	if (gh->master_flag & M_EXPIRE) {
	    must_expire = 1;
	    continue;
	}
	if (expire_level > 0 &&
	    (gh->first_db_article + expire_level) <= gh->first_a_article) {
	    if (trace)
		log_entry('T', "%s expire level", gh->group_name);
	    if (debug_mode)
		printf("Expire level: %s\n", gh->group_name);
	    gh->master_flag |= M_EXPIRE;
	    must_expire = 1;
	    continue;
	}
    }

    if (!must_expire)
	return 1;

    start_time = cur_time();
    exp_article_count = exp_group_count = 0;
    temp = 0;

    Loop_Groups_Header(gh) {
	if (s_hangup) {
	    temp = -1;
	    break;
	}
	if (gh->master_flag & M_IGNORE_GROUP)
	    continue;
	if ((gh->master_flag & M_EXPIRE) == 0)
	    continue;
	if (gh->master_flag & M_MUST_CLEAN)
	    continue;

	if (trace)
	    log_entry('T', "Exp %s (%ld -> %ld)", gh->group_name,
		   (long) gh->first_db_article, (long) gh->first_a_article);

	switch (expire_method) {
	    case 1:
		block_group(gh);
		temp = expire_in_database(gh);
		unblock_group(gh);
		break;

	    case 2:
		block_group(gh);
		temp = expire_sliding(gh);
		unblock_group(gh);
		break;

	    case 4:
		temp = gh->first_a_article - gh->first_db_article;
		if (temp <= 0)
		    break;
		/* FALLTHRU */
	    case 3:
		gh->master_flag |= M_MUST_CLEAN;
		break;

	}

#ifdef NNTP
	if (nntp_failed) {
	    /* connection broken while reading article list */
	    /* so no harm was done - leave the group to be expired */
	    /* again on next expire */
	    break;
	}
#endif

	if (temp > 0) {
	    exp_article_count += temp;
	    exp_group_count++;
	}
    }

    if (exp_article_count > 0)
	log_entry('X', "Expire: %ld art, %d gr, %ld s",
		  exp_article_count, exp_group_count,
		  (long) (cur_time() - start_time));

    return temp >= 0;
}
