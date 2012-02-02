/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	nnadmin - nn administator program
 */

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "admin.h"
#include "db.h"
#include "execute.h"
#include "group.h"
#include "nn.h"
#include "proto.h"
#include "nn_term.h"

/* admin.c */

static int      get_cmd(char *prompt1, char *prompt2);
static long     get_entry(char *prompt_str, long min_val, long max_val);
static int      admin_confirm(char *action, int must_confirm);
static char    *get_groupname(void);
static void     update_master(void);
static void     find_files(group_header * gh);
static void     master_status(void);
static void     dump_g_flag(register group_header * gh);
static void     dump_m_entry(register group_header * gh);
static int      validate_group(group_header * gh);
static int      dump_group(group_header * gh);
static void     kill_master(int term);
static void     master_run_check(void);
static void     master_admin(void);
static void     log_admin(void);
static void     flag_admin(group_header * gh, char *mode_str, int set_flag);
static void     rmgroup(group_header * gh);
static void     group_admin(register group_header * gh);
static void     show_config(void);


extern char
               *master_directory, *db_directory, *db_data_directory, *bin_directory, *news_directory,
               *news_lib_directory, *log_file, *news_active, *pager, group_path_name[];

extern char    *exec_chdir_to;

extern int      db_data_subdirs;
extern char     proto_host[];

#ifdef NNTP
extern char    *nntp_server;
#endif

static char    *pre_input;
static int      verbose2 = 1;

static int
get_cmd(char *prompt1, char *prompt2)
{
    char            c;

    if (s_hangup)
	nn_exitmsg(0, "\nnnmaster hangup\n");

    s_keyboard = 0;

    if (pre_input) {
	if ((c = *pre_input++) == NUL)
	    nn_exit(0);
    } else {
	do {
	    if (prompt1)
		tprintf("\r%s\n", prompt1);
	    if (prompt2)
		tprintf("\r%s >>>", prompt2);
	    fl;
	    nn_raw();
	    c = get_c();
	    unset_raw();
	    if (c == K_interrupt)
		s_keyboard++;
	    else if (c == '!') {
		tputc(NL);
		if (exec_chdir_to != NULL)
		    tprintf("\n\rDirectory: %s", exec_chdir_to);
		run_shell((char *) NULL, 0, 0);
	    } else
		tprintf("%c\n\n\r", c);
	} while (c == '!');
    }

    if (islower(c))
	c = toupper(c);

    return c;
}


static long
get_entry(char *prompt_str, long min_val, long max_val)
{
    char            buf[100];
    long            val;

loop:

    tprintf("%s %ld..%ld (or all): ", prompt_str, min_val, max_val);
    fl;
    fgets(buf, sizeof(buf), stdin);
    if (buf[0] == 'a' || buf[0] == NUL)
	return -1L;

    val = atol(buf);
    if (val < min_val || val > max_val)
	goto loop;

    return val;
}


static int
admin_confirm(char *action, int must_confirm)
{
    char            buffer[100];

    if (pre_input && !must_confirm)
	return 1;

    sprintf(buffer, "Confirm %s  Y)es N)o", action);

    return get_cmd((char *) NULL, buffer) == 'Y';
}

static char    *
get_groupname(void)
{
    char           *groupname;

    nn_raw();
    tprintf("\n\n\r");
    prompt_line = Lines - 2;
    prompt("Group: ");
    fl;
    groupname = get_s(NONE, NONE, NONE, group_completion);
    unset_raw();

    tputc(NL);
    tputc(CR);

    if (groupname == NULL)
	return NULL;

    if (groupname[0])
	return groupname;

    if (current_group == NULL)
	return NULL;
    if (current_group->group_flag & G_FOLDER)
	return NULL;

    return current_group->group_name;
}


static void
update_master(void)
{
    register group_header *gh;
    int             ngp;

    if (who_am_i != I_AM_ADMIN) {
	tprintf("Can only perform (U)pdate as nnadmin\n");
	return;
    }
    ngp = master.number_of_groups;

    open_master(OPEN_READ);

    if (master.number_of_groups != ngp) {
	tprintf("\nNumber of groups changed from %d to %d\n",
		ngp, master.number_of_groups);
    }
    ngp = 0;

    Loop_Groups_Header(gh)
	if (update_group(gh) == -1)
	ngp++;

    if (ngp)
	tprintf("There are %d blocked groups\n", ngp);
}

static void
find_files(group_header * gh)
{
    char            command[512], name[FILENAME];

    if (gh == NULL) {
	if (db_data_directory == NULL) {
	    tprintf("Cannot list all files (they are scattered over the news partition)\n");
	    return;
	}
	if (db_data_subdirs)
	    sprintf(command, "cd %s ; ls -l [0-9] | %s", db_data_directory, pager);
	else
	    sprintf(command, "ls -l %s | %s", db_data_directory, pager);
    } else
	sprintf(command, "ls -l %s", db_data_path(name, gh, '*'));
    system(command);
}

static void
master_status(void)
{
    int             nblocked, nignored;
    long            articles1, disk_use;
    register group_header *gh;

    tprintf("\nMaster:\n");
    tprintf("   initialized:   %s\n", date_time(master.db_created));
    tprintf("   last_scan:     %s\n", date_time(master.last_scan));
    tprintf("   last_size:     %ld\n", (long) master.last_size);
    tprintf("   no of groups:  %d\n", master.number_of_groups);

    articles1 = disk_use = 0;
    nblocked = nignored = 0;

    Loop_Groups_Header(gh) {

#define DISK_BLOCKS(bytes) (((bytes) + 1023) / 1024)

	disk_use += DISK_BLOCKS(gh->index_write_offset);
	disk_use += DISK_BLOCKS(gh->data_write_offset);

	articles1 += gh->last_db_article - gh->first_db_article + 1;

	if (gh->master_flag & M_BLOCKED)
	    nblocked++;
	if (gh->master_flag & M_IGNORE_GROUP)
	    nignored++;
    }

    tprintf("\n   Articles:   %ld\n", articles1);
    tprintf("   Disk usage: %ld k\n\n", disk_use);

    if (nblocked)
	tprintf("Blocked groups: %3d\n", nblocked);
    if (nignored)
	tprintf("Ignored groups: %3d\n", nignored);
}

static void
dump_g_flag(register group_header * gh)
{
    tprintf("Flags: ");
    if (gh->master_flag & M_BLOCKED)
	tprintf(" BLOCKED");
    if (gh->master_flag & M_EXPIRE)
	tprintf(" EXPIRE");
    if (gh->master_flag & M_MODERATED)
	tprintf(" MODERATED");
    if (gh->master_flag & M_CONTROL)
	tprintf(" CONTROL");
    if (gh->master_flag & M_NO_DIRECTORY)
	tprintf(" NO_DIRECTORY");
    if (gh->master_flag & M_ALWAYS_DIGEST)
	tprintf(" ALWAYS_DIGEST");
    if (gh->master_flag & M_NEVER_DIGEST)
	tprintf(" NEVER_DIGEST");
    if (gh->master_flag & M_INCLUDE_OLD)
	tprintf(" INCL_OLD");
    if (gh->master_flag & M_AUTO_RECOLLECT)
	tprintf(" RECOLLECT");
    if (gh->master_flag & M_AUTO_ARCHIVE)
	tprintf(" ARCHIVE");
    if (gh->master_flag & M_IGNORE_GROUP)
	tprintf(" IGNORE");
    if (gh->master_flag & M_VALID)
	tprintf(" VALID");
    tprintf("\n");
}

static void
dump_m_entry(register group_header * gh)
{
    update_group(gh);

    tprintf("\n%s\t%d\n", gh->group_name, gh->group_num);
    tprintf("first/last art: %06ld %06d\n",
	    gh->first_db_article, gh->last_db_article);
    tprintf("   active info: %06ld %06d\n",
	    gh->first_a_article, gh->last_a_article);
    tprintf("Offsets: index->%ld, data->%ld\n",
	    gh->index_write_offset,
	    gh->data_write_offset);
    if (gh->master_flag & M_AUTO_ARCHIVE)
	tprintf("Archive file: %s\n", gh->archive_file);
    if (gh->master_flag)
	dump_g_flag(gh);
}

#define valerr( xxx , tp) { \
    if (verbose2) { tprintf xxx ; fl; } \
    err_type = tp; \
    goto err; \
}

static int
validate_group(group_header * gh)
{
    FILE           *data, *ix;
    long            data_offset, next_offset;
    cross_post_number cross_post;
    article_number  cur_article;
    int             n, err_type;

    data = ix = NULL;

    if (gh->master_flag & M_IGNORE_GROUP)
	return 1;

    if (gh->first_db_article == (gh->last_db_article + 1)
	&& gh->index_write_offset == 0)
	return 1;

    if (verbose2) {
	tprintf("\r%s: ", gh->group_name);
	clrline();
    }
    /* init_group returns ok for gh == current_group before check on NO_DIR */
    if ((gh->master_flag & M_NO_DIRECTORY) || init_group(gh) <= 0) {
	if (verbose2)
	    tprintf("NO DIRECTORY for %s (ok)", gh->group_name);
	return 1;		/* no directory/ignored */
    }
    update_group(gh);

    if (gh->master_flag & M_BLOCKED) {
	if (verbose2)
	    tprintf("BLOCKED (ok)\n");
	return 1;
    }

    /*
     * Check for major inconcistencies. Sometimes, news expire will reset
     * article numbers to start from 0 again
     */

    if (gh->last_db_article == 0) {
	return 1;
    }

#ifdef RENUMBER_DANGER
    if (gh->first_a_article > (gh->last_db_article + 1) ||
	gh->last_db_article > gh->last_a_article ||
	gh->first_db_article > gh->first_a_article) {

	if (verbose2)
	    tprintf("RENUMBERING OF ARTICLES (active=%ld..%ld master=%ld..%ld)",
		    gh->first_a_article, gh->last_a_article,
		    gh->first_db_article, gh->last_db_article);
	err_type = 99;
	goto err;
    }
#endif

    ix = open_data_file(gh, 'x', OPEN_READ);
    if (ix == NULL)
	valerr(("NO INDEX FILE"), 1);

    data = open_data_file(gh, 'd', OPEN_READ);
    if (data == NULL)
	valerr(("NO DATA FILE"), 2);

    cur_article = gh->first_db_article - 1;

    while (cur_article <= gh->last_db_article) {
	if (s_hangup || s_keyboard)
	    goto out;

	data_offset = ftell(data);

	switch (db_read_art(data)) {
	    case 0:
		if (data_offset == gh->data_write_offset)
		    goto out;
		valerr(("No header for article # %ld", (long) cur_article + 1), 3);
	    default:
		break;
	    case -1:
		valerr(("END OF FILE on DATA FILE"), 4);
	}

	if (db_hdr.dh_number <= 0 || cur_article > db_hdr.dh_number)
	    valerr(("OUT OF SEQUENCE: %ld after %ld", (long) db_hdr.dh_number, (long) cur_article), 11);

	if (cur_article < db_hdr.dh_number) {
	    if (db_data.dh_type == DH_SUB_DIGEST)
		valerr(("NO DIGEST HEADER: %ld", (long) db_hdr.dh_number), 5);

	    do {
		cur_article++;
		if (!db_read_offset(ix, &next_offset))
		    valerr(("NO INDEX FOR ARTICLE %ld", (long) cur_article), 6);

		if (data_offset != next_offset)
		    valerr(("OFFSET ERROR: %ld: %ld != %ld", (long) cur_article, data_offset, next_offset), 7);
	    } while (cur_article < db_hdr.dh_number);
	}
	for (n = 0; n < (int) db_hdr.dh_cross_postings; n++) {
	    cross_post = NETW_CROSS_INT(db_data.dh_cross[n]);
	    if (cross_post < master.number_of_groups)
		continue;
	    valerr(("CROSS POST RANGE ERROR: %ld (article # %ld)", (long) cross_post, (long) cur_article), 8);
	}
    }

out:
    if (!s_keyboard && !s_hangup) {
	data_offset = ftell(data);
	if (data_offset != gh->data_write_offset)
	    valerr(("DATA OFFSET %ld != %ld", gh->data_write_offset, data_offset), 9);

	while (++cur_article <= gh->last_db_article) {
	    if (!db_read_offset(ix, &next_offset))
		valerr(("NO INDEX FOR ARTICLE %ld", (long) cur_article), 12);
	    if (data_offset != next_offset)
		valerr(("OFFSET ERROR: %ld: %ld != %ld", (long) cur_article, data_offset, next_offset), 13);
	}

	data_offset = ftell(ix);
	if (data_offset != gh->index_write_offset)
	    valerr(("INDEX OFFSET %ld != %ld", gh->index_write_offset, data_offset), 10);
    }
    fclose(data);
    fclose(ix);
    if (verbose2)
	tprintf("OK");
    return 1;

err:
    if (data != NULL)
	fclose(data);
    if (ix != NULL)
	fclose(ix);
    log_entry('V', "%s: database error %d", gh->group_name, err_type);
    if (verbose2) {
	tputc(NL);
	dump_m_entry(gh);
    }
    if (!pre_input) {
	ding();

	for (;;) {
	    switch (get_cmd((char *) NULL,
			    "\nRepair group   Y)es N)o E)nter Q)uit")) {
		case 'Y':
		    break;
		case 'N':
		    return 0;
		case 'Q':
		    s_keyboard++;
		    return 0;
		case 'E':
		    group_admin(gh);
		    continue;
		default:
		    continue;
	    }
	    break;
	}
    }
    send_master(SM_RECOLLECT, gh, SP, 0L);
    return 0;
}

static int
dump_group(group_header * gh)
{
    FILE           *data, *ix;
    long            offset;
    cross_post_number cross_post;
    article_number  first_article;
    int             n;

    if (init_group(gh) <= 0) {
	tprintf("cannot access group %s\n", gh->group_name);
	return 0;
    }
    update_group(gh);

    if (gh->index_write_offset == 0) {
	tprintf("group %s is empty\n", gh->group_name);
	return 1;
    }
    first_article = get_entry("First article",
			      (long) gh->first_db_article,
			      (long) gh->last_db_article);

    if (first_article < 0)
	first_article = gh->first_db_article;
    if (first_article <= 0)
	first_article = 1;

    ix = open_data_file(gh, 'x', OPEN_READ);
    data = open_data_file(gh, 'd', OPEN_READ);
    if (ix == NULL || data == NULL)
	goto err;

    fseek(ix, get_index_offset(gh, first_article), 0);
    if (!db_read_offset(ix, &offset))
	goto err;
    fseek(data, offset, 0);

    clrdisp();
    pg_init(1, 1);

    for (;;) {
	if (s_hangup || s_keyboard)
	    break;

	if (pg_scroll(6)) {
	    s_keyboard = 1;
	    break;
	}
	offset = ftell(data);

	switch (db_read_art(data)) {
	    case 0:
		goto out;
	    default:
		break;
	    case -1:
		goto err;
	}

	tprintf("\noffset = %ld, article # = %ld",
		offset, (long) (db_hdr.dh_number));

	switch (db_data.dh_type) {
	    case DH_DIGEST_HEADER:
		tprintf(" (digest header)\n");
		break;
	    case DH_SUB_DIGEST:
		tprintf(" (digest sub-article)\n");
		break;
	    case DH_NORMAL:
		tputc(NL);
		break;
	}

	if (db_hdr.dh_cross_postings) {
	    tprintf("xpost(%d):", db_hdr.dh_cross_postings);

	    for (n = 0; n < (int) db_hdr.dh_cross_postings; n++) {
		cross_post = NETW_CROSS_INT(db_data.dh_cross[n]);
		tprintf(" %d", cross_post);
	    }
	    tputc(NL);
	}
	tprintf("ts=%lu hp=%ld, fp=+%d, lp=%ld, ref=%d%s, lines=%d\n",
		(long unsigned) db_hdr.dh_date, (long) db_hdr.dh_hpos,
		(int) db_hdr.dh_fpos, (long) db_hdr.dh_lpos,
		db_hdr.dh_replies & 0x7f,
		(db_hdr.dh_replies & 0x80) ? "+Re" : "",
		db_hdr.dh_lines);

	if (db_hdr.dh_sender_length)
	    tprintf("Sender(%d): %s\n", db_hdr.dh_sender_length, db_data.dh_sender);
	else
	    tprintf("No sender\n");

	if (db_hdr.dh_subject_length)
	    tprintf("Subj(%d): %s\n", db_hdr.dh_subject_length, db_data.dh_subject);
	else
	    tprintf("No subject\n");
    }

out:
    if (!s_keyboard && !s_hangup && ftell(data) != gh->data_write_offset)
	goto err;

    fclose(data);
    fclose(ix);
    return 1;

err:
    if (data != NULL)
	fclose(data);
    if (ix != NULL)
	fclose(ix);
    if (gh->master_flag & M_IGNORE_GROUP)
	return 0;
    tprintf("\n*** DATABASE INCONSISTENCY DETECTED - run V)alidate\n");
    return 0;
}

static void
kill_master(int term)
{
    switch (proto_lock(I_AM_MASTER, term ? PL_TERMINATE : PL_WAKEUP_SOFT)) {
	    case 1:
	    if (verbose2)
		tprintf("sent %s signal to master\n", term ? "termination" : "wakeup");
	    break;
	case 0:
	    tprintf("cannot signal master\n");
	    break;
	case -1:
	    if (verbose2)
		tprintf("master is not running\n");
	    break;
    }
}

static void
master_run_check(void)
{
    int             running = proto_lock(I_AM_MASTER, PL_CHECK) >= 0;

    if (!verbose2 && pre_input != NULL)
	exit(running ? 0 : 1);

    if (running)
	tprintf("Master is running%s%s\n",
		proto_host[0] ? " on host " : "", proto_host);
    else
	tprintf("Master is NOT running");
}

static void
master_admin(void)
{
    register char   c;
    int             cur_group;
    long            value;
    register group_header *gh;

    exec_chdir_to = db_directory;

    for (;;) {
	switch (c = get_cmd(
		 "\nC)heck D)ump F)iles G)roup K)ill O)ptions S)tat T)race",
			    "MASTER")) {

	    case 'C':
		master_run_check();
		break;

	    case 'G':
		cur_group = (int) get_entry("Group number",
				  0L, (long) (master.number_of_groups - 1));
		if (cur_group >= 0)
		    dump_m_entry(ACTIVE_GROUP(cur_group));
		break;

	    case 'D':
		do {
		    c = get_cmd(
				"\nA)ll B)locked E)mpty H)oles I)gnored N)on-empty V)alid in(W)alid",
				"DUMP");
		    pg_init(1, 1);

		    Loop_Groups_Header(gh) {
			if (s_keyboard || s_hangup)
			    break;
#define NODUMP(cond) if (cond) continue; break
			switch (c) {
			    case 'N':
				NODUMP(gh->index_write_offset == 0);
			    case 'E':
				NODUMP(gh->index_write_offset != 0);
			    case 'H':
				NODUMP(gh->first_a_article >= gh->first_db_article);
			    case 'B':
				NODUMP((gh->master_flag & M_BLOCKED) == 0);
			    case 'I':
				NODUMP((gh->master_flag & M_IGNORE_GROUP) == 0);
			    case 'V':
				NODUMP((gh->master_flag & M_VALID) == 0);
			    case 'W':
				NODUMP(gh->master_flag & M_VALID);
			    case 'A':
				break;
			    default:
				s_keyboard = 1;
				continue;
			}

			if (pg_scroll(6))
			    break;
			dump_m_entry(gh);
		    }
		} while (!s_keyboard && !s_hangup);
		break;

	    case 'F':
		find_files((group_header *) NULL);
		break;

	    case 'O':
		c = get_cmd("r)epeat_delay  e)xpire_level", "OPTION");
		if (c != 'R' && c != 'E')
		    break;
		value = get_entry("Option value", 1L, 10000L);
		if (value < 0)
		    break;
		send_master(SM_SET_OPTION, (group_header *) NULL, tolower(c), value);
		break;

	    case 'S':
		master_status();
		break;

	    case 'T':
		send_master(SM_SET_OPTION, (group_header *) NULL, 't', -1L);
		break;

	    case 'K':
		if (admin_confirm("Stop nn Master", 0))
		    kill_master(1);
		break;

	    default:
		exec_chdir_to = NULL;
		return;
	}
    }
}


static void
log_admin(void)
{
    char            command[FILENAME + 100], c;

    if (pre_input && *pre_input == NUL) {
	c = SP;
	goto log_tail;
    }
loop:

    c = get_cmd(
		"\n1-9)tail A)dm C)ollect E)rr N)ntp G)roup R)eport e(X)pire .)all @)clean",
		"LOG");

    if (c < SP || c == 'Q')
	return;

    if (c == '@') {
	if (admin_confirm("Truncation", 0)) {
	    sprintf(command, "%s.old", log_file);
	    unlink(command);
	    if (link(log_file, command) < 0)
		goto tr_failed;
	    if (unlink(log_file) < 0) {
		unlink(command);
		goto tr_failed;
	    }
	    log_entry('A', "Log Truncated");
	    chmod(log_file, 0666);
	}
	return;

tr_failed:
	tprintf("Truncation failed -- check permissions etc.\n");
	goto loop;
    }
    if (c == 'G') {
	char           *groupname;

	if ((groupname = get_groupname()) == NULL)
	    goto loop;
	sprintf(command, "fgrep '%s' %s | %s",
		groupname, log_file, pager);
	system(command);

	goto loop;
    }
log_tail:
    if (c == '$' || c == SP || isdigit(c)) {
	int             n;

	n = isdigit(c) ? 10 * (c - '0') : 10;
	sprintf(command, "tail -%d %s", n, log_file);
	system(command);
	goto loop;
    }
    if (c == '*') {
	c = '.';
    }
    sprintf(command, "grep '^%c:' %s | %s", c, log_file, pager);
    system(command);

    goto loop;
}


static void
flag_admin(group_header * gh, char *mode_str, int set_flag)
{
    char            buffer[50];
    int             new_flag = 0;

    tputc(NL);

    dump_g_flag(gh);

    sprintf(buffer, "%s FLAG", mode_str);

    switch (get_cmd(
	      "\nA)lways_digest N)ever_digest M)oderated C)ontrol no_(D)ir",
		    buffer)) {

	default:
	    return;

	case 'M':
	    new_flag = M_MODERATED;
	    break;

	case 'C':
	    new_flag = M_CONTROL;
	    break;

	case 'D':
	    new_flag = M_NO_DIRECTORY;
	    break;

	case 'A':
	    new_flag = M_ALWAYS_DIGEST;
	    break;

	case 'N':
	    new_flag = M_NEVER_DIGEST;
	    break;
    }

    if (new_flag & (M_CONTROL | M_NO_DIRECTORY))
	if (!admin_confirm("Flag Change", 0))
	    new_flag = 0;

    if (new_flag) {
	if (set_flag) {
	    if (gh->master_flag & new_flag)
		new_flag = 0;
	    else {
		send_master(SM_SET_FLAG, gh, 's', (long) new_flag);
		gh->master_flag |= new_flag;
	    }
	} else {
	    if ((gh->master_flag & new_flag) == 0)
		new_flag = 0;
	    else {
		send_master(SM_SET_FLAG, gh, 'c', (long) new_flag);
		gh->master_flag &= ~new_flag;
	    }
	}
    }
    if (new_flag == 0)
	tprintf("NO CHANGE\n");
    else
	dump_g_flag(gh);
}


static void
rmgroup(group_header * gh)
{
    char            command[FILENAME * 2];
    char           *rmprog;

    if (user_id != 0 && !file_exist(news_active, "w")) {
	tprintf("Not privileged to run rmgroup\n");
	return;
    }

#ifdef RMGROUP_PATH
    rmprog = RMGROUP_PATH;
#else
    rmprog = "rmgroup";
#endif

    if (*rmprog != '/')
	rmprog = relative(news_lib_directory, rmprog);
    if (file_exist(rmprog, "x"))
	goto rm_ok;

#ifndef RMGROUP_PATH
    rmprog = relative(news_lib_directory, "delgroup");
    if (file_exist(rmprog, "x"))
	goto rm_ok;
#endif

    tprintf("Program %s not found\n", rmprog);
    return;
rm_ok:
    sprintf(command, "%s %s", rmprog, gh->group_name);
    system(command);
    any_key(0);
    gh->master_flag &= ~M_VALID;/* just for nnadmin */
    gh->master_flag |= M_IGNORE_A;
}

static void
group_admin(register group_header * gh)
{
    char           *groupname, gbuf[FILENAME], dirbuf[FILENAME];

    if (gh != NULL)
	goto have_group;

new_group:
    if ((groupname = get_groupname()) == NULL)
	return;

    if ((gh = lookup_regexp(groupname, "Enter", 0)) == NULL)
	goto new_group;

have_group:
    if (!use_nntp && init_group(gh)) {
	strcpy(dirbuf, group_path_name);
	dirbuf[strlen(dirbuf) - 1] = NUL;
	exec_chdir_to = dirbuf;
    }
    sprintf(gbuf, "GROUP %s", gh->group_name);

    for (;;) {
	switch (get_cmd(
	  "\nD)ata E)xpire F)iles G)roup H)eader R)ecollect V)alidate Z)ap",
			gbuf)) {
	    case 'D':
		dump_group(gh);
		break;

	    case 'V':
		verbose2 = 1;
		validate_group(gh);
		tputc(NL);
		break;

	    case 'H':
		dump_m_entry(gh);
		break;

	    case 'F':
		find_files(gh);
		break;

	    case 'S':
		flag_admin(gh, "Set", 1);
		break;

	    case 'C':
		flag_admin(gh, "Clear", 0);
		break;

	    case 'R':
		if (admin_confirm("Recollection of Group", 0))
		    send_master(SM_RECOLLECT, gh, SP, 0L);
		break;

	    case 'Z':
		if (admin_confirm("Remove Group (run rmgroup)", 1))
		    rmgroup(gh);
		break;

	    case 'E':
		if (admin_confirm("Expire Group", 0))
		    send_master(SM_EXPIRE, gh, SP, 0L);
		break;

	    case 'G':
		goto new_group;

	    default:
		exec_chdir_to = NULL;
		return;
	}
    }
}


static void
show_config(void)
{

    tprintf("\nConfiguration:\n\n");
    tprintf("BIN:  %s\nLIB:  %s\nNEWS SPOOL: %s\nNEWS LIB: %s\n",
	    bin_directory,
	    lib_directory,
	    news_directory,
	    news_lib_directory);

    tprintf("DB:   %s\nDATA: ", db_directory);
    if (db_data_directory != NULL) {

#ifdef DB_LONG_NAMES
	tprintf("%s/{group.name}.[dx]\n", db_data_directory);
#else
	tprintf("%s%s/{group-number}.[dx]\n", db_data_directory,
		db_data_subdirs ? "/[0-9]" : "");
#endif
    } else {
	tprintf("%s/{group/name/path}/.nn[dx]\n", news_directory);
    }

    tprintf("ACTIVE: %s\n", news_active);

#ifdef NNTP
    if (use_nntp)
	tprintf("NNTP ACTIVE. server=%s\n", nntp_server);
    else
	tprintf("NNTP NOT ACTIVE\n");
#endif

#ifdef NETWORK_DATABASE
    tprintf("Database is machine independent (network format).\n");

#ifdef NETWORK_BYTE_ORDER
    tprintf("Local system assumes to use network byte order\n");
#endif

#else
    tprintf("Database format is machine dependent (byte order and alignment)\n");
#endif

    tprintf("Database version: %d\n", NNDB_MAGIC & 0xff);

#ifdef STATISTICS
    tprintf("Recording usage statistics\n");
#else
    tprintf("No usage statistics are recorded\n");
#endif

    tprintf("Mail delivery program: %s\n", REC_MAIL);

#ifdef APPEND_SIGNATURE
    tprintf("Query for appending .signature ENABLED\n");
#else
    tprintf("Query for appending .signature DISABLED\n");
#endif

    tprintf("Max pathname length is %d bytes\n", FILENAME - 1);
}


void
admin_mode(char *input_string)
{
    register group_header *gh;
    int             was_raw = unset_raw();

    if (input_string && *input_string) {
	pre_input = input_string;
    } else {
	pre_input = NULL;
	tputc(NL);
	master_run_check();
    }

    for (;;) {
	switch (get_cmd(
			"\nC)onf E)xpire G)roups I)nit L)og M)aster Q)uit S)tat U)pdate V)alidate W)akeup",
			"ADMIN")) {

	    case 'M':
		master_admin();
		break;

	    case 'W':
		kill_master(0);
		break;

	    case 'E':
		if (admin_confirm("Expire All Groups", 1))
		    send_master(SM_EXPIRE, (group_header *) NULL, SP, 0L);
		break;

	    case 'I':
		if (admin_confirm("INITIALIZATION, i.e. recollect all groups", 1))
		    send_master(SM_RECOLLECT, (group_header *) NULL, SP, 0L);
		break;

	    case 'G':
		group_admin((group_header *) NULL);
		break;

	    case 'L':
		log_admin();
		break;

	    case 'S':
		master_status();
		break;

	    case 'C':
		show_config();
		break;

	    case 'U':
		update_master();
		break;

	    case 'Z':
		verbose2 = 0;

		/* FALLTHROUGH */
	    case 'V':
		Loop_Groups_Sorted(gh) {
		    if (s_hangup || s_keyboard)
			break;
		    validate_group(gh);
		}
		verbose2 = 1;
		break;

	    case '=':
		verbose2 = !verbose2;
		break;

	    case 'Q':
		if (was_raw)
		    nn_raw();
		return;
	}
    }
}
