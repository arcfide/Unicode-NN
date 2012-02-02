/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2003 Michael T Pins.  All rights reserved.
 *
 *	Include file for generic option parsing
 */

/*
 * To use this routine, you must a table called an Option_Description.
 * Each element in this table describes one possible option:
 *	Its option letter
 *	Its argument type (if any)
 *	Whether an argument is mandatory or optional
 *	The address of the variable holding the option value
 *	The defualt value if argument is optional
 *
 * Example:
 *
 *	A program accepts the following options:
 *		-a	[no value]
 *		-b N	[a numeric value]
 *		-p [N]	[an optional numeric value]
 *		-t S	[a string value]
 *
 * The corresponding option description table would then look like:
 *
 *	#include <options.h>
 *	int a_flg = 1, b_value = 0, p_value = 0;
 *	char *t_string = "default";
 *
 *	Option_Description( options ) {
 *	    'a', Bool_Option(a_flg),
 *	    'b', Int_Option(b_value),
 *	    'p', Int_Option_Optional(p_value, -1),
 *	    't', String_Option(t_string),
 *	    '\0',
 *	 }
 * To parse the argument list - and the contents of the environment variable
 * XXINIT, all that has to be done is to issue the following call:
 *
 *	files = parse_options(argc, argv, "XXINIT", options, NULL);
 *
 * If no environment variable is associated with the program, use NULL as
 * the third parameter.
 *
 * Upon return, the elements argv[1] .. argv[files] will contain
 * the file names (and other 'non-options') that occur in the argument list.
 *
 * The last NULL argument may be replaced by your own 'usage routine'
 * which will be called in the following way:
 *
 *	usage(pname)
 *	char *pname; /+ argv[0] without path +/
 *
 *
 * char *program_name(argv)
 *
 * return a pointer to the last component of argv[0] (the program name with
 * with the path deleted).
 *

 */

#ifndef _NN_OPTIONS_H
#define _NN_OPTIONS_H 1

struct option_descr {
    char            option_letter;
    char            option_type;
    char          **option_address;
    char           *option_default;
};


#define	Option_Description(name) \
    struct option_descr name[] =

#define	Bool_Option(addr) \
    1, (char **)(&addr), (char *)0

#define	String_Option(addr) \
    2, &addr, (char *)0

#define String_Option_Optional(addr, default) \
    3, &addr, default

#define	Int_Option(addr) \
    4, (char **)(&addr), (char *)0

#define	Int_Option_Optional(addr, default) \
    5, (char **)(&addr), (char *)default


char           *program_name(char **);
void            parseargv_variables(char **, int);
int             parse_options(int, char **, char *, struct option_descr[], char *);
#endif				/* _NN_OPTIONS_H */
