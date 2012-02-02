/************** Machine (and compiler) dependent definitions. **************
 *
 *	This is for the Intel 80286 processor.
 */



/*      MACHINE TYPE	DEFINED TYPE		VALUE RANGE	*/

typedef unsigned char	int8;		/*        0 ..     255 */
typedef short		int16;		/*  -10,000 ..  10,000 */
typedef long		int32;		/* -100,000 .. 100,000 */
typedef unsigned long	uint32;		/* 	  0 ..  2^31-1 */

#define I286_BUG
#define MALLOC_64K_LIMITATION

#ifdef NETWORK_DATABASE

#undef NETWORK_BYTE_ORDER	/* THEY ARE NOT */

/*
 * Needs byte swapping here!
 */

YOU LOSE -- HOW IS THIS DONE ON THE 286?

#define htonl(l)	...	/* host long to network long */
#define ntohl(l)	...	/* network long to host long */

#endif	/* NETWORK DATABASE */
