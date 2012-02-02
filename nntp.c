/*
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 * nntp module for nn.
 *
 * The original taken from the nntp 1.5 clientlib.c
 * Modified heavily for nn.
 *
 * Rene' Seindal (seindal@diku.dk) Thu Dec  1 18:41:23 1988
 *
 * I have modified Rene's code quite a lot for 6.4 -- I hope he
 * can still recognize a bit here and a byte there; in any case,
 * any mistakes are mine :-)  ++Kim
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/param.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "fullname.h"
#include "hash.h"
#include "hostname.h"
#include "libnov.h"
#include "nn_term.h"

/*
 *	nn maintains a cache of recently used articles to improve efficiency.
 *	To change the size of the cache, define NNTPCACHE in config.h to be
 *	the new size of this cache.
 */

#ifndef NNTPCACHE
#define NNTPCACHE	10
#endif

#ifdef NNTP
#include "nntp.h"
#include "patchlevel.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <strings.h>

#ifndef EXCELAN
#include <netdb.h>
#endif

#include <errno.h>
#include <pwd.h>

#ifdef NOV
#include "newsoverview.h"
#endif

/* This is necessary due to the definitions in m-XXX.h */

#if !defined(NETWORK_DATABASE) || defined(NETWORK_BYTE_ORDER)
#include <netinet/in.h>
#endif

#ifdef EXCELAN

#ifndef IPPORT_NNTP
#define	IPPORT_NNTP	119
#endif

#endif

/* nntp.c */

static int      reconnect_server(int);
static int      connect_server(void);
static void     debug_msg(char *prefix, char *str);
static void     find_server(void);
static int      get_server_line(char *string, int size);
static int      get_server(char *string, int size);
static int      get_socket(void);
static void     put_server(char *string);
static int      copy_text(register FILE * fp);
static struct cache *search_cache(article_number art, group_header * gh);
static struct cache *new_cache_slot(void);
static void     clean_cache(void);
static void     set_domain(void);
static void     nntp_doauth(void);

#ifndef NOV
static int      sort_art_list(register article_number * f1, register article_number * f2);
#endif				/* !NOV */

#ifdef NeXT
static char    *strdup(char *);
#endif				/* NeXT */

extern char    *db_directory, *tmp_directory, *news_active;
extern char     delayed_msg[];

static char     host_name[MAXHOSTNAMELEN];
char            domain[MAXHOSTNAMELEN];
static char     last_put[NNTP_STRLEN];

int             nntp_failed = 0;/* bool: t iff connection is broken in
				 * nntp_get_article() or nntp_get_active() */

int             nntp_cache_size = NNTPCACHE;
char           *nntp_cache_dir = NULL;
char           *nntp_server = NNTP_SERVER;	/* name of nntp server */
char           *nntp_user, *nntp_password;	/* so can set on command line */

int             nntp_local_server = 0;
int             nntp_debug = 0;

extern char    *home_directory;
extern int      silent;

extern char    *mktemp();

static FILE    *nntp_in = NULL;	/* fp for reading from server */
static FILE    *nntp_out = NULL;/* fp for writing to server */
static int      reconnecting = 0;
static int      is_connected = 0;	/* bool: t iff we are connected */
static int      need_auth = 0;
static group_header *group_hd;	/* ptr to servers current group */
static int      can_post = 0;	/* bool: t iff NNTP server accepts postings */

#define ERR_TIMEOUT	503	/* Response code for timeout */
 /* Same value as ERR_FAULT */

#ifdef NO_RENAME
static int
rename(char *old, char *new)
{
    if (unlink(new) < 0 && errno != ENOENT)
	return -1;
    if (link(old, new) < 0)
	return -1;
    return unlink(old);
}

#endif				/* NO_RENAME */

/*
 * debug_msg: print a debug message.
 *
 *	The master appends prefix and str to a log file, and clients
 *	prints it as a message.
 *
 *	This is controlled via the nntp-debug variable in nn, and
 *	the option -D2 (or -D3 if the normal -D option should also
 *	be turned on).	Debug output from the master is written in
 *	$TMP/nnmaster.log.
 */

static void
debug_msg(char *prefix, char *str)
{
    static FILE    *f = NULL;

    if (who_am_i == I_AM_MASTER) {
	if (f == NULL) {
	    f = open_file(relative(tmp_directory, "nnmaster.log"), OPEN_CREATE);
	    if (f == NULL) {
		nntp_debug = 0;
		return;
	    }
	}
	fprintf(f, "%s %s\n", prefix, str);
	fflush(f);
	return;
    }
    msg("NNTP%s %s", prefix, str);
    user_delay(3);
}


/*
 * find_server: Find out which host to use as NNTP server.
 *
 *	This is done by consulting the file NNTP_SERVER (defined in
 *	config.h).  Set nntp_server[] to the host's name.
 */

static void
find_server(void)
{
    char           *cp, *name;
    char            buf[BUFSIZ];
    FILE           *fp;

    /*
     * This feature cannot normally be enabled, because the database and the
     * users rc file contains references to articles by number, and these
     * numbers are not unique across NNTP servers.
     */

#ifdef NOV

    /*
     * But, since we no longer have a database to worry about, give the user
     * the rope. If he wants to hang himself, then let him! :-) Let the user
     * worry about keeping his .newsrc straight.
     */
    if ((cp = getenv("NNTPSERVER")) != NULL) {
	nntp_server = cp;
	return;
    } else if (*nntp_server != '/')
	return;			/* variable was set on cmd line, or in init
				 * file */
#endif				/* NOV */

    name = nntp_server;		/* default, or variable was set to a filename */

    if ((fp = open_file(name, OPEN_READ)) != NULL) {
	while (fgets(buf, sizeof buf, fp) != 0) {
	    if (*buf == '#' || *buf == '\n')
		continue;
	    if ((cp = strchr(buf, '\n')) != 0)
		*cp = '\0';
	    nntp_server = strdup(buf);
	    if (!nntp_server)
		sys_error("Failed to allocate space for name of NNTP server!");
	    fclose(fp);
	    return;
	}
	fclose(fp);
    }
    if (who_am_i != I_AM_MASTER)
	printf("\nCannot find name of NNTP server.\nCheck %s\n", name);

    sys_error("Failed to find name of NNTP server!");
}

/*
 * get_server_line: get a line from the server.
 *
 *	Expects to be connected to the server.
 *	The line can be any kind of line, i.e., either response or text.
 *	Returns length of line if no error.
 *	If error and master, then return -1, else terminate.
 */

static int
get_server_line(register char *string, register int size)
{
    int             retry = 2;

    while (fgets(string, size, nntp_in) == NULL)
	retry = (reconnect_server(retry));

    size = strlen(string);
    if (size < 2 || !(string[size - 2] == '\r' && string[size - 1] == '\n'))
	return size;		/* XXX */

    string[size - 2] = '\0';	/* nuke CRLF */
    return size - 2;
}


/*
 * get_server: get a response line from the server.
 *
 *	Expects to be connected to the server.
 *	Returns the numerical value of the reponse, or -1 in case of errors.
 */

static int
get_server(char *string, int size)
{
    if (get_server_line(string, size) < 0)
	return -1;

    if (nntp_debug)
	debug_msg("<<<", string);

    return isdigit(*string) ? atoi(string) : 0;
}

/*
 * get_socket:	get a connection to the nntp server.
 *
 * Errors can happen when YP services or DNS are temporarily down or
 * hung, so we log errors and return failure rather than exitting if we
 * are the master.  The effects of retrying every 15 minutes (or whatever
 * the -r interval is) are not that bad.  Dave Olson, SGI
 */

static int
get_socket(void)
{
    int             s;
    struct sockaddr_in sin;

#ifndef EXCELAN
    struct servent *sp;
    struct hostent *hp;

#ifdef h_addr
    int             x = 0;
    register char **cp;
#endif				/* h_addr */

    if ((sp = getservbyname("nntp", "tcp")) == NULL)
	return sys_warning("nntp/tcp: Unknown service.\n");

    s = who_am_i == I_AM_MASTER ? 10 : 2;
    while ((hp = gethostbyname(nntp_server)) == NULL) {
	if (--s < 0)
	    goto host_err;
	sleep(10);
    }

    clearobj(&sin, sizeof(sin), 1);
    sin.sin_family = hp->h_addrtype;
    sin.sin_port = sp->s_port;

#else				/* EXCELAN */
    char           *machine;

    clearobj(&sin, sizeof(sin), 1);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(0);
#endif				/* EXCELAN */

#ifdef h_addr
    /* get a socket and initiate connection -- use multiple addresses */

    s = x = -1;
    for (cp = hp->h_addr_list; cp && *cp; cp++) {
	s = socket(hp->h_addrtype, SOCK_STREAM, 0);
	if (s < 0)
	    goto sock_err;

#ifdef NO_MEMMOVE
	bcopy(*cp, (char *) &sin.sin_addr, hp->h_length);
#else
	memmove((char *) &sin.sin_addr, *cp, hp->h_length);
#endif				/* NO_MEMMOVE */

	/* Quick hack to work around interrupting system calls.. */
	while ((x = connect(s, (struct sockaddr *) & sin, sizeof(sin))) < 0 &&
	       errno == EINTR)
	    sleep(1);
	if (x == 0)
	    break;
	if (who_am_i != I_AM_MASTER)
	    msg("Connecting to %s failed: %s", nntp_server, strerror(errno));
	(void) close(s);
	s = -1;
    }
    if (x < 0)
	sys_warning("Giving up on NNTP server %s!", nntp_server);
#else	/* h_addr */		/* no name server */

#ifdef EXCELAN
    if ((s = socket(SOCK_STREAM, NULL, &sin, SO_KEEPALIVE)) < 0)
	goto sock_err;

    sin.sin_port = htons(IPPORT_NNTP);
    machine = nntp_server;
    if ((sin.sin_addr.s_addr = rhost(&machine)) == -1) {
	(void) close(s);
	goto host_err;
    }
    /* And then connect */
    if (connect(s, &sin) < 0)
	goto conn_err;
#else				/* not EXCELAN */
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	goto sock_err;

    /* And then connect */

#ifdef NO_MEMMOVE
    bcopy(hp->h_addr, (char *) &sin.sin_addr, hp->h_length);
#else
    memmove((char *) &sin.sin_addr, hp->h_addr, hp->h_length);
#endif				/* NO_MEMMOVE */

    if (connect(s, (struct sockaddr *) & sin, sizeof(sin)) < 0)
	goto conn_err;
#endif				/* EXCELAN */

#endif				/* h_addr */

    return s;

host_err:
    sys_warning("NNTP server %s unknown.\n", nntp_server);
    return -1;

sock_err:
    sys_warning("Can't get NNTP socket: %s", strerror(errno));
    return -1;

#ifndef h_addr
conn_err:
    (void) close(s);
    if (who_am_i == I_AM_MASTER)
	sys_warning("Connecting to %s failed: %s", nntp_server, strerror(errno));
    return -1;
#endif

}


/*
 * connect_server: initialise a connection to the nntp server.
 *
 *	It expects nntp_server[] to be set previously, by a call to
 *	nntp_check.  It is called from nntp_get_article() and
 *	nntp_get_active() if there is no established connection.
 *	This gets called at initialization, and whenever it looks
 *	like the server may have closed the connection on us.
 */

static int
connect_server(void)
{
    int             sockt_rd, sockt_wr;
    int             response;
    int             triedauth = 0;
    char            line[NNTP_STRLEN];

    if (who_am_i == I_AM_VIEW)
	nntp_check();

    if (who_am_i != I_AM_MASTER && !silent && !reconnecting)
	msg("Connecting to NNTP server %s ...", nntp_server);

    nntp_failed = 1;
    is_connected = 0;

    sockt_rd = get_socket();
    if (sockt_rd < 0)
	return -1;

    if ((nntp_in = fdopen(sockt_rd, "r")) == NULL) {
	close(sockt_rd);
	return -1;
    }
    sockt_wr = dup(sockt_rd);
    if ((nntp_out = fdopen(sockt_wr, "w")) == NULL) {
	close(sockt_wr);
	fclose(nntp_in);
	nntp_in = NULL;		/* from above */
	return -1;
    }
    /* Now get the server's signon message */
    response = get_server(line, sizeof(line));

    if (who_am_i == I_AM_MASTER) {
	if (response != OK_CANPOST && response != OK_NOPOST) {
	    log_entry('N', "Failed to connect to NNTP server");
	    log_entry('N', "Response: %s", line);
	    fclose(nntp_out);
	    fclose(nntp_in);
	    return -1;
	}
    } else if (reconnecting && need_auth) {
	nntp_doauth();
	triedauth++;
    } else {
	switch (response) {
	    case OK_AUTH:
		break;
	    case OK_CANPOST:
		can_post = 1;
		break;
	    case OK_NOPOST:
		can_post = 0;
		break;
	    default:
		nn_exitmsg(1, "%s", line);
		/* NOTREACHED */
	}
    }

retrymode:
    /* Try and speak NNRP style reader protocol */
    sprintf(line, "MODE READER");
    put_server(line);
    /* See what we got if from a NNRP compiant server like INN's nnrpd */
    response = get_server(line, sizeof(line));

    switch (response) {
	case OK_CANPOST:
	    can_post = 1;
	    break;
	case OK_NOPOST:
	    can_post = 0;
	    break;
	case ERR_NEEDAUTH:
	    if (!triedauth) {
		nntp_doauth();
		triedauth++;
		goto retrymode;
	    }
	    break;		/* will probably fail hard later */
	case ERR_COMMAND:
	default:
	    /* if it doesn't understand MODE READER, we dont care.. :-) */
	    break;
    }

    if (!can_post && !triedauth) {

	/*
	 * sometimes servers say no posting until auth done, so try. it might
	 * be better if this was controlled by some variable, since some
	 * people are OK with readonly access
	 */
	msg("No post, so trying authorization; if readonly OK, just skip");
	nntp_doauth();
	triedauth++;
	goto retrymode;
    }
    if (who_am_i != I_AM_MASTER && !silent && !reconnecting)
	msg("Connecting to NNTP server %s ... ok (%s)",
	    nntp_server, can_post ? "posting is allowed" : "no posting");

    is_connected = 1;
    nntp_failed = 0;
    return 0;
}


/*
 * reconnect_server -- reconnect to server after a timeout.
 * Re-establish correct group and resend last command to get back to the
 * correct state.
 *
 *	Returns:	-1 if the error is fatal (and we should exit).
 *			# of retries left otherwise.
 *
 *	Side Effects:	If fails to connect and retry==0, calls nn_exitmsg.
 */

static int
reconnect_server(int retry)
{
    char            buf[NNTP_STRLEN];

    retry--;
    msg("Reconnecting");
    if (nntp_debug) {
	msg("retry = %d", retry);
	user_delay(2);
    }
    reconnecting = 1;
    strcpy(buf, last_put);
    nntp_close_server();
    if (connect_server() < 0) {
	if (nntp_debug)
	    debug_msg("failed to connect", "");
	if (retry > 0)
	    return (retry);
	else
	    nn_exitmsg(1, "failed to reconnect to server");
    }
    if (group_hd)
	if (nntp_set_group(group_hd) < 1)
	    nn_exitmsg(1, "unable to set group");

    if (nntp_debug)
	debug_msg(" Resend last command", buf);
    put_server(buf);
    reconnecting = 0;
    return (retry);
}


/*
 * put_server:	send a line to the nntp server.
 *
 *	Expects to be connected to the server.
 */

static void
put_server(char *string)
{
    if (nntp_debug)
	debug_msg(">>>", string);

    strcpy(last_put, string);
    fprintf(nntp_out, "%s\r\n", string);
    (void) fflush(nntp_out);
    return;
}

/*
 * ask_server:	ask the server a question and return the answer.
 *
 *	Expects to be connected to the server.
 *	Returns the numerical value of the reponse, or -1 in case of
 *	errors.
 *	Contains some code to handle server timeouts intelligently.
 */

/* LIST XXX returns fatal ERR_FAULT code if requested list does not exist */
/* This is only fatal for LIST ACTIVE -- else change to ERR_NOGROUPS */
static int      fix_list_response = 0;
static char     ask_reply[NNTP_STRLEN];

static int
ask_server(char *fmt,...)
{
    char            ask[NNTP_STRLEN];
    int             response;
    int             fix_err;
    va_list         ap;

    fix_err = fix_list_response;
    fix_list_response = 0;

    va_start(ap, fmt);
    vsprintf(ask, fmt, ap);
    va_end(ap);

    put_server(ask);
    response = get_server(ask_reply, sizeof(ask_reply));

    /*
     * Handle the response from the server.  Responses are handled as
     * followes:
     * 
     * 100-199	Informational.	Passed back. (should they be ignored?).
     * 200-299	Ok messages.  Passed back. 300-399	Ok and proceed.  Can
     * not happen in nn. 400-499	Errors (no article, etc).  Passed up
     * and handled there. 480 (NEED_AUTH) handle specially.  Done here
     * because some servers can send it at any point, after an idle period,
     * as well as at various points during connect. 500-599	Fatal NNTP
     * errors.  Handled below.
     */

    if (response == ERR_TIMEOUT) {
	(void) reconnect_server(1);
	response = get_server(ask_reply, sizeof(ask_reply));
    }
    if (response == ERR_NEEDAUTH) {

	/*
	 * try auth, but just once, at least for now, and then retry the
	 * original command, and continue
	 */
	nntp_doauth();
	put_server(ask);
	response = get_server(ask_reply, sizeof(ask_reply));
    }
    if (response == ERR_GOODBYE || response > ERR_COMMAND) {
	if (fix_err && (response == ERR_FAULT || response == ERR_CMDSYN))
	    return ERR_NOGROUP;

	nntp_failed = 1;
	nntp_close_server();

	if (response != ERR_GOODBYE) {
	    /* if not goodbye, complain */
	    sys_error("NNTP %s response: %d", ask_reply, response);
	    /* NOTREACHED */
	}
	if (response == ERR_GOODBYE) {
	    sys_warning("NNTP %s response: %d", ask_reply, response);
	}
    }
    return response;
}

/*
 * copy_text: copy text response into file.
 *
 *	Copies a text response into an open file.
 *	Return -1 on error, 0 otherwise.  It is treated as an error, if
 *	the returned response it not what was expected.
 */

static int      last_copy_blank;

static int
copy_text(register FILE * fp)
{
    char            buf[NNTP_STRLEN * 2];
    register char  *cp;
    register int    nlines;

    nlines = 0;
    last_copy_blank = 0;
    while (get_server_line(buf, sizeof buf) >= 0) {
	cp = buf;
	if (*cp == '.')
	    if (*++cp == '\0') {
		if (nlines <= 0)
		    break;
		if (nntp_debug) {
		    sprintf(buf, "%d lines", nlines);
		    debug_msg("COPY", buf);
		}
		return 0;
	    }
	last_copy_blank = (*cp == NUL);
	fputs(cp, fp);
	putc('\n', fp);
	nlines++;
    }
    fclose(fp);
    if (nntp_debug)
	debug_msg("COPY", "EMPTY");
    return -1;
}


/*
 * The following functions implements a simple lru cache of recently
 * accessed articles.  It is a simple way to improve effeciency.  Files
 * must be kept by name, because the rest of the code expects to be able
 * to open an article multiple times, and get separate file pointers.
 */

struct cache {
    char           *file_name;	/* file name */
    article_number  art;	/* article stored in file */
    group_header   *grp;	/* from this group */
    unsigned        time;	/* time last accessed */
}               cache[NNTPCACHE];

static unsigned time_counter = 1;	/* virtual time */

/*
 * search_cache: search the cache for an (article, group) pair.
 *
 *	Returns a pointer to the slot where it is, null otherwise
 */

static struct cache *
search_cache(article_number art, group_header * gh)
{
    struct cache   *cptr = cache;
    int             i;

    if (who_am_i == I_AM_MASTER)
	return NULL;

    if (nntp_cache_size > NNTPCACHE)
	nntp_cache_size = NNTPCACHE;

    for (i = 0; i < nntp_cache_size; i++, cptr++)
	if (cptr->art == art && cptr->grp == gh) {
	    cptr->time = time_counter++;
	    return cptr;
	}
    return NULL;
}

/*
 * new_cache_slot: get a free cache slot.
 *
 *	Returns a pointer to the allocated slot.
 *	Frees the old filename, and allocates a new, unused filename.
 *	Cache files can also stored in a common directory defined in
 *	~/.nn or CACHE_DIRECTORY if defined in config.h.
 */

static struct cache *
new_cache_slot(void)
{
    register struct cache *cptr = cache;
    int             i, lru = 0;
    unsigned        min_time = time_counter;
    char            name[FILENAME];

    if (nntp_cache_dir == NULL) {

#ifdef CACHE_DIRECTORY
	nntp_cache_dir = CACHE_DIRECTORY;
#else				/* CACHE_DIRECTORY */
	if (who_am_i == I_AM_MASTER)
	    nntp_cache_dir = db_directory;
	else
	    nntp_cache_dir = nn_directory;
#endif				/* CACHE_DIRECTORY */
    }
    if (who_am_i == I_AM_MASTER) {
	cptr = &cache[0];
	if (cptr->file_name == NULL)
	    cptr->file_name = mk_file_name(nntp_cache_dir, "master_cache");
	return cptr;
    }
    for (i = 0; i < nntp_cache_size; i++, cptr++)
	if (min_time > cptr->time) {
	    min_time = cptr->time;
	    lru = i;
	}
    cptr = &cache[lru];

    if (cptr->file_name == NULL) {
	sprintf(name, "%s/nn-%d.%02d~", nntp_cache_dir, process_id, lru);
	cptr->file_name = copy_str(name);
    } else
	unlink(cptr->file_name);

    cptr->time = time_counter++;
    return cptr;
}

/*
 * clean_cache: clean up the cache.
 *
 *	Removes all allocated files.
 */

static void
clean_cache(void)
{
    struct cache   *cptr = cache;
    int             i;

    for (i = 0; i < nntp_cache_size; i++, cptr++)
	if (cptr->file_name)
	    unlink(cptr->file_name);
}

/*
 * set_domain: set the domain name
 */

static void
set_domain(void)
{

#if !defined(DOMAIN) || !defined(HIDDENNET)
    char           *cp;
#endif

#ifdef DOMAIN

#ifdef HIDDENNET
    strncpy(domain, DOMAIN, MAXHOSTNAMELEN);
#else
    strcpy(domain, host_name);
    cp = index(domain, '.');
    if (cp == NULL) {
	strcat(domain, ".");
	strncat(domain, DOMAIN, MAXHOSTNAMELEN - sizeof(host_name) - 1);
    }
#endif				/* HIDDENNET */

#else				/* DOMAIN */

    domain[0] = '\0';

    cp = index(host_name, '.');
    if (cp == NULL) {
	FILE           *resolv;

	if ((resolv = fopen("/etc/resolv.conf", "r")) != NULL) {
	    char            line[MAXHOSTNAMELEN + 1];
	    char           *p;

	    line[MAXHOSTNAMELEN] = '\0';
	    while (fgets(line, MAXHOSTNAMELEN, resolv)) {
		if (strncmp(line, "domain", 6) == 0) {
		    for (p = &line[6]; *p && isspace(*p); p++);
		    (void) strncpy(domain, p, MAXHOSTNAMELEN);
		    p = domain + strlen(domain) - 1;
		    while ((p >= domain) && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p--;
		    *++p = '\0';
		    break;
		}
	    }
	}
	if (domain[0] == '\0')
	    nn_exitmsg(1, "hostname=%s, You need a fully qualified domain name", host_name);
    } else {

#ifdef HIDDENNET
	strncpy(domain, ++cp, MAXHOSTNAMELEN);
#else
	strncpy(domain, host_name, MAXHOSTNAMELEN);
#endif
    }
#endif				/* DOMAIN */
}

/*
 * nntp_check: Find out whether we need to use NNTP.
 *
 *	This is done by comparing the NNTP servers name with whatever
 *	nn_gethostname() returns.
 *	use_nntp and news_active are initialised as a side effect.
 */

void
nntp_check(void)
{
    char           *server_real_name = NULL;
    struct hostent *hp;

    nn_gethostname(host_name, sizeof host_name);
    set_domain();

    last_put[0] = '\0';

    if (nntp_local_server)
	return;

    find_server();

    if ((hp = gethostbyname(host_name)) != NULL)
	strncpy(host_name, hp->h_name, sizeof host_name);

    if ((hp = gethostbyname(nntp_server)) != NULL)
	server_real_name = hp->h_name;
    else
	nn_exitmsg(1, "NNTPSERVER is invalid");
    use_nntp = (strcmp(host_name, server_real_name) != 0);

    if (use_nntp) {
	freeobj(news_active);

#ifndef NOV
	news_active = mk_file_name(db_directory, "ACTIVE");
#else				/* NOV */
	news_active = mk_file_name(nn_directory, "ACTIVE");
#endif				/* NOV */
    }
}


/*
 * nntp_set_group -- set the server's current group.
 *
 *      Parameters:     gh is the structure containing the group name.
 *
 *      Returns:         1 if OK.
 *                      -1 if no such group.
 *                       0 otherwise.
 */

int
nntp_set_group(group_header * gh)
{
    int             n;

    switch (n = ask_server("GROUP %s", gh->group_name)) {
	case OK_GROUP:
	    group_hd = gh;
	    return 1;

	case ERR_NOGROUP:
	    log_entry('N', "NNTP: group %s not found\n", gh->group_name);
	    return -1;

	default:		/* ??? returns this if connection down */
	    log_entry('N', "GROUP %s response: %d", group_hd->group_name, n);
	    nntp_failed = 1;
	    return -1;		/* What did it return? */
    }
}


#ifndef NOV
/*
 * nntp_get_active:  get a copy of the active file.
 *
 *	If we are the master get a copy of the file from the nntp server.
 *	nnadmin just uses the one we already got.  In this way the master
 *	can maintain a remote copy of the servers active file.
 *	We try to be a little smart, if not inefficient, about the
 *	modification times on the local active file.
 *	Even when the master is running on the nntp server, a separate
 *	copy of the active file will be made for access via NFS.
 */

int
nntp_get_active(void)
{
    FILE           *old, *new;
    char            bufo[NNTP_STRLEN], bufn[NNTP_STRLEN];
    char           *new_name;
    int             same, n;

    if (who_am_i != I_AM_MASTER)
	return access(news_active, 4);

    if (!is_connected && connect_server() < 0)
	return -1;

    new_name = mktemp(relative(db_directory, ".actXXXXXX"));

    switch (n = ask_server("LIST")) {
	case OK_GROUPS:
	    new = open_file(new_name, OPEN_CREATE_RW | MUST_EXIST);
	    if (copy_text(new) == 0) {
		if (fflush(new) != EOF)
		    break;
		fclose(new);
	    }
	    unlink(new_name);
	    if (!nntp_failed) {
		log_entry('N', "LIST empty");
		nntp_failed = 1;
	    }
	    return -1;
	default:
	    log_entry('N', "LIST response: %d", n);
	    return -1;
    }

    rewind(new);
    same = 0;
    if ((old = open_file(news_active, OPEN_READ)) != NULL) {
	do {
	    fgets(bufo, sizeof bufo, old);
	    fgets(bufn, sizeof bufn, new);
	} while (!feof(old) && !feof(new) && strcmp(bufo, bufn) == 0);
	same = feof(old) && feof(new);
	fclose(old);
    }
    fclose(new);

    if (same)
	unlink(new_name);
    else if (rename(new_name, news_active) != 0)
	sys_error("Cannot rename %s to %s", new_name, news_active);

    return 0;
}

#endif				/* !NOV */

/*
 * nntp_get_newsgroups:  get a copy of the newsgroups file.
 *
 *	Use the "LIST NEWSGROUPS" command to get the newsgroup descriptions.
 *	Based on code from: olson%anchor.esd@sgi.com (Dave Olson)
 */

FILE           *
nntp_get_newsgroups(void)
{
    char           *new_name;
    FILE           *new;
    int             n;

    new_name = mktemp(relative(tmp_directory, "nngrXXXXXX"));
    new = open_file(new_name, OPEN_CREATE_RW | OPEN_UNLINK);
    if (new == NULL)
	return NULL;

    if (!is_connected && connect_server() < 0)
	goto err;

    fix_list_response = 1;
    switch (n = ask_server("LIST NEWSGROUPS")) {
	case ERR_NOGROUP:	/* really ERR_FAULT */
	    goto err;

	case OK_GROUPS:
	    if (copy_text(new) == 0) {
		if (fflush(new) != EOF)
		    break;
		fclose(new);
	    }
	    if (!nntp_failed) {
		log_entry('N', "LIST NEWSGROUPS empty");
		nntp_failed = 1;
	    }
	    return NULL;

	default:
	    log_entry('N', "LIST NEWSGROUPS response: %d", n);
	    goto err;
    }
    rewind(new);
    return new;

err:
    fclose(new);
    return NULL;
}

#ifndef NOV
/*
 * nntp_get_article_list: get list of all article numbers in group
 *
 *	Sends XHDR command to the server, and parses the following
 *	text response to get a list of article numbers which is saved
 *	in a list and returned.
 *	Return NULL on error.  It is treated as an error, if
 *	the returned response it not what was expected.
 */

static article_number *article_list = NULL;
static long     art_list_length = 0;

static int
sort_art_list(register article_number * f1, register article_number * f2)
{
    return (*f1 < *f2) ? -1 : (*f1 == *f2) ? 0 : 1;
}

article_number *
nntp_get_article_list(group_header * gh)
{
    char            buf[NNTP_STRLEN];
    register article_number *art;
    register char  *cp;
    register long   count = 0;	/* No. of completions plus one */
    int             n;
    static int      try_listgroup = 1;

    if (!is_connected && connect_server() < 0)
	return NULL;

    /* it is really an extreme waste of time to use XHDR since all we	 */
    /* are interested in is the article numbers (as we do locally).	 */
    /* If somebody hacks up an nntp server that understands LISTGROUP	 */
    /* they will get much less load on the nntp server			 */
    /* It should simply return the existing article numbers is the group */
    /* -- they don't even have to be sorted (only XHDR needs that)	 */

    if (try_listgroup) {
	switch (n = ask_server("LISTGROUP %s", group_hd->group_name)) {
	    case OK_GROUP:
		break;
	    default:
		log_entry('N', "LISTGROUP response: %d", n);
		return NULL;
	    case ERR_COMMAND:
		try_listgroup = 0;
	}
    }
    if (!try_listgroup) {
	switch (n = ask_server("XHDR message-id %ld-%ld",
		 (long) gh->first_db_article, (long) gh->last_db_article)) {
	    case OK_HEAD:
		break;
	    default:
		log_entry('N', "XHDR response: %d", n);
		return NULL;
	    case ERR_COMMAND:
		nntp_failed = 2;
		return NULL;
	}
    }
    count = 0;
    art = article_list;

    while (get_server_line(buf, sizeof buf) >= 0) {
	cp = buf;
	if (*cp == '.' && *++cp == '\0')
	    break;

	if (count == art_list_length) {
	    art_list_length += 250;
	    article_list = resizeobj(article_list, article_number, art_list_length + 1);
	    art = article_list + count;
	}
	*art++ = atol(cp);
	count++;
    }

    if (article_list != NULL) {
	*art = 0;
	if (try_listgroup && count > 1)
	    quicksort(article_list, count, article_number, sort_art_list);
    }
    return article_list;
}

#endif				/* NOV */

/*
 * nntp_get_article: get an article from the server.
 *
 *	Returns a FILE pointer.
 *	If necessary the server's current group is set.
 *	The article (header and body) are copied into a file, so they
 *	are seekable (nn likes that).
 */

static char    *mode_cmd[] = {
    "ARTICLE",
    "HEAD",
    "BODY"
};

FILE           *
nntp_get_article(article_number article, int mode)
 /* mode: 0 => whole article, 1 => head only, 2 => body only */
{
    FILE           *tmp;
    static struct cache *cptr;
    int             n;

    if (!is_connected && connect_server() < 0) {
	return NULL;
    }

    /*
     * Search the cache for the requested article, and allocate a new slot if
     * necessary (if appending body, we already got it).
     */

    if (mode != 2) {
	cptr = search_cache(article, group_hd);
	if (cptr != NULL)
	    goto out;
	cptr = new_cache_slot();
    }

    /*
     * Copy the article.
     */
    switch (n = ask_server("%s %ld", mode_cmd[mode], (long) article)) {
	case OK_ARTICLE:
	case OK_HEAD:
	    tmp = open_file(cptr->file_name, OPEN_CREATE | MUST_EXIST);
	    if (copy_text(tmp) < 0)
		return NULL;

	    if (mode == 1 && !last_copy_blank)
		fputc(NL, tmp);	/* add blank line after header */

	    if (fclose(tmp) == EOF)
		goto err;
	    cptr->art = article;
	    cptr->grp = group_hd;
	    goto out;

	case OK_BODY:
	    tmp = open_file(cptr->file_name, OPEN_APPEND | MUST_EXIST);
	    fseek(tmp, 0, 2);
	    if (copy_text(tmp) < 0)
		return NULL;
	    if (fclose(tmp) == EOF)
		goto err;
	    goto out;

	case ERR_NOARTIG:
	    /* Matt Heffron: ANUNEWS on VMS uses no such article error */
	case ERR_NOART:
	    return NULL;

	default:
	    /* Matt Heffron: Which group? */
	    log_entry('N', "ARTICLE %ld response: %d (in Group %s)",
		      (long) article, n, group_hd->group_name);
	    nntp_failed = 1;
	    return NULL;
    }

out:
    return open_file(cptr->file_name, OPEN_READ | MUST_EXIST);

err:
    /* sys_error('N', "Cannot write temporary file %s", cptr->file_name); */
    nn_exitmsg(1, "Cannot write temporary file %s", cptr->file_name);
    return NULL;		/* for lint, we can't actually get here */
}

/*
 *	Return local file name holding article
 */

char           *
nntp_get_filename(article_number art, group_header * gh)
{
    struct cache   *cptr;

    cptr = search_cache(art, gh);

    return cptr == NULL ? NULL : cptr->file_name;
}

/*
 * nntp_close_server: close the connection to the server.
 */

void
nntp_close_server(void)
{
    if (!is_connected)
	return;

    if (!nntp_failed)		/* avoid infinite recursion */
	put_server("QUIT");

    (void) fclose(nntp_out);
    (void) fclose(nntp_in);

    is_connected = 0;
}


/*
 * nntp_cleanup:  clean up after an nntp session.
 *
 *	Called from nn_exit().
 */

void
nntp_cleanup(void)
{
    if (is_connected)
	nntp_close_server();
    is_connected = 0;
    clean_cache();
}


/*************************************************************/

#ifdef NOV
/*
** Prime the nntp server to snarf the overview file for a newsgroup.
** Sends the XOVER command and prepares to read the result.
*/
struct novgroup *
nntp_get_overview(group_header * gh, article_number first, article_number last)
{
    int             n;

    if (!is_connected && connect_server() < 0) {
	return NULL;
    }
    switch (nntp_set_group(gh)) {
	case -1:
	case 0:
	    return NULL;
    }

    n = ask_server("XOVER %d-%d", first, last);
    switch (n) {

	case OK_NOV:
	    return novstream(nntp_in);

	default:
	    log_entry('N', "XOVER response: %d", n);
	    return NULL;

    }
}

/*
 * nntp_fopen_list(cmd):  Send some variant of a LIST command to the
 * NNTP server.  returns NULL if the file to be LISTed doesn't exist
 * on the server, else returns the nntp_in FILE descriptor, thus
 * simulating fopen().
 * nntp_fgets() is later used to read a line from the nntp_in FILE.
 */

FILE           *
nntp_fopen_list(char *cmd)
{
    int             n;

    if (!is_connected && connect_server() < 0)
	return NULL;

    fix_list_response = 1;
    switch (n = ask_server(cmd)) {
	case ERR_NOGROUP:	/* really ERR_FAULT - no such file on server */
	    return NULL;

	case OK_GROUPS:	/* aka NNTP_LIST_FOLLOWS_VAL */
	    return nntp_in;

	default:
	    log_entry('N', "`%s' response: %d", cmd, n);
	    return NULL;
    }
}

/*
 * nntp_fgets() - Get a line from a file stored on the NNTP server.
 * Strips any hidden "." at beginning of line and returns a pointer to
 * the line.  line will be terminated by NL NUL.
 * Returns NULL when NNTP sends the terminating "." to indicate EOF.
 */
char           *
nntp_fgets(char *buf, int bufsize)
{
    char           *cp;
    register int    size;

    if ((size = get_server_line(buf, bufsize - 1)) < 0)
	return NULL;		/* Can't happen with NOV (we'd rather die
				 * first) */

    cp = buf;
    if (*cp == '.') {
	if (*++cp == '\0')
	    return NULL;
    }
    cp[size] = '\n';
    cp[size + 1] = '\0';
    return cp;
}

#endif				/* NOV */

/* do all the AUTH stuff; used from 3 places that I've found where
 * various news servers ask for authentication.  Given that we exit
 * on failed auth, probably should just be void.
 * OLSON: oops, loops on empty user input "forever". Should fix
*/
static void
nntp_doauth(void)
{
    int             len;
    char            line[NNTP_STRLEN], *nl, *pass = NULL;

    strcpy(line, "authinfo user ");
    len = strlen(line);

    log_entry('N', "NNTP Authentication");

    if (nntp_user) {
	msg("NNTP authentication");
	strcpy(line + len, nntp_user);
    } else {
	init_term(1);
	clrdisp();
	prompt("Authentication required; enter username: ");
	(void) fgets(line + len, sizeof(line) - len, stdin);
	if ((nl = strpbrk(line, "\r\n")))
	    *nl = '\0';
	nntp_user = strdup(line + len);
    }
    put_server(line);
    if ((len = get_server(line, sizeof(line))) == OK_NEEDPASS) {
	strcpy(line, "authinfo pass ");
	len = strlen(line);

	if (nntp_password)
	    strcpy(line + len, nntp_password);
	else {

#ifdef USEGETPASSPHRASE
	    pass = getpassphrase("Enter password: ");
#else
	    pass = getpass("Enter password: ");
#endif

	    strcat(line, pass);
	}
	put_server(line);
	if ((len = get_server(line, sizeof(line))) == OK_AUTH) {
	    if (!nntp_password) {
		nntp_password = strdup(pass);
		clrdisp();
		msg("Remembering password for reconnections");
	    }
	} else {
	    nn_exitmsg(1, "Authentication failed (%d): %s", len, line);
	}
    }
    need_auth = 1;
}


/* ZZZZZ */

/*
 * The rest of this is cribbed (and modified) from the old mini-inews.
 *
 * Simply accept input on stdin (or via a named file) and dump this
 * to the server; add a From: and Path: line if missing in the original.
 * Print meaningful errors from the server.
 * Limit .signature files to MAX_SIGNATURE lines.
 *
 * Original by Steven Grady <grady@ucbvax.Berkeley.EDU>, with thanks from
 * Phil Lapsley <phil@ucbvax.berkeley.edu>
 */

/*
 * gen_frompath -- generate From: and Path: lines, in the form
 *
 *	From: Full Name <user@host.domain>
 *	Path: host!user
 *
 * This routine should only be called if the message doesn't have
 * a From: line in it.
 */

static void
gen_frompath(void)
{
    struct passwd  *passwd;

    passwd = getpwuid(getuid());

    fprintf(nntp_out, "From: ");
    fprintf(nntp_out, "%s ", full_name());

    fprintf(nntp_out, "<%s@%s>\r\n",
	    passwd->pw_name,
	    domain);

#ifdef HIDDENNET
    /* Only the login name - nntp server will add uucp name */
    fprintf(nntp_out, "Path: %s\r\n", passwd->pw_name);
#else				/* HIDDENNET */
    fprintf(nntp_out, "Path: %s!%s\r\n", host_name, passwd->pw_name);
#endif				/* HIDDENNET */
}


/*
 * lower -- convert a character to lower case, if it's
 *	upper case.
 *
 *	Parameters:	"c" is the character to be
 *			converted.
 *
 *	Returns:	"c" if the character is not
 *			upper case, otherwise the lower
 *			case eqivalent of "c".
 *
 *	Side effects:	None.
 */

static char
lower(register char c)
{
    if (isascii(c) && isupper(c))
	c = c - 'A' + 'a';
    return (c);
}

/*
 * strneql -- determine if two strings are equal in the first n
 * characters, ignoring case.
 *
 *	Parameters:	"a" and "b" are the pointers
 *			to characters to be compared.
 *			"n" is the number of characters to compare.
 *
 *	Returns:	1 if the strings are equal, 0 otherwise.
 *
 *	Side effects:	None.
 */

static int
strneql(register char *a, register char *b, int n)
{
    while (n && lower(*a) == lower(*b)) {
	if (*a == '\0')
	    return (1);
	a++;
	b++;
	n--;
    }
    if (n)
	return (0);
    else
	return (1);
}


/*
 * valid_header -- determine if a line is a valid header line
 *
 *	Parameters:	"h" is the header line to be checked.
 *
 *	Returns:	1 if valid, 0 otherwise
 *
 *	Side Effects:	none
 *
 */

static int
valid_header(register char *h)
{
    char           *colon, *space;

    /*
     * blank or tab in first position implies this is a continuation header
     */
    if (h[0] == ' ' || h[0] == '\t')
	return (1);

    /*
     * just check for initial letter, colon, and space to make sure we
     * discard only invalid headers
     */
    colon = index(h, ':');
    space = index(h, ' ');
    if (isalpha(h[0]) && colon && space == colon + 1)
	return (1);

    /*
     * anything else is a bad header -- it should be ignored
     */
    return (0);
}

int
nntp_post(char *temp_file)
{
    int             n, response;
    FILE           *in = fopen(temp_file, "r");
    char            s[4 * NNTP_STRLEN];
    int             seen_fromline, in_header, seen_header;
    register char  *cp;

    if (!in) {
	sprintf(delayed_msg, "Posting failed because we couldn't re-open file %s.", temp_file);
	return 1;
    }
    if (!is_connected && connect_server() < 0)
	return 1;

    switch (n = ask_server("POST")) {
	case CONT_POST:
	    break;
	default:
	    sprintf(delayed_msg, "Request to post failed with error %d, %s", n, ask_reply);
	    fclose(in);
	    return 1;
    }

    in_header = 1;
    seen_header = 0;
    seen_fromline = 0;

    while (fgets(s, sizeof(s), in) != NULL) {
	if ((cp = strchr(s, '\n')))
	    *cp = '\0';
	if (s[0] == '.')	/* Single . is eof, so put in extra one */
	    (void) fputc('.', nntp_out);
	if (in_header && strneql(s, "From:", sizeof("From:") - 1)) {
	    seen_header = 1;
	    seen_fromline = 1;
	}
	if (in_header && s[0] == '\0') {
	    if (seen_header) {
		in_header = 0;
		if (!seen_fromline)
		    gen_frompath();
		fprintf(nntp_out, "User-Agent: nn/%s.%s\r\n", RELEASE, PATCHLEVEL);
	    } else {
		continue;
	    }
	} else if (in_header) {
	    if (valid_header(s))
		seen_header = 1;
	    else
		continue;
	}
	fprintf(nntp_out, "%s\r\n", s);
    }

    fprintf(nntp_out, ".\r\n");
    (void) fflush(nntp_out);
    response = get_server(ask_reply, sizeof(ask_reply));
    switch (response) {
	case OK_POSTED:
	    break;
	case ERR_POSTFAIL:
	    msg("Article not accepted by server; not posted.");
	    user_delay(2);
	    return 1;
	default:
	    msg("Remote error: %s", ask_reply);
	    user_delay(2);
	    return 1;
    }
    return 0;
}

#ifdef NeXT
/*
 * posted to comp.sys.next.programmer:
 *
 *
 * From: moser@ifor.math.ethz.ch (Dominik Moser,CLV A4,2 40 19,720 49 89)
 * Subject: Re: Compile problems (pgp 2.6.3i)
 * Date: 10 Jul 1996 06:50:42 GMT
 * Organization: Swiss Federal Institute of Technology (ETHZ)
 * References: <4rrhvj$6fr@bagan.srce.hr>
 * Message-ID: <4rvjs2$6oh@elna.ethz.ch>
 *
 * Most systems don't have this (yet)
 */
static char    *
strdup(char *str)
{
    char           *p;

    if ((p = malloc(strlen(str) + 1)) == NULL)
	return ((char *) NULL);

    (void) strcpy(p, str);

    return (p);
}

#endif				/* NeXT */

#endif				/* NNTP */
