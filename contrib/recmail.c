/*
 *	Copyright (c) 1990 Tom Dawes-Gamble.
 *      For Copyright restrictions see the README file distributed with nn. 
 *          
 *	Mail sender for nn with C news and smail.
 *	
 *      Written by Tom Dawes-Gamble Texas Instruments Ltd, Bedford England.
 *
 *	e-mail tmdg@texins.uucp
 *      uucp   ..!ukc.co.uk!pyrltd!miclon!texins!tmdg
 *
 *      When I installed nn with Cnews I could not find an equivalent
 *	to the Bnews recmail so I wrote this to do the job.
 *	It seems to work for me.
 *	I have no idea how this will work for mailers other than smail
 *
 *	YOU WILL need to look at the way get_host_name works for you system
 *	
 *	If you use this program please let me know so that I can mail you
 *	any enhancements.
 *
 */

/*  you can use the smail defs if required */
#ifdef SMAIL_DEFS_H
#include  SMAIL_DEFS_H
#else
#define MYDOM ".uucp"
#endif /* SMAIL_DEFS_H */

#define MAILER "/bin/smail"   /* The pathname of your mailer */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <time.h>

/* where is the local time zone name ? */

	/* xenix 2.3.2 has this in the tm structure */
/*#define TIMEZONE_NAME(tm) ((tm)->tm_name) /* */
	/* some systems have it here */
#define TIMEZONE_NAME(tm) (asctime(tm)?tzname[(tm)->tm_isdst?1:0]:"") /* */

extern char *getlogin();
extern char *getenv();
extern char *malloc();
extern char *mktemp();
extern struct passwd *getpwnam();
void get_host_name();

char *day_name[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

char *month[] = {	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

char *mail_template = "/tmp/rmXXXXXX";
char *mail_spool = "/tmp/rmXXXXXX";

#undef BUFSIZ
#define BUFSIZ 1024	      /* a 1024 byte buffer should be enough */

#define ERROR_MSG_0 "%s: Unable to read stdin ** Error = %d\n"
#define ERROR_MSG_1 "%s: Unable to create spool file ** Error = %d\n"

FILE *sfd, *fopen();

void send_the_message();
void build_the_header();

int main(argc, argv)
int argc;
char **argv;
{
	char inbuff[BUFSIZ];	/*  Buffer for stdin  */
	char pbuff[BUFSIZ];	/*  Buffer to hold the name of recipients */
        char **tolist, *tonames[100];  /* I think that should cope for now */ 

	if (argc != 1) {
		(void) fprintf(stderr,
			"%s does not allow any arguments yet!\n", argv[0]);
		exit(1);
	}
	tolist = tonames;

/*  FIX  this could be much better */
	*tolist++ = MAILER;
	*tolist++ = "-f";
	*tolist++ = mktemp(strcpy(mail_spool, mail_template));
	*tolist = pbuff;

	if ((sfd = fopen(mail_spool, "w")) == NULL){
		(void) fprintf(stderr, ERROR_MSG_1, argv[0], MAILER, errno);
		exit(1);
	}
	build_the_header();

	/* Now send the mail down the tube */

	while((fgets(inbuff, BUFSIZ, stdin)) == inbuff){
			/* strip blind copies */
		if (strncmp(inbuff, "Bc: ", 4)){ /* true if not Bc: */
			(void) fprintf(sfd, "%s", inbuff);
		}
	 	if (!strncmp(inbuff, "To: ", 4)){
			tolist += get_names(inbuff, tolist);
		} else if (!strncmp(inbuff, "Cc: ", 4)){
			tolist += get_names(inbuff, tolist);
		} else if (!strncmp(inbuff, "Bc: ", 4)){
			tolist += get_names(inbuff, tolist);
		} else if (!strcmp(inbuff, "\n")){
			break;
		}
	}
	while((fgets(inbuff, BUFSIZ, stdin)) == inbuff){
		(void) fprintf(sfd, "%s", inbuff);
	}
	(void) fflush(sfd);
	(void) fclose(sfd);
	*tolist = NULL;
	send_the_message(tonames);
	exit(0);
}

get_names(line_buffer, to_names)
char *line_buffer;
char **to_names;
{
	char *curr_name;
	char *next_name;
	int name_count;

	name_count = 0;
	(void) strip_comments(line_buffer);
        (void) check_for_bracket(line_buffer);
	while (next_name = strchr(line_buffer, ' ')){
		*next_name++ = '\0';
		while (*next_name == ' ')
			next_name++;
		curr_name = *to_names;
		curr_name += strlen(curr_name) + 1;
		(void) strcpy(*to_names++, line_buffer);
		*to_names = curr_name;
		(void) strcpy(line_buffer, next_name);
		name_count++;
	}
	if (strlen(line_buffer)){
		curr_name = *to_names;
		curr_name += strlen(curr_name) + 1;
		(void) strcpy(*to_names++, line_buffer);
		*to_names = curr_name;
		name_count++;
	}
	return name_count;
}
	
/*	FIX	Correct the order of the head lines */
void
build_the_header()
{
	struct passwd *login, *getpwnam(), *getpwuid();
	struct passwd *effective;
	struct tm *gmt;
	struct tm *loc;
	long tics;
	char hostname[9];

	if(!(login = getpwnam(getlogin())))
		login = getpwuid(getuid());
	get_host_name(hostname);
	gmt = (struct tm *) malloc(sizeof (struct tm));
	tics = time((long *) 0);
	loc = gmtime(&tics );
	*gmt = *loc;
	loc = localtime(&tics );
	(void) fprintf(sfd, "%s\n", login->pw_name);
/* the Reply to: line doesn't look nice on xenix */
	(void) fprintf(sfd, "Reply-To:   %s@%s%s (%s)\n", login->pw_name,
		hostname, MYDOM, login->pw_comment);
/* I'm not sure about this so if it's wrong please tell me */
	if (getuid() != login->pw_uid){
		effective = getpwuid(getuid());
		(void) fprintf(sfd, "Sender:     %s@%s%s (%s)\n", 
			effective->pw_name, hostname, MYDOM,
			effective->pw_comment);
	}
	(void) fprintf(sfd, "Message-Id: <%d%02d%02d%02d%02d.%s%05d@%s%s>\n", 
		gmt->tm_year, (gmt->tm_mon + 1), gmt->tm_mday,
		gmt->tm_hour, gmt->tm_min,
		"AA", getpid(), hostname, MYDOM);
	(void) fprintf(sfd, "Date: %s, %d %s %d %02d:%02d:%02d GMT\n", 
		day_name[gmt->tm_wday], gmt->tm_mday, month[gmt->tm_mon],
		gmt->tm_year, gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
	(void) fprintf(sfd, "From: %s@%s%s \(%s\)\n",
		 login->pw_name, hostname, MYDOM, login->pw_comment);
/*  To help local people know when you sent the mail */
	if (loc->tm_isdst || strncmp(TIMEZONE_NAME(loc),  "GMT", 3))
		(void) fprintf(sfd,
			"X-Local-time: %s, %d %s %d %02d:%02d:%02d %s\n", 
			day_name[loc->tm_wday], loc->tm_mday,
			month[loc->tm_mon], loc->tm_year, loc->tm_hour,
			loc->tm_min, loc->tm_sec, TIMEZONE_NAME(loc));
}

void
send_the_message(arg_list)
char *arg_list[];
{
	int  cid, rid;
	int  child_stat;

	/*fprintf(stderr, "%s\n", *arg_list);*/

	if (cid = fork()){
		if ( (rid = wait((int *)&child_stat)) == -1){
			(void) fprintf(stderr, "%d %d %d %x\n",
					cid, rid, errno, child_stat);
		} else  {
			cid = 0;
/* FIX I would like to unlink but can't since it causes smail to fail */
		/*(void) unlink(mail_spool);*/
		}
	} else {
		(void) execvp(*arg_list, arg_list);
	}
	return;
	
}
 
/* FIX  This is for xenix  System V can use unames */
void
get_host_name(hostname)
char *hostname;
{
	FILE *hfd;
	char *h;
	
	hfd = fopen("/etc/systemid", "r");
	if (hfd == NULL) {
#ifdef HOSTNAME
	    strcpy(hostname, HOSTNAME);	/* maybe from smail defs.h */
#else
	    strcpy(hostname, "UNKNOWN");
#endif
	    return;
	}
	h = fgets(hostname, 9, hfd);
	(void) fclose(hfd);
	while (*h != '\0')
		if (*h == '\n')
			*h = '\0';
		else
			h++;
	return;
}

/*	remove first word and any comments  */
/*	Note comment is totaly removed and replaced by space  */
/*	this may need changeing  */
/*	all space will be compressed to a single space  */
/*      FIX required to handle tabs at the moment \tword starting
	a line will be lost but thats not what I want */

strip_comments(buff)
char *buff;
{
	char *begin, *end;

	if (end = strchr(buff, ' ')){
		while (*end == ' ') end++;
		(void) strcpy(buff, end);
	}
	while(begin = strchr(buff, '(')){
		end = strchr(begin, ')');
		end++;
		(void) strcpy(begin, end);
	}
	end = strchr(buff, '\n');
	*end = '\0';
	return;
}


/*	This routine test the line for <machine readable field>
        if found then only those fields are left   */

check_for_bracket(buff)
char *buff;
{
	char *begin, *end;

	while(begin = strchr(buff, '<')){
		begin++;
		(void) strcpy(buff, begin);
		end = strchr(begin, '>');
		*end = ' ';
		buff = end + 1;
	}
	return;
}
 
