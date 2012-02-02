/************** Machine (and compiler) dependent definitions. **************
 *
 *	This file is for a DECstation 3100, running Ultrix
 */

/*      MACHINE TYPE	DEFINED TYPE		VALUE RANGE	*/

typedef unsigned char	int8;		/*        0 ..     255 */
typedef short		int16;		/*  -10,000 ..  10,000 */
typedef long		int32;		/* -100,000 .. 100,000 */
typedef unsigned long	uint32;		/* 	  0 ..  2^31-1 */


#ifdef NETWORK_DATABASE
#undef NETWORK_BYTE_ORDER
#include <netinet/in.h>
#endif	/* NETWORK DATABASE */
