#include <stdio.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "imapfilter.h"


static int ifcore_noop(lua_State *lua);
static int ifcore_login(lua_State *lua);
static int ifcore_logout(lua_State *lua);
static int ifcore_status(lua_State *lua);
static int ifcore_select(lua_State *lua);
static int ifcore_close(lua_State *lua);
static int ifcore_expunge(lua_State *lua);
static int ifcore_search(lua_State *lua);
static int ifcore_list(lua_State *lua);
static int ifcore_lsub(lua_State *lua);
static int ifcore_fetchfast(lua_State *lua);
static int ifcore_fetchflags(lua_State *lua);
static int ifcore_fetchdate(lua_State *lua);
static int ifcore_fetchsize(lua_State *lua);
static int ifcore_fetchheader(lua_State *lua);
static int ifcore_fetchtext(lua_State *lua);
static int ifcore_fetchfields(lua_State *lua);
static int ifcore_fetchstructure(lua_State *lua);
static int ifcore_fetchpart(lua_State *lua);
static int ifcore_store(lua_State *lua);
static int ifcore_copy(lua_State *lua);
static int ifcore_append(lua_State *lua);
static int ifcore_create(lua_State *lua);
static int ifcore_delete(lua_State *lua);
static int ifcore_rename(lua_State *lua);
static int ifcore_subscribe(lua_State *lua);
static int ifcore_unsubscribe(lua_State *lua);
static int ifcore_idle(lua_State *lua);


/* Lua imapfilter core library functions. */
static const luaL_Reg ifcorelib[] = {
	{ "noop", ifcore_noop },
	{ "logout", ifcore_logout },
	{ "login", ifcore_login },
	{ "select", ifcore_select },
	{ "create", ifcore_create },
	{ "delete", ifcore_delete },
	{ "rename", ifcore_rename },
	{ "subscribe", ifcore_subscribe },
	{ "unsubscribe", ifcore_unsubscribe },
	{ "list", ifcore_list },
	{ "lsub", ifcore_lsub },
	{ "status", ifcore_status },
	{ "append", ifcore_append },
	{ "close", ifcore_close },
	{ "expunge", ifcore_expunge },
	{ "search", ifcore_search },
	{ "fetchfast", ifcore_fetchfast },
	{ "fetchflags", ifcore_fetchflags },
	{ "fetchdate", ifcore_fetchdate },
	{ "fetchsize", ifcore_fetchsize },

	/*
	 * RFC 822: message == header + body
	 * RFC 3501: body == header + text
	 * 
	 * RFC 3501 notation is used internally, and RFC 822 notation is used
	 * for the interface available to the user.
	 */
	{ "fetchheader", ifcore_fetchheader },
	{ "fetchbody", ifcore_fetchtext }, 

	{ "fetchfields", ifcore_fetchfields },
	{ "fetchstructure", ifcore_fetchstructure },
	{ "fetchpart", ifcore_fetchpart },
	{ "store", ifcore_store },
	{ "copy", ifcore_copy },
	{ "idle", ifcore_idle },
	{ NULL, NULL }
};


#define TRY(F)								\
	if ((F) == STATUS_NONE)						\
		F;
#define DISCOVER_PORT(P, S)	((P) ? (P) : (!(S) ? "143" : "993"))


/*
 * Core function to reset any inactivity autologout timer on the server.
 */
static int
ifcore_noop(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);

	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));

	TRY(r = request_noop(s, p, u));

	lua_pop(lua, 1);
	
	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to login to the server.
 */
static int
ifcore_login(lua_State *lua)
{
	const char *s, *u, *w, *p;
	int r;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);

	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	if (!(w = get_table_string("password")))
		luaL_error(lua, "no password specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));

	r = request_login(s, p, get_table_string("ssl"), u, w);

	lua_pop(lua, 1);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK ||
	    r == STATUS_PREAUTH));

	return 1;
}


/*
 * Core function to logout from the server.
 */
static int
ifcore_logout(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);

	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));

	TRY(r = request_logout(s, p, u));

	lua_pop(lua, 1);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to get the status of a mailbox.
 */
static int
ifcore_status(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	unsigned int exists, recent, unseen, uidnext;

	exists = recent = unseen = uidnext = -1;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_status(s, p, u, lua_tostring(lua, 2), &exists, &recent,
	    &unseen, &uidnext));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));
	lua_pushnumber(lua, (lua_Number) (exists));
	lua_pushnumber(lua, (lua_Number) (recent));
	lua_pushnumber(lua, (lua_Number) (unseen));
	lua_pushnumber(lua, (lua_Number) (uidnext));

	return 5;
}


/*
 * Core function to select a mailbox.
 */
static int
ifcore_select(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_select(s, p, u, lua_tostring(lua, 2)));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to close a mailbox.
 */
static int
ifcore_close(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);

	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));

	TRY(r = request_close(s, p, u));

	lua_pop(lua, 1);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to expunge a mailbox.
 */
static int
ifcore_expunge(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);

	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));

	TRY(r = request_expunge(s, p, u));

	lua_pop(lua, 1);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to list available mailboxes.
 */
static int
ifcore_list(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *mboxs, *folders;

	mboxs = folders = NULL;

	if (lua_gettop(lua) != 3)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);
	luaL_checktype(lua, 3, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_list(s, p, u, lua_tostring(lua, 2), lua_tostring(lua, 3),
	    &mboxs, &folders));

	lua_pop(lua, 3);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!mboxs && !folders)
		return 1;

	lua_pushstring(lua, mboxs);
	lua_pushstring(lua, folders);

	xfree(mboxs);
	xfree(folders);

	return 3;
}


/*
 * Core function to list subscribed mailboxes.
 */
static int
ifcore_lsub(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *mboxs, *folders;

	mboxs = folders = NULL;

	if (lua_gettop(lua) != 3)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);
	luaL_checktype(lua, 3, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_lsub(s, p, u, lua_tostring(lua, 2), lua_tostring(lua, 3),
	    &mboxs, &folders));

	lua_pop(lua, 3);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!mboxs)
		return 1;

	lua_pushstring(lua, mboxs);
	lua_pushstring(lua, folders);

	xfree(mboxs);

	return 3;
}


/*
 * Core function to search the messages of a mailbox.
 */
static int
ifcore_search(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *mesgs;

	mesgs = NULL;

	if (lua_gettop(lua) != 3)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);
	luaL_checktype(lua, 3, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_search(s, p, u, lua_tostring(lua, 2), lua_tostring(lua, 3),
	    &mesgs));

	lua_pop(lua, 3);

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!mesgs)
		return 1;

	lua_pushstring(lua, mesgs);

	xfree(mesgs);

	return 2;
}


/*
 * Core function to fetch message information (flags, date, size).
 */
static int
ifcore_fetchfast(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *flags, *date, *size;

	flags = date = size = NULL;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_fetchfast(s, p, u, lua_tostring(lua, 2), &flags, &date,
	    &size));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!flags || !date || !size)
		return 1;

	lua_pushstring(lua, flags);
	lua_pushstring(lua, date);
	lua_pushstring(lua, size);

	xfree(flags);
	xfree(date);
	xfree(size);

	return 4;
}


/*
 * Core function to fetch message flags.
 */
static int
ifcore_fetchflags(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *flags;

	flags = NULL;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_fetchflags(s, p, u, lua_tostring(lua, 2), &flags));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!flags)
		return 1;

	lua_pushstring(lua, flags);

	xfree(flags);

	return 2;
}


/*
 * Core function to fetch message date.
 */
static int
ifcore_fetchdate(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *date;

	date = NULL;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_fetchdate(s, p, u, lua_tostring(lua, 2), &date));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!date)
		return 1;

	lua_pushstring(lua, date);

	xfree(date);

	return 2;
}


/*
 * Core function to fetch message size.
 */
static int
ifcore_fetchsize(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *size;

	size = NULL;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_fetchsize(s, p, u, lua_tostring(lua, 2), &size));

	lua_pop(lua, 2);

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!size)
		return 1;

	lua_pushstring(lua, size);

	xfree(size);

	return 2;
}


/*
 * Core function to fetch message body structure.
 */
static int
ifcore_fetchstructure(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *structure;

	structure = NULL;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_fetchstructure(s, p, u, lua_tostring(lua, 2), &structure));

	lua_pop(lua, 3);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!structure)
		return 1;

	lua_pushstring(lua, structure);

	return 2;
}


/*
 * Core function to fetch message header.
 */
static int
ifcore_fetchheader(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *header;
	size_t len;

	header = NULL;
	len = 0;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_fetchheader(s, p, u, lua_tostring(lua, 2), &header, &len));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!header)
		return 1;

	lua_pushlstring(lua, header, len);

	return 2;
}


/*
 * Core function to fetch message text.
 */
static int
ifcore_fetchtext(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *text;
	size_t len;

	text = NULL;
	len = 0;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_fetchtext(s, p, u, lua_tostring(lua, 2), &text, &len));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!text)
		return 1;

	lua_pushlstring(lua, text, len);

	return 2;
}


/*
 * Core function to fetch message specific header fields.
 */
static int
ifcore_fetchfields(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *fields;
	size_t len;

	fields = NULL;
	len = 0;

	if (lua_gettop(lua) != 3)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);
	luaL_checktype(lua, 3, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_fetchfields(s, p, u, lua_tostring(lua, 2),
	    lua_tostring(lua, 3), &fields, &len));

	lua_pop(lua, 3);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!fields)
		return 1;

	lua_pushlstring(lua, fields, len);

	return 2;
}


/*
 * Core function to fetch message specific part.
 */
static int
ifcore_fetchpart(lua_State *lua)
{
	const char *s, *u, *p;
	int r;
	char *part;
	size_t len;

	part = NULL;
	len = 0;

	if (lua_gettop(lua) != 3)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);
	luaL_checktype(lua, 3, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_fetchpart(s, p, u, lua_tostring(lua, 2),
	    lua_tostring(lua, 3), &part, &len));

	lua_pop(lua, 3);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	if (!part)
		return 1;

	lua_pushlstring(lua, part, len);

	return 2;
}


/*
 * Core function to change message flags.
 */
static int
ifcore_store(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 4)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);
	luaL_checktype(lua, 3, LUA_TSTRING);
	luaL_checktype(lua, 4, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_store(s, p, u, lua_tostring(lua, 2), lua_tostring(lua, 3),
	    lua_tostring(lua, 4)));

	lua_pop(lua, 4);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to copy messages between mailboxes.
 */
static int
ifcore_copy(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 3)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);
	luaL_checktype(lua, 3, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_copy(s, p, u, lua_tostring(lua, 2), lua_tostring(lua, 3)));

	lua_pop(lua, 3);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to append messages to a mailbox.
 */
static int
ifcore_append(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 5)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);
	luaL_checktype(lua, 3, LUA_TSTRING);
	if (lua_type(lua, 4) != LUA_TNIL)
		luaL_checktype(lua, 4, LUA_TSTRING);
	if (lua_type(lua, 5) != LUA_TNIL)
		luaL_checktype(lua, 5, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_append(s, p, u, lua_tostring(lua, 2), lua_tostring(lua, 3),
#if LUA_VERSION_NUM < 502
	    lua_objlen(lua, 3),
#else
	    lua_rawlen(lua, 3),
#endif
	    lua_type(lua, 4) == LUA_TSTRING ?
	    lua_tostring(lua, 4) : NULL, lua_type(lua, 5) == LUA_TSTRING ?
	    lua_tostring(lua, 5) : NULL));

	lua_pop(lua, lua_gettop(lua));

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to create a mailbox.
 */
static int
ifcore_create(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_create(s, p, u, lua_tostring(lua, 2)));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to delete a mailbox.
 */
static int
ifcore_delete(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_delete(s, p, u, lua_tostring(lua, 2)));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to rename a mailbox.
 */
static int
ifcore_rename(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 3)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);
	luaL_checktype(lua, 3, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_rename(s, p, u, lua_tostring(lua, 2), lua_tostring(lua, 3)));

	lua_pop(lua, 3);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to subscribe a mailbox.
 */
static int
ifcore_subscribe(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_subscribe(s, p, u, lua_tostring(lua, 2)));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to unsubscribe a mailbox.
 */
static int
ifcore_unsubscribe(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 2)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);
	luaL_checktype(lua, 2, LUA_TSTRING);

	lua_pushvalue(lua, 1);
	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));
	lua_pop(lua, 1);

	TRY(r = request_unsubscribe(s, p, u, lua_tostring(lua, 2)));

	lua_pop(lua, 2);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Core function to go to idle state.
 */
static int
ifcore_idle(lua_State *lua)
{
	const char *s, *u, *p;
	int r;

	if (lua_gettop(lua) != 1)
		luaL_error(lua, "wrong number of arguments");

	luaL_checktype(lua, 1, LUA_TTABLE);

	if (!(s = get_table_string("server")))
		luaL_error(lua, "no mail server specified");
	if (!(u = get_table_string("username")))
		luaL_error(lua, "no username specified");
	p = DISCOVER_PORT(get_table_string("port"), get_table_string("ssl"));

	TRY(r = request_idle(s, p, u));

	lua_pop(lua, 1);

	if (r == -1)
		return 0;

	lua_pushboolean(lua, (r == STATUS_OK));

	return 1;
}


/*
 * Open imapfilter core library.
 */
LUALIB_API int
luaopen_ifcore(lua_State *lua)
{

#if LUA_VERSION_NUM < 502
	luaL_register(lua, "ifcore", ifcorelib);
#else
	luaL_newlib(lua, ifcorelib);
	lua_setglobal(lua, "ifcore");
#endif

	return 1;
}
