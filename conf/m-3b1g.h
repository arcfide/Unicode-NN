/************** Machine (and compiler) dependent definitions. **************
 *
 *	These defs for GCC 1.34 on a UNIX-PC.  Written by Steve
 *	Simmons (scs@lokkur.dexter.mi.us).  Shoot me, not Kim.
 */

/*      MACHINE TYPE	DEFINED TYPE		VALUE RANGE	*/

typedef unsigned char	int8;		/*        0 ..     255 */
typedef short		int16;		/*  -10,000 ..  10,000 */
typedef long		int32;		/* -100,000 .. 100,000 */
typedef unsigned long	uint32;		/* 	  0 ..  2^31-1 */


#ifdef NETWORK_DATABASE
YOU LOSE ON NETWORK_DATABASE
#endif
