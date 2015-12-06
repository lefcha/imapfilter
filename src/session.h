#ifndef SESSION_H
#define SESSION_H


#include <openssl/ssl.h>


/* IMAP session. */
typedef struct session {
	const char *server;	/* Server hostname. */
	const char *port;	/* Server port. */ 
	const char *sslproto;	/* SSL protocol. */
	const char *username;	/* User name. */
	const char *password;	/* User password. */
        const char *oauth2;     /* Oauth2 token */
	int socket;		/* Socket. */
	SSL *sslconn;		/* SSL connection. */
	unsigned int protocol;	/* IMAP protocol.  Currently IMAP4rev1 and
				 * IMAP4 are supported. */
	unsigned int capabilities;	/* Capabilities of the mail server. */
	struct {		/* Namespace of the mail server's mailboxes. */
		char *prefix;	/* Namespace prefix. */
		char delim;	/* Namespace delimiter. */
	} ns;
	char *selected;	/* Selected mailbox. */
} session;


/*	session.c	*/
session *session_new(void);
void session_destroy(session *ssn);


#endif				/* SESSION_H */
