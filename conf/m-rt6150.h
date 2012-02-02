/************** Machine (and compiler) dependent definitions. **************
 *
 *	For IBM/RT 6150
 *	From: marcel@duteca.tudelft.nl (Marcel J.E. Mol)
 */

/*      MACHINE TYPE	DEFINED TYPE		VALUE RANGE	*/

typedef unsigned char	int8;		/*        0 ..     255 */
typedef short		int16;		/*  -10,000 ..  10,000 */
typedef long		int32;		/* -100,000 .. 100,000 */
typedef unsigned long	uint32;		/* 	  0 ..  2^31-1 */

#define NETWORK_BYTE_ORDER	/* */
