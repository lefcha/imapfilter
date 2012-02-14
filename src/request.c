#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

#include "imapfilter.h"
#include "session.h"
#include "buffer.h"


extern options opts;

buffer obuf;			/* Output buffer. */

static int tag = 0x1000;	/* Every IMAP command is prefixed with a
				 * unique [:alnum:] string. */


int send_request(session *s, const char *fmt,...);
int send_continuation(session *s, const char *data, size_t len);


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
	case STATUS_BYE:			\
		close_connection(s);		\
		session_destroy(s);		\
		return -1;			\
		break;				\
	}					\


/*
 * Sends to server data; a command.
 */
int
send_request(session *s, const char *fmt,...)
{
	int n;
	va_list args;
	int t = tag;

	if (s->socket == -1)
		return -1;

	buffer_reset(&obuf);
	obuf.len = snprintf(obuf.data, obuf.size + 1, "%04X ", tag);

	va_start(args, fmt);
	n = vsnprintf(obuf.data + obuf.len, obuf.size - obuf.len -
	    strlen("\r\n") + 1, fmt, args);
	if (n > (int)obuf.size) {
		buffer_check(&obuf, n);
		vsnprintf(obuf.data + obuf.len, obuf.size - obuf.len -
		    strlen("\r\n") + 1, fmt, args);
	}
	obuf.len = strlen(obuf.data);
	va_end(args);

	snprintf(obuf.data + obuf.len, obuf.size - obuf.len + 1, "\r\n");
	obuf.len = strlen(obuf.data);

	if (!strncasecmp(fmt, "LOGIN", strlen("LOGIN"))) {
		debug("sending command (%d):\n\n%.*s*\r\n\n", s->socket,
		    obuf.len - strlen(s->password) - strlen("\"\"\r\n"),
		    obuf.data);
		verbose("C (%d): %.*s*\r\n", s->socket, obuf.len -
		    strlen(s->password) - strlen("\"\"\r\n"),  obuf.data);
	} else {
		debug("sending command (%d):\n\n%s\n", s->socket, obuf.data);
		verbose("C (%d): %s", s->socket, obuf.data);
	}

	if (socket_write(s, obuf.data, obuf.len) == -1)
		return -1;

	if (tag == 0xFFFF)	/* Tag always between 0x1000 and 0xFFFF. */
		tag = 0x0FFF;
	tag++;

	return t;
}

/*
 * Sends a response to a command continuation request.
 */
int
send_continuation(session *s, const char *data, size_t len)
{

	if (s->socket == -1)
		return -1;

	if (socket_write(s, data, len) == -1 ||
	    socket_write(s, "\r\n", strlen("\r\n")) == -1)
		return -1;

	if (opts.debug) {
		unsigned int i;

		debug("sending continuation data (%d):\n\n", s->socket);
		
		for (i = 0; i < len; i++)
			debugc(data[i]);
		
		debug("\r\n\n");
			
	}

	return 0;
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

	TRY(t = send_request(s, "NOOP"));
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
	int t, r = -1, rg = -1;
	session *s = NULL;

	if ((s = session_find(server, port, user)) && s->socket != -1)
		return STATUS_NONE;

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

	if (opts.debug) {
		t = send_request(s, "NOOP");
		if (response_generic(s, t) == -1)
			goto fail;
	}

	t = send_request(s, "CAPABILITY");
	if (response_capability(s, t) == -1)
		goto fail;

#ifndef NO_SSLTLS
	if (!ssl && s->capabilities & CAPABILITY_STARTTLS &&
	    get_option_boolean("starttls")) {
		t = send_request(s, "STARTTLS");
		switch (response_generic(s, t)) {
		case STATUS_OK:
			if (open_secure_connection(s) == -1)
				goto fail;
			t = send_request(s, "CAPABILITY");
			if (response_capability(s, t) == -1)
				goto fail;
			break;
		case -1:
			goto fail;
			break;
		}
	}
#endif

	if (rg != STATUS_PREAUTH) {
#ifndef NO_CRAMMD5
		if (s->capabilities & CAPABILITY_CRAMMD5 &&
		    get_option_boolean("crammd5")) {
			unsigned char *in, *out;
			if ((t = send_request(s, "AUTHENTICATE CRAM-MD5"))
			    == -1)
				goto fail;
			if (response_authenticate(s, t, &in) ==
			    STATUS_CONTINUE) {
				if ((out = auth_cram_md5(user, pass, in)) == NULL)
					goto fail;
				send_continuation(s, (char *)(out),
				    strlen((char *)(out)));
				xfree(out);
				if ((r = response_generic(s, t)) == -1)
					goto fail;
			} else
				goto fail;
		}
#endif
		if (r != STATUS_OK) {
			t = send_request(s, "LOGIN \"%s\" \"%s\"", user, pass);
			if ((r = response_generic(s, t)) == -1)
				goto fail;
		}

		if (r == STATUS_NO) {
			error("username %s or password rejected at %s\n",
			    user, server);
			goto fail;
		}
	} else {
		r = STATUS_PREAUTH;
	}

	t = send_request(s, "CAPABILITY");
	if (response_capability(s, t) == -1)
		goto fail;

	if (s->capabilities & CAPABILITY_NAMESPACE &&
	    get_option_boolean("namespace")) {
		t = send_request(s, "NAMESPACE");
		if (response_namespace(s, t) == -1)
			goto fail;
	}

	if (s->selected) {
		t = send_request(s, "SELECT \"%s\"", s->selected);
		if (response_select(s, t) == -1)
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
	int t, r;
	session *s;

	if (!(s = session_find(server, port, user)))
		return -1;

	t = send_request(s, "LOGOUT");
	r = response_generic(s, t);

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
		TRY(t = send_request(s,
		    "STATUS \"%s\" (MESSAGES RECENT UNSEEN UIDNEXT)", m));
		TRY(r = response_status(s, t, exists, recent, unseen, uidnext));
	} else {
		TRY(t = send_request(s, "EXAMINE \"%s\"", m));
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

	TRY(t = send_request(s, "SELECT \"%s\"", m));
	TRY(r = response_select(s, t));

	if (r == STATUS_OK) {
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

	TRY(t = send_request(s, "CLOSE"));
	TRY(r = response_generic(s, t));

	if (r == STATUS_OK && s->selected) {
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

	TRY(t = send_request(s, "EXPUNGE"));
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
	
	TRY(t = send_request(s, "LIST \"%s\" \"%s\"", refer, n));
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

	TRY(t = send_request(s, "LSUB \"%s\" \"%s\"", refer, n));
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

	if (charset != NULL && *charset != '\0') {
		TRY(t = send_request(s, "UID SEARCH CHARSET \"%s\" %s", charset,
		    criteria));
	} else {
		TRY(t = send_request(s, "UID SEARCH %s", criteria));
	}
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

	TRY(t = send_request(s, "UID FETCH %s FAST", mesg));
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

	TRY(t = send_request(s, "UID FETCH %s FLAGS", mesg));
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

	TRY(t = send_request(s, "UID FETCH %s INTERNALDATE", mesg));
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

	TRY(t = send_request(s, "UID FETCH %s RFC822.SIZE", mesg));
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

	TRY(t = send_request(s, "UID FETCH %s BODYSTRUCTURE", mesg));
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

	TRY(t = send_request(s, "UID FETCH %s BODY.PEEK[HEADER]", mesg));
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

	TRY(t = send_request(s, "UID FETCH %s BODY.PEEK[TEXT]", mesg));
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
		TRY(t = send_request(s, "UID FETCH %s %s", mesg, f));
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
		TRY(t = send_request(s, "UID FETCH %s %s", mesg, f));
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

	TRY(t = send_request(s, "UID STORE %s %sFLAGS.SILENT (%s)", mesg, 
	    (!strncasecmp(mode, "add", 3) ? "+" :
	    !strncasecmp(mode, "remove", 6) ? "-" : ""), flags));
	TRY(r = response_generic(s, t));

	if (xstrcasestr(flags, "\\Deleted") && get_option_boolean("expunge")) {
		TRY(t = send_request(s, "EXPUNGE"));
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
		TRY(t = send_request(s, "UID COPY %s \"%s\"", mesg, m));
		TRY(r = response_generic(s, t));
		switch (r) {
		case STATUS_TRYCREATE:
			TRY(t = send_request(s, "CREATE \"%s\"", m));
			TRY(response_generic(s, t));

			if (get_option_boolean("subscribe")) {
				TRY(t = send_request(s, "SUBSCRIBE \"%s\"", m));
				TRY(response_generic(s, t));
			}
			break;
		case -1:
			return -1;
			break;
		}
	} while (r == STATUS_TRYCREATE);

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
		TRY(t = send_request(s, "APPEND \"%s\"%s%s%s%s%s%s {%d}", m,
			(flags ? " (" : ""), (flags ? flags : ""),
			(flags ? ")" : ""), (date ? " \"" : ""),
			(date ? date : ""), (date ? "\"" : ""), mesglen));
		TRY(r = response_continuation(s));

		switch (r) {
		case STATUS_CONTINUE:
			TRY(send_continuation(s, mesg, mesglen)); 
			TRY(r = response_generic(s, t));
			break;
		case STATUS_TRYCREATE:
			TRY(t = send_request(s, "CREATE \"%s\"", m));
			TRY(response_generic(s, t));

			if (get_option_boolean("subscribe")) {
				TRY(t = send_request(s, "SUBSCRIBE \"%s\"", m));
				TRY(response_generic(s, t));
			}
			break;
		case -1:
			return -1;
			break;
		}
	} while (r == STATUS_TRYCREATE);

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

	TRY(t = send_request(s, "CREATE \"%s\"", m));
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

	TRY(t = send_request(s, "DELETE \"%s\"", m));
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

	TRY(t = send_request(s, "RENAME \"%s\" \"%s\"", o, n));
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

	TRY(t = send_request(s, "SUBSCRIBE \"%s\"", m));
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

	TRY(t = send_request(s, "UNSUBSCRIBE \"%s\"", m));
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

		TRY(t = send_request(s, "IDLE"));
		TRY(r = response_continuation(s));
		if (r == STATUS_CONTINUE) {
			TRY(ri = response_idle(s, t));
			TRY(send_continuation(s, "DONE", strlen("DONE")));
			TRY(r = response_generic(s, t));
		}
	} while (ri == STATUS_TIMEOUT);

	return r;
}
