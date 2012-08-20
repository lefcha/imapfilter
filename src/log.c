#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "imapfilter.h"
#include "session.h"
#include "list.h"
#include "pathnames.h"


extern options opts;
extern environment env;
extern unsigned int flags;
extern list *sessions;

static FILE *debugfp = NULL;	/* Pointer to debug file. */
static FILE *logfp = NULL;	/* Pointer to log file. */


char *log_time(void);


/*
 * Print message if in verbose mode.
 */
void
verbose(const char *fmt,...)
{
	va_list args;

	if (!opts.verbose)
		return;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}


/*
 * Write message to debug file.
 */
void
debug(const char *fmt,...)
{
	va_list args;

	if (!opts.debug || !debugfp)
		return;

	va_start(args, fmt);
	vfprintf(debugfp, fmt, args);
	fflush(debugfp);

	va_end(args);
}

/*
 * Write character to debug file.
 */
void
debugc(char c)
{

	if (!opts.debug || !debugfp)
		return;

	fputc(c, debugfp);
}


/*
 * Print error message and write it into log file.
 */
void
error(const char *fmt,...)
{
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "imapfilter: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	if (logfp) {
		va_start(args, fmt);
		fprintf(logfp, "%s: ", log_time());
		vfprintf(logfp, fmt, args);
		fflush(logfp);
		va_end(args);
	}
}


/*
 * Print error message and exit program.
 */
void
fatal(unsigned int errnum, const char *fmt,...)
{
	va_list args;
	list *l;
	session *s;

	va_start(args, fmt);
	fprintf(stderr, "imapfilter: ");
	vfprintf(stderr, fmt, args);
	va_end(args);

	if (logfp) {
		va_start(args, fmt);
		fprintf(logfp, "%s: ", log_time());
		vfprintf(logfp, fmt, args);
		fflush(logfp);
		va_end(args);
	}

	for (l = sessions; l; l = l->next) {
		s = l->data;
		close_connection(s);
	}

	close_log();
	close_debug();

	exit(errnum);
}


/*
 * Open temporary debug file and associate a stream with the returned file
 * descriptor.
 */
int
open_debug(void)
{

	if (!opts.debug)
		return 0;

	if (create_file(opts.debug, S_IRUSR | S_IWUSR))
		return 1;

	debugfp = fopen(opts.debug, "w");
	if (debugfp == NULL) {
		error("opening debug file %s: %s\n", opts.debug,
		    strerror(errno));
		return 1;
	}
	return 0;
}


/*
 * Close temporary debug file.
 */
int
close_debug(void)
{

	if (debugfp == NULL)
		return 0;
	else
		return fclose(debugfp);
}


/*
 * Open the file for saving of logging information.
 */
int
open_log(void)
{

	if (opts.log == NULL)
		return 0;

	debug("log file: '%s'\n", opts.log);

	if (create_file(opts.log, S_IRUSR | S_IWUSR))
		return 1;

	logfp = fopen(opts.log, "a");
	if (logfp == NULL) {
		error("opening log file %s: %s\n", opts.log, strerror(errno));
		return 1;
	}
	return 0;
}


/*
 * Close the log file.
 */
int
close_log(void)
{

	if (logfp == NULL)
		return 0;
	else
		return fclose(logfp);
}


/*
 * Return current local time and date.
 */
char *
log_time(void)
{
	char *ct;
	time_t t;

	t = time(NULL);

	ct = ctime(&t);
	*(strchr(ct, '\n')) = '\0';

	return ct;
}
