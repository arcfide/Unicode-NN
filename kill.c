/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Kill file handling
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "db.h"
#include "kill.h"
#include "match.h"
#include "menu.h"
#include "regexp.h"
#include "nn_term.h"

/* prototypes further down */

int             killed_articles;
int             dflt_kill_select = 30;
int             kill_file_loaded = 0;
int             kill_debug = 0;
int             kill_ref_count = 0;

extern char    *temp_file;
extern char     delayed_msg[];

static char     KILL_FILE[] = "kill";
static char     COMPILED_KILL[] = "KILL.COMP";

#define COMP_KILL_MAGIC	0x4b694c6f	/* KiLo */

/*
 * kill flags
 */

#define COMP_KILL_ENTRY		0x80000000

#define GROUP_REGEXP		0x01000000
#define GROUP_REGEXP_HDR	0x02000000

#define AND_MATCH		0x00020000
#define OR_MATCH		0x00010000

#define	KILL_CASE_MATCH		0x00000100
#define KILL_ON_REGEXP		0x00000200
#define KILL_UNLESS_MATCH	0x00000400

#define	AUTO_KILL		0x00000001
#define AUTO_SELECT		0x00000002
#define ON_SUBJECT		0x00000004
#define	ON_SENDER		0x00000008
#define ON_FOLLOW_UP		0x00000010
#define ON_ANY_REFERENCES	0x00000020
#define ON_NOT_FOLLOW_UP	0x00000040

/*
 * external flag representation
 */

#define	EXT_AUTO_KILL		'!'
#define EXT_AUTO_SELECT		'+'
#define EXT_KILL_UNLESS_MATCH	'~'
#define EXT_ON_FOLLOW_UP	'>'
#define EXT_ON_NOT_FOLLOW_UP	'<'
#define EXT_ON_ANY_REFERENCES	'a'
#define EXT_ON_SUBJECT		's'
#define	EXT_ON_SENDER		'n'
#define	EXT_KILL_CASE_MATCH	'='
#define EXT_KILL_ON_REGEXP	'/'
#define EXT_AND_MATCH		'&'
#define EXT_OR_MATCH		'|'

/*
 * period = nnn DAYS
 */

#define	DAYS	* 24 * 60 * 60


/*
 * kill_article
 *
 *	return 1 to kill article, 0 to include it
 */

typedef struct kill_list_entry {
    flag_type       kill_flag;
    char           *kill_pattern;
    regexp         *kill_regexp;
    struct kill_list_entry *next_kill;
}               kill_list_entry;

static kill_list_entry *kill_tab;
static char    *kill_patterns;

static kill_list_entry *global_kill_list = NULL;
static kill_list_entry latest_kl_entry;

typedef struct {
    regexp         *group_regexp;
    kill_list_entry *kill_entry;
}               kill_group_regexp;

static kill_group_regexp *group_regexp_table = NULL;
static int      regexp_table_size = 0;
static kill_list_entry *regexp_kill_list = NULL;
static group_header *current_kill_group = NULL;

/* kill.c */

static void     build_regexp_kill(void);
static kill_list_entry *exec_kill(register kill_list_entry * kl, register article_header * ah, int *unlessp, int do_kill, int do_select);
static void     fput_pattern(register char *p, register FILE * f);
static char    *get_pattern(register char *p, int *lenp, int more);
static int      compile_kill_file(void);
static void     free_kill_list(register kill_list_entry * kl);
static int      print_kill(register kill_list_entry * kl);

/*
 *	Build regexp_kill_list for current_group
 */

static void
build_regexp_kill(void)
{
    register kill_group_regexp *tb;
    register int    n, used_last;
    register char  *name;

    regexp_kill_list = NULL;
    current_kill_group = current_group;
    name = current_group->group_name;
    used_last = 0;		/* get AND_MATCH/OR_MATCH for free */

    for (n = regexp_table_size, tb = group_regexp_table; --n >= 0; tb++) {
	if (tb->group_regexp != NULL) {
	    used_last = 0;
	    if (!regexec(tb->group_regexp, name))
		continue;
	} else if (!used_last)
	    continue;

	tb->kill_entry->next_kill = regexp_kill_list;
	regexp_kill_list = tb->kill_entry;
	used_last = 1;
    }
}

/*
 *	execute kill patterns on article
 */

static kill_list_entry *
exec_kill(register kill_list_entry * kl, register article_header * ah, int *unlessp, int do_kill, int do_select)
{
    register flag_type flag;
    register char  *string;

    for (; kl != NULL; kl = kl->next_kill) {
	flag = kl->kill_flag;

	if (do_select && (flag & AUTO_SELECT) == 0)
	    goto failed;
	if (do_kill && (flag & AUTO_KILL) == 0)
	    goto failed;

	if (kill_debug && print_kill(kl) < 0)
	    kill_debug = 0;

	if (flag & KILL_UNLESS_MATCH)
	    *unlessp = 1;

	if (flag & ON_ANY_REFERENCES) {
	    if (ah->replies & 0x7f)
		goto match;
	    goto failed;
	}
	if (flag & ON_SUBJECT) {
	    if (flag & ON_FOLLOW_UP) {
		if ((ah->replies & 0x80) == 0)
		    goto failed;
	    } else if (flag & ON_NOT_FOLLOW_UP) {
		if (ah->replies & 0x80)
		    goto failed;
	    }
	    string = ah->subject;
	} else
	    string = ah->sender;

	if (flag & KILL_CASE_MATCH) {
	    if (flag & KILL_ON_REGEXP) {
		if (regexec(kl->kill_regexp, string))
		    goto match;
	    } else if (strcmp(kl->kill_pattern, string) == 0)
		goto match;
	} else if (flag & KILL_ON_REGEXP) {
	    if (regexec_fold(kl->kill_regexp, string))
		goto match;
	} else if (strmatch_fold(kl->kill_pattern, string))
	    goto match;

failed:
	if ((flag & AND_MATCH) == 0)
	    continue;

	do			/* skip next */
	    kl = kl->next_kill;
	while (kl && (kl->kill_flag & AND_MATCH));
	if (kl)
	    continue;
	break;

match:
	if (kill_debug) {
	    pg_next();
	    tprintf("%sMATCH\n", flag & AND_MATCH ? "PARTIAL " : "");
	}
	if (flag & AND_MATCH)
	    continue;
	break;
    }
    return kl;
}


int
kill_article(article_header * ah)
{
    register kill_list_entry *kl;
    int             unless_match = 0;

    if (kill_debug) {
	clrdisp();
	pg_init(0, 1);
	pg_next();
	so_printf("\1KILL: %s: %s%-.40s (%d)\1",
		  ah->sender, ah->replies & 0x80 ? "Re: " : "",
		  ah->subject, ah->replies & 0x7f);
    }
    kl = exec_kill((kill_list_entry *) (current_group->kill_list), ah,
		   &unless_match, 0, 0);
    if (kl == NULL && group_regexp_table != NULL) {
	if (current_kill_group != current_group)
	    build_regexp_kill();
	kl = exec_kill(regexp_kill_list, ah, &unless_match, 0, 0);
    }
    if (kl == NULL)
	kl = exec_kill(global_kill_list, ah, &unless_match, 0, 0);

    if (kl != NULL) {
	if (kl->kill_flag & AUTO_KILL)
	    goto did_kill;

	if (kl->kill_flag & AUTO_SELECT) {
	    ah->attr = A_AUTO_SELECT;
	    goto did_select;
	}
	goto no_kill;
    }
    if (unless_match)
	goto did_kill;

no_kill:
    if (kill_ref_count && (int) (ah->replies & 0x7f) >= kill_ref_count) {
	if (kill_debug) {
	    pg_next();
	    tprintf("REFERENCE COUNT (%d) >= %d\n",
		    ah->replies & 0x7f, kill_ref_count);
	}
	goto did_kill;
    }
did_select:
    if (kill_debug && pg_end() < 0)
	kill_debug = 0;
    return 0;

did_kill:
    if (kill_debug && pg_end() < 0)
	kill_debug = 0;
    killed_articles++;
    return 1;
}


int
auto_select_article(article_header * ah, int do_select)
{
    register kill_list_entry *kl;
    int             dummy;

    if (do_select == 1) {
	kl = ah->a_group ? (kill_list_entry *) (ah->a_group->kill_list) :
	    (kill_list_entry *) (current_group->kill_list);
	kl = exec_kill(kl, ah, &dummy, !do_select, do_select);
	if (kl == NULL && group_regexp_table != NULL) {
	    if (current_kill_group != current_group)
		build_regexp_kill();
	    kl = exec_kill(regexp_kill_list, ah, &dummy, !do_select, do_select);
	}
	if (kl == NULL)
	    kl = exec_kill(global_kill_list, ah, &dummy, !do_select, do_select);
    } else {
	kl = exec_kill(&latest_kl_entry, ah, &dummy, !do_select, do_select);
    }

    if (kl == NULL)
	return 0;

    if (!do_select)
	killed_articles++;
    return 1;
}


static void
fput_pattern(register char *p, register FILE * f)
{
    register char   c;

    while ((c = *p++)) {
	if (c == ':' || c == '\\')
	    putc('\\', f);
	putc(c, f);
    }
}

static char    *
get_pattern(register char *p, int *lenp, int more)
{
    register char   c, *q, *start;

    start = q = p;
    while ((c = *p++)) {
	if (c == '\\') {
	    c = *p++;
	    if (c != ':' && c != '\\')
		*q++ = '\\';
	    *q++ = c;
	    continue;
	}
	if (more) {
	    if (c == ':')
		break;
	    if (c == NL)
		return NULL;
	} else if (c == NL)
	    break;

	*q++ = c;
    }

    if (c == NUL)
	return NULL;

    *q++ = NUL;
    *lenp = q - start;
    return p;
}

void
enter_kill_file(group_header * gh, char *pattern, register flag_type flag, int days)
{
    FILE           *killf;
    register kill_list_entry *kl;
    regexp         *re;
    char           *str;

    str = copy_str(pattern);

    if ((flag & KILL_CASE_MATCH) == 0)
	fold_string(str);

    if (flag & KILL_ON_REGEXP) {
	re = regcomp(pattern);
	if (re == NULL)
	    return;
    } else
	re = NULL;

    killf = open_file(relative(nn_directory, KILL_FILE), OPEN_APPEND);
    if (killf == NULL) {
	msg("cannot create kill file");
	return;
    }
    if (days >= 0) {
	if (days == 0)
	    days = 30;
	fprintf(killf, "%ld:", (long) (cur_time() + days DAYS));
    }
    if (gh)
	fputs(gh->group_name, killf);
    fputc(':', killf);

    if (flag & KILL_UNLESS_MATCH)
	fputc(EXT_KILL_UNLESS_MATCH, killf);
    if (flag & AUTO_KILL)
	fputc(EXT_AUTO_KILL, killf);
    if (flag & AUTO_SELECT)
	fputc(EXT_AUTO_SELECT, killf);
    if (flag & ON_FOLLOW_UP)
	fputc(EXT_ON_FOLLOW_UP, killf);
    if (flag & ON_NOT_FOLLOW_UP)
	fputc(EXT_ON_NOT_FOLLOW_UP, killf);
    if (flag & ON_ANY_REFERENCES)
	fputc(EXT_ON_ANY_REFERENCES, killf);
    if (flag & ON_SENDER)
	fputc(EXT_ON_SENDER, killf);
    if (flag & ON_SUBJECT)
	fputc(EXT_ON_SUBJECT, killf);
    if (flag & KILL_CASE_MATCH)
	fputc(EXT_KILL_CASE_MATCH, killf);
    if (flag & KILL_ON_REGEXP)
	fputc(EXT_KILL_ON_REGEXP, killf);
    fputc(':', killf);

    fput_pattern(pattern, killf);
    fputc(NL, killf);

    fclose(killf);
    rm_kill_file();

    kl = newobj(kill_list_entry, 1);

    latest_kl_entry.kill_pattern = kl->kill_pattern = str;
    latest_kl_entry.kill_regexp = kl->kill_regexp = re;
    latest_kl_entry.kill_flag = kl->kill_flag = flag;
    latest_kl_entry.next_kill = NULL;

    if (gh) {
	kl->next_kill = (kill_list_entry *) (gh->kill_list);
	gh->kill_list = (char *) kl;
    } else {
	kl->next_kill = global_kill_list;
	global_kill_list = kl;
    }
}


typedef struct {
    group_number    ck_group;
    flag_type       ck_flag;
    long            ck_pattern_index;
}               comp_kill_entry;

typedef struct {
    long            ckh_magic;
    time_t          ckh_db_check;
    off_t           ckh_pattern_offset;
    long            ckh_pattern_size;
    long            ckh_entries;
    long            ckh_regexp_size;
}               comp_kill_header;


int
kill_menu(article_header * ah)
{
    int             days;
    register flag_type flag;
    char           *mode1, *mode2;
    char           *pattern, *dflt, *days_str, buffer[512];
    group_header   *gh;

    days = dflt_kill_select % 100;
    flag = (dflt_kill_select / 100) ? AUTO_SELECT : AUTO_KILL;
    prompt("\1AUTO\1 (k)ill or (s)elect (CR => %s subject %d days) ",
	   flag == AUTO_KILL ? "Kill" : "Select", days);

    switch (get_c()) {
	case CR:
	case NL:
	    if (ah == NULL) {
		ah = get_menu_article();
		if (ah == NULL)
		    return -1;
	    }
	    strcpy(buffer, ah->subject);
	    enter_kill_file(current_group, buffer,
			    flag | ON_SUBJECT | KILL_CASE_MATCH, days);
	    msg("DONE");
	    return 1;

	case 'k':
	case 'K':
	case '!':
	    flag = AUTO_KILL;
	    mode1 = "KILL";
	    break;
	case 's':
	case 'S':
	case '+':
	    flag = AUTO_SELECT;
	    mode1 = "SELECT";
	    break;
	default:
	    return -1;
    }

    prompt("\1AUTO %s\1 on (s)ubject or (n)ame  (s)", mode1);

    dflt = NULL;
    switch (get_c()) {
	case 'n':
	case 'N':
	    flag |= ON_SENDER;
	    if (ah)
		dflt = ah->sender;
	    mode2 = "Name";
	    break;
	case 's':
	case 'S':
	case SP:
	case CR:
	case NL:
	    flag |= ON_SUBJECT;
	    if (ah)
		dflt = ah->subject;
	    mode2 = "Subject";
	    break;
	default:
	    return -1;
    }

    prompt("\1%s %s:\1 (%=/) ", mode1, mode2);

    pattern = get_s(dflt, NONE, "%=/", NULL_FCT);
    if (pattern == NULL)
	return -1;
    if (*pattern == NUL || *pattern == '%' || *pattern == '=') {
	if (dflt && *dflt)
	    pattern = dflt;
	else {
	    if ((ah = get_menu_article()) == NULL)
		return -1;
	    pattern = (flag & ON_SUBJECT) ? ah->subject : ah->sender;
	}
	flag |= KILL_CASE_MATCH;
    } else if (*pattern == '/') {
	prompt("\1%s %s\1 (regexp): ", mode1, mode2);

	pattern = get_s(NONE, NONE, NONE, NULL_FCT);
	if (pattern == NULL || *pattern == NUL)
	    return -1;
	flag |= KILL_ON_REGEXP;
    }
    strcpy(buffer, pattern);
    pattern = buffer;

    prompt("\1%s\1 in (g)roup '%s' or in (a)ll groups  (g)",
	   mode1, current_group->group_name);

    switch (get_c()) {
	case 'g':
	case 'G':
	case SP:
	case CR:
	case NL:
	    gh = current_group;
	    break;
	case 'A':
	case 'a':
	    gh = NULL;
	    break;
	default:
	    return -1;
    }

    prompt("\1Lifetime of entry in days\1 (p)ermanent  (30) ");
    days_str = get_s(" 30 days", NONE, "pP", NULL_FCT);
    if (days_str == NULL)
	return -1;

    if (*days_str == NUL) {
	days_str = "30 days";
	days = 30;
    } else if (*days_str == 'p' || *days_str == 'P') {
	days_str = "perm";
	days = -1;
    } else if (isdigit(*days_str)) {
	days = atoi(days_str);
	sprintf(days_str, "%d days", days);
    } else {
	ding();
	return -1;
    }

    prompt("\1CONFIRM\1 %s %s %s%s: %-.35s%s ",
	   mode1, mode2, days_str,
	   (flag & KILL_CASE_MATCH) ? " exact" :
	   (flag & KILL_ON_REGEXP) ? " regexp" : "",
	   pattern, (int) strlen(pattern) > 35 ? "..." : "");
    if (yes(0) <= 0)
	return -1;

    enter_kill_file(gh, pattern, flag, days);

    return (flag & AUTO_KILL) ? 1 : 0;
}

static int
compile_kill_file(void)
{
    FILE           *killf, *compf, *patternf, *dropf;
    comp_kill_header header;
    comp_kill_entry entry;
    time_t          now, age;
    long            cur_line_start;
    char            line[512];
    register char  *cp, *np;
    register int    c;
    group_header   *gh;
    flag_type       flag, fields[10];
    int             any_errors, nfield, nf, len;

    any_errors = 0;
    header.ckh_entries = header.ckh_regexp_size = 0;

    killf = open_file(relative(nn_directory, KILL_FILE),
		      OPEN_READ | DONT_CREATE);
    if (killf == NULL)
	return 0;

    dropf = NULL;

    compf = open_file(relative(nn_directory, COMPILED_KILL), OPEN_CREATE);
    if (compf == NULL)
	goto err1;

    new_temp_file();
    if ((patternf = open_file(temp_file, OPEN_CREATE)) == NULL)
	goto err2;

    msg("Compiling kill file");

    fseek(compf, sizeof(header), 0);

    now = cur_time();

next_entry:

    for (;;) {
	cur_line_start = ftell(killf);

	if (fgets(line, 512, killf) == NULL)
	    break;

	cp = line;
	while (*cp && isascii(*cp) && isspace(*cp))
	    cp++;
	if (*cp == NUL || *cp == '#' || !isascii(*cp))
	    continue;

	if ((np = strchr(cp, ':')) == NULL)
	    goto bad_entry;

	/* optional "age:" */

	if (np != cp && isdigit(*cp)) {
	    *np++ = NUL;
	    age = (time_t) atol(cp);
	    if (age < now)
		goto drop_entry;
	    cp = np;
	    if ((np = strchr(cp, ':')) == NULL)
		goto bad_entry;
	}
	/* "group-name:"  or "/regexp:" or ":" for all groups */

	flag = COMP_KILL_ENTRY;

	if (np == cp) {
	    entry.ck_group = -1;
	    np++;
	} else {
	    *np++ = NUL;
	    if (*cp == '/') {
		entry.ck_group = (long) ftell(patternf);
		cp++;
		len = strlen(cp) + 1;
		if (fwrite(cp, sizeof(char), len, patternf) != len)
		    goto err3;
		flag |= GROUP_REGEXP | GROUP_REGEXP_HDR;
	    } else {
		if ((gh = lookup(cp)) == NULL || (gh->master_flag & M_IGNORE_GROUP)) {
		    tprintf("Unknown/ignored group in kill file: %s\n", cp);
		    any_errors++;
		    goto drop_entry;
		}
		entry.ck_group = gh->group_num;
	    }
	}

	/* flags */

	cp = np;
	nfield = 0;

	for (;;) {
	    switch (*cp++) {
		case EXT_AND_MATCH:
		case EXT_OR_MATCH:
		    fields[nfield++] = flag;
		    flag &= ~(AND_MATCH | ON_SUBJECT | ON_SENDER |
			      KILL_CASE_MATCH | KILL_ON_REGEXP |
			      GROUP_REGEXP_HDR);
		    flag |= (cp[-1] == EXT_AND_MATCH) ? AND_MATCH : OR_MATCH;
		    continue;
		case EXT_AUTO_KILL:
		    flag |= AUTO_KILL;
		    continue;
		case EXT_AUTO_SELECT:
		    flag |= AUTO_SELECT;
		    continue;
		case EXT_ON_FOLLOW_UP:
		    flag |= ON_FOLLOW_UP;
		    continue;
		case EXT_ON_NOT_FOLLOW_UP:
		    flag |= ON_NOT_FOLLOW_UP;
		    continue;
		case EXT_ON_ANY_REFERENCES:
		    flag |= ON_ANY_REFERENCES;
		    continue;
		case EXT_ON_SUBJECT:
		    flag |= ON_SUBJECT;
		    continue;
		case EXT_ON_SENDER:
		    flag |= ON_SENDER;
		    continue;
		case EXT_KILL_CASE_MATCH:
		    flag |= KILL_CASE_MATCH;
		    continue;
		case EXT_KILL_UNLESS_MATCH:
		    flag |= KILL_UNLESS_MATCH;
		    continue;
		case EXT_KILL_ON_REGEXP:
		    flag |= KILL_ON_REGEXP;
		    continue;
		case ':':
		    break;
		case NL:
		    goto bad_entry;
		default:
		    tprintf("Ignored flag '%c' in kill file\n", cp[-1]);
		    any_errors++;
		    continue;
	    }
	    break;
	}

	fields[nfield++] = flag;

	for (nf = 0; --nfield >= 0; nf++) {
	    entry.ck_flag = flag = fields[nf];
	    np = cp;
	    if ((cp = get_pattern(np, &len, nfield)) == NULL)
		goto bad_entry;

	    if ((flag & KILL_CASE_MATCH) == 0)
		fold_string(np);

	    entry.ck_pattern_index = ftell(patternf);

	    if (fwrite((char *) &entry, sizeof(entry), 1, compf) != 1)
		goto err3;

	    if (fwrite(np, sizeof(char), len, patternf) != len)
		goto err3;

	    header.ckh_entries++;
	    if (flag & GROUP_REGEXP)
		header.ckh_regexp_size++;
	}
    }

    header.ckh_pattern_size = ftell(patternf);

    fclose(patternf);
    patternf = open_file(temp_file, OPEN_READ | OPEN_UNLINK);
    if (patternf == NULL)
	goto err2;

    header.ckh_pattern_offset = ftell(compf);

    while ((c = getc(patternf)) != EOF)
	putc(c, compf);

    fclose(patternf);

    rewind(compf);

    header.ckh_magic = COMP_KILL_MAGIC;
    header.ckh_db_check = master.db_created;

    if (fwrite((char *) &header, sizeof(header), 1, compf) != 1)
	goto err2;

    fclose(compf);
    fclose(killf);
    if (dropf != NULL)
	fclose(dropf);

    if (any_errors) {
	tputc(NL);
	any_key(0);
    }
    return 1;

bad_entry:
    tprintf("Incomplete kill file entry:\n%s", line);
    fl;
    any_errors++;

drop_entry:
    if (dropf == NULL) {
	dropf = open_file(relative(nn_directory, KILL_FILE),
			  OPEN_UPDATE | DONT_CREATE);
	if (dropf == NULL)
	    goto next_entry;
    }
    fseek(dropf, cur_line_start, 0);
    fwrite("# ", sizeof(char), 2, dropf);
    goto next_entry;

err3:
    fclose(patternf);
    unlink(temp_file);
err2:
    fclose(compf);
    rm_kill_file();
err1:
    fclose(killf);
    if (dropf != NULL)
	fclose(dropf);

    msg("cannot compile kill file");
    return 0;
}

int
init_kill(void)
{
    FILE           *killf;
    comp_kill_header header;
    comp_kill_entry entry;
    register kill_list_entry *kl;
    register kill_group_regexp *tb;
    register group_header *gh;
    time_t          kill_age, comp_age;
    register long   n;
    int             first_try = 1;

    Loop_Groups_Header(gh)
	gh->kill_list = NULL;

    kill_age = file_exist(relative(nn_directory, KILL_FILE), "frw");
    if (kill_age == 0)
	return 0;

    comp_age = file_exist(relative(nn_directory, COMPILED_KILL), "fr");
again:
    if (comp_age < kill_age && !compile_kill_file())
	return 0;

    kill_tab = NULL;
    kill_patterns = NULL;
    group_regexp_table = NULL;
    regexp_table_size = 0;

    killf = open_file(relative(nn_directory, COMPILED_KILL), OPEN_READ);
    if (killf == NULL)
	return 0;

    if (fread((char *) &header, sizeof(header), 1, killf) != 1)
	goto err;
    /* MAGIC check: format changed or using different hardware */
    if (header.ckh_magic != COMP_KILL_MAGIC)
	goto err;

#ifndef NOV
    /* DB check: if database is rebuilt, group numbers may change */
    if (header.ckh_db_check != master.db_created)
	goto err;
#else
    /* ugly hack for NOV as there isn't a master to check */
    if (first_try)
	goto err;
#endif

    if (header.ckh_entries == 0) {
	fclose(killf);
	kill_file_loaded = 1;
	return 0;
    }
    if (header.ckh_pattern_size > 0) {
	kill_patterns = newstr(header.ckh_pattern_size);
	fseek(killf, header.ckh_entries * sizeof(entry), 1);
	if (fread(kill_patterns, sizeof(char), (int) header.ckh_pattern_size, killf)
	    != header.ckh_pattern_size)
	    goto err;
    } else
	kill_patterns = newstr(1);

    kill_tab = newobj(kill_list_entry, header.ckh_entries);
    if ((regexp_table_size = header.ckh_regexp_size))
	group_regexp_table = newobj(kill_group_regexp, header.ckh_regexp_size);

    tb = group_regexp_table;

    fseek(killf, sizeof(header), 0);
    for (n = header.ckh_entries, kl = kill_tab; --n >= 0; kl++) {
	if (fread((char *) &entry, sizeof(entry), 1, killf) != 1)
	    goto err;
	if (header.ckh_pattern_size <= entry.ck_pattern_index ||
	    entry.ck_pattern_index < 0)
	    goto err;

	kl->kill_pattern = kill_patterns + entry.ck_pattern_index;
	kl->kill_flag = entry.ck_flag;

	if (kl->kill_flag & KILL_ON_REGEXP)
	    kl->kill_regexp = regcomp(kl->kill_pattern);
	else
	    kl->kill_regexp = NULL;

	if (kl->kill_flag & GROUP_REGEXP) {
	    if (kl->kill_flag & GROUP_REGEXP_HDR) {
		if (header.ckh_pattern_size <= entry.ck_group ||
		    entry.ck_group < 0)
		    goto err;
		tb->group_regexp = regcomp(kill_patterns + entry.ck_group);
	    } else
		tb->group_regexp = NULL;
	    tb->kill_entry = kl;
	    tb++;
	} else if (entry.ck_group >= 0) {
	    gh = ACTIVE_GROUP(entry.ck_group);
	    kl->next_kill = (kill_list_entry *) (gh->kill_list);
	    gh->kill_list = (char *) kl;
	} else {
	    kl->next_kill = global_kill_list;
	    global_kill_list = kl;
	}
    }

    fclose(killf);

    kill_file_loaded = 1;
    return 1;

err:
    if (group_regexp_table != NULL)
	freeobj(group_regexp_table);
    if (kill_patterns != NULL)
	freeobj(kill_patterns);
    if (kill_tab != NULL)
	freeobj(kill_tab);

    fclose(killf);
    rm_kill_file();
    if (first_try) {
	first_try = 0;
	comp_age = 0;
	goto again;
    }
    strcpy(delayed_msg, "Error in compiled kill file (ignored)");

    Loop_Groups_Header(gh)
	gh->kill_list = NULL;

    global_kill_list = NULL;
    group_regexp_table = NULL;

    return 0;
}



void
rm_kill_file(void)
{
    unlink(relative(nn_directory, COMPILED_KILL));
}


static void
free_kill_list(register kill_list_entry * kl)
{
    register kill_list_entry *nxt;
    while (kl) {
	nxt = kl->next_kill;
	if (kl->kill_regexp != NULL)
	    freeobj(kl->kill_regexp);
	if ((kl->kill_flag & COMP_KILL_ENTRY) == 0) {
	    if (kl->kill_pattern != NULL)
		freeobj(kl->kill_pattern);
	    freeobj(kl);
	}
	kl = nxt;
    }
}

void
free_kill_entries(void)
{
    register group_header *gh;
    register kill_group_regexp *tb;
    register int    n;

    Loop_Groups_Header(gh)
	if (gh->kill_list) {
	free_kill_list((kill_list_entry *) (gh->kill_list));
	gh->kill_list = NULL;
    }
    free_kill_list(global_kill_list);
    global_kill_list = NULL;

    if ((tb = group_regexp_table)) {
	for (n = regexp_table_size; --n >= 0; tb++)
	    if (tb->group_regexp != NULL)
		freeobj(tb->group_regexp);

	freeobj(group_regexp_table);
	group_regexp_table = NULL;
    }
    if (kill_patterns != NULL)
	freeobj(kill_patterns);
    if (kill_tab != NULL)
	freeobj(kill_tab);
    kill_file_loaded = 0;
}


static flag_type pk_prev_and;

static int
print_kill(register kill_list_entry * kl)
{
    register flag_type flag = kl->kill_flag;

    if (pg_next() < 0)
	return -1;

    if (pk_prev_and)
	tprintf("\r    AND ");
    else
	tprintf("\r%s%s ON ",
		flag & AUTO_KILL ? "AUTO KILL" :
		flag & AUTO_SELECT ? "AUTO SELECT" : "",

		(flag & KILL_UNLESS_MATCH) == 0 ? "" :
		flag & AUTO_KILL ? " UNLESS" :
		flag & AUTO_SELECT ? "" : "KEEP");

    tprintf("%s '%s%.35s'%s\n",
	    flag & ON_SUBJECT ? "SUBJECT" :
	    flag & ON_SENDER ? "NAME" :
	    flag & ON_ANY_REFERENCES ? "ANY REF" : "????",

	    flag & ON_NOT_FOLLOW_UP ? "!Re: " :
	    flag & ON_FOLLOW_UP ? "Re: " : "",
	    kl->kill_pattern,

	    flag & KILL_CASE_MATCH ?
	    (flag & KILL_ON_REGEXP ? " (re exact)" : " (exact)") :
	    (flag & KILL_ON_REGEXP ? " (re fold)" : ""));

    pk_prev_and = flag & AND_MATCH;

    return 0;
}

void
dump_kill_list(void)
{
    register kill_list_entry *kl;

    pg_init(0, 1);
    pg_next();

    kl = (kill_list_entry *) (current_group->kill_list);
    if (current_kill_group != current_group)
	build_regexp_kill();

    if (kl == NULL && regexp_kill_list == NULL) {
	tprintf("No kill entries for %s", current_group->group_name);
    } else {
	so_printf("\1GROUP %s kill list entries\1", current_group->group_name);

	pk_prev_and = 0;
	for (; kl; kl = kl->next_kill)
	    if (print_kill(kl) < 0)
		goto out;

	pk_prev_and = 0;
	for (kl = regexp_kill_list; kl; kl = kl->next_kill)
	    if (print_kill(kl) < 0)
		goto out;

	if (pg_next() < 0)
	    goto out;
    }

    if (pg_next() < 0)
	goto out;
    so_printf("\1GLOBAL kill list entries:\1");

    pk_prev_and = 0;
    for (kl = global_kill_list; kl != NULL; kl = kl->next_kill)
	if (print_kill(kl) < 0)
	    goto out;

out:
    pg_end();
}
