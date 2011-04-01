#ifndef IMAPFILTER_H
#define IMAPFILTER_H


#include <stdio.h>
#include <sys/types.h>
#include <limits.h>

#include <lua.h>
#include <lualib.h>

#include "session.h"

#ifndef NO_SSLTLS
#include <openssl/ssl.h>
#endif


/* Fatal error exit codes. */
#define ERROR_SIGNAL			1
#define ERROR_CONFIG			2
#define ERROR_MEMALLOC			3
#define ERROR_PATHNAME			4
#define ERROR_CERTIFICATE		5

/* IMAP protocol supported by the server. */
#define PROTOCOL_NONE			0
#define PROTOCOL_IMAP4REV1		1
#define PROTOCOL_IMAP4			2

/* Capabilities of mail server. */
#define CAPABILITY_NONE			0x00
#define CAPABILITY_NAMESPACE		0x01
#define CAPABILITY_CRAMMD5		0x02
#define CAPABILITY_STARTTLS		0x04
#define CAPABILITY_CHILDREN		0x08
#define CAPABILITY_IDLE			0x10

/* Status responses and response codes. */
#define STATUS_RESPONSE_NONE		0
#define STATUS_RESPONSE_OK		1
#define STATUS_RESPONSE_NO		2
#define STATUS_RESPONSE_BAD		3
#define STATUS_RESPONSE_UNTAGGED	4
#define STATUS_RESPONSE_CONTINUE	5
#define STATUS_RESPONSE_BYE		6
#define STATUS_RESPONSE_PREAUTH		7
#define STATUS_RESPONSE_READONLY	8
#define STATUS_RESPONSE_TRYCREATE	9
#define STATUS_RESPONSE_TIMEOUT		10


/* Initial buffer size for input, output and namespace buffers. */
#define INPUT_BUF			4096
#define OUTPUT_BUF			1024
#define NAMESPACE_BUF			128

/* Maximum length, in bytes, of a utility's input line. */
#ifndef LINE_MAX
#define LINE_MAX			2048
#endif


/* Program's options. */
typedef struct options {
	int debug;		/* Debugging level (0..2). */
	int verbose;		/* Verbose mode. */
	int interactive;	/* Act as an interpreter. */
	char *log;		/* Log file for error messages. */
	char *config;		/* Configuration file. */
	char *oneline;		/* One line of program/configuration. */
} options;

/* Environment variables. */
typedef struct environment {
	char *home;		/* User's home directory. */
	long pathmax;		/* Maximum pathname. */
} environment;


/*	auth.c		*/
#ifndef NO_CRAMMD5
int auth_cram_md5(session *ssn, const char *user, const char *pass);
#endif

/*	cert.c		*/
#ifndef NO_SSLTLS
int get_cert(session *ssn);
#endif

/*	core.c		*/
LUALIB_API int luaopen_ifcore(lua_State *lua);

/*	file.c		*/
int create_homedir(void);
int exists_file(char *fname);
int exists_dir(char *fname);
int create_file(char *fname, mode_t mode);
int get_pathmax(void);

/*	imap.c		*/
int imap_continuation(session *ssn, const char *cont, size_t len);
int imap_capability(session *ssn);
int imap_noop(session *ssn);
int imap_logout(session *ssn);
#ifndef NO_SSLTLS
int imap_starttls(session *ssn);
#endif
int imap_authenticate(session *ssn, const char *auth);
int imap_login(session *ssn, const char *user, const char *pass);
int imap_select(session *ssn, const char *mbox);
int imap_examine(session *ssn, const char *mbox);
int imap_create(session *ssn, const char *mbox);
int imap_delete(session *ssn, const char *mbox);
int imap_rename(session *ssn, const char *oldmbox, const char *newmbox);
int imap_subscribe(session *ssn, const char *mbox);
int imap_unsubscribe(session *ssn, const char *mbox);
int imap_list(session *ssn, const char *refer, const char *name);
int imap_lsub(session *ssn, const char *refer, const char *name);
int imap_status(session *ssn, const char *mbox, const char *items);
int imap_append(session *ssn, const char *mbox, const char *flags,
    const char *date, unsigned int size);
int imap_check(session *ssn);
int imap_close(session *ssn);
int imap_expunge(session *ssn);
int imap_search(session *ssn, const char *charset, const char *criteria);
int imap_fetch(session *ssn, const char *mesg, const char *items);
int imap_store(session *ssn, const char *mesg, const char *mode,
    const char *flags);
int imap_copy(session *ssn, const char *mesg, const char *mbox);
int imap_namespace(session *ssn);
int imap_idle(session *ssn);
int imap_done(session *ssn);

/*	log.c		*/
void verbose(const char *info,...);
void debug(const char *debug,...);
void debugc(char c);
void error(const char *errmsg,...);
void fatal(unsigned int errnum, const char *fatal,...);
LUALIB_API int luaopen_iflog(lua_State *lua);

int open_debug(void);
int close_debug(void);

int open_log(void);
int close_log(void);

/*	lua.c	*/
void start_lua(void);
void stop_lua(void);

int get_option_boolean(const char *opt);
lua_Number get_option_number(const char *opt);
const char *get_option_string(const char *opt);

int get_table_type(const char *key);
lua_Number get_table_number(const char *key);
const char *get_table_string(const char *key);

int set_table_nil(const char *key);
int set_table_boolean(const char *key, int value);
int set_table_number(const char *key, lua_Number value);
int set_table_string(const char *key, const char *value);

/*	memory.c	*/
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
void xfree(void *ptr);
char *xstrdup(const char *str);
char *xstrndup(const char *str, size_t len);

/*	misc.c		*/
const char *xstrcasestr(const char *haystack, const char *needle);
char *xstrncpy(char *dest, const char *src, size_t size);

/*	namespace.c	*/
const char *apply_namespace(const char *mbox, char *prefix, char delim);
const char *reverse_namespace(const char *mbox, char *prefix, char delim);

/*	pcre.c		*/
LUALIB_API int luaopen_ifre(lua_State *lua);

/*	request.c	*/
int request_noop(const char *server, const char *port, const char *user);
int request_login(const char *server, const char *port, const char *ssl,
    const char *user, const char *pass);
int request_logout(const char *server, const char *port, const char *user);
int request_status(const char *server, const char *port, const char *user,
    const char *mbox, unsigned int *exist, unsigned int *recent,
    unsigned int *unseen, unsigned int *uidnext);
int request_select(const char *server, const char *port, const char *user,
    const char *mbox);
int request_close(const char *server, const char *port, const char *user);
int request_expunge(const char *server, const char *port, const char *user);
int request_list(const char *server, const char *port, const char *user,
    const char *refer, const char *name, char **mboxs, char **folders);
int request_lsub(const char *server, const char *port, const char *user,
    const char *refer, const char *name, char **mboxs, char **folders);
int request_search(const char *server, const char *port, const char *user,
    const char *criteria, const char *charset, char **mesgs);
int request_fetchfast(const char *server, const char *port, const char *user,
    const char *mesg, char **flags, char **date, char **size);
int request_fetchflags(const char *server, const char *port, const char *user,
    const char *mesg, char **flags);
int request_fetchdate(const char *server, const char *port, const char *user,
    const char *mesg, char **date);
int request_fetchsize(const char *server, const char *port, const char *user,
    const char *mesg, char **size);
int request_fetchstructure(const char *server, const char *port,
    const char *user, const char *mesg, char **structure);
int request_fetchheader(const char *server, const char *port, const char *user,
    const char *mesg, char **header, size_t *len);
int request_fetchtext(const char *server, const char *port, const char *user,
    const char *mesg, char **text, size_t *len);
int request_fetchfields(const char *server, const char *port, const char *user,
    const char *mesg, const char *headerfields, char **fields, size_t *len);
int request_fetchpart(const char *server, const char *port, const char *user,
    const char *mesg, const char *bodypart, char **part, size_t *len);
int request_store(const char *server, const char *port, const char *user,
    const char *mesg, const char *mode, const char *flags);
int request_copy(const char *server, const char *port, const char *user,
    const char *mesg, const char *mbox);
int request_append(const char *server, const char *port, const char *user,
    const char *mbox, const char *mesg, size_t mesglen, const char *flags,
    const char *date);
int request_create(const char *server, const char *port, const char *user,
    const char *mbox);
int request_delete(const char *server, const char *port, const char *user,
    const char *mbox);
int request_rename(const char *server, const char *port, const char *user,
    const char *oldmbox, const char *newmbox);
int request_subscribe(const char *server, const char *port, const char *user,
    const char *mbox);
int request_unsubscribe(const char *server, const char *port, const char *user,
    const char *mbox);
int request_idle(const char *server, const char *port, const char *user);


/*	response.c	*/
int response_generic(session *ssn, int tag);
int response_continuation(session *ssn);
int response_greeting(session *ssn);
int response_capability(session *ssn, int tag);
int response_authenticate(session *ssn, int tag, unsigned char **cont);
int response_namespace(session *ssn, int tag);
int response_status(session *ssn, int tag, unsigned int *exist,
    unsigned int *recent, unsigned int *unseen, unsigned int *uidnext);
int response_examine(session *ssn, int tag, unsigned int *exist,
    unsigned int *recent);
int response_select(session *ssn, int tag);
int response_list(session *ssn, int tag, char **mboxs, char **folders);
int response_search(session *ssn, int tag, char **mesgs);
int response_fetchfast(session *ssn, int tag, char **flags, char **date,
    char **size);
int response_fetchflags(session *ssn, int tag, char **flags);
int response_fetchdate(session *ssn, int tag, char **date);
int response_fetchsize(session *ssn, int tag, char **size);
int response_fetchstructure(session *ssn, int tag, char **structure);
int response_fetchbody(session *ssn, int tag, char **body, size_t *len);
int response_idle(session *ssn, int tag);

/*	signal.c	*/
void catch_signals(void);
void release_signals(void);

/*	socket.c	*/
int open_connection(session *ssn, const char *server, const char *port,
    const char *protocol);
int close_connection(session *ssn);
ssize_t socket_read(session *ssn, char *buf, size_t len, long timeout,
    int timeoutfail);
ssize_t socket_write(session *ssn, const char *buf, size_t len);
#ifndef NO_SSLTLS
int open_secure_connection(session *ssn, const char *server, const char *port,
    const char *protocol);
int close_secure_connection(session *ssn);
ssize_t socket_secure_read(session *ssn, char *buf, size_t len);
ssize_t socket_secure_write(session *ssn, const char *buf, size_t len);
#endif

/*	system.c	*/
LUALIB_API int luaopen_ifsys(lua_State *lua);


#endif				/* IMAPFILTER_H */
