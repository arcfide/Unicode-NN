/*******************WARNING*********************

This is a *MODIFIED* version of Geoff Coller's proof-of-concept NOV
implementation.

It has been modified to support threading directly from a file handle
to a NNTP server without a temporary file.

This is not a complete distribution.  We have only distributed enough
to support NN's needs.

The original version came from world.std.com:/src/news/nov.dist.tar.Z
and was dated 11 Aug 1993.

In any case, bugs you find here are probably my fault, as I've trimmed
a fair bit of unused code.

-Peter Wemm  <peter@DIALix.oz.au>
*/

/*
 * Copyright (c) Geoffrey Collyer 1992, 1993.
 * All rights reserved.
 * Written by Geoffrey Collyer.
 * Thanks to UUNET Communications Services Inc for financial support.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company, the Regents of the University of California, or
 * the Free Software Foundation.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The authors are not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */


/*
 * general-purpose in-core hashing, normal interface (wrapper)
 */

#include <string.h>
#include "config.h"
#include "hash.h"
#include "hdbm.h"

#ifdef notdef
static unsigned			/* not yet taken modulus table size */
hashdef(register HASHDATUM key)
{
    register unsigned hash = 0;
    register char   c;

    while ((c = *key++) != '\0')
	hash += c;
    return hash;
}

#endif

HASHTABLE      *
hashcreate(unsigned size, unsigned (*hashfunc) ())
 /* size		a crude guide to size */
{
    return hdbmcreate(size, hashfunc);
}

void
hashdestroy(register HASHTABLE * tbl)
{
    hdbmdestroy(tbl);
}

int
hashstore(HASHTABLE * tbl, register HASHDATUM key, register HASHDATUM data)
{
    register HDBMDATUM hdbmkey, hdbmdata;

    hdbmkey.dat_ptr = key;	/* actually a string */
    hdbmkey.dat_len = strlen(key);
    hdbmdata.dat_ptr = data;	/* just an address */
    hdbmdata.dat_len = 0;	/* no promises */
    return hdbmstore(tbl, hdbmkey, hdbmdata);
}

#if 0
static          HASHDATUM(*hashalloc) ();

static          HDBMDATUM
hdbmalloc(HDBMDATUM key)
{				/* hdbm->hash->hdbm allocator translator */
    register HDBMDATUM hdbmdata;

    hdbmdata.dat_ptr = (*hashalloc) (key.dat_ptr);
    hdbmdata.dat_len = 0;	/* just a string */
    return hdbmdata;
}

/* return any existing entry for key; otherwise call allocator to make one */
static          HASHDATUM
hashentry(register HASHTABLE * tbl, HASHDATUM key, HASHDATUM(*allocator) ())
{
    register HDBMDATUM hdbmkey, hdbmdata;

    hdbmkey.dat_ptr = key;	/* just a string */
    hdbmkey.dat_len = strlen(key);
    hashalloc = allocator;
    hdbmdata = hdbmentry(tbl, hdbmkey, hdbmalloc);
    return hdbmdata.dat_ptr;
}

#endif

HASHDATUM			/* data corresponding to key */
hashfetch(HASHTABLE * tbl, register HASHDATUM key)
{
    register HDBMDATUM hdbmkey, hdbmdata;

    hdbmkey.dat_ptr = key;	/* actually a string */
    hdbmkey.dat_len = strlen(key);
    hdbmdata = hdbmfetch(tbl, hdbmkey);
    return hdbmdata.dat_ptr;	/* just an address */
}

int
hashdelete(HASHTABLE * tbl, register HASHDATUM key)
{
    register HDBMDATUM hdbmkey;

    hdbmkey.dat_ptr = key;	/* actually a string */
    hdbmkey.dat_len = strlen(key);
    return hdbmdelete(tbl, hdbmkey);
}

struct translate {
    char           *realhook;
    int             (*func) ();
};

static int
hdbmtohash(HDBMDATUM key, HDBMDATUM data, char *hook)
{
    register struct translate *thp = (struct translate *) hook;

    (*thp->func) (key.dat_ptr, data.dat_ptr, thp->realhook);
}

/*
 * arrange that at each node, hdbmtohash gets called to map the
 * HDBMDATUM arguments to HASHDATUM arguments.  this also demonstrates
 * how to use the hook argument.
 */
void
                hashwalk(HASHTABLE * tbl, int (*nodefunc) (), char *hook)
 /* hook		(void *) really */
{
    struct translate transhook;
    register struct translate *tp = &transhook;

    tp->realhook = hook;
    tp->func = nodefunc;
    hdbmwalk(tbl, hdbmtohash, (char *) tp);
}
