#include <stdio.h>
#include <string.h>

#include "imapfilter.h"
#include "session.h"
#include "list.h"


extern list *sessions;


void session_init(session *ssn);


/*
 * Allocate memory for a new session and add it to the sessions linked list.
 */
session *
session_new(void)
{
	session *s = (session *)xmalloc(sizeof(session));

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
	ssn->ssl = NULL;
	ssn->username = NULL;
	ssn->password = NULL;
	ssn->socket = -1;
#ifndef NO_SSLTLS
	ssn->sslsocket = NULL;
#endif
	ssn->protocol = PROTOCOL_NONE;
	ssn->capabilities = CAPABILITY_NONE;
	ssn->ns.prefix = NULL;
	ssn->ns.delim = '\0';
	ssn->selected = NULL;
}


/*
 * Remove session from sessions linked list and free allocated memory.
 */
void
session_destroy(session *ssn)
{
	
	if (!ssn)
		return;

	sessions = list_remove(sessions, ssn);

	if (ssn->ns.prefix)
		xfree(ssn->ns.prefix);
	xfree(ssn);
}
