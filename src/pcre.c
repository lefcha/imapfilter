#include <stdio.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>

#include "imapfilter.h"


static int ifre_compile(lua_State *lua);
static int ifre_exec(lua_State *lua);
static int ifre_free(lua_State *lua);

/* Lua imapfilter library of PCRE related functions. */
static const luaL_Reg ifrelib[] = {
	{ "compile", ifre_compile },
	{ "exec", ifre_exec },
	{ "free", ifre_free },
	{ NULL, NULL }
};


/*
 * Lua implementation of the PCRE compile function.
 */
static int
ifre_compile(lua_State *lua)
{
	pcre2_code **re;
	PCRE2_SPTR pattern;
	int errornumber;
	PCRE2_SIZE erroroffset;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");
	luaL_checktype(lua, 1, LUA_TSTRING);

	pattern = (PCRE2_SPTR)lua_tostring(lua, 1);

#if LUA_VERSION_NUM < 504
	re = (pcre2_code **)(lua_newuserdata(lua, sizeof(pcre2_code *)));
#else
	re = (pcre2_code **)(lua_newuserdatauv(lua, sizeof(pcre2_code *), 1));
#endif

	*re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, 0, &errornumber,
	    &erroroffset, NULL);

	if (*re == NULL) {
		PCRE2_UCHAR buffer[256];
		pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
		fprintf(stderr, "RE failed at offset %d: %s\n",
		    (int)erroroffset, buffer);
		lua_pop(lua, 1);
	}

	lua_remove(lua, 1);

	lua_pushboolean(lua, (*re != NULL));
	lua_insert(lua, 1);
	
	return (*re != NULL ? 2 : 1);
}


/*
 * Lua implementation of the PCRE exec function.
 */
static int
ifre_exec(lua_State *lua)
{
	int i, rc;
	pcre2_code *re;
	pcre2_match_data *match_data;
	PCRE2_SPTR subject;
	size_t subject_length;
	PCRE2_SIZE *ovector;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");
	luaL_checktype(lua, 1, LUA_TUSERDATA);
	luaL_checktype(lua, 2, LUA_TSTRING);

	re = *(pcre2_code **)(lua_touserdata(lua, 1));
	subject = (PCRE2_SPTR)lua_tostring(lua, 2);
	subject_length = strlen((char *)subject);

	match_data = pcre2_match_data_create_from_pattern(re, NULL);

	rc = pcre2_match(re, subject, subject_length, 0, 0, match_data, NULL);

	if (rc > 0) {
		ovector = pcre2_get_ovector_pointer(match_data);
		for (i = 0; i < rc; i++) {
			if (ovector[2 * i] != PCRE2_UNSET &&
			    ovector[2 * i + 1] != PCRE2_UNSET) {
				PCRE2_SPTR start = subject + ovector[2 * i];
				size_t len = ovector[2 * i + 1] - ovector[2 * i];
				lua_pushlstring(lua, (const char *)start, len);
			} else {
				lua_pushnil(lua);
			}
		}
	}

	pcre2_match_data_free(match_data);
	pcre2_code_free(re);

	lua_remove(lua, 1);
	lua_remove(lua, 1);

	lua_pushboolean(lua, (rc > 0));
	lua_insert(lua, 1);

	return (rc > 0 ? rc : 1);
}


/*
 * Lua implementation of a PCRE free function.
 */
static int
ifre_free(lua_State *lua)
{
	pcre2_code *re;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");
	luaL_checktype(lua, 1, LUA_TUSERDATA);

	re = *(pcre2_code **)(lua_touserdata(lua, 1));

	xfree(re);

	lua_remove(lua, 1);

	return 0;
}


/*
 * Open imapfilter library of PCRE related functions.
 */
LUALIB_API int
luaopen_ifre(lua_State *lua)
{

#if LUA_VERSION_NUM < 502
	luaL_register(lua, "ifre", ifrelib);
#else
	luaL_newlib(lua, ifrelib);
	lua_setglobal(lua, "ifre");
#endif

	return 1;
}
