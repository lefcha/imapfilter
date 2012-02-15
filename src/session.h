#ifndef SESSION_H
#define SESSION_H


#ifndef NO_SSLTLS
#include <openssl/ssl.h>
#endif


/* IMAP session. */
typedef struct session {
	const char *server;	/* Server hostname. */
	const char *port;	/* Server port. */ 
	const char *ssl;	/* SSL protocol. */
	const char *username;	/* User name. */
	const char *password;	/* User password. */
	int socket;		/* Socket. */
#ifndef NO_SSLTLS
	SSL *sslsocket;		/* SSL socket. */
#endif
	unsigned int protocol;	/* IMAP protocol.  Currently IMAP4rev1 and
				 * IMAP4 are supported. */
	unsigned int capabilities;	/* Capabilities of the mail server. */
	struct {		/* Namespace of the mail server's mailboxes. */
		char *prefix;	/* Namespace prefix. */
		char delim;	/* Namespace delimiter. */
	} ns;
	const char *selected;	/* Selected mailbox. */
} session;


/*	session.c	*/
session *session_new(void);
void session_destroy(session *ssn);


#endif				/* SESSION_H */
