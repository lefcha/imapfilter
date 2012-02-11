#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "imapfilter.h"
#include "session.h"
#include "buffer.h"


extern options opts;


#define TRY(F)					\
	switch ((F)) {				\
	case -1:				\
		if (request_login(s->server,	\
		    s->port,			\
		    s->ssl,			\
		    s->username,		\
		    s->password) != -1)		\
			F;			\
		else				\
			return -1;		\
		break;				\
	case STATUS_RESPONSE_BYE:		\
		close_connection(s);		\
		session_destroy(s);		\
		return -1;			\
		break;				\
	}


/*
 * Reset any inactivity autologout timer on the server.
 */
int
request_noop(const char *server, const char *port, const char *user)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_noop(s));
	TRY(r = response_generic(s, t));

	return r;
}


/*
 * Connect to the server, login to the IMAP server, get it's capabilities, get
 * the namespace of the mailboxes.
 */
int
request_login(const char *server, const char *port, const char *ssl,
    const char *user, const char *pass)
{
	int r = -1, rg = -1;
	session *s = NULL;

	if ((s = session_find(server, port, user)) && s->socket != -1)
		return STATUS_RESPONSE_NONE;

	if (!s) {
		s = session_new();

		s->server = xstrdup(server);
		s->port = xstrdup(port);
		s->username = xstrdup(user);
		s->password = xstrdup(pass);

		if (ssl && (!strncasecmp(ssl, "tls1", 4) ||
		    !strncasecmp(ssl, "ssl3", 4) ||
		    strncasecmp(ssl, "ssl2", 4)))
			s->ssl = xstrdup(ssl);
	}

	if (open_connection(s) == -1)
		goto fail;

	if ((rg = response_greeting(s)) == -1)
		goto fail;

	if (opts.debug)
		if (response_generic(s, imap_noop(s)) == -1)
			goto fail;

	if (response_capability(s, imap_capability(s)) == -1)
		goto fail;

#ifndef NO_SSLTLS
	if (!ssl && s->capabilities & CAPABILITY_STARTTLS &&
	    get_option_boolean("starttls"))
		switch (response_generic(s, imap_starttls(s))) {
		case STATUS_RESPONSE_OK:
			if (open_secure_connection(s) == -1)
				goto fail;
			if (response_capability(s, imap_capability(s)) == -1)
				goto fail;
			break;
		case -1:
			goto fail;
			break;
		}
#endif

	if (rg != STATUS_RESPONSE_PREAUTH) {
#ifndef NO_CRAMMD5
		if (s->capabilities & CAPABILITY_CRAMMD5 &&
		    get_option_boolean("crammd5"))
			if ((r = auth_cram_md5(s, user, pass)) == -1)
				goto fail;
#endif
		if (r != STATUS_RESPONSE_OK &&
		    (r = response_generic(s, imap_login(s, user, pass))) == -1)
			goto fail;

		if (r == STATUS_RESPONSE_NO) {
			error("username %s or password rejected at %s\n",
			    user, server);
			goto fail;
		}
	} else {
		r = STATUS_RESPONSE_PREAUTH;
	}

	if (response_capability(s, imap_capability(s)) == -1)
		goto fail;

	if (s->capabilities & CAPABILITY_NAMESPACE &&
	    get_option_boolean("namespace"))
		if (response_namespace(s, imap_namespace(s)) == -1)
			goto fail;

	if (s->selected)
		if (response_select(s, imap_select(s, s->selected)) == -1)
			goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Logout from the IMAP server and disconnect from the server.
 */
int
request_logout(const char *server, const char *port, const char *user)
{
	int r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	r = response_generic(s, imap_logout(s));

	close_connection(s);
	session_destroy(s);

	return r;
}


/*
 * Get mailbox's status.
 */
int
request_status(const char *server, const char *port, const char *user,
    const char *mbox, unsigned int *exists, unsigned int *recent,
    unsigned int *unseen, unsigned int *uidnext)
{
	int t, r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	if (s->protocol == PROTOCOL_IMAP4REV1) {
		TRY(t = imap_status(s, m, "MESSAGES RECENT UNSEEN UIDNEXT"));
		TRY(r = response_status(s, t, exists, recent, unseen, uidnext));
	} else {
		TRY(t = imap_examine(s, m));
		TRY(r = response_examine(s, t, exists, recent));
	}

	return r;
}


/*
 * Open mailbox in read-write mode.
 */
int
request_select(const char *server, const char *port, const char *user,
    const char *mbox)
{
	int t, r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	TRY(t = imap_select(s, m));
	TRY(r = response_select(s, t));

	if (r == STATUS_RESPONSE_OK) {
		if (s && s->selected)
			xfree(s->selected);
		s->selected = xstrdup(m);
	}
	
	return r;
}


/*
 * Close examined/selected mailbox.
 */
int
request_close(const char *server, const char *port, const char *user)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_close(s));
	TRY(r = response_generic(s, t));

	if (r == STATUS_RESPONSE_OK && s->selected) {
		xfree(s->selected);
		s->selected = NULL;
	}

	return r;
}


/*
 * Remove all messages marked for deletion from selected mailbox.
 */
int
request_expunge(const char *server, const char *port, const char *user)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_expunge(s));
	TRY(r = response_generic(s, t));

	return r;
}


/*
 * List available mailboxes.
 */
int
request_list(const char *server, const char *port, const char *user,
    const char *refer, const char *name, char **mboxs, char **folders)
{
	int t, r;
	session *s;
	const char *n;

	if (!(s = session_find(server, port, user)))
		return -1;

	n = apply_namespace(name, s->ns.prefix, s->ns.delim);
	
	TRY(t = imap_list(s, refer, n));
	TRY(r = response_list(s, t, mboxs, folders));

	return r;
}


/*
 * List subscribed mailboxes.
 */
int
request_lsub(const char *server, const char *port, const char *user,
    const char *refer, const char *name, char **mboxs, char **folders)
{
	int t, r;
	session *s;
	const char *n;

	if (!(s = session_find(server, port, user)))
		return -1;

	n = apply_namespace(name, s->ns.prefix, s->ns.delim);

	TRY(t = imap_lsub(s, refer, n));
	TRY(r = response_list(s, t, mboxs, folders));

	return r;
}


/*
 * Search selected mailbox according to the supplied search criteria.
 */
int
request_search(const char *server, const char *port, const char *user,
    const char *criteria, const char *charset, char **mesgs)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_search(s, charset, criteria));
	TRY(r = response_search(s, t, mesgs));

	return r;
}


/*
 * Fetch the FLAGS, INTERNALDATE and RFC822.SIZE of the messages.
 */
int
request_fetchfast(const char *server, const char *port, const char *user,
    const char *mesg, char **flags, char **date, char **size)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_fetch(s, mesg, "FAST"));
	TRY(r = response_fetchfast(s, t, flags, date, size));

	return r;
}


/*
 * Fetch the FLAGS of the messages.
 */
int
request_fetchflags(const char *server, const char *port, const char *user,
    const char *mesg, char **flags)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_fetch(s, mesg, "FLAGS"));
	TRY(r = response_fetchflags(s, t, flags));

	return r;
}


/*
 * Fetch the INTERNALDATE of the messages.
 */
int
request_fetchdate(const char *server, const char *port, const char *user,
    const char *mesg, char **date)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_fetch(s, mesg, "INTERNALDATE"));
	TRY(r = response_fetchdate(s, t, date));

	return r;
}
/*
 * Fetch the RFC822.SIZE of the messages.
 */
int
request_fetchsize(const char *server, const char *port, const char *user,
    const char *mesg, char **size)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_fetch(s, mesg, "RFC822.SIZE"));
	TRY(r = response_fetchsize(s, t, size));

	return r;
}


/*
 * Fetch the body structure, ie. BODYSTRUCTURE, of the messages.
 */
int
request_fetchstructure(const char *server, const char *port, const char *user,
    const char *mesg, char **structure)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_fetch(s, mesg, "BODYSTRUCTURE"));
	TRY(r = response_fetchstructure(s, t, structure));

	return r;
}


/*
 * Fetch the header, ie. BODY[HEADER], of the messages.
 */
int
request_fetchheader(const char *server, const char *port, const char *user,
    const char *mesg, char **header, size_t *len)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_fetch(s, mesg, "BODY.PEEK[HEADER]"));
	TRY(r = response_fetchbody(s, t, header, len));

	return r;
}


/*
 * Fetch the text, ie. BODY[TEXT], of the messages.
 */
int
request_fetchtext(const char *server, const char *port, const char *user,
    const char *mesg, char **text, size_t *len)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_fetch(s, mesg, "BODY.PEEK[TEXT]"));
	TRY(r = response_fetchbody(s, t, text, len));

	return r;
}


/*
 * Fetch the specified header fields, ie. BODY[HEADER.FIELDS (<fields>)], of
 * the messages.
 */
int
request_fetchfields(const char *server, const char *port, const char *user,
    const char *mesg, const char *headerfields, char **fields, size_t *len)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	{
		int n = strlen("BODY.PEEK[HEADER.FIELDS ()]") +
		    strlen(headerfields) + 1;
		char f[n];

		snprintf(f, n, "%s%s%s", "BODY.PEEK[HEADER.FIELDS (",
		    headerfields, ")]");
		TRY(t = imap_fetch(s, mesg, f));
	}
	TRY(r = response_fetchbody(s, t, fields, len));

	return r;
}


/*
 * Fetch the specified message part, ie. BODY[<part>], of the
 * messages.
 */
int
request_fetchpart(const char *server, const char *port, const char *user,
    const char *mesg, const char *part, char **bodypart, size_t *len)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	{
		int n = strlen("BODY.PEEK[]") + strlen(part) + 1;
		char f[n];

		snprintf(f, n, "%s%s%s", "BODY.PEEK[", part, "]");
		TRY(t = imap_fetch(s, mesg, f));
	}
	TRY(r = response_fetchbody(s, t, bodypart, len));

	return r;
}


/*
 * Add, remove or replace the specified flags of the messages.
 */
int
request_store(const char *server, const char *port, const char *user,
    const char *mesg, const char *mode, const char *flags)
{
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	TRY(t = imap_store(s, mesg, mode, flags));
	TRY(r = response_generic(s, t));

	if (xstrcasestr(flags, "\\Deleted") && get_option_boolean("expunge")) {
		TRY(t = imap_expunge(s));
		TRY(response_generic(s, t));
	}

	return r;
}


/*
 * Copy the specified messages to another mailbox.
 */
int
request_copy(const char *server, const char *port, const char *user,
    const char *mesg, const char *mbox)
{
	int t, r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	do {
		TRY(t = imap_copy(s, mesg, m));
		TRY(r = response_generic(s, t));
		switch (r) {
		case STATUS_RESPONSE_TRYCREATE:
			TRY(t = imap_create(s, m));
			TRY(response_generic(s, t));

			if (get_option_boolean("subscribe")) {
				TRY(t = imap_subscribe(s, m));
				TRY(response_generic(s, t));
			}
			break;
		case -1:
			return -1;
			break;
		}
	} while (r == STATUS_RESPONSE_TRYCREATE);

	return r;
}


/*
 * Append supplied message to the specified mailbox.
 */
int
request_append(const char *server, const char *port, const char *user,
    const char *mbox, const char *mesg, size_t mesglen, const char *flags,
    const char *date)
{
	int t, r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	do {
		TRY(t = imap_append(s, m, flags, date, mesglen));
		TRY(r = response_continuation(s));

		switch (r) {
		case STATUS_RESPONSE_CONTINUE:
			TRY(imap_continuation(s, mesg, mesglen)); 
			TRY(r = response_generic(s, t));
			break;
		case STATUS_RESPONSE_TRYCREATE:
			TRY(t = imap_create(s, m));
			TRY(response_generic(s, t));

			if (get_option_boolean("subscribe")) {
				TRY(t = imap_subscribe(s, m));
				TRY(response_generic(s, t));
			}
			break;
		case -1:
			return -1;
			break;
		}
	} while (r == STATUS_RESPONSE_TRYCREATE);

	return r;
}


/*
 * Create the specified mailbox.
 */
int
request_create(const char *server, const char *port, const char *user,
    const char *mbox)
{
	int t, r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	TRY(t = imap_create(s, m));
	TRY(r = response_generic(s, t));

	return r;
}


/*
 * Delete the specified mailbox.
 */
int
request_delete(const char *server, const char *port, const char *user,
    const char *mbox)
{
	int t, r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	TRY(t = imap_delete(s, m));
	TRY(r = response_generic(s, t));

	return r;
}


/*
 * Rename a mailbox.
 */
int
request_rename(const char *server, const char *port, const char *user,
    const char *oldmbox, const char *newmbox)
{
	int t, r;
	session *s;
	char *o, *n;

	if (!(s = session_find(server, port, user)))
		return -1;

	o = xstrdup(apply_namespace(oldmbox, s->ns.prefix, s->ns.delim));
	n = xstrdup(apply_namespace(newmbox, s->ns.prefix, s->ns.delim));

	TRY(t = imap_rename(s, o, n));
	TRY(r = response_generic(s, t));

	return r;
}


/*
 * Subscribe the specified mailbox.
 */
int
request_subscribe(const char *server, const char *port, const char *user,
    const char *mbox)
{
	int t, r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	TRY(t = imap_subscribe(s, m));
	TRY(r = response_generic(s, t));

	return r;
}


/*
 * Unsubscribe the specified mailbox.
 */
int
request_unsubscribe(const char *server, const char *port, const char *user,
    const char *mbox)
{
	int t, r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	TRY(t = imap_unsubscribe(s, m));
	TRY(r = response_generic(s, t));

	return r;
}


int
request_idle(const char *server, const char *port, const char *user)
{
	int t, r, ri;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;


	if (!(s->capabilities & CAPABILITY_IDLE))
		return -1;

	do {
		ri = 0;

		TRY(t = imap_idle(s));
		TRY(r = response_continuation(s));
		if (r == STATUS_RESPONSE_CONTINUE) {
			TRY(ri = response_idle(s, t));
			TRY(imap_done(s));
			TRY(r = response_generic(s, t));
		}
	} while (ri == STATUS_RESPONSE_TIMEOUT);

	return r;
}
