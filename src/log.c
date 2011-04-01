#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

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

	if (opts.debug <= 0 || !debugfp)
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

	if (opts.debug <= 0 || !debugfp)
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
	int n;
	char b;
	char *dt;
	int fd;

	if (!opts.debug)
		return 0;

	n = snprintf(&b, 1, "%s/%s", env.home, PATHNAME_DEBUG);

	if (env.pathmax != -1 && n > env.pathmax)
		fatal(ERROR_PATHNAME,
		    "pathname limit %ld exceeded: %d\n", env.pathmax, n);

	dt = (char *)xmalloc((n + 1) * sizeof(char));
	snprintf(dt, n + 1, "%s/%s", env.home, PATHNAME_DEBUG);

	fd = mkstemp(dt);

	if (fd != -1) {
		debugfp = fdopen(fd, "w");
		if (debugfp == NULL) {
			error("opening debug file %s: %s\n", dt,
			    strerror(errno));
			return -1;
		}
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


/*
 * Lua bindings for accessing the logging
 */

static int iflog_fatal(lua_State *lua);
static int iflog_error(lua_State *lua);
static int iflog_verbose(lua_State *lua);
static int iflog_debug(lua_State *lua);

/* Lua imapfilter log library function mapping */
static const luaL_reg ifloglib[] = {
	{ "debug", iflog_debug },
	{ "verbose", iflog_verbose },
	{ "error", iflog_error },
	{ "fatal", iflog_fatal },
	{ NULL, NULL }
};

/*
 * write to debug log
 */

static int
iflog_debug(lua_State *lua)
{
	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TSTRING);

	debug(lua_tostring(lua, 1));

	return 1;
}

/*
 * print message if in verbose mode
 */
static int
iflog_verbose(lua_State *lua)
{
	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TSTRING);

	verbose(lua_tostring(lua, 1));

	return 1;
}

/*
 * print error AND write into log
 */
static int
iflog_error(lua_State *lua)
{
	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TSTRING);

	error(lua_tostring(lua, 1));

	return 1;
}

/*
 * print error and write into log then exit
 */
static int
iflog_fatal(lua_State *lua)
{
	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TNUMBER);
	luaL_checktype(lua, 2, LUA_TSTRING);

	fatal(lua_tointeger(lua, 1), lua_tostring(lua, 2));

	return 1;
}


/*
 * Open imapfilter log library.
 */
LUALIB_API int
luaopen_iflog(lua_State *lua)
{

	luaL_register(lua, "iflog", ifloglib);

	return 1;
}
