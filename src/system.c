#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <termios.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "imapfilter.h"

#pragma GCC diagnostic ignored "-Wunused-result"

static int ifsys_echo(lua_State *lua);
static int ifsys_noecho(lua_State *lua);
static int ifsys_popen(lua_State *lua);
static int ifsys_pclose(lua_State *lua);
static int ifsys_read(lua_State *lua);
static int ifsys_write(lua_State *lua);
static int ifsys_sleep(lua_State *lua);
static int ifsys_daemon(lua_State *lua);

/* Lua imapfilter library of system's functions. */
static const luaL_Reg ifsyslib[] = {
	{ "echo", ifsys_echo },
	{ "noecho", ifsys_noecho },
	{ "popen", ifsys_popen },
	{ "pclose", ifsys_pclose },
	{ "read", ifsys_read },
	{ "write", ifsys_write },
	{ "sleep", ifsys_sleep },
	{ "daemon", ifsys_daemon },
	{ NULL, NULL }
};


/*
 * Enable character echoing.
 */
static int
ifsys_echo(lua_State *lua)
{
	struct termios t;

	if (lua_gettop(lua) != 0)
		luaL_error(lua, "wrong number of arguments");

	if (tcgetattr(fileno(stdin), &t)) {
		fprintf(stderr, "getting term attributs; %s\n",
		    strerror(errno));
		return 1;
	}

	t.c_lflag |= (ECHO);
	t.c_lflag &= ~(ECHONL);

	if (tcsetattr(fileno(stdin), TCSAFLUSH, &t)) {
		fprintf(stderr, "setting term attributes; %s\n",
		    strerror(errno));
		return 1;
	}

	return 0;
}


/*
 * Disable character echoing.
 */
static int
ifsys_noecho(lua_State *lua)
{
	struct termios t;

	if (lua_gettop(lua) != 0)
		luaL_error(lua, "wrong number of arguments");

	if (tcgetattr(fileno(stdin), &t)) {
		fprintf(stderr, "getting term attributs; %s\n",
		    strerror(errno));
		return 1;
	}

	t.c_lflag &= ~(ECHO);
	t.c_lflag |= (ECHONL);

	if (tcsetattr(fileno(stdin), TCSAFLUSH, &t)) {
		fprintf(stderr, "setting term attributes; %s\n",
		    strerror(errno));
		return 1;
	}

	return 0;
}


/*
 * Lua implementation of the POSIX popen() function.
 */
static int
ifsys_popen(lua_State *lua)
{
	FILE **fp;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");
	luaL_checktype(lua, 1, LUA_TSTRING);
	luaL_checktype(lua, 2, LUA_TSTRING);

	fp = (FILE **) lua_newuserdata(lua, sizeof(FILE *));

	*fp = NULL;

	*fp = popen(lua_tostring(lua, 1), lua_tostring(lua, 2));

	lua_remove(lua, 1);
	lua_remove(lua, 1);

	return (*fp == NULL ? 0 : 1);
}


/*
 * Lua implementation of the POSIX pclose() function.
 */
static int
ifsys_pclose(lua_State *lua)
{
	lua_Number r;
	FILE *fp;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");
	luaL_checktype(lua, 1, LUA_TUSERDATA);

	fp = *(FILE **) (lua_touserdata(lua, 1));

	r = (lua_Number) (pclose(fp) >> 8);

	lua_pop(lua, 1);

	if (r == -1)
		return 0;

	fp = NULL;
	lua_pushnumber(lua, r);

	return 1;
}


/*
 * Reads a line from a file stream.
 */
static int
ifsys_read(lua_State *lua)
{
	FILE *fp;
	luaL_Buffer b;
	char *c;
	size_t n;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");
	luaL_checktype(lua, 1, LUA_TUSERDATA);

	fp = *(FILE **) (lua_touserdata(lua, 1));

	lua_pop(lua, 1);

	luaL_buffinit(lua, &b);

	for (;;) {
		c = luaL_prepbuffer(&b);

		if (fgets(c, LUAL_BUFFERSIZE, fp) == NULL && feof(fp)) {
			luaL_pushresult(&b);

			return (
#if LUA_VERSION_NUM < 502
			    lua_objlen(lua, -1)
#else
			    lua_rawlen(lua, -1)
#endif
			    > 0);
		}
		n = strlen(c);

		if (c[n - 1] != '\n')
			luaL_addsize(&b, n);
		else {
			luaL_addsize(&b, n);
			luaL_pushresult(&b);

			return 1;
		}
	}
}


/*
 * Writes a string to a file stream.
 */
static int
ifsys_write(lua_State *lua)
{
	size_t n;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");
	luaL_checktype(lua, 1, LUA_TUSERDATA);
	luaL_checktype(lua, 2, LUA_TSTRING);

	n = fwrite(lua_tostring(lua, 2), sizeof(char), strlen(lua_tostring(lua,
	    2)), *(FILE **) (lua_touserdata(lua, 1)));

	lua_pop(lua, 2);

	lua_pushboolean(lua, (n != 0));

	return 1;
}


/*
 * Lua implementation of the POSIX sleep() function.
 */
static int
ifsys_sleep(lua_State *lua)
{

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");
	luaL_checktype(lua, 1, LUA_TNUMBER);

	lua_pushnumber(lua,
	    (lua_Number) (sleep) ((unsigned int)(lua_tonumber(lua, 1))));

	lua_remove(lua, 1);

	return 1;
}


/*
 * Lua implementation of the BSD daemon() function.
 */
static int
ifsys_daemon(lua_State *lua)
{

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");
	luaL_checktype(lua, 1, LUA_TBOOLEAN);
	luaL_checktype(lua, 2, LUA_TBOOLEAN);

	switch (fork()) {
	case -1:
		fprintf(stderr, "forking; %s\n", strerror(errno));
		lua_pushboolean(lua, 0);
		return 1;
		/* NOTREACHED */
		break;
	case 0:
		break;
	default:
		exit(0);
		/* NOTREACHED */
		break;
	}

	if (setsid() == -1) {
		fprintf(stderr, "creating session; %s\n", strerror(errno));
	}

	if (lua_toboolean(lua, 1) == 0)
		chdir("/");

	if (lua_toboolean(lua, 2) == 0) {
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		if (open("/dev/null", O_RDWR) == -1 ||
		    dup(STDIN_FILENO) == -1 ||
		    dup(STDIN_FILENO) == -1)
			fprintf(stderr, "duplicating file descriptors; %s\n",
			    strerror(errno));
	}

	return 0;
}


/*
 * Open imapfilter library of system's functions.
 */
LUALIB_API int
luaopen_ifsys(lua_State *lua)
{

#if LUA_VERSION_NUM < 502
	luaL_register(lua, "ifsys", ifsyslib);
#else
	luaL_newlib(lua, ifsyslib);
	lua_setglobal(lua, "ifsys");

#endif

	return 1;
}
