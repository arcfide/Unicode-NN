
/************** Machine (and compiler) dependent definitions. **************
 *
 *    This is for HP9000 Series 320 and 800 (at least)
 */


/*      MACHINE TYPE	DEFINED TYPE		VALUE RANGE	*/

typedef unsigned char	int8;		/*        0 ..     255 */
typedef short		int16;		/*  -10,000 ..  10,000 */
typedef long		int32;		/* -100,000 .. 100,000 */
typedef unsigned long	uint32;		/* 	  0 ..  2^31-1 */


/*
 *	Define NETWORK_BYTE_ORDER if the machine's int32's are
 *	already in network byte order, i.e. m68k based.
 */

#define NETWORK_BYTE_ORDER	/* */
