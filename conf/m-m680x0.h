/************** Machine (and compiler) dependent definitions. **************
 *
 *	For all Motorola 68000/68020/68030/... based systems.
 */


/*      MACHINE TYPE	DEFINED TYPE		VALUE RANGE	*/

typedef unsigned char	int8;		/*        0 ..     255 */
typedef short		int16;		/*  -10,000 ..  10,000 */
typedef long		int32;		/* -100,000 .. 100,000 */
typedef unsigned long	uint32;		/* 	  0 ..  2^31-1 */

#define NETWORK_BYTE_ORDER
