/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Debug flags and defines
 *
 *	Notice:  no modules are conditioned by this file in the
 *		 makefile.  touch the source file to have a change
 *		 in debugging setup to reflect the program behavior
 */

#ifndef _NN_DEBUG_H
#define _NN_DEBUG_H 1

/* debugging */

#define RC_TEST		1	/* rc file updates */
#define DG_TEST		2	/* digest decoding */
#define SEQ_TEST	4	/* sequence file decoding */
#define SEQ_DUMP	8	/* dump sequence after read */

extern int      Debug;
#endif				/* _NN_DEBUG_H */
