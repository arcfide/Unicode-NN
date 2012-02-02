/*
 *      Copyright (c) 2003 Michael T Pins.  All rights reserved.
 */

int             set_variable(char *, int, char *);
void            toggle_variable(char *);
void            lock_variable(char *);
int             test_variable(char *);
void            var_compl_opts(int);
int             var_completion(char *, int);
void            mark_var_stack(void);
int             push_variable(char *);
void            restore_variables(void);
void            disp_variables(int, char *);
void            print_variable_config(FILE *, int);
void            var_options(char **, register char *, flag_type *);
