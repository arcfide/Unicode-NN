/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Master/slave communication and locking.
 */

/*
 *	proto_lock() modes
 */

#include <stdio.h>

#define PL_SET		1	/* set lock (if not set) */
#define PL_SET_WAIT	2	/* set lock (wait until set) */
#define PL_SET_QUICK	3	/* as _WAIT, but using sleep(1) */
#define PL_CLEAR	4	/* clear lock */
#define PL_CLEAR_WAIT	5	/* wait for lock to disappear */
#define PL_CHECK	6	/* check running status */
#define PL_WAKEUP	7	/* send wakeup (must succeed) */
#define PL_WAKEUP_SOFT	8	/* send wakeup (may fail) */
#define PL_TERMINATE	9	/* send termination */
#define PL_TRANSFER	10	/* transfer lock to current process (forked) */

/*
 * types for send_master(type, group, opt, arg)
 */

#define SM_SET_OPTION	'O'	/* set option to arg (toggle if -1) */
#define SM_SET_FLAG	'F'	/* opt=set/clear flag 'arg' in group */
#define SM_RECOLLECT	'R'	/* recollect group (or all groups if NULL) */
#define SM_EXPIRE	'X'	/* expire group (or all groups if NULL) */
#define SM_SCAN_ONCE	'U'	/* scan unconditionally (ignore active) */

int             proto_lock(int, int);
FILE           *open_gate_file(int);
void            send_master(char, group_header *, int, long);
