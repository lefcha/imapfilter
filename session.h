#ifndef SESSION_H
#define SESSION_H


#ifndef NO_SSLTLS
#include <openssl/ssl.h>
#endif


/* IMAP session. */
typedef struct session {
	char *server;		/* Server hostname. */
	char *port;		/* Server port. */ 
	char *username;		/* User name. */
	int socket;		/* Socket. */
#ifndef NO_SSLTLS
	SSL *ssl;		/* SSL socket. */
#endif
	unsigned int protocol;	/* IMAP protocol.  Currently IMAP4rev1 and
				 * IMAP4 are supported. */
	unsigned int capabilities;	/* Capabilities of the mail server. */
	struct {		/* Namespace of the mail server's mailboxes. */
		char *prefix;	/* Namespace prefix. */
		char delim;	/* Namespace delimiter. */
	} ns;
} session;


/*	session.c	*/
session *session_new(void);
void session_init(session *ssn);
void session_destroy(session *ssn);
void session_free(session *ssn);
session *session_find(const char *server, const char *port, const char *user);


#endif				/* SESSION_H */
