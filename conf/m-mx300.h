/************** Machine (and compiler) dependent definitions. **************
 *
 *	For Siemens MX300
 *	From: Bill Wohler <wohler@sap-ag.de>
 */

/*      MACHINE TYPE	DEFINED TYPE		VALUE RANGE	*/

typedef unsigned char	int8;		/*        0 ..     255 */
typedef short		int16;		/*  -10,000 ..  10,000 */
typedef long		int32;		/* -100,000 .. 100,000 */
typedef unsigned long	uint32;		/* 	  0 ..  2^31-1 */


/*
 *	Define NO_SIGINTERRUPT on BSD based systems which don't have
 *	a siginterrupt() function, but provides an SV_INTERRUPT flag
 *	in <signal.h>.
 */

#define NO_SIGINTERRUPT	/* */

#define NETWORK_BYTE_ORDER	/* */
