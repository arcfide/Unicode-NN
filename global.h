/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Global declarations and definitions.
 */

#ifndef _NN_GLOBAL_H
#define _NN_GLOBAL_H 1

#include <stdio.h>

/*
 *	Various constants and types
 */

typedef int8    attr_type;
typedef int32   flag_type;

#ifdef I286_BUG
#define FLAG(n)	(1L<<(n-1))
#else
#define FLAG(n)	(((flag_type)1)<<(n-1))
#endif

typedef long    article_number;
typedef int32   group_number;
typedef uint32  time_stamp;

typedef int     (*fct_type) ();
#define CALL(fct)  (* (fct))
#define NULL_FCT  (fct_type)NULL

/* frequently used characters */

#define NUL	'\0'
#define TAB	'\t'
#define NL	'\n'
#define CR	'\r'
#define BS	'\b'
#define SP	' '

/* misc macros */

#define fl fflush(stdout)

#ifdef CONTROL_
#undef CONTROL_
#endif

#define CONTROL_(c)	(c&037)

#ifndef HAVE_STRCHR
#define	strrchr		rindex
#define strchr		index
#endif

#ifdef SIGNAL_HANDLERS_ARE_VOID
#define sig_type void
#else
#define sig_type int
#endif


/* define types of own functions */

#define	OPEN_READ	0	/* open for reading */
#define	OPEN_UPDATE	1	/* open/create for update */
#define	OPEN_CREATE	2	/* create/truncate for write */
#define	OPEN_APPEND	3	/* open for append */
#define OPEN_CREATE_RW	4	/* create for read/write */

#define	DONT_CREATE	0x40	/* return if file does not exist */
#define	MUST_EXIST	0x80	/* fatal error if cannot open */
#define	OPEN_UNLINK	0x100	/* unlink after open (not OPEN_UPDATE) */


/*
 *	Other external definitions
 *
 *	NOTICE: the distinction between pointers and arrays is important
 *		here (they are global variables - not function arguments)
 */

extern char
               *nn_directory, *lib_directory, version_id[];

extern int      use_nntp;	/* 1 iff we are using nntp */

extern int
                s_hangup,	/* hangup signal */
                s_keyboard,	/* keyboard signal */
                s_pipe,		/* broken pipe */
                s_redraw;	/* continue signal after stop */

extern int      who_am_i;

#define I_AM_MASTER	0
#define I_AM_NN		1
#define I_AM_ADMIN	2
#define I_AM_CHECK	3
#define I_AM_TIDY	4
#define I_AM_EMACS	5
#define I_AM_GOBACK	6
#define I_AM_POST	7
#define I_AM_GREP	8
#define I_AM_DAILY	9
#define I_AM_SPEW	10
#define I_AM_EXPIRE	11
#define I_AM_ACCT	12
#define I_AM_VIEW	13

extern uid_t    user_id;

extern int      process_id;

extern int      errno;

/*
 *	Storage management
 */

#define	newobj(type, nelt) \
    (type *)mem_obj(sizeof(type), (int32)(nelt))

#define	newstr(nelt) \
    mem_str((int32)(nelt))

#define	resizeobj(obj, type, nelt) \
    (type *)mem_resize((char *)(obj), sizeof(type), (int32)(nelt))

#define	clearobj(obj, type, nelt) \
    mem_clear((char *)(obj), sizeof(type), (int32)(nelt))

#define	freeobj(obj) mem_free((char *)(obj))

#define quicksort(a,n,t,f) qsort((char *)(a), (long)(n), sizeof(t), (int (*)(const void *,const void *)) f)

#include "data.h"

/*
 *	db external data
 */

extern master_header master;

/* group headers */

#ifdef MALLOC_64K_LIMITATION
extern group_header **active_groups, **sorted_groups;
#else
extern group_header *active_groups, **sorted_groups;
#endif

/* current group information */

extern char     group_path_name[];
extern char    *group_file_name;

extern group_header *current_group, *group_sequence, *rc_sequence;


int             l_g_index, s_g_first;

#define	Loop_Groups_Number(num) \
    for (num = 0; num < master.number_of_groups; num++)

#ifdef MALLOC_64K_LIMITATION
#define Loop_Groups_Header(gh) \
    for (l_g_index = 0; \
	 (l_g_index < master.number_of_groups) && \
	 (gh = active_groups[l_g_index]) ;\
	 l_g_index++)
#else
#define Loop_Groups_Header(gh) \
    for (l_g_index = 0, gh=active_groups; \
	 l_g_index < master.number_of_groups; \
	 l_g_index++, gh++)
#endif

#define Loop_Groups_Sorted(gh) \
    for (l_g_index = s_g_first; \
	 (l_g_index < master.number_of_groups) && \
	 (gh = sorted_groups[l_g_index]) ;\
	 l_g_index++)

#define	Loop_Groups_Sequence(gh) \
    for (gh = group_sequence; gh; gh = gh->next_group)

#define	Loop_Groups_Newsrc(gh) \
    for (gh = rc_sequence; gh; gh = gh->newsrc_seq)

#ifdef MALLOC_64K_LIMITATION
#define ACTIVE_GROUP(n)	active_groups[n]
#else
#define ACTIVE_GROUP(n)	&active_groups[n]
#endif

/* 8 bit support (ISO 8859/...) */

#ifdef HAVE_8BIT_CTYPE

#ifdef isascii
#undef isascii
#endif

#define isascii(c)	1

#define iso8859(c)	(isprint(c))
#else
#define iso8859(c)	((c) & 0x60)
#endif


/*
 *	Some systems don't define these in <sys/stat.h>
 */

#ifndef S_IFMT
#define	S_IFMT	0170000		/* type of file */
#define S_IFDIR	0040000		/* directory */
#define S_IFREG	0100000		/* regular */
#endif

/* global.c */

sig_type        catch_hangup(int);
int             init_global(void);
int             init_global2(void);
void            new_temp_file(void);
FILE           *open_file(char *, int);
FILE           *open_file_search_path(char *, int);
int             fgets_multi(char *, int, FILE *);
char           *relative(char *, char *);
char           *mk_file_name(char *, char *);
char           *home_relative(char *);
char           *substchr(char *, char, char);
char           *copy_str(char *);
time_t          m_time(FILE *);
time_t          file_exist(char *, char *);
int             cmp_file(char *, char *);
int             copy_file(char *, char *, int);
int             move_file(char *, char *, int);
int             save_old_file(char *, char *);
void            sys_error(char *,...);
int             sys_warning(char *,...);
int             log_entry(int, char *,...);
char           *user_name(void);
time_t          cur_time(void);
char           *date_time(time_t);
char           *plural(long);
char           *mem_obj(unsigned, int32);
char           *mem_str(int32);
char           *mem_resize(char *, unsigned, int32);
void            mem_clear(char *, unsigned, int32);
void            mem_free(char *);
int             nn_truncate(char *, off_t);
char           *strsave(char *);
char           *str3save(char *, char *, char *);
char           *fgetstr(FILE *);
int             getline(char *, int);
extern char    *tmp_directory;
extern char    *nntp_cache_dir;
#endif				/* _NN_GLOBAL_H */
