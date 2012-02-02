/*
 *      Copyright (c) 2003 Michael T Pins.  All rights reserved.
 */

int             only_folder_args(char **);
void            parse_save_files(register FILE *);
void            named_group_sequence(char **);
void            normal_group_sequence(void);
void            start_group_search(char *);
group_header   *get_group_search(void);
