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
 * general-purpose in-core hashing, dbm interface
 */

#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "hdbm.h"
#include "hdbmint.h"

/* tunable parameters */
#define RETAIN 300		/* retain & recycle this many HASHENTs */

/* fundamental constants */
#define YES 1
#define NO 0

static HASHENT *hereuse = NULL;
static int      reusables = 0;

static HASHENT *healloc(void);

/*
 * Gosling EMACS hash (h = 33*h + *s++) optimized, loop unrolled with
 * "Duff's Device".  Thanks to Chris Torek.
 */
static unsigned
hdbmdef(register HDBMDATUM key)
{
    register unsigned char *s = (unsigned char *) key.dat_ptr;
    register int    len = key.dat_len;
    register unsigned h;
    register int    loop;

#define HASHC   h = (h << 5) + h + *s++;

    h = 0;
    if (len > 0) {
	loop = (len + 8 - 1) >> 3;

	switch (len & (8 - 1)) {
	    case 0:
		do {		/* All fall throughs */
		    HASHC;
	    case 7:
		    HASHC;
	    case 6:
		    HASHC;
	    case 5:
		    HASHC;
	    case 4:
		    HASHC;
	    case 3:
		    HASHC;
	    case 2:
		    HASHC;
	    case 1:
		    HASHC;
		} while (--loop);
	}

    }
    return (h);
}

HASHTABLE      *
hdbmcreate(register unsigned size, unsigned (*hashfunc) ())
 /* size			a crude guide to size */
{
    register HASHTABLE *tbl;
    register HASHENT **hepp;

    /*
     * allocate HASHTABLE and (HASHENT *) array together to reduce the number
     * of malloc calls.  this idiom ensures correct alignment of the array.
     * dmr calls the one-element array trick `unwarranted chumminess with the
     * compiler' though.
     */
    register struct alignalloc {
	HASHTABLE       ht;
	HASHENT        *hepa[1];/* longer than it looks */
    }              *aap;

    aap = (struct alignalloc *)
	malloc(sizeof *aap + (size - 1) * sizeof(HASHENT *));
    if (aap == NULL)
	return NULL;
    tbl = &aap->ht;
    tbl->ht_size = (size == 0 ? 1 : size);	/* size of 0 is nonsense */
    tbl->ht_magic = HASHMAG;
    tbl->ht_hash = (hashfunc == NULL ? hdbmdef : hashfunc);
    tbl->ht_addr = hepp = aap->hepa;
    while (size-- > 0)
	hepp[size] = NULL;
    return tbl;
}

static void
hefree(register HASHENT * hp)
{				/* free a hash entry */
    if (reusables >= RETAIN)	/* compost heap is full? */
	free((char *) hp);	/* yup, just pitch this one */
    else {			/* no, just stash for reuse */
	++reusables;
	hp->he_next = hereuse;
	hereuse = hp;
    }
}

/*
 * free all the memory associated with tbl, erase the pointers to it, and
 * invalidate tbl to prevent further use via other pointers to it.
 */
void
hdbmdestroy(register HASHTABLE * tbl)
{
    register unsigned idx;
    register HASHENT *hp, *next;
    register HASHENT **hepp;
    register int    tblsize;

    if (tbl == NULL || BADTBL(tbl))
	return;
    tblsize = tbl->ht_size;
    hepp = tbl->ht_addr;
    for (idx = 0; idx < tblsize; idx++) {
	for (hp = hepp[idx]; hp != NULL; hp = next) {
	    next = hp->he_next;
	    hp->he_next = NULL;
	    hefree(hp);
	}
	hepp[idx] = NULL;
    }
    tbl->ht_magic = 0;		/* de-certify this table */
    tbl->ht_addr = NULL;
    free((char *) tbl);
}

/*
 * The returned value is the address of the pointer that refers to the
 * found object.  Said pointer may be NULL if the object was not found;
 * if so, this pointer should be updated with the address of the object
 * to be inserted, if insertion is desired.
 */
static HASHENT **
hdbmfind(register HASHTABLE * tbl, HDBMDATUM key)
{
    register HASHENT *hp, *prevhp = NULL;
    register char  *hpkeydat, *keydat = key.dat_ptr;
    register int    keylen = key.dat_len;
    register HASHENT **hepp;
    register unsigned size;

    if (BADTBL(tbl))
	return NULL;
    size = tbl->ht_size;
    if (size == 0)		/* paranoia: avoid division by zero */
	size = 1;
    hepp = &tbl->ht_addr[(*tbl->ht_hash) (key) % size];
    for (hp = *hepp; hp != NULL; prevhp = hp, hp = hp->he_next) {
	hpkeydat = hp->he_key.dat_ptr;
	if (hp->he_key.dat_len == keylen && hpkeydat[0] == keydat[0] &&
	    memcmp(hpkeydat, keydat, keylen) == 0)
	    break;
    }
    /* assert: *(returned value) == hp */
    return (prevhp == NULL ? hepp : &prevhp->he_next);
}

static HASHENT *
healloc(void)
{				/* allocate a hash entry */
    register HASHENT *hp;

    if (hereuse == NULL)
	return (HASHENT *) malloc(sizeof(HASHENT));
    /* pull the first reusable one off the pile */
    hp = hereuse;
    hereuse = hereuse->he_next;
    hp->he_next = NULL;		/* prevent accidents */
    reusables--;
    return hp;
}

int
hdbmstore(register HASHTABLE * tbl, HDBMDATUM key, HDBMDATUM data)
{
    register HASHENT *hp;
    register HASHENT **nextp;

    if (BADTBL(tbl))
	return NO;
    nextp = hdbmfind(tbl, key);
    if (nextp == NULL)
	return NO;
    hp = *nextp;
    if (hp == NULL) {		/* absent; allocate an entry */
	hp = healloc();
	if (hp == NULL)
	    return NO;
	hp->he_next = NULL;
	hp->he_key = key;
	*nextp = hp;		/* append to hash chain */
    }
    hp->he_data = data;		/* supersede any old data for this key */
    return YES;
}

/* return any existing entry for key; otherwise call allocator to make one */
HDBMDATUM
hdbmentry(register HASHTABLE * tbl, HDBMDATUM key, HDBMDATUM(*allocator) ())
{
    register HASHENT *hp;
    register HASHENT **nextp;
    static HDBMDATUM errdatum = {NULL, 0};

    if (BADTBL(tbl))
	return errdatum;
    nextp = hdbmfind(tbl, key);
    if (nextp == NULL)
	return errdatum;
    hp = *nextp;
    if (hp == NULL) {		/* absent; allocate an entry */
	hp = healloc();
	if (hp == NULL)
	    return errdatum;
	hp->he_next = NULL;
	hp->he_key = key;
	hp->he_data = (*allocator) (key);
	*nextp = hp;		/* append to hash chain */
    }
    return hp->he_data;
}

int
hdbmdelete(register HASHTABLE * tbl, HDBMDATUM key)
{
    register HASHENT *hp;
    register HASHENT **nextp;

    nextp = hdbmfind(tbl, key);
    if (nextp == NULL)
	return NO;
    hp = *nextp;
    if (hp == NULL)		/* absent */
	return NO;
    *nextp = hp->he_next;	/* skip this entry */
    hp->he_next = NULL;
    hp->he_data.dat_ptr = hp->he_key.dat_ptr = NULL;
    hefree(hp);
    return YES;
}

HDBMDATUM			/* data corresponding to key */
hdbmfetch(register HASHTABLE * tbl, HDBMDATUM key)
{
    register HASHENT *hp;
    register HASHENT **nextp;
    static HDBMDATUM errdatum = {NULL, 0};

    if (BADTBL(tbl))
	return errdatum;
    nextp = hdbmfind(tbl, key);
    if (nextp == NULL)
	return errdatum;
    hp = *nextp;
    if (hp == NULL)		/* absent */
	return errdatum;
    else
	return hp->he_data;
}

/*
 * visit each entry by calling nodefunc at each, with key, data and hook as
 * arguments.  hook is an attempt to allow side-effects and reentrancy at
 * the same time.
 */
void
                hdbmwalk(HASHTABLE * tbl, register int (*nodefunc) (), register char *hook)
 /* hook			(void *) really */
{
    register unsigned idx;
    register HASHENT *hp;
    register HASHENT **hepp;
    register int    tblsize;

    if (BADTBL(tbl))
	return;
    hepp = tbl->ht_addr;
    tblsize = tbl->ht_size;
    for (idx = 0; idx < tblsize; idx++)
	for (hp = hepp[idx]; hp != NULL; hp = hp->he_next)
	    (*nodefunc) (hp->he_key, hp->he_data, hook);
}
