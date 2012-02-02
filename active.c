/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	read/update incore copy of active file
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "active.h"
#include "db.h"
#include "master.h"
#include "newsrc.h"

void
read_active_file(FILE * act, FILE * copy)
{
    char            line[512];
    register char  *cp, *name;
    register group_header *gh, *gh1;
    int             must_update;
    register flag_type old_flag;

    Loop_Groups_Header(gh) {
	gh->master_flag &= ~M_VALID;
	gh->first_a_article = 0;
	gh->last_a_article = 0;
    }

    while (fgets(line, 512, act)) {
	if (copy != NULL)
	    fputs(line, copy);
	must_update = 0;

	cp = line;
	while (*cp && isspace(*cp))
	    cp++;		/* eat blank lines */
	if (*cp == NUL || *cp == '#')
	    continue;

	/* cp -> NAME space 00888 ... nl */
	name = cp;
	while (*cp != ' ')
	    cp++;
	*cp++ = NUL;

	gh = lookup_no_alias(name);
	if (gh == NULL) {
	    /* new group */
	    gh = add_new_group(name);
	    if (gh == NULL)
		continue;
	    must_update = 1;
	}
	while (*cp && isspace(*cp))
	    cp++;
	gh->last_a_article = atol(cp);

	while (*cp && isdigit(*cp))
	    cp++;
	while (*cp && isspace(*cp))
	    cp++;

	if (*cp == NUL) {
	    log_entry('E', "Error in active file for entry %s", name);
	    continue;
	}
	gh->first_a_article = atol(cp);
	if (gh->first_a_article == 0)
	    gh->first_a_article = 1;
	while (*cp && isdigit(*cp))
	    cp++;
	while (*cp && isspace(*cp))
	    cp++;

	gh->master_flag |= M_VALID;
	if (gh->master_flag & M_IGNORE_G)
	    continue;

	old_flag = gh->master_flag &
	    (M_IGNORE_A | M_MODERATED | M_NOPOST | M_ALIASED);
	gh->master_flag &=
	    ~(M_IGNORE_A | M_MODERATED | M_NOPOST | M_ALIASED);

	switch (*cp) {
	    default:
		break;

	    case 'x':
		gh->master_flag |= M_IGNORE_A;
		if ((old_flag & (M_IGNORE_A | M_ALIASED)) == M_IGNORE_A)
		    continue;
		must_update++;
		break;

	    case 'm':
		gh->master_flag |= M_MODERATED;
		if (old_flag & M_MODERATED)
		    continue;
		must_update++;
		break;

	    case 'n':
		gh->master_flag |= M_NOPOST;
		if (old_flag & M_NOPOST)
		    continue;
		must_update++;
		break;

	    case '=':
		while (*++cp && isspace(*cp))
		    cp++;
		name = cp;
		while (*cp && !isspace(*cp))
		    cp++;
		*cp = NUL;
		if (old_flag & M_ALIASED) {
		    /* quick check: has the alias changed */
		    int32           n = (int32) gh->data_write_offset;
		    if (n >= 0 && n < master.number_of_groups) {
			gh1 = ACTIVE_GROUP(n);
			if (strcmp(gh1->group_name, name) == 0) {
			    gh->master_flag |= M_ALIASED | M_IGNORE_A;
			    continue;
			}
		    }
		}
		gh1 = lookup_no_alias(name);
		if (gh1 == NULL) {
		    /* '=unknown.group' is treated just like 'x' */
		    if ((old_flag & M_IGNORE_A) == 0)
			log_entry('R', "Group %s aliased to unknown group (%s)",
				  gh->group_name, name);
		    gh->master_flag |= M_IGNORE_A;
		    if ((old_flag & (M_IGNORE_A | M_ALIASED)) == M_IGNORE_A)
			continue;
		} else {
		    gh->master_flag |= M_ALIASED | M_IGNORE_A;
		    gh->data_write_offset = (long) gh1->group_num;
		}
		must_update = 1;
		break;
	}

	if ((old_flag & M_ALIASED) && (gh->master_flag & M_ALIASED) == 0) {
	    gh->data_write_offset = 0;
	    gh->master_flag |= M_MUST_CLEAN;
	    continue;
	}
	if (must_update && who_am_i == I_AM_MASTER)
	    db_write_group(gh);
    }
}
