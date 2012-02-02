/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2003 Michael T Pins.  All rights reserved.
 *
 *	Directory access routines (faked ones).
 *
 *	If HAVE_DIRECTORY is not defined, traditional sysV directory
 *	structure is assumed unless NOT_SYS5_DIRECTORY is defined (in
 *	which case a shell command is used to list a directory).
 */

#ifndef _NN_DIR_H
#define _NN_DIR_H 1

#ifndef HAVE_DIRECTORY

#ifndef NOT_SYS5_DIRECTORY
static struct dir_entry {
    short int       d_ino;
    char            d_name[15];
}               dirbuf;

#define Direntry struct dir_entry
#define	DIR FILE
#define opendir(name)	fopen(name, "r")
#define readdir(dirp)	(fread(&dirbuf, 16, 1, dirp) == 1 ? &dirbuf : NULL)
#undef rewinddir
#define rewinddir(dirp)	rewind(dirp)
#define closedir(dirp)	fclose(dirp)

#define HAVE_DIRECTORY
#define FAKED_DIRECTORY
#endif

#endif

int             list_directory(char *, char *);
int             next_directory(register char *, int);
int             compl_help_directory(void);
void            close_directory(void);
#endif				/* _NN_DIR_H */
