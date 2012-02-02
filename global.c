/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Global declarations and auxiliary functions.
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <pwd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "config.h"
#include "global.h"
#include "execute.h"
#include "nn.h"
#include "patchlevel.h"
#include "nn_term.h"

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif				/* HAVE_SYSLOG */

/* global.c */

static sig_type catch_keyboard(int n);
static sig_type catch_pipe(int n);
static sig_type catch_suspend(int n);
static void     mail_sys_error(char *err, int isfatal);
static void     mem_error(int t, int32 bytes);

#ifdef NEED_STRERROR
char           *strerror(int);
#endif

char           *home_directory;
char           *nn_directory;	/* ~/.nn as default */
char           *news_directory;	/* /usr/spool/news */
char           *news_lib_directory;	/* /usr/lib/news */
char           *lib_directory;	/* /usr/local/lib/nn */
char           *master_directory;	/* = lib */
char           *help_directory;	/* = lib/help */
char           *bin_directory = BIN_DIRECTORY;

char           *db_directory;	/* /usr/spool/nn or NEWS_DIR/.nn */
char           *db_data_directory;	/* ..../DATA	 or undefined	 */
int             db_data_subdirs = 0;	/* set if DATA/[0-9]/ exist	 */

char           *news_active;	/* NLIB/active or DB/ACTIVE */

char           *pager;
char           *organization;

char           *log_file;	/* = lib/Log */
char           *log_entry_filter = NULL;

char           *temp_file;

#ifndef TMP_DIRECTORY
#define TMP_DIRECTORY "/var/tmp"
#endif

char           *tmp_directory = TMP_DIRECTORY;

char            version_id[100];

int             use_nntp = 0;	/* bool: t iff we use nntp */

uid_t           user_id;
static gid_t    group_id;
int             process_id;
int             who_am_i;
int             dont_write_console = 0;
int             mail_errors_mode = 2;

extern int      errno;
extern char    *getlogin(), *getenv();


#ifdef HAVE_MULTIGROUP

#ifndef NGROUPS
#include <sys/param.h>
#endif

static uid_t    user_eid;
static int      ngroups;
static gid_t    gidset[NGROUPS];

static int
in_grplist(gid_t gid)
{
    int             n;

    if (gid == group_id)
	return 1;

    for (n = 0; n < ngroups; ++n)
	if (gidset[n] == gid)
	    return 1;

    return 0;
}

#define group_access(gpid)	in_grplist(gpid)
#else
#define group_access(gid)	((gid) == group_id)
#endif

/* signal handler interface */

int             s_hangup = 0;	/* hangup signal */
int             s_keyboard = 0;	/* keyboard interrupt */
int             s_pipe = 0;	/* broken pipe */
int             s_redraw = 0;	/* redraw signal (if job control) */

#ifdef RESIZING
int             s_resized = 0;	/* screen resized */
#endif

#ifdef FAKE_INTERRUPT
#include <setjmp.h>

jmp_buf         fake_keyb_sig;
int             arm_fake_keyb_sig = 0;
char            fake_keyb_siglist[NSIG];
#endif

sig_type
catch_hangup(int n)
{
    s_hangup = 1;
    signal(n, SIG_IGN);

#ifdef FAKE_INTERRUPT
    if (fake_keyb_siglist[n] && arm_fake_keyb_sig)
	longjmp(fake_keyb_sig, 1);
#endif
}

static          sig_type
catch_keyboard(int n)
{

#ifdef RESET_SIGNAL_WHEN_CAUGHT
    signal(n, catch_keyboard);
#endif

#ifdef FAKE_INTERRUPT
    if (fake_keyb_siglist[n] && arm_fake_keyb_sig)
	longjmp(fake_keyb_sig, 1);
#endif

    s_keyboard++;
}

static          sig_type
catch_pipe(int n)
{
    s_pipe++;

#ifdef RESET_SIGNAL_WHEN_CAUGHT
    signal(n, catch_pipe);
#endif

#ifdef FAKE_INTERRUPT
    if (fake_keyb_siglist[n] && arm_fake_keyb_sig)
	longjmp(fake_keyb_sig, 1);
#endif
}

#ifdef HAVE_JOBCONTROL
static          sig_type
catch_suspend(int n)
{
    s_redraw++;

#ifdef RESET_SIGNAL_WHEN_CAUGHT
    signal(n, catch_suspend);
#endif

    suspend_nn();

#ifdef FAKE_INTERRUPT
    if (fake_keyb_siglist[n] && arm_fake_keyb_sig) {

#ifdef RESIZING
	s_resized++;
#endif

	longjmp(fake_keyb_sig, 1);
    }
#endif
}

#endif


int
init_global(void)
{

#ifdef NOV
    char           *nov_id = " (NOV)";
#else
    char           *nov_id = "";
#endif

#ifdef FAKE_INTERRUPT
    for (i = 0; i < NSIG; i++)
	fake_keyb_siglist[i] = i == 2 ? 1 : 0;
#endif

    if (who_am_i != I_AM_NN) {
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
    }
    signal(SIGTERM, catch_hangup);
    signal(SIGHUP, catch_hangup);
    signal(SIGPIPE, catch_pipe);
    signal(SIGALRM, SIG_IGN);

#ifdef SIGPWR
    signal(SIGPWR, catch_hangup);
#endif

#ifdef CONFIG_NUM_IN_VERSION
    sprintf(version_id, "%s.%s #%d%s", RELEASE, PATCHLEVEL,
#include "update.h"
	    ,nov_id);
#else
    sprintf(version_id, "%s.%s%s", RELEASE, PATCHLEVEL, nov_id);
#endif

    user_id = getuid();

#ifdef HAVE_MULTIGROUP
    ngroups = getgroups(NGROUPS, gidset);	/* Get users's group set */
#endif

    group_id = getegid();
    user_eid = geteuid();

    process_id = getpid();

#ifdef CLIENT_DIRECTORY
    lib_directory = CLIENT_DIRECTORY;
#else
    lib_directory = LIB_DIRECTORY;
#endif

#ifdef NEWS_DIRECTORY
    news_directory = NEWS_DIRECTORY;
#else
    news_directory = "/usr/spool/news";
#endif

#ifdef DB_DIRECTORY
    db_directory = DB_DIRECTORY;
#else
    db_directory = mk_file_name(lib_directory, ".nn");
#endif

#ifdef ACCOUNTING
    if (who_am_i == I_AM_ACCT)
	return 0;
#endif

#ifdef DB_DATA_DIRECTORY
    db_data_directory = DB_DATA_DIRECTORY;
#else

#ifdef DB_DIRECTORY
    db_data_directory = mk_file_name(db_directory, "DATA");
#else
    db_data_directory = NULL;
#endif

#endif

#ifndef DB_LONG_NAMES
    if (db_data_directory != NULL)
	db_data_subdirs = file_exist(relative(db_data_directory, "0"), "dx");
#endif

#ifdef NEWS_LIB_DIRECTORY
    news_lib_directory = NEWS_LIB_DIRECTORY;
#else
    news_lib_directory = "/usr/lib/news";
#endif

    /* this may later be changed by nntp_check */
    news_active = mk_file_name(news_lib_directory, "active");

#ifdef MASTER_DIRECTORY
    master_directory = MASTER_DIRECTORY;
#else
    master_directory = LIB_DIRECTORY;
#endif

#ifdef HELP_DIRECTORY
    help_directory = HELP_DIRECTORY;
#else
    help_directory = mk_file_name(lib_directory, "help");
#endif

#ifdef LOG_FILE
    log_file = LOG_FILE;
#else
    log_file = mk_file_name(LIB_DIRECTORY, "Log");
#endif

    if (who_am_i == I_AM_MASTER || who_am_i == I_AM_SPEW)
	return 0;

    signal(SIGINT, catch_keyboard);
    signal(SIGQUIT, catch_keyboard);

#ifdef HAVE_JOBCONTROL
    signal(SIGTSTP, catch_suspend);
#endif

    if ((home_directory = getenv("HOME")) == NULL)
	nn_exitmsg(1, "No HOME environment variable");

    if ((pager = getenv("PAGER")) == NULL)
	pager = DEFAULT_PAGER;

    if (!nn_directory)		/* if not set on command line */
	nn_directory = mk_file_name(home_directory, ".nn");

    organization = getenv("ORGANIZATION");

    return 0;
}

int
init_global2(void)
{
    if (who_am_i != I_AM_ADMIN && !file_exist(nn_directory, "drwx")) {
	if (who_am_i != I_AM_NN)
	    return -1;
	if (mkdir(nn_directory, 0755) < 0)
	    nn_exitmsg(1, "Cannot create %s directory", nn_directory);
	return 1;
    }
    return 0;
}

void
new_temp_file(void)
{
    static char     buf[FILENAME];
    static char    *temp_dir = NULL;

    if (temp_dir == NULL) {
	if ((temp_dir = getenv("TMPDIR")) == NULL)
	    temp_dir = tmp_directory;	/* just to make test above false */
	else
	    tmp_directory = temp_dir;
    }

    sprintf(buf, "%s/nn.XXXXXX", tmp_directory);
    mktemp(buf);
    temp_file = buf;
}


FILE           *
open_file(char *name, int mode)
{
    FILE           *f = NULL;
    int             fd;

    if ((mode & DONT_CREATE) && !file_exist(name, (char *) NULL))
	return NULL;

    switch (mode & 0x0f) {

	case OPEN_READ:

	    f = fopen(name, "r");
	    break;

	case OPEN_UPDATE:

/*	f = fopen(name, "r+");	-- not reliable on many systems (sigh) */

	    if ((fd = open(name, O_WRONLY)) >= 0) {
		if ((f = fdopen(fd, "w")) != NULL)
		    return f;
		close(fd);
	    }
	    /* FALLTHROUGH */
	case OPEN_CREATE:

	    f = fopen(name, "w");
	    break;

	case OPEN_APPEND:

	    f = fopen(name, "a");
	    break;

	case OPEN_CREATE_RW:

	    f = fopen(name, "w+");	/* not safe on all systems -- beware */
	    break;

	default:

	    sys_error("Illegal mode: open_file(%s, 0x%x)", name, mode);
    }

    if (f) {
	if (mode & OPEN_UNLINK)
	    unlink(name);
	return f;
    }
    if ((mode & MUST_EXIST) == 0)
	return NULL;

    sys_error("Cannot open file %s, mode=0x%x, errno=%d", name, mode, errno);

    return NULL;
}

FILE           *
open_file_search_path(char *name, int mode)
{
    FILE           *f;

    if (name == NULL)
	return NULL;

    if (*name == '/')
	return open_file(name, mode);

    f = NULL;

    if (!use_nntp)
	f = open_file(relative(news_lib_directory, name), OPEN_READ);
    if (f == NULL)
	f = open_file(relative(lib_directory, name), OPEN_READ);
    if (f == NULL)
	f = open_file(relative(db_directory, name), OPEN_READ);

    return f;
}

int
fgets_multi(char *buf, int size, register FILE * f)
{
    register char  *s = buf;
    register int    c, n = size;

    while (--n > 0) {
	c = getc(f);
	if (c == '\\') {
	    if ((c = getc(f)) == NL)
		continue;
	    *s++ = '\\';
	    if (--n < 0)
		break;
	}
	if (c == EOF) {
	    *s = NUL;
	    return s != buf;
	}
	if (c == NL) {
	    *s = NUL;
	    return 1;
	}
	*s++ = c;
    }
    buf[30] = NUL;
    sys_error("Line too long \"%s...\" (max %d)", buf, size);
    return 0;
}

/*
 *	relative -- concat directory name and file name
 */

char           *
relative(char *dir, char *name)
{
    static char     concat_path[FILENAME];

    sprintf(concat_path, "%s/%s", dir, name);
    return concat_path;
}


char           *
mk_file_name(char *dir, char *name)
{
    char           *buf;

    buf = newstr(strlen(dir) + strlen(name) + 2);
    sprintf(buf, "%s/%s", dir, name);

    return buf;
}


char           *
home_relative(char *dir)
{
    if (dir) {
	if (*dir == '/')
	    return copy_str(dir);
	else {
	    if (*dir == '~' && *++dir == '/')
		dir++;
	    return mk_file_name(home_directory, dir);
	}
    }
    return NULL;
}


char           *
substchr(char *str, char c1, char c2)
{
    char           *p;

    if ((p = strchr(str, c1)) != NULL)
	*p = c2;
    return p;
}

char           *
copy_str(char *str)
{
    char           *new;

    new = newstr(strlen(str) + 1);
    if (new)
	strcpy(new, str);

    return new;
}

time_t
m_time(FILE * f)
{
    struct stat     st;

    if (fstat(fileno(f), &st) < 0)
	return 0;
    return st.st_mtime;
}


time_t
file_exist(char *name, char *mode)
{
    static struct stat statb;
    int             mask;

    if (name != NULL && stat(name, &statb))
	return 0;

    if (mode == NULL)
	return statb.st_mtime;

    if (statb.st_uid == user_eid)
	mask = 0700;
    else if (group_access(statb.st_gid))
	mask = 0070;
    else
	mask = 0007;

    while (*mode) {
	switch (*mode++) {
	    case 'd':
		if ((statb.st_mode & S_IFMT) == S_IFDIR)
		    continue;
		errno = ENOTDIR;
		return 0;
	    case 'f':
		if ((statb.st_mode & S_IFMT) == S_IFREG)
		    continue;
		if ((statb.st_mode & S_IFMT) == 0000000)
		    continue;
		if ((statb.st_mode & S_IFMT) == S_IFDIR) {
		    errno = EISDIR;
		    return 0;
		}
		break;
	    case 'r':
		if (statb.st_mode & mask & 0444)
		    continue;
		break;
	    case 'w':
		if (statb.st_mode & mask & 0222)
		    continue;
		break;
	    case 'x':
		if (statb.st_mode & mask & 0111)
		    continue;
		break;
	}
	errno = EACCES;
	return 0;
    }

    /* all modes are ok */
    return statb.st_mtime;
}

/*
 *	cmp_file: compare two files
 *
 *	Returns		0: The files are identical.
 *			1: The files are different.
 *			2: An error occurred.
 */

int
cmp_file(char *work, char *copy)
{
    int             fd1, fd2;
    struct stat     sb1, sb2;
    u_char         *p1, *p2;
    off_t           len1, len2;

    if ((fd1 = open(work, O_RDONLY, 0)) < 0) {
	msg("%s %s", work, strerror(errno));
	return (2);
    }
    if ((fd2 = open(copy, O_RDONLY, 0)) < 0) {
	msg("%s %s", copy, strerror(errno));
	return (2);
    }
    if (fstat(fd1, &sb1)) {
	msg("%s %s", work, strerror(errno));
	return (2);
    }
    if (fstat(fd2, &sb2)) {
	msg("%s %s", copy, strerror(errno));
	return (2);
    }
    len1 = sb1.st_size;
    len2 = sb2.st_size;

    if (len1 != len2)
	return (1);

    if ((p1 = (u_char *) mmap(NULL, (size_t) len1, PROT_READ, MAP_SHARED, fd1, (off_t) 0)) == (u_char *) - 1) {
	msg("work=%s %s", work, strerror(errno));
	sleep(5);
	return (2);
    }
    if ((p2 = (u_char *) mmap(NULL, (size_t) len2, PROT_READ, MAP_SHARED, fd2, (off_t) 0)) == (u_char *) - 1) {
	msg("copy=%s %s", copy, strerror(errno));
	sleep(5);
	return (2);
    }
    for (; len1--; ++p1, ++p2) {
	if (*p1 != *p2)
	    return (1);
    }
    return (0);
}

/*
 * copy_file: copy (or append) src file to dest file.
 *
 * Returns number of characters copied or an error code:
 *  -1: source file not found
 *  -2: cannot create destination
 *  -3: write error
 */

int
copy_file(char *src, char *dest, int append)
{
    register FILE  *s, *d;
    register int    n;
    register int    c;

    s = open_file(src, OPEN_READ);
    if (s == NULL)
	return -1;

    d = open_file(dest, append ? OPEN_APPEND : OPEN_CREATE);
    if (d == NULL) {
	fclose(s);
	return -2;
    }
    n = 0;
    while ((c = getc(s)) != EOF) {
	putc(c, d);
	n++;
    }

    fclose(s);
    if (fclose(d) == EOF) {
	if (!append)
	    unlink(dest);
	return -3;
    }
    return n;
}

/*
 * move_file: move old file to new file, linking if possible.
 *
 * The third arg determines what is acceptable if the old file cannot be
 * removed after copying to the new file:
 *   0: must remove old, else remove new and fail,
 *   1: must remove or truncate old, else remove new and fail,
 *   2: just leave old if it cannot be removed or truncated.
 *
 * Returns positive value for success, negative for failure:
 *   0: file renamed (link)
 *   1: file copied, old removed
 *   2: file copied, but old file is only truncated.
 *   3: file copied, but old file still exist.
 *  -1: source file not found
 *  -2: cannot create destination
 *  -3: write error
 *  -4: cannot unlink/truncate old
 *  -5: cannot unlink new
 *  -6: cannot link old to new
 *  -9: messy situation: old and new linked on return (cannot happen?)
 */

int
move_file(char *old, char *new, int may_keep_old)
{
    int             n;

    if (file_exist(new, (char *) NULL)) {
	if (file_exist((char *) NULL, "d"))
	    return -5;
	if (unlink(new) < 0)	/* careful - new may be directory ? */
	    switch (errno) {
		case ENOENT:
		    break;
		case EACCES:
		    if (file_exist((char *) NULL, "w"))
			goto do_copy;
		default:
		    return -5;
	    }
    }
    if (link(old, new) < 0)
	switch (errno) {
	    case EACCES:	/* can just as well try to copy */
	    case EXDEV:
		goto do_copy;
	    default:
		return -6;
	}

    if (unlink(old) == 0)
	return 0;

    /* we were able to link but not unlink old	 */
    /* remove new, and attempt a copy instead	 */
    if (unlink(new) < 0)
	return -9;		/* cannot happen? */

do_copy:
    if ((n = copy_file(old, new, 0)) < 0)
	return n;
    if (unlink(old) == 0)
	return 1;
    if (may_keep_old)
	if (n == 0 || nn_truncate(old, (off_t) 0) == 0)
	    return 2;
    if (may_keep_old == 2)
	return 3;
    unlink(new);
    return -4;
}

int
save_old_file(char *name, char *suffix)
{
    char            buf[FILENAME];
    sprintf(buf, "%s%s", name, suffix);
    return move_file(name, buf, 0);
}


static int
enter_log(int type, char *fmt, va_list ap)
{
    FILE           *log;
    char            buf[512];

    vsprintf(buf, fmt, ap);

    /* cannot use relative: one of the args may be generated by it */

    log = open_file(log_file, OPEN_APPEND);
    if (log == NULL)
	return 0;

    fprintf(log, "%c: %s (%s): %s\n", type,
	    date_time((time_t) 0), user_name(), buf);

    fclose(log);
    return 1;
}

static void
mail_sys_error(char *err, int isfatal)
{
    FILE           *f;
    char            cmd[FILENAME * 2];

    switch (mail_errors_mode) {
	case 0:
	    return;
	case 1:
	    if (!isfatal)
		return;
	default:
	    break;
    }

#ifdef FATAL_ERROR_MAIL_CMD
    strcpy(cmd, FATAL_ERROR_MAIL_CMD);
#else

#ifdef MAILX
    sprintf(cmd, "%s -s 'nnmaster %s' %s", MAILX,
	    isfatal ? "fatal error" : "warning", OWNER);
#else
    sprintf(cmd, "mail %s", OWNER);
#endif

#endif

    if ((f = popen(cmd, "w")) == NULL)
	return;
    fprintf(f, "nnmaster %s\n\n%system error:\n%s\n",
	    isfatal ? "terminated" : "warning",
	    isfatal ? "Fatal s" : "S", err);
    pclose(f);
}

void
sys_error(char *fmt,...)
{
    va_list         ap;
    int             rval;
    char            buf[512];

#ifndef HAVE_SYSLOG
    FILE           *f;
#endif

    va_start(ap, fmt);
    rval = enter_log('E', fmt, ap);
    va_end(ap);

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    if (who_am_i == I_AM_MASTER) {
	mail_sys_error(buf, 1);
	if (dont_write_console)
	    nn_exit(7);

#ifndef HAVE_SYSLOG
	f = open_file("/dev/console", OPEN_CREATE);
	if (f == NULL)
	    nn_exit(8);
	fprintf(f, "\n\rNNMASTER FATAL ERROR\n\r%s\n\n\r", buf);
	fclose(f);
#else				/* HAVE_SYSLOG */
	openlog("nnmaster", LOG_CONS, LOG_DAEMON);
	syslog(LOG_ALERT, "%s", buf);
	closelog();
#endif				/* HAVE_SYSLOG */

	nn_exit(7);
    }
    if (rval)
	nn_exitmsg(1, "%s", buf);
    else
	nn_exitmsg(1, "Can't open log file!\n%s", buf);
}

/*
 *	sys_warning: like sys_error but MASTER will return with -1
 *	instead of exit.  Clients still terminate!
 */

int
sys_warning(char *fmt,...)
{
    va_list         ap;
    int             rval;
    char            buf[512];

#ifndef HAVE_SYSLOG
    FILE           *f;
#endif

    static char    *last_err = NULL;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    if (last_err != NULL) {
	if (strcmp(last_err, buf) == 0)
	    return -1;
	free(last_err);
    }
    last_err = copy_str(buf);

    va_start(ap, fmt);
    rval = enter_log('R', fmt, ap);
    va_end(ap);

    if (who_am_i != I_AM_MASTER) {
	if (rval)
	    nn_exitmsg(1, "%s", buf);
	else
	    nn_exitmsg(1, "Can't open log file!\n%s", buf);
    }
    mail_sys_error(buf, 0);
    if (dont_write_console)
	return -1;

#ifndef HAVE_SYSLOG
    if ((f = open_file("/dev/console", OPEN_CREATE)) != NULL) {
	fprintf(f, "\n\rNNMASTER WARNING\n\r%s\n\n\r", buf);
	fclose(f);
    }
#else				/* HAVE_SYSLOG */
    openlog("nnmaster", LOG_CONS, LOG_DAEMON);
    syslog(LOG_ALERT, "%s", buf);
    closelog();
#endif				/* HAVE_SYSLOG */

    return -1;
}

int
log_entry(int type, char *fmt,...)
{
    va_list         ap;
    int             rval;

    va_start(ap, fmt);
    rval = enter_log(type, fmt, ap);
    va_end(ap);

    return rval;
}

char           *
user_name(void)
{
    static char    *user = NULL;
    struct passwd  *pw;

    if (who_am_i == I_AM_MASTER)
	return "M";
    if (who_am_i == I_AM_EXPIRE)
	return "X";

    if (user == NULL) {
	user = getlogin();
	if (user != NULL && *user != NUL)
	    goto out;

	pw = getpwuid(user_id);
	if (pw != NULL && pw->pw_name[0] != NUL) {
	    user = copy_str(pw->pw_name);
	    goto out;
	}
	user = getenv("LOGNAME");
	if (user != NULL && *user != NUL)
	    goto out;
	user = getenv("USER");
	if (user != NULL && *user != NUL)
	    goto out;
	user = "?";
    }
out:
    return user;
}

time_t
cur_time(void)
{
    time_t          t;

    time(&t);
    return t;
}

char           *
date_time(time_t t)
{
    char           *str;

    if (t == (time_t) 0)
	t = cur_time();
    str = ctime(&t);

    str[16] = 0;
    return str + 4;
}

char           *
plural(long n)
{
    return n != 1 ? "s" : "";
}

/*
 *	memory management
 */

 /* #define MEM_DEBUG *//* trace memory usage */

static void
mem_error(int t, int32 bytes)
{
    char            buf[200];

    if (t == 1) {
	sprintf(buf, "Alloc failed: unsigned too short to represent %ld bytes",
		(long) bytes);
    } else {
	sprintf(buf, "Out of memory - cannot allocate %ld bytes",
		(long) bytes);
    }

    sys_error(buf);
}

char           *
mem_obj(unsigned size, int32 nelt)
{
    unsigned        n;
    char           *obj;

    n = nelt;
    if (n != nelt)
	mem_error(1, nelt);

    obj = calloc(n, size);

#ifdef MEM_DEBUG
    printf("CALLOC(%u,%u) => %lx\n", n, size, (long) obj);
#endif

    if (obj == NULL)
	mem_error(2, (int32) (size * nelt));
    return obj;
}

char           *
mem_str(int32 nelt)
{
    unsigned        n;
    char           *obj;

    n = nelt;
    if (n != nelt)
	mem_error(1, nelt);

    obj = malloc(n);

#ifdef MEM_DEBUG
    printf("MALLOC(%u) => %lx\n", n, (long) obj);
#endif

    if (obj == NULL)
	mem_error(2, nelt);
    return obj;
}

char           *
mem_resize(char *obj, unsigned size, int32 nelt)
{
    unsigned        n;
    char           *obj1;

    if (obj == NULL)
	return mem_obj(size, nelt);

    nelt *= size;

    n = nelt;
    if (n != nelt)
	mem_error(1, nelt);

    obj1 = realloc(obj, n);

#ifdef MEM_DEBUG
    printf("REALLOC(%lx, %u) => %lx\n", (long) obj, n, (long) obj1);
#endif

    if (obj1 == NULL)
	mem_error(2, (int32) size);
    return obj1;
}


void
mem_clear(register char *obj, unsigned size, register int32 nelt)
{
    nelt *= size;
    while (--nelt >= 0)
	*obj++ = NUL;
}

void
mem_free(char *obj)
{

#ifdef MEM_DEBUG
    printf("FREE(%lx)\n", (long) obj);
#endif

    if (obj != NULL)
	free(obj);
}

#ifndef HAVE_MKDIR
mkdir(char *path, int mode)
{
    char            command[FILENAME * 2 + 20];

    sprintf(command, "{ mkdir %s && chmod %o %s ; } > /dev/null 2>&1",
	    path, mode, path);
    return system(command) != 0 ? -1 : 0;
}

#endif


int
nn_truncate(char *path, off_t len)
{

#ifdef HAVE_TRUNCATE
    /* Easy... we already have it... */
    return truncate(path, len);

#else				/* HAVE_TRUNCATE */

    int             fd;

#ifndef O_TRUNC
    struct stat     st;
#endif

    if (len != 0)
	sys_error("nn_truncate(%s,%ld): non-zero length", path, (long) len);

#ifdef O_TRUNC
    fd = open(path, O_WRONLY | O_TRUNC);
#else
    if (stat(path, &st) < 0)
	return -1;
    fd = creat(path, st.st_mode & 07777);
#endif

    if (fd < 0)
	return -1;
    close(fd);
    return 0;
#endif
}

/* Should really have some sort of configure file and use strdup().. */
char           *
strsave(char *s)
{
    int             l;
    char           *buf = NULL;

    if (s) {
	l = strlen(s);
	buf = malloc(l + 1);
	if (buf) {
	    if (l)
		strcpy(buf, s);
	    else
		buf[0] = '\0';
	}
    }
    return buf;
}

char           *
str3save(char *s1, char *s2, char *s3)
{
    int             l;
    char           *buf;

    if (!s1)
	s1 = "";
    if (!s2)
	s2 = "";
    if (!s3)
	s3 = "";

    l = strlen(s1) + strlen(s2) + strlen(s3);

    buf = malloc(l + 1);
    if (buf) {
	strcpy(buf, s1);
	strcat(buf, s2);
	strcat(buf, s3);
    }
    return buf;
}

/*
 * This quick function is a hack of a replacement for fgets, but is
 * unlimited in line length.  It returns a pointer to a buffer which
 * just happens to be malloc()ed and is reused next time.  IE: you had
 * best strdup() it.
 *
 * This was inspired by an article I saw in alt.sources but forgot to
 * save.
 *
 * -Peter Wemm <peter@DIALix.oz.au>
 */

static char    *buf = NULL;	/* buffer for line */
static int      size = 0;	/* size of buffer */

#define INITIAL_CHUNK	256	/* Initial malloc size */
#define GROW_CHUNK	256	/* extend with realloc in units if this */

/*
 * Get a LONG line.
 * Returns a pointer to a '\0' terminated string which will be
 * overwritten on the next call.
 * Returns NULL if error or eof, or if nothing could be read at all.
 *
 * Note: It does not react very well to '\0' characters in the byte stream
 */

char           *
fgetstr(FILE * f)
{
    int             len;	/* # of chars stored into buf before '\0' */
    char           *s;

    if (size == 0 && buf == NULL) {
	size = INITIAL_CHUNK;
	buf = malloc(size);
	if (buf == NULL) {
	    size = 0;
	    nn_exitmsg(50, "cant allocate initial chunk\n");
	    return NULL;
	}
	clearobj(buf, size, 1);
    }
    len = 0;

    while (fgets(buf + len, size - len, f) != NULL) {
	len += strlen(len + buf);
	if (len > 0 && buf[len - 1] == '\n')
	    break;		/* the whole line has been read */

	size += GROW_CHUNK;
	s = realloc(buf, size);

	if (!s) {		/* Malloc failure */
	    size = 0;		/* panic... */
	    buf = NULL;
	    nn_exitmsg(51, "cant realloc chunk\n");
	    return NULL;
	}
	buf = s;
	clearobj(buf + len, size - len, 1);
    }

    if (len == 0) {
	return NULL;		/* nothing read (eof or error) */
    }
    /* For when we are reading from a NNTP stream. */
    if (len > 1 && buf[len - 1] == '\n')
	--len;
    if (len > 1 && buf[len - 1] == '\r')
	--len;
    buf[len] = '\0';		/* unconditionally terminate string, */
    /* possibly overwriting CR and/or NL */
    return buf;
}

/*
 *	getline - gets a line from stdin
 *			returns the length of the line
 */

int
getline(char *line, int max)
{
    if (fgets(line, max, stdin) == NULL)
	return 0;
    else
	return strlen(line);
}

#ifdef NEED_STRERROR
/* strerror for old OSs that don't include it */

extern int      sys_nerr;
extern char    *sys_errlist[];

char           *
strerror(int num)
{
    static char     mesg[40];

    if (num < 0 || num > sys_nerr) {
	sprintf(mesg, "Unknown error (%d)", num);
	return mesg;
    } else
	return sys_errlist[num];
}

#endif
