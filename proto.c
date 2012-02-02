/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Master/slave communication and locking.
 */

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include "config.h"
#include "global.h"
#include "hostname.h"
#include "proto.h"

/* proto.c */

static void     write_lock(char *lock, char *operation);
static int      read_lock(char *lock);


#ifndef ACCOUNTING

#ifdef AUTHORIZE
#define ACCOUNTING
#endif

#endif

extern char    *master_directory, *db_directory;

#define HOSTBUF 80
char            proto_host[HOSTBUF];	/* host having the lock */

/*
 *	When setting a lock, we must check a little later that
 *	we really got the lock set, i.e. that another process
 *	didn't set it at the same time!
 */

#define LOCK_SAFETY	5	/* seconds */

/*
 *	proto_lock(program, mode)
 *
 *	Returns:
 *	-1	Not running.
 *	0	Running, no permission (PL_WAKEUP_SOFT)
 *	0	Lock set (PL_SET)
 *	1	Lock not set (PL_SET)  (another is running)
 *	1       Locked and running (PL_WAKEUP)
 *	pid	Locked and running (PL_CHECK)
 */

static void
write_lock(char *lock, char *operation)
{
    FILE           *m_pid;
    char            host[HOSTBUF];

    m_pid = open_file(lock, OPEN_CREATE);
    if (m_pid == NULL)
	sys_error("Cannot %s lock file: %s", operation, lock);
    nn_gethostname(host, HOSTBUF);
    fprintf(m_pid, "%d\n%s\n", process_id, host);
    fclose(m_pid);
}

static int
read_lock(char *lock)
{
    FILE           *m_pid;
    char            host[HOSTBUF];
    char            pid[10];

    pid[0] = NUL;
    proto_host[0] = NUL;

    m_pid = open_file(lock, OPEN_READ);
    if (m_pid == NULL)
	return -2;		/* no lock */
    fgets(pid, 10, m_pid);
    fgets(proto_host, HOSTBUF, m_pid);
    fclose(m_pid);

    if (pid[0] == NUL)
	return 0;		/* corrupted lock */

    if (proto_host[0] != NUL) {
	substchr(proto_host, NL, NUL);
	nn_gethostname(host, HOSTBUF);
	if (strncmp(proto_host, host, HOSTBUF) != 0)
	    return -1;		/* locked by another host */
	proto_host[0] = NUL;
    }
    return atoi(pid);
}

int
proto_lock(int prog, int command)
{
    int             try, pid;
    char           *lock = NULL;

    switch (prog) {
	case I_AM_MASTER:
	case I_AM_EXPIRE:
	    lock = relative(master_directory, "MPID");
	    break;
	case I_AM_SPEW:
	    lock = relative(master_directory, "WPID");
	    break;

#ifdef ACCOUNTING
	case I_AM_ACCT:
	    lock = relative(db_directory, "LCK..acct");
	    break;
#endif

	case I_AM_NN:
	    lock = relative(nn_directory, "LOCK");
	    break;
	default:
	    sys_error("Invalid LOCK prog");
    }

    if (command == PL_TRANSFER) {
	write_lock(lock, "transfer");
	return 1;
    }
    if (command == PL_CLEAR)
	goto rm_lock;

    try = 1;
again:

    switch (pid = read_lock(lock)) {
	case -2:
	    goto no_lock;
	case -1:		/* wrong host */
	    return 1;
	case 0:
	case 1:
	case 2:		/* corrupted lock file */
	    if (who_am_i == I_AM_NN)
		goto rm_lock;
	    if (--try < 0)
		goto rm_lock;
	    sleep(LOCK_SAFETY);	/* maybe it is being written */
	    goto again;
	default:
	    break;
    }

    if (kill(pid, command == PL_TERMINATE ? SIGHUP : SIGALRM) == 0) {
	switch (command) {
	    case PL_SET_QUICK:
		sleep(1);
		goto again;

	    case PL_SET_WAIT:
	    case PL_CLEAR_WAIT:
		sleep(30);
		goto again;

	    case PL_CHECK:
		return pid;

	    default:
		return 1;
	}
    }
    if (command == PL_CHECK)
	return (errno == EPERM) ? pid : -1;
    if (command == PL_WAKEUP_SOFT)
	return (errno == EPERM) ? 0 : -1;

    /* lock file contains a non-existing process, or a process with */
    /* wrong owner, ie. neither master or expire, so remove it */

rm_lock:
    unlink(lock);

no_lock:
    if (command != PL_SET && command != PL_SET_QUICK && command != PL_SET_WAIT)
	return -1;

    write_lock(lock, "create");

    /* a user will not start nn twice at the exact same time! */
    if (who_am_i == I_AM_NN || command == PL_SET_QUICK)
	return 0;

    sleep(LOCK_SAFETY);

    if (read_lock(lock) != process_id)
	return 1;		/* somebody stole the lock file */
    return 0;			/* lock is set */
}

FILE           *
open_gate_file(int mode)
{
    char           *gate, *err;
    FILE           *gf;

    gate = relative(master_directory, "GATE");
    gf = open_file(gate, mode);
    err = NULL;

    switch (mode) {
	case OPEN_READ:
	    if (gf != NULL) {
		if (unlink(gate) == 0)
		    break;
		err = "unlink";
		break;
	    }
	    if (errno != ENOENT)
		err = "read";
	    break;

	default:
	    if (gf != NULL)
		chmod(gate, 0644);	/* override restrictive umask */
	    /* caller must complain if cannot open! */
	    break;
    }

    if (err != NULL) {
	sys_warning("Cannot %s %s (err=%d).  Check modes!!", err, gate, errno);
	if (gf != NULL)
	    fclose(gf);
	return NULL;
    }
    return gf;
}

void
send_master(char command, group_header * gh, int opt, long arg)
{
    FILE           *gate;

    gate = open_gate_file(OPEN_APPEND);

    if (gate == NULL) {
	printf("Cannot send to master (check GATE file)\n");
	return;
    }
    fprintf(gate, "%c;%ld;%c;%ld;%s %s;\n",
	    command, gh == NULL ? -1L : gh->group_num, opt, arg,
	    user_name(), date_time((time_t) 0));

    fclose(gate);

    log_entry('A', "SEND %c %s %c %ld",
	      command, gh == NULL ? "(all)" : gh->group_name, opt, arg);

    if (who_am_i == I_AM_ADMIN)
	proto_lock(I_AM_MASTER, PL_WAKEUP_SOFT);
}
