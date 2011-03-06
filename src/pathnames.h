#ifndef PATHNAMES_H
#define PATHNAMES_H


/* Program's home directory. */
#define PATHNAME_HOME			".imapfilter"

/* Program's configuration file. */
#define PATHNAME_CONFIG			PATHNAME_HOME "/config.lua"

/* Lua imapfilter set functions file. */
#define PATHNAME_COMMON			MAKEFILE_SHAREDIR "/common.lua"

/* Lua imapfilter set functions file. */
#define PATHNAME_SET			MAKEFILE_SHAREDIR "/set.lua"

/* Lua imapfilter account functions file. */
#define PATHNAME_ACCOUNT		MAKEFILE_SHAREDIR "/account.lua"

/* Lua imapfilter mailbox functions file. */
#define PATHNAME_MAILBOX		MAKEFILE_SHAREDIR "/mailbox.lua"

/* Lua imapfilter message functions file. */
#define PATHNAME_MESSAGE		MAKEFILE_SHAREDIR "/message.lua"

/* Lua imapfilter message functions file. */
#define PATHNAME_OPTIONS		MAKEFILE_SHAREDIR "/options.lua"

/* Lua imapfilter regex functions file. */
#define PATHNAME_REGEX			MAKEFILE_SHAREDIR "/regex.lua"

/* Lua imapfilter auxiliary functions file. */
#define PATHNAME_AUXILIARY		MAKEFILE_SHAREDIR "/auxiliary.lua"

/* Lua imapfilter old interface functions file. */
#define PATHNAME_DEPRECATED		MAKEFILE_SHAREDIR "/deprecated.lua"

/* SSL/TLS certificates file. */
#define PATHNAME_CERTS			PATHNAME_HOME "/certificates"

/* Debug temporary file template. */
#define PATHNAME_DEBUG			PATHNAME_HOME "/debug.XXXXXX"


#endif				/* PATHNAMES_H */
