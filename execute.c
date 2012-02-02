/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	UNIX command execution.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include "config.h"
#include "global.h"
#include "execute.h"
#include "folder.h"
#include "nn_term.h"


/* execute.c */

static int      shell_check(void);


int             shell_restrictions = 0;	/* disable shell escapes */

static char    *init_shell = SHELL;
char           *user_shell;
char           *exec_chdir_to = NULL;

extern int      errno;


static int
shell_check(void)
{
    if (shell_restrictions) {
	msg("Restricted operation - not allowed");
	return -1;
    }
    return 0;
}

void
init_execute(void)
{
    if ((user_shell = getenv("SHELL")) == NULL)
	user_shell = SHELL;
}

int
execute(char *path, char **args, int toggle_visual)
{
    int             was_raw, pid, i, status;
    sig_type(*quit) (), (*intr) (), (*tstp) ();

    was_raw = toggle_visual ? visual_off() : unset_raw();

    while ((pid = fork()) == -1)
	sleep(1);

    if (pid == 0) {
	for (i = 3; i < 20; i++)
	    close(i);

	if (exec_chdir_to != NULL)
	    chdir(exec_chdir_to);

	if (execvp(path, args)) {
	    msg("%s: %s", path, strerror(errno));
	    user_delay(2);
	    exit(20);
	}
    }
    quit = signal(SIGQUIT, SIG_IGN);
    intr = signal(SIGINT, SIG_IGN);

#ifdef HAVE_JOBCONTROL
    tstp = signal(SIGTSTP, SIG_DFL);
#endif

    while ((i = wait(&status)) != pid && (i != -1 || errno == EINTR));

    signal(SIGQUIT, quit);
    signal(SIGINT, intr);

#ifdef HAVE_JOBCONTROL
    signal(SIGTSTP, tstp);
#endif

    if (toggle_visual) {
	visual_on();
	if (toggle_visual == 2)
	    s_redraw++;
    }
    if (was_raw)
	nn_raw();

    return (status & 0xff) ? 0x100 : (status >> 8);
}


int
shell_escape(void)
{
    static char     command[FILENAME] = "";
    char           *cmd;
    int             first = 1;

    if (shell_check())
	return 0;

    for (;;) {
	prompt("\1!\1");
	cmd = get_s(command, NONE, NONE, NULL_FCT);
	if (cmd == NULL)
	    return !first;

	if (*cmd == NUL) {
	    if (first)
		run_shell((char *) NULL, 1, 0);
	    return 1;
	}
	strcpy(command, cmd);

	if (run_shell(command, first, 0) < 0)
	    return !first;
	first = 0;
	prompt_line = -1;
    }
}


static char    *exec_sh_args[] = {
    "nnsh",
    (char *) NULL,		/* "-c" or "-i" */
    (char *) NULL,		/* cmdstring */
    (char *) NULL
};

int
run_shell(char *command, int clear, int init_sh)

 /*
  * clear	-2 => no command output (:!!command) - keep visual, output
  * before command: -1 => none, 0 => CR/NL, 1 => clear
  */
 /* init_sh	0 => use user_shell, else use init_shell */
{
    char            cmdstring[512];

    if (shell_check())
	return -1;

    if (command != NULL) {
	if (!expand_file_name(cmdstring, command, 1))
	    return -1;
	exec_sh_args[1] = "-c";
	exec_sh_args[2] = cmdstring;
    } else {
	exec_sh_args[1] = "-i";
	exec_sh_args[2] = NULL;
    }

    if (clear > 0) {
	clrdisp();
	fl;
    } else if (clear == 0) {
	tputc(CR);
	tputc(NL);
    }
    return execute(init_sh ? init_shell : user_shell,
		   exec_sh_args, clear == -2 ? 0 : 1);
}

#ifndef HAVE_JOBCONTROL
static char    *exec_suspend_args[] = {
    "nnsh",
    "-i",
    (char *) NULL
};

#endif

int
suspend_nn(void)
{
    int             was_raw;

    if (shell_check())
	return 0;

    gotoxy(0, Lines - 1);
    clrline();

#ifdef HAVE_JOBCONTROL
    was_raw = visual_off();
    kill(process_id, SIGSTOP);
    visual_on();
    s_redraw++;
    if (was_raw)
	nn_raw();
#else
    execute(user_shell, exec_suspend_args, 2);
#endif

    return 1;
}
