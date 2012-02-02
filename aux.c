/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Response handling.
 */

/* aux.c */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "config.h"
#include "global.h"
#include "chset.h"
#include "execute.h"
#include "news.h"
#include "nn.h"
#include "nntp.h"
#include "reroute.h"
#include "nn_term.h"

extern char    *temp_file;

extern int      novice;
extern char     delayed_msg[];
extern char    *pager;
extern char    *inews_program;
extern int      inews_pipe_input;
extern int      use_mmdf_folders;
extern int      data_bits;
extern int      append_sig_post;
extern char    *home_directory;

int             empty_answer_check = 1;	/* reject replies that are not edited */
int             query_signature = 1;	/* query user */
char           *editor_program = NULL;
int             use_ed_line = 1;
char           *sign_type = SIGN_TYPE;
char           *spell_checker = NULL;
char           *mail_alias_expander = NULL;
char           *mailer_program = REC_MAIL;
int             mailer_pipe_input = 1;
int             response_check_pause = 0;	/* time to wait for
						 * background cmds */
char           *response_dflt_answer = "send";

#ifndef NNTP
/*
 * invoke aux shell script with suitable arguments
 *
 * WARNING: record may be NULL, so it must be the last argument!!
 */

static void
aux_param_bool(FILE * f, char *name, int on)
{
    fprintf(f, "%s=%s\n", name, on ? "true" : "false");
}

static void
aux_param_str(FILE * f, char *name, char *str)
{
    int             xport = (*name == '*');

    if (xport)
	name++;
    fprintf(f, "%s='%s'\n", name, str != NULL ? str : "");
    if (xport)
	fprintf(f, "export %s\n", name);
}

static void
aux_param_int(FILE * f, char *name, int i)
{
    fprintf(f, "%s=%d\n", name, i);
}

int
aux_sh(article_header * ah, char *script, char *prog, char *action, char *record, char *sent_fmt, int append_sig, int ed_line)
{
    FILE           *param;
    char           *args[10], *fn;
    char            route[512], *poster;
    register char **ap = args;

    if (strncmp(prog, "COMPLETE", 8) == 0)
	goto no_params;

    param = open_file(relative(nn_directory, ".param"), OPEN_CREATE);
    if (param == NULL) {
	strcpy(delayed_msg, "cannot create .param file for aux script");
	return 1;
    }
    if (getenv("LOGNAME") == NULL)
	aux_param_str(param, "*LOGNAME", user_name());

    /*
     * with the nn-directory variable, the aux script needs to know where to
     * find the .param file, can't always look in $HOME/.nn
     */
    if (!getenv("NNDIR")) {
	char            ebuf[FILENAME + 8];
	sprintf(ebuf, "NNDIR=%s", nn_directory);
	putenv(ebuf);
    }
    if (strcmp(prog, "cancel") == 0) {
	aux_param_str(param, "ART_ID", action);	/* article id for cancel */
	aux_param_str(param, "GROUP", record);	/* group name for cancel */
	if (news.ng_dist)
	    aux_param_str(param, "DIST", news.ng_dist);	/* dist for cancel */
	else
	    aux_param_str(param, "DIST", "none");	/* dist for cancel */
    } else {
	aux_param_str(param, "FIRST_ACTION", action);
	aux_param_str(param, "RECORD", record);
	aux_param_str(param, "WORK", temp_file);
	aux_param_int(param, "ED_LINE", ed_line);

	aux_param_bool(param, "NOVICE", novice);
	aux_param_bool(param, "EMPTY_CHECK", empty_answer_check);
	aux_param_bool(param, "APPEND_SIG", append_sig);
	aux_param_bool(param, "QUERY_SIG", append_sig && query_signature);

	if (editor_program != NULL)
	    aux_param_str(param, "EDITOR", editor_program);
	aux_param_str(param, "SPELL_CHECKER", spell_checker);
	aux_param_str(param, "ALIAS_EXPANDER", mail_alias_expander);
	aux_param_str(param, "PAGER", pager);
	aux_param_str(param, "MAILER", mailer_program);
	aux_param_bool(param, "MAILER_PIPE", mailer_pipe_input);
	aux_param_int(param, "WAIT_PERIOD", response_check_pause);
	aux_param_str(param, "DFLT_ANSW", response_dflt_answer);
	aux_param_str(param, "POST", inews_program);
	aux_param_bool(param, "POST_PIPE", inews_pipe_input);
	aux_param_str(param, "MMDF_SEP", use_mmdf_folders ? "\1\1\1\1" : "");

	aux_param_str(param, "CHSET_NAME", curchset->cs_name);
	if (curchset->cs_width != 0)
	    aux_param_int(param, "CHSET_WIDTH", curchset->cs_width);
	else
	    aux_param_int(param, "CHSET_WIDTH", data_bits);

	if (current_group != NULL) {
	    aux_param_str(param, "*G", current_group->group_name);
	    if (ah == NULL)
		fn = NULL;
	    else
		fn = group_path_name;
	    aux_param_str(param, "*A", fn != NULL ? fn : "");
	}
	poster = NULL;
	if (ah != NULL && strcmp(prog, "follow") == 0) {
	    poster = news.ng_reply != NULL ? news.ng_reply : news.ng_from;
	    if (poster && reroute(route, poster))
		poster = route;
	    else
		poster = NULL;
	}
	aux_param_str(param, "POSTER_ADR", poster);
    }

    fclose(param);

no_params:
    stop_usage();

    /* OBS: relative() returns ptr to static data below */
    *ap++ = "nnaux";
    *ap++ = script != NULL ? script : relative(lib_directory, "aux");
    *ap++ = prog;
    *ap++ = temp_file;
    *ap++ = NULL;

    if (execute(SHELL, args, 1)) {
	sprintf(delayed_msg, sent_fmt, " not");
	return 1;
    }
    sprintf(delayed_msg, sent_fmt, "");
    return 0;
}

#else				/* NNTP */

#include <sys/stat.h>
#include <pwd.h>

static char    *
tempsuffix(char *name, char *suffix)
{
    static char     concat_name[FILENAME];

    sprintf(concat_name, "%s%s", name, suffix);
    return concat_name;
}

static FILE    *
find_signature(void)
{
    struct passwd  *passwd;
    char           *dotdir;
    char            sigfile[FILENAME];
    FILE           *fp;

    passwd = getpwuid(getuid());
    if (passwd == NULL)
	return (NULL);
    dotdir = passwd->pw_dir;

    if (dotdir[0] == '~') {
	(void) strcpy(sigfile, passwd->pw_dir);
	(void) strcat(sigfile, &dotdir[1]);
    } else {
	(void) strcpy(sigfile, dotdir);
    }
    (void) strcat(sigfile, "/");
    (void) strcat(sigfile, ".signature");

#ifdef DEBUG
    fprintf(stderr, "sigfile = '%s'\n", sigfile);
    sleep(2);
#endif

    fp = fopen(sigfile, "r");

#ifdef DEBUG
    if (fp != NULL) {
	fprintf(stderr, "sigfile opened OK\n");
	sleep(2);
    }
#endif

    return fp;
}

#define MAX_SIGNATURE   4

static void
append_signature(FILE * sigfile)
{
    char            line[256];
    char           *cp;
    int             count = 0;
    FILE           *temp;

    temp = open_file(temp_file, OPEN_APPEND);

#ifdef DEBUG
    if (temp)
	fprintf(stderr, "temp = %s\n", temp_file);
    else
	fprintf(stderr, "temp = NULL\n");
#endif

    fprintf(temp, "-- \n");
    while (fgets(line, sizeof(line), sigfile)) {
	count++;
	if (count > MAX_SIGNATURE) {
	    fprintf(stderr, "Warning: .signature files should be no longer than %d lines.\n", MAX_SIGNATURE);
	    fprintf(stderr, "(Only %d lines of your .signature were posted.)\n", MAX_SIGNATURE);
	    break;
	}
	if ((cp = strchr(line, '\n')))
	    *cp = '\0';
	fprintf(temp, "%s\n", line);
    }
    (void) fclose(temp);

#ifdef DEBUG
    printf(".signature appended (from %s)\n", sigfile);
    sleep(10);
#endif
}


/*
 * return codes:	0 post succeeded
 *			1 post failed
 *			22 aborted
 *			   unable to stat temp file
 *			   original and copy identical
 *			   hold response
 */

int
aux_sh(article_header * ah, char *script, char *prog, char *action, char *record, char *sent_fmt, int append_sig, int ed_line)
{
    FILE           *sigfile, *temp, *tempf;
    char            final[FILENAME], copy[FILENAME];
    char            hdrs[FILENAME], bdy[FILENAME], sgn[FILENAME];
    char            route[512], *poster = NULL;
    int             goodsigntype = 0;
    int             loop = 1, prmpt = 0;
    char            cc[256], pr[80], pr1[80], fname[FILENAME], buf[80];
    char            buf2[10];
    char            lookfor[16], send[8], sent[8], sendpr[8], message[8];
    int             x, act = response_dflt_answer[0];
    struct stat     statb;

    if (strncmp(prog, "COMPLETE", 8) != 0) {
	poster = NULL;
	if (ah != NULL && strcmp(prog, "follow") == 0) {
	    poster = news.ng_reply != NULL ? news.ng_reply : news.ng_from;
	    if (poster && reroute(route, poster))
		poster = route;
	    else
		poster = NULL;
	}
    }
    stop_usage();

    cc[0] = CR;

    if (strncmp(prog, "COMPLETE", 8) == 0) {
	empty_answer_check = 0;
	action = "edit";
	if (strcmp(prog, "COMPLETE.post") == 0)
	    prog = "post";
	else
	    prog = "mail";
	copy_file(relative(nn_directory, "hold.work"), temp_file, 0);
	unlink(relative(nn_directory, "hold.work"));
    }
    temp = open_file(temp_file, OPEN_APPEND);

    if (strcmp(prog, "cancel") == 0) {
	fprintf(temp, "Newsgroups: %s\n", record);
	fprintf(temp, "Subject: cmsg cancel %s\n", action);
	fprintf(temp, "Control: cancel %s\n", action);
	if (news.ng_dist)
	    fprintf(temp, "Distribution: %s\n", news.ng_dist);
	fprintf(temp, "\n");
	fprintf(temp, "cancel %s in newsgroup %s\n", action, record);
	fprintf(temp, "\n");
	fprintf(temp, "This article was cancelled from within %s\n", version_id);
	fprintf(temp, "\n");
	fclose(temp);
	x = (nntp_post(temp_file));
	if (x != 0)
	    msg("Unable to cancel article");
	unlink(temp_file);
	return (x);
    } else if (strcmp(prog, "post") == 0 || strcmp(prog, "follow") == 0) {
	strcpy(lookfor, "Newsgroups:");
	strcpy(send, "post ");
	strcpy(sent, "posted ");
	strcpy(sendpr, "p)ost ");
	strcpy(message, "article");
    } else {
	strcpy(lookfor, "To:");
	strcpy(send, "send ");
	strcpy(sent, "sent ");
	strcpy(sendpr, "s)end ");
	strcpy(message, "letter");
    }

    if (action != "send") {
	strcpy(copy, tempsuffix(temp_file, "C"));
	copy_file(temp_file, copy, 0);
    }
    strcpy(pr, "a)bort ");

    if (poster)
	strcat(pr, "c)c ");

    strcat(pr, "e)dit h)old ");

    if (spell_checker)
	strcat(pr, "i)spell ");

    strcat(pr, "m)ail ");

    if (strcmp(prog, "post") == 0 || strcmp(prog, "follow") == 0)
	strcat(pr, "p)ost ");

    strcat(pr, "r)eedit ");

    if (strcmp(prog, "post") != 0 && strcmp(prog, "follow") != 0)
	strcat(pr, "s)end ");

    strcat(pr, "v)iew w)rite S)ign ");

    if (response_dflt_answer) {
	if (strncmp(response_dflt_answer, "p", 1) || strncmp(response_dflt_answer, "s", 1)) {
	    strcpy(pr1, send);
	    strcat(pr1, message);
	} else {
	    strcpy(pr1, response_dflt_answer);
	}
    } else {
	strcpy(pr1, "");
    }


    while (loop) {
	if (action != NULL) {
	    act = action[0];
	    action = NULL;
	} else if (prmpt) {
	    prompt_line = Lines - 1;
	    prompt("%s\r\nAction: (%s) ", pr, pr1);
	    act = get_c();
	    if (act == CR)
		act = response_dflt_answer[0];
	}
	prmpt = 1;

	switch (act) {
	    case 'a':
		prompt_line = Lines - 1;
		prompt("Confirm abort: (y) ");
		switch (get_c()) {
		    case CR:
		    case 'y':
		    case 'Y':
			unlink(temp_file);
			unlink(copy);
			return (22);
		}
		break;

	    case 'c':
		if (poster) {
		    prompt_line = Lines - 1;
		    prompt("CC: %s (y)", poster);
		    switch (get_c()) {
			case CR:
			case 'y':
			case 'Y':
			    msg("Sending copy to poster....");
			    strcpy(cc, poster);
			    break;
		    }
		} else
		    msg("Not doing follow-up");
		break;

	    case 'e':
		if (!editor_program)
		    editor_program = getenv("EDITOR");

		if (!editor_program)
		    editor_program = "vi";

		strncpy(buf, editor_program, 50);

		if (use_ed_line) {
		    sprintf(buf2, " +%d", ed_line);
		    strcat(buf, buf2);
		}
		strcat(buf, " ");
		strcat(buf, temp_file);
		unset_raw();
		system(buf);
		nn_raw();

		if (stat(temp_file, &statb) < 0 || statb.st_size == 0) {
		    sprintf(delayed_msg, sent_fmt, " not");
		    unlink(temp_file);
		    unlink(copy);
		    return (22);
		}
		if (empty_answer_check)
		    if (cmp_file(temp_file, copy) != 1) {
			sprintf(delayed_msg, sent_fmt, " not");
			unlink(temp_file);
			unlink(copy);
			return (22);
		    }
		if (lookfor);	/* grep for lookfor */

		break;

	    case 'h':
		prompt_line = Lines - 1;
		prompt("Complete response later: (y) ");
		switch (get_c()) {
		    case CR:
		    case 'y':
		    case 'Y':
			copy_file(temp_file, relative(nn_directory, "hold.work"), 0);
			unlink(temp_file);
			unlink(copy);
			return (22);
		}
		break;

	    case 'i':
		if (spell_checker) {
		    unset_raw();
		    clrdisp();
		    gotoxy(0, 1);
		    strncpy(buf, spell_checker, 50);
		    strcat(buf, " ");
		    strcat(buf, temp_file);
		    system(buf);
		    nn_raw();
		} else {
		    msg("spell-checker not defined");
		    user_delay(2);
		}
		break;

	    case 'm':
		prompt_line = Lines - 1;
		prompt("To: ");
		unset_raw();
		if (getline(&cc[0], sizeof(cc)) == 0)
		    cc[0] = CR;
		nn_raw();
		if (cc[0] != CR)
		    msg("Sending copy....");
		break;

	    case 'n':
		act = 'a';
		prmpt = 0;
		break;

	    case 'p':
	    case 's':
		loop = 0;
		break;

	    case 'r':
		prompt_line = Lines - 1;
		prompt("Undo all changes? (n) ");
		switch (get_c()) {
		    case 'y':
		    case 'Y':
			copy_file(copy, temp_file, 0);
		}
		action = "edit";
		break;

	    case 'v':
		unset_raw();
		clrdisp();
		gotoxy(0, 1);
		if (!pager)
		    pager = getenv("PAGER");
		if (!pager)
		    pager = "cat";
		strncpy(buf, pager, 50);
		strcat(buf, " ");
		strcat(buf, temp_file);
		system(buf);
		nn_raw();
		break;

	    case 'w':
		prompt_line = Lines - 1;
		prompt("Append %s to file: ", message);
		strcpy(fname, get_s((char *) NULL, (char *) NULL, (char *) NULL, NULL_FCT));
		if (fname)
		    copy_file(temp_file, fname, 1);
		break;

	    case 'y':
		act = response_dflt_answer[0];
		prmpt = 0;
		break;

	    case 'S':
		unset_raw();
		strcpy(hdrs, tempsuffix(temp_file, "H"));
		strcpy(bdy, tempsuffix(temp_file, "B"));
		strcpy(sgn, tempsuffix(temp_file, "S"));
		sprintf(buf, "sed -e \'/^$/q\' < %s > %s", temp_file, hdrs);
		system(buf);
		sprintf(buf, "awk \'{if (S== 1) print $0; if ($0 == \"\") S=1}\' < %s > %s", temp_file, bdy);
		system(buf);
		if (strcmp(sign_type, "pgp") == 0) {
		    sprintf(buf, "pgp -stfaw < %s > %s", bdy, sgn);
		    goodsigntype = 1;
		} else if (strcmp(sign_type, "gpg") == 0) {
		    sprintf(buf, "gpg -sta < %s > %s", bdy, sgn);
		    goodsigntype = 1;
		}
		if (goodsigntype) {
		    system(buf);
		    sprintf(buf, "cat %s %s > %s", hdrs, sgn, temp_file);
		    system(buf);
		} else
		    msg("sign-type must be either pgp or gpg");
		sleep(5);
		unlink(hdrs);
		unlink(bdy);
		unlink(sgn);
		nn_raw();
		break;
	}

	if (cc[0] != CR) {
	    strcpy(final, tempsuffix(temp_file, "F"));
	    tempf = open_file(final, OPEN_CREATE);
	    x = fprintf(tempf, "To: %s\n", cc);
	    fclose(tempf);
	    sprintf(buf, "sed -e \"s/^To:/X-To:/\" -e \"/^Orig-To:/d\" %s >> %s",
		    temp_file, final);
	    system(buf);
	    if (mailer_pipe_input) {
		sprintf(buf, "%s < %s", mailer_program, final);
		x = system(buf);
	    } else {
		strncpy(buf, mailer_program, 50);
		strcat(buf, " ");
		strcat(buf, final);
		x = system(buf);
	    }
	    if (!x)
		msg("Done");
	    else
		msg("%s failed", mailer_program);
	    cc[0] = CR;
	}
    }

    sigfile = find_signature();
    if (sigfile != NULL) {
	if (query_signature) {
	    prompt_line = Lines - 1;
	    prompt("Append .signature? (%c) ", append_sig ? 'Y' : 'N');

	    switch (get_c()) {
		case 'y':
		case 'Y':
		    append_sig = 1;
		    break;

		case 'n':
		case 'N':
		    append_sig = 0;
		    break;

		default:
		    break;
	    }
	}
	if (append_sig)
	    append_signature(sigfile);
	(void) fclose(sigfile);
    }
    if (!strcmp("post", prog) || !strcmp("follow", prog))
	if (novice) {
	    msg("Be patient! Your new %s will not show up immediately.", message);
	    if (response_check_pause < 2)
		user_delay(2);
	    else
		user_delay(response_check_pause);
	}
/* trap signals 1 2 3 here */

    if (record) {
	FILE           *rec;
	char           *logname;
	time_t          t;

	rec = open_file(record, OPEN_APPEND);
	if (rec != NULL) {
	    if (use_mmdf_folders)
		fprintf(rec, "\001\001\001\001\n");
	    logname = getenv("LOGNAME");
	    if (!logname)
		logname = user_name();
	    time(&t);
	    fprintf(rec, "From %s %s", logname, ctime(&t));
	    fprintf(rec, "From: %s\n", logname);
	    fclose(rec);
	    sprintf(buf, "cat %s >> %s", temp_file, record);
	    system(buf);
	}
    }
    if (!strcmp("reply", prog) || !strcmp("forward", prog) || !strcmp("mail", prog)) {
	if (mail_alias_expander) {
	    strncpy(buf, mail_alias_expander, 50);
	    strcat(buf, " ");
	    strcat(buf, temp_file);
	    system(buf);
	}
	if (mailer_pipe_input) {
	    sprintf(buf, "%s < %s", mailer_program, temp_file);
	    x = system(buf);
	} else {
	    strncpy(buf, mailer_program, 50);
	    strcat(buf, " ");
	    strcat(buf, temp_file);
	    x = system(buf);
	}
	if (x)
	    msg("%s failed", mailer_program);
    } else if (!strcmp("post", prog) || !strcmp("follow", prog)) {
	if (nntp_post(temp_file)) {
	    copy_file(temp_file, relative(home_directory, "dead.letter"), 1);
	    msg("failed post saved in dead.letter");
	    user_delay(2);
	    return (1);
	}
    } else {
	msg("Invalid operation: %s -- help", prog);
	user_delay(2);
    }

    /* onfail copy to dead.letter and email user */

    unlink(temp_file);
    unlink(copy);
    unlink(final);
    sprintf(delayed_msg, sent_fmt, "");
    return (0);
}

#endif
