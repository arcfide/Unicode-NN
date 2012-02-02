/*
 *      Copyright (c) 2005 Michael T Pins.  All rights reserved.
 */

#include "config.h"
#include "global.h"

/* printconf.c */

extern char    *news_directory;
extern char    *news_lib_directory;
extern char    *master_directory;
extern char    *help_directory;
extern char    *bin_directory;
extern char    *db_directory;
extern char    *db_data_directory;
extern char    *tmp_directory;
extern char    *log_file;
extern char     domain[];

void
print_config(FILE * f)
{
    fprintf(f, "VERSION=\"%s\"\n", version_id);

#ifdef NOV
    fprintf(f, "NOV=true\n");

#ifdef DO_NOV_DIGEST
    fprintf(f, "DO_NOV_DIGEST=true\n");
#else
    fprintf(f, "DO_NOV_DIGEST=false\n");
#endif

#ifdef NOV_DIRECTORY
    fprintf(f, "NOV_DIRECTORY=%s\n", NOV_DIRECTORY);
#endif

#ifdef NOV_FILENAME
    fprintf(f, "NOV_FILENAME=%s\n", NOV_FILENAME);
#endif

#else
    fprintf(f, "NOV=false\n");

#ifdef NETWORK_DATABASE
    fprintf(f, "NETWORK_DATABASE=true\n");
#endif

#endif

#ifdef NNTP
    fprintf(f, "NNTP=true\n");
    fprintf(f, "ACTIVE=%s/ACTIVE\n", db_directory);

#ifndef CACHE_DIRECTORY
#define CACHE_DIRECTORY ""
#endif

    fprintf(f, "NNTPCACHE=%s\n", CACHE_DIRECTORY);
#else
    fprintf(f, "NNTP=false\n");
    fprintf(f, "ACTIVE=%s/active\n", news_lib_directory);
#endif

#ifdef DOMAIN
    fprintf(f, "DOMAIN=%s\n", DOMAIN);
#else
    fprintf(f, "domain=%s\n", domain);
#endif

#ifdef HIDDENNET
    fprintf(f, "HIDDENNET=true\n");
#endif

#ifdef HOSTNAME
    fprintf(f, "HOSTNAME=%s\n", HOSTNAME);
#endif

    fprintf(f, "LOG=%s\n", log_file);
    fprintf(f, "TMP=${TMPDIR-%s}\n", tmp_directory);
    fprintf(f, "DB=%s\n", db_directory);
    fprintf(f, "BIN=%s\n", bin_directory);
    fprintf(f, "LIB=%s\n", lib_directory);

    fprintf(f, "RECMAIL=\"%s\"\n", REC_MAIL);
    fprintf(f, "SPOOL=%s\n", news_directory);
    fprintf(f, "NLIB=%s\n", news_lib_directory);
    fprintf(f, "MASTER=%s\n", master_directory);
    fprintf(f, "HELP=%s\n", help_directory);
    fprintf(f, "DBDATA=\"%s\"\n", db_data_directory ? db_data_directory : "");

#ifdef DB_LONG_NAMES
    fprintf(f, "DBSHORTNAME=false\n");
#else
    fprintf(f, "DBSHORTNAME=true\n");
#endif

#ifdef DONT_COUNT_LINES
    fprintf(f, "DONT_COUNT_LINES=true\n");
#endif

#ifdef APPEND_SIGNATURE
    fprintf(f, "APPEND_SIGNATURE=true\n");
#endif

#ifdef STATISTICS
    fprintf(f, "STATISTICS=%d\n", STATISTICS);
#endif

#ifdef ACCOUNTING
    fprintf(f, "ACCOUNTING=true\n");
#endif

#ifdef AUTHORIZE
    fprintf(f, "AUTHORIZE=true\n");
#else
    fprintf(f, "AUTHORIZE=false\n");
#endif

    fprintf(f, "OWNER=%s\n", OWNER);
    fprintf(f, "GROUP=%s\n", GROUP);

#ifdef ART_GREP
    fprintf(f, "ART_GREP=true\n");
#endif

#ifdef CACHE_PURPOSE
    fprintf(f, "CACHE_PURPOSE=true\n");
#endif

}
