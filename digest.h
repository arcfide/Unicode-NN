/*
 *      Copyright (c) 2003 Michael T Pins.  All rights reserved.
 */

#include "news.h"

void            init_digest_parsing(void);
int             is_digest(register char *);
int             get_folder_type(FILE *);
int             get_digest_article(FILE *, news_header_buffer);
int             skip_digest_body(register FILE *);
int             parse_digest_header(FILE *, int, news_header_buffer);
