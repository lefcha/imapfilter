#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "imapfilter.h"
#include "session.h"
#include "buffer.h"


extern options opts;


int create_mailbox(session *ssn, const char *mbox);


/*
 * Reset any inactivity autologout timer on the server.
 */
int
request_noop(const char *server, const char *port, const char *user)
{
	int r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	if ((r = response_generic(s, imap_noop(s))) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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
	session *s;

	if ((s = session_find(server, port, user)))
		return STATUS_RESPONSE_NONE;

	s = session_new();

	s->server = xstrdup(server);
	s->port = xstrdup(port);
	s->username = xstrdup(user);

	if (ssl && strncasecmp(ssl, "tls1", 4) &&
	    strncasecmp(ssl, "ssl3", 4) && strncasecmp(ssl, "ssl2", 4))
		ssl = NULL;

	if (open_connection(s, server, port, ssl) == -1)
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
			if (open_secure_connection(s, server, port, "tls1")
			    == -1)
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
		    get_option_boolean("crammd5")) {
			if ((r = auth_cram_md5(s, user, pass)) == -1)
				goto fail;
		}
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
	    get_option_boolean("namespace")) {
		if (response_namespace(s, imap_namespace(s)) == -1)
			goto fail;
	}

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
		t = imap_status(s, m, "MESSAGES RECENT UNSEEN UIDNEXT");
		if ((r = response_status(s, t, exists, recent, unseen, uidnext)) == -1)
			goto fail;
	} else {
		t = imap_examine(s, m);
		if ((r = response_examine(s, t, exists, recent)) == -1)
			goto fail;
	}

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Open mailbox in read-write mode.
 */
int
request_select(const char *server, const char *port, const char *user,
    const char *mbox)
{
	int r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	if ((r = response_select(s, imap_select(s, m))) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Close examined/selected mailbox.
 */
int
request_close(const char *server, const char *port, const char *user)
{
	int r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	if ((r = response_generic(s, imap_close(s))) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Remove all messages marked for deletion from selected mailbox.
 */
int
request_expunge(const char *server, const char *port, const char *user)
{
	int r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	if ((r = response_generic(s, imap_expunge(s))) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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
	
	t = imap_list(s, refer, n);
	if ((r = response_list(s, t, mboxs, folders)) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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

	t = imap_lsub(s, refer, n);
	if ((r = response_list(s, t, mboxs, folders)) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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

	t = imap_search(s, charset, criteria);
	if ((r = response_search(s, t, mesgs)) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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

	t = imap_fetch(s, mesg, "FAST");
	if ((r = response_fetchfast(s, t, flags, date, size)) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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

	t = imap_fetch(s, mesg, "FLAGS");
	if ((r = response_fetchflags(s, t, flags)) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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

	t = imap_fetch(s, mesg, "INTERNALDATE");
	if ((r = response_fetchdate(s, t, date)) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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

	t = imap_fetch(s, mesg, "RFC822.SIZE");
	if ((r = response_fetchsize(s, t, size)) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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

	t = imap_fetch(s, mesg, "BODYSTRUCTURE");
	if ((r = response_fetchstructure(s, t, structure)) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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

	t = imap_fetch(s, mesg, "BODY.PEEK[HEADER]");
	if ((r = response_fetchbody(s, t, header, len)) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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

	t = imap_fetch(s, mesg, "BODY.PEEK[TEXT]");
	if ((r = response_fetchbody(s, t, text, len)) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Fetch the specified header fields, ie. BODY[HEADER.FIELDS (<fields>)], of
 * the messages.
 */
int
request_fetchfields(const char *server, const char *port, const char *user,
    const char *mesg, const char *headerfields, char **fields, size_t *len)
{
	int t, r, n;
	session *s;
	char *f;

	n = strlen("BODY.PEEK[HEADER.FIELDS ()]") + strlen(headerfields) + 1;
	f = (char *)xmalloc(n * sizeof(char));
	snprintf(f, n, "%s%s%s", "BODY.PEEK[HEADER.FIELDS (", headerfields, ")]");

	if (!(s = session_find(server, port, user)))
		return -1;

	t = imap_fetch(s, mesg, f);
	if ((r = response_fetchbody(s, t, fields, len)) == -1)
		goto fail;

	xfree(f);

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Fetch the specified message part, ie. BODY[<part>], of the
 * messages.
 */
int
request_fetchpart(const char *server, const char *port, const char *user,
    const char *mesg, const char *part, char **bodypart, size_t *len)
{
	int t, r, n;
	session *s;
	char *f;

	n = strlen("BODY.PEEK[]") + strlen(part) + 1;
	f = (char *)xmalloc(n * sizeof(char));
	snprintf(f, n, "%s%s%s", "BODY.PEEK[", part, "]");

	if (!(s = session_find(server, port, user)))
		return -1;

	t = imap_fetch(s, mesg, f);
	if ((r = response_fetchbody(s, t, bodypart, len)) == -1)
		goto fail;

	xfree(f);

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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

	t = imap_store(s, mesg, mode, flags);
	if ((r = response_generic(s, t)) == -1)
		goto fail;

	if (xstrcasestr(flags, "\\Deleted") && get_option_boolean("expunge"))
		if (response_generic(s, imap_expunge(s)) == -1)
			goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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
		t = imap_copy(s, mesg, m);
		switch (r = response_generic(s, t)) {
		case STATUS_RESPONSE_TRYCREATE:
			if (create_mailbox(s, mbox) == -1)
				goto fail;
			break;
		case -1:
			goto fail;
			break;
		}
	} while (r == STATUS_RESPONSE_TRYCREATE);

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
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
		if ((t = imap_append(s, m, flags, date, mesglen)) == -1)
			goto fail;
		if ((r = response_continuation(s)) == -1)
			goto fail;

		switch (r) {
		case STATUS_RESPONSE_CONTINUE:
			if (imap_continuation(s, mesg, mesglen) == -1)
				goto fail;
			if ((r = response_generic(s, t)) == -1)
				goto fail;
			break;
		case STATUS_RESPONSE_TRYCREATE:
			if (create_mailbox(s, mbox) == -1)
				goto fail;
			break;
		case -1:
			goto fail;
			break;
		}
	} while (r == STATUS_RESPONSE_TRYCREATE);

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Create the specified mailbox.
 */
int
request_create(const char *server, const char *port, const char *user,
    const char *mbox)
{
	int r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	if ((r = response_generic(s, imap_create(s, m))) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Delete the specified mailbox.
 */
int
request_delete(const char *server, const char *port, const char *user,
    const char *mbox)
{
	int r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	if ((r = response_generic(s, imap_delete(s, m))) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Rename a mailbox.
 */
int
request_rename(const char *server, const char *port, const char *user,
    const char *oldmbox, const char *newmbox)
{
	int r;
	session *s;
	char *o, *n;

	if (!(s = session_find(server, port, user)))
		return -1;

	o = xstrdup(apply_namespace(oldmbox, s->ns.prefix, s->ns.delim));
	n = xstrdup(apply_namespace(newmbox, s->ns.prefix, s->ns.delim));

	r = response_generic(s, imap_rename(s, o, n));

	xfree(o);
	xfree(n);

	if (r == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Subscribe the specified mailbox.
 */
int
request_subscribe(const char *server, const char *port, const char *user,
    const char *mbox)
{
	int r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	if ((r = response_generic(s, imap_subscribe(s, m))) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Unsubscribe the specified mailbox.
 */
int
request_unsubscribe(const char *server, const char *port, const char *user,
    const char *mbox)
{
	int r;
	session *s;
	const char *m;

	if (!(s = session_find(server, port, user)))
		return -1;

	m = apply_namespace(mbox, s->ns.prefix, s->ns.delim);

	if ((r = response_generic(s, imap_unsubscribe(s, m))) == -1)
		goto fail;

	return r;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


int
request_idle(const char *server, const char *port, const char *user)
{
	int t, rg, ri;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;


	if (!(s->capabilities & CAPABILITY_IDLE))
		return -1;

	do {
		ri = 0;

		t = imap_idle(s);

		if ((rg = response_continuation(s)) == -1)
			goto fail;

		if (rg == STATUS_RESPONSE_CONTINUE) {
			if ((ri = response_idle(s, t)) == -1)
				goto fail;

			imap_done(s);

			if ((rg = response_generic(s, t)) == -1)
				goto fail;
		}
	} while (ri == STATUS_RESPONSE_TIMEOUT);

	return rg;
fail:
	close_connection(s);
	session_destroy(s);

	return -1;
}


/*
 * Auxiliary function to create a mailbox.
 */
int
create_mailbox(session *ssn, const char *mbox)
{
	int r;
	const char *m;

	m = apply_namespace(mbox, ssn->ns.prefix, ssn->ns.delim);

	if ((r = response_generic(ssn, imap_create(ssn, m))) == -1)
		return -1;

	if (get_option_boolean("subscribe"))
		if (response_generic(ssn, imap_subscribe(ssn, m)) == -1)
			return -1;

	return r;
}

