#include <stdio.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <pcre.h>

#include "imapfilter.h"


static int ifre_flags(lua_State *lua);
static int ifre_compile(lua_State *lua);
static int ifre_exec(lua_State *lua);
static int ifre_free(lua_State *lua);

/* Lua imapfilter library of PCRE related functions. */
static const luaL_Reg ifrelib[] = {
	{ "flags", ifre_flags },
	{ "compile", ifre_compile },
	{ "exec", ifre_exec },
	{ "free", ifre_free },
	{ NULL, NULL }
};


/*
 * Return PCRE available compile and exec flags.
 */
static int
ifre_flags(lua_State *lua)
{

	if (lua_gettop(lua) != 0)
		luaL_error(lua, "wrong number of arguments");

	lua_newtable(lua);

#ifdef PCRE_CASELESS
	set_table_number("CASELESS", PCRE_CASELESS);
#endif
#ifdef PCRE_MULTILINE
	set_table_number("MULTILINE", PCRE_MULTILINE);
#endif
#ifdef PCRE_DOTALL
	set_table_number("DOTALL", PCRE_DOTALL);
#endif
#ifdef PCRE_EXTENDED
	set_table_number("EXTENDED", PCRE_EXTENDED);
#endif
#ifdef PCRE_ANCHORED
	set_table_number("ANCHORED", PCRE_ANCHORED);
#endif
#ifdef PCRE_DOLLAR_ENDONLY
	set_table_number("DOLLAR_ENDONLY", PCRE_DOLLAR_ENDONLY);
#endif
#ifdef PCRE_EXTRA
	set_table_number("EXTRA", PCRE_EXTRA);
#endif
#ifdef PCRE_NOTBOL
	set_table_number("NOTBOL", PCRE_NOTBOL);
#endif
#ifdef PCRE_NOTEOL
	set_table_number("NOTEOL", PCRE_NOTEOL);
#endif
#ifdef PCRE_UNGREEDY
	set_table_number("UNGREEDY", PCRE_UNGREEDY);
#endif
#ifdef PCRE_NOTEMPTY
	set_table_number("NOTEMPTY", PCRE_NOTEMPTY);
#endif
#ifdef PCRE_UTF8
	set_table_number("UTF8", PCRE_UTF8);
#endif
#ifdef PCRE_NO_AUTO_CAPTURE
	set_table_number("NO_AUTO_CAPTURE", PCRE_NO_AUTO_CAPTURE);
#endif
#ifdef PCRE_NO_UTF8_CHECK
	set_table_number("NO_UTF8_CHECK", PCRE_NO_UTF8_CHECK);
#endif
#ifdef PCRE_FIRSTLINE
	set_table_number("FIRSTLINE", PCRE_FIRSTLINE);
#endif
#ifdef PCRE_AUTO_CALLOUT
	set_table_number("AUTO_CALLOUT", PCRE_AUTO_CALLOUT);
#endif
#ifdef PCRE_PARTIAL
	set_table_number("PARTIAL", PCRE_PARTIAL);
#endif
#ifdef PCRE_DFA_SHORTEST
	set_table_number("DFA_SHORTEST", PCRE_DFA_SHORTEST);
#endif
#ifdef PCRE_DFA_RESTART
	set_table_number("DFA_RESTART", PCRE_DFA_RESTART);
#endif
#ifdef PCRE_FIRSTLINE
	set_table_number("FIRSTLINE", PCRE_FIRSTLINE);
#endif
#ifdef PCRE_DUPNAMES
	set_table_number("DUPNAMES", PCRE_DUPNAMES);
#endif
#ifdef PCRE_NEWLINE_CR
	set_table_number("NEWLINE_CR)", PCRE_NEWLINE_CR);
#endif
#ifdef PCRE_NEWLINE_LF
	set_table_number("NEWLINE_LF", PCRE_NEWLINE_LF);
#endif
#ifdef PCRE_NEWLINE_CRLF
	set_table_number("NEWLINE_CRLF", PCRE_NEWLINE_CRLF);
#endif
#ifdef PCRE_NEWLINE_ANY
	set_table_number("NEWLINE_ANY", PCRE_NEWLINE_ANY);
#endif
#ifdef PCRE_NEWLINE_ANYCRLF
	set_table_number("NEWLINE_ANYCRLF", PCRE_NEWLINE_ANYCRLF);
#endif
#ifdef PCRE_BSR_ANYCRLF
	set_table_number("PCRE_BSR_ANYCRLF", PCRE_BSR_ANYCRLF);
#endif
#ifdef PCRE_BSR_UNICODE
	set_table_number("PCRE_BSR_UNICODE", PCRE_BSR_UNICODE);
#endif
#ifdef PCRE_JAVASCRIPT_COMPAT
	set_table_number("PCRE_JAVASCRIPT_COMPAT", PCRE_JAVASCRIPT_COMPAT);
#endif
#ifdef PCRE_NO_START_OPTIMIZE
	set_table_number("PCRE_NO_START_OPTIMIZE", PCRE_NO_START_OPTIMIZE);
#endif
#ifdef PCRE_NO_START_OPTIMISE
	set_table_number("PCRE_NO_START_OPTIMISE", PCRE_NO_START_OPTIMISE);
#endif
#ifdef PCRE_PARTIAL_HARD
	set_table_number("PCRE_PARTIAL_HARD", PCRE_PARTIAL_HARD);
#endif
#ifdef PCRE_NOTEMPTY_ATSTART
	set_table_number("PCRE_NOTEMPTY_ATSTART", PCRE_NOTEMPTY_ATSTART);
#endif
#ifdef PCRE_UCP
	set_table_number("PCRE_UCP", PCRE_UCP);
#endif

	return 1;
}


/*
 * Lua implementation of the PCRE compile function.
 */
static int
ifre_compile(lua_State *lua)
{
	pcre **re;
	const char *error;
	int erroffset;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TSTRING);
	luaL_checktype(lua, 2, LUA_TNUMBER);

	re = (pcre **)(lua_newuserdata(lua, sizeof(pcre *)));

	*re = pcre_compile(lua_tostring(lua, 1), lua_tonumber(lua, 2), &error,
	    &erroffset, NULL);

	if (*re == NULL) {
		fprintf(stderr, "RE failed at offset %d: %s\n", erroffset,
		    error);
		lua_pop(lua, 1);
	}

	lua_remove(lua, 1);
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
	int i, n;
	pcre *re;
	int ovecsize;
	int *ovector;

	if (lua_gettop(lua) != 3)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TUSERDATA);
	luaL_checktype(lua, 2, LUA_TSTRING);
	luaL_checktype(lua, 3, LUA_TNUMBER);

	re = *(pcre **)(lua_touserdata(lua, 1));

	pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &ovecsize);

	ovector = (int *)xmalloc(sizeof(int) * (ovecsize + 1) * 3);

	for (i = 0; i <= ovecsize; i++)
		ovector[2 * i] = ovector[2 * i + 1] = -1;

	n = pcre_exec(re, NULL, lua_tostring(lua, 2),
#if LUA_VERSION_NUM < 502
	    lua_objlen(lua, 2),
#else
	    lua_rawlen(lua, 2),
#endif
	    0, lua_tonumber(lua, 3), ovector, (ovecsize + 1) * 3);

	if (n > 0)
		for (i = 1; i < n; i++)
			if (ovector[2 * i] != -1 && ovector[2 * i + 1] != -1)
				lua_pushlstring(lua, lua_tostring(lua, 2) +
				    ovector[2 * i], ovector[2 * i + 1] -
				    ovector[2 * i]);
			else
				lua_pushnil(lua);

	xfree(ovector);

	lua_remove(lua, 1);
	lua_remove(lua, 1);
	lua_remove(lua, 1);

	lua_pushboolean(lua, (n > 0));
	lua_insert(lua, 1);

	return (n > 0 ? n : 1);
}


/*
 * Lua implementation of a PCRE free function.
 */
static int
ifre_free(lua_State *lua)
{
	pcre *re;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TUSERDATA);

	re = *(pcre **)(lua_touserdata(lua, 1));

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
