#ifndef IMAPFILTER_H
#define IMAPFILTER_H


#include <stdio.h>
#include <sys/types.h>
#include <limits.h>

#include <lua.h>
#include <lualib.h>

#include <openssl/ssl.h>

#include "session.h"


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
#define CAPABILITY_STARTTLS		0x02
#define CAPABILITY_CHILDREN		0x04
#define CAPABILITY_IDLE			0x08
#define CAPABILITY_XOAUTH2		0x10
#define CAPABILITY_ENABLE		0x20
#define CAPABILITY_UTF8_ACCEPT	0x80
#define CAPABILITY_UTF8_ONLY	0xC0 /* Implies UTF8=ACCEPT */

/* Status responses and response codes. */
#define STATUS_BYE			-2
#define STATUS_ERROR			-1
#define STATUS_NONE			0
#define STATUS_OK			1
#define STATUS_NO			2
#define STATUS_BAD			3
#define STATUS_UNTAGGED			4
#define STATUS_CONTINUE			5
#define STATUS_PREAUTH			6
#define STATUS_READONLY			7
#define STATUS_TRYCREATE		8
#define STATUS_TIMEOUT			9
#define STATUS_INTERRUPT		10

#define STATUS_DRYRUN			STATUS_OK

/* Initial size for buffers. */
#define INPUT_BUF			4096
#define OUTPUT_BUF			1024
#define NAMESPACE_BUF			512
#define CONVERSION_BUF			512

/* Maximum length, in bytes, of a utility's input line. */
#ifndef LINE_MAX
#define LINE_MAX			2048
#endif

/* Program's options. */
typedef struct options {
	int verbose;		/* Verbose mode. */
	int interactive;	/* Act as an interpreter. */
	int dryrun;		/* Don't send commands that do changes. */
	char *log;		/* Log file for error messages. */
	char *config;		/* Configuration file. */
	char *oneline;		/* One line of program/configuration. */
	char *pidfile;          /* Write the PID on a file. */
	char *debug;		/* Debug file. */
	char *truststore;       /* CA TrustStore. */
} options;

/* Environment variables. */
typedef struct environment {
	char *home;		/* Program's home directory. */
	long pathmax;		/* Maximum pathname. */
} environment;


/*	cert.c		*/
int get_cert(session *ssn);

/*	core.c		*/
LUALIB_API int luaopen_ifcore(lua_State *lua);

/*	file.c		*/
void create_homedir(void);
int exists_file(char *fname);
int exists_dir(char *fname);
int create_file(char *fname, mode_t mode);
int get_pathmax(void);
char *get_filepath(char *fname);
void write_pidfile(void);
void remove_pidfile(void);

/*	log.c		*/
void verbose(const char *info,...);
void debug(const char *debug,...);
void debugc(char c);
void error(const char *errmsg,...);
void fatal(unsigned int errnum, const char *fatal,...);

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
const char *apply_namespace(const char *mbox, char *prefix, char delim, char utf8_accept_enabled);
const char *reverse_namespace(const char *mbox, char *prefix, char delim, char utf8_accept_enabled);

/*	pcre.c		*/
LUALIB_API int luaopen_ifre(lua_State *lua);

/*	request.c	*/
int request_noop(session *ssn);
int request_login(session **ssnptr, const char *server, const char *port, const char *sslproto,
    const char *username, const char *password, const char *oauth2);
int request_logout(session *ssn);
int request_status(session *ssn, const char *mbox, unsigned int *exist,
    unsigned int *recent, unsigned int *unseen, unsigned int *uidnext);
int request_select(session *ssn, const char *mbox);
int request_close(session *ssn);
int request_expunge(session *ssn);
int request_list(session *ssn, const char *refer, const char *name, char
    **mboxs, char **folders);
int request_lsub(session *ssn, const char *refer, const char *name, char
    **mboxs, char **folders);
int request_search(session *ssn, const char *criteria, const char *charset,
    char **mesgs);
int request_fetchfast(session *ssn, const char *mesg, char **flags, char
    **date, char **size);
int request_fetchflags(session *ssn, const char *mesg, char **flags);
int request_fetchdate(session *ssn, const char *mesg, char **date);
int request_fetchsize(session *ssn, const char *mesg, char **size);
int request_fetchstructure(session *ssn, const char *mesg, char **structure);
int request_fetchheader(session *ssn, const char *mesg, char **header, size_t
    *len);
int request_fetchtext(session *ssn, const char *mesg, char **text, size_t
    *len);
int request_fetchfields(session *ssn, const char *mesg, const char
    *headerfields, char **fields, size_t *len);
int request_fetchpart(session *ssn, const char *mesg, const char *bodypart,
    char **part, size_t *len);
int request_store(session *ssn, const char *mesg, const char *mode, const char
    *flags);
int request_copy(session *ssn, const char *mesg, const char *mbox);
int request_append(session *ssn, const char *mbox, const char *mesg, size_t
    mesglen, const char *flags, const char *date);
int request_create(session *ssn, const char *mbox);
int request_delete(session *ssn, const char *mbox);
int request_rename(session *ssn, const char *oldmbox, const char *newmbox);
int request_subscribe(session *ssn, const char *mbox);
int request_unsubscribe(session *ssn, const char *mbox);
int request_idle(session *ssn, char **event);

/*	response.c	*/
int response_generic(session *ssn, int tag);
int response_continuation(session *ssn, int tag);
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
int response_idle(session *ssn, int tag, char **event);

/*	signal.c	*/
void catch_signals(void);
void release_signals(void);
void ignore_user_signals(void);
void catch_user_signals(void);

/*	socket.c	*/
int open_connection(session *ssn, const char *server, const char *port,
    const char *sslproto);
int close_connection(session *ssn);
ssize_t socket_read(session *ssn, char *buf, size_t len, long timeout,
    int timeoutfail, int *interrupt);
ssize_t socket_write(session *ssn, const char *buf, size_t len);
int open_secure_connection(session *ssn, const char *server,
    const char *sslproto);
int close_secure_connection(session *ssn);
ssize_t socket_secure_read(session *ssn, char *buf, size_t len);
ssize_t socket_secure_write(session *ssn, const char *buf, size_t len);

/*	system.c	*/
LUALIB_API int luaopen_ifsys(lua_State *lua);


#endif				/* IMAPFILTER_H */
