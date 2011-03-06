#include <stdio.h>
#include <string.h>

#include "imapfilter.h"
#include "session.h"
#include "list.h"


extern list *sessions;


/*
 * Allocate memory for a new session and add it to the sessions linked list.
 */
session *
session_new(void)
{
	session *s;

	s = (session *)xmalloc(sizeof(session));

	session_init(s);

	sessions = list_append(sessions, s);

	return s;
}


/*
 * Set session variables to safe values.
 */
void
session_init(session *ssn)
{

	ssn->server = NULL;
	ssn->port = NULL;
	ssn->username = NULL;
	ssn->socket = -1;
#ifndef NO_SSLTLS
	ssn->ssl = NULL;
#endif
	ssn->protocol = PROTOCOL_NONE;
	ssn->capabilities = CAPABILITY_NONE;
	ssn->ns.prefix = NULL;
	ssn->ns.delim = '\0';
}


/*
 * Remove session from sessions linked list.
 */
void
session_destroy(session *ssn)
{

	sessions = list_remove(sessions, ssn);

	session_free(ssn);
}


/*
 * Free session allocated memory.
 */
void
session_free(session *ssn)
{

	if (ssn->server)
		xfree(ssn->server);
	if (ssn->port)
		xfree(ssn->port);
	if (ssn->username)
		xfree(ssn->username);
	if (ssn->ns.prefix)
		xfree(ssn->ns.prefix);
	xfree(ssn);
}


/*
 * Based on the specified socket, find an active IMAP session.
 */
session *
session_find(const char *serv, const char *port, const char *user)
{
	list *l;
	session *s;

	for (l = sessions; l; l = l->next) {
		s = l->data;
		if (!strcmp(s->server, serv) &&
		    !strcmp(s->port, port) &&
		    !strcmp(s->username, user))
			return s;
	}

	return NULL;
}
