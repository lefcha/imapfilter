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


int send_command(session *ssn, char *cmd, char *alt);
void prepare_command(const char *fmt,...);


/*
 * Sends to server data; a command.
 */
int
send_command(session *ssn, char *cmd, char *alt)
{
	int t = tag;

	if (ssn->socket == -1)
		return -1;

	debug("sending command (%d):\n\n%s\n", ssn->socket,
	    (opts.debug == 1 && alt ? alt : cmd));

	verbose("C (%d): %s", ssn->socket, (alt ? alt : cmd));

	if (socket_write(ssn, cmd, strlen(cmd)) == -1)
		return -1;

	if (tag == 0xFFFF)	/* Tag always between 0x1000 and 0xFFFF. */
		tag = 0x0FFF;
	tag++;

	return t;
}


/*
 * Prepares data for sending and check that the output buffer size is
 * sufficient.
 */
void
prepare_command(const char *fmt,...)
{
	int n;
	va_list args;

	va_start(args, fmt);

	buffer_reset(&obuf);
	n = vsnprintf(obuf.data, obuf.size + 1, fmt, args);
	if (n > (int)obuf.size) {
		buffer_check(&obuf, n);
		vsnprintf(obuf.data, obuf.size + 1, fmt, args);
	}
	va_end(args);
}


/*
 * Sends a response to a command continuation request.
 */
int
imap_continuation(session *ssn, const char *cont, size_t len)
{

	if (ssn->socket == -1)
		return -1;


	if (socket_write(ssn, cont, len) == -1 ||
	    socket_write(ssn, "\r\n", strlen("\r\n")) == -1)
		return -1;

	if (opts.debug > 0) {
		unsigned int i;

		debug("sending continuation data (%d):\n\n", ssn->socket);
		
		for (i = 0; i < len; i++)
			debugc(cont[i]);
		
		debug("\r\n\n");
			
	}

	return 0;
}


/*
 * IMAP CAPABILITY: requests listing of capabilities that the server supports.
 */
int
imap_capability(session *ssn)
{

	prepare_command("%04X CAPABILITY\r\n", tag);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP NOOP: does nothing always succeeds.
 */
int
imap_noop(session *ssn)
{

	prepare_command("%04X NOOP\r\n", tag);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP LOGOUT: informs server that client is done.
 */
int
imap_logout(session *ssn)
{

	prepare_command("%04X LOGOUT\r\n", tag);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP STARTTLS: begins TLS negotiation.
 */
int
imap_starttls(session *ssn)
{

	prepare_command("%04X STARTTLS\r\n", tag);

	return send_command(ssn, obuf.data, NULL);
}


#ifndef NO_CRAMMD5
/*
 * IMAP AUTHENTICATE: indicates authentication mechanism and performs an
 * authentication protocol exchange.
 */
int
imap_authenticate(session *ssn, const char *auth)
{

	prepare_command("%04X AUTHENTICATE %s\r\n", tag, auth);

	return send_command(ssn, obuf.data, NULL);
}
#endif


/*
 * IMAP LOGIN: identifies client to server.
 */
int
imap_login(session *ssn, const char *user, const char *pass)
{
	int n, r;
	char c;
	char *s;

	/* Command to send to server. */
	prepare_command("%04X LOGIN \"%s\" \"%s\"\r\n", tag, user, pass);

	/* Alternate command with password shrouded for safe printing. */
	n = snprintf(&c, 1, "%04X LOGIN \"%s\" *\r\n", tag, user);
	s = (char *)xmalloc((n + 1) * sizeof(char));
	snprintf(s, n + 1, "%04X LOGIN \"%s\" *\r\n", tag, user);

	r = send_command(ssn, obuf.data, s);

	xfree(s);

	return r;
}


/*
 * IMAP SELECT: accesses a mailbox in READ-WRITE mode.
 */
int
imap_select(session *ssn, const char *mbox)
{

	prepare_command("%04X SELECT \"%s\"\r\n", tag, mbox);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP EXAMINE: accesses a mailbox in READ-ONLY mode.
 */
int
imap_examine(session *ssn, const char *mbox)
{

	prepare_command("%04X EXAMINE \"%s\"\r\n", tag, mbox);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP CREATE: creates mailbox.
 */
int
imap_create(session *ssn, const char *mbox)
{

	prepare_command("%04X CREATE \"%s\"\r\n", tag, mbox);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP DELETE: deletes mailbox.
 */
int
imap_delete(session *ssn, const char *mbox)
{

	prepare_command("%04X DELETE \"%s\"\r\n", tag, mbox);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP RENAME: renames mailbox.
 */
int
imap_rename(session *ssn, const char *oldmbox, const char *newmbox)
{

	prepare_command("%04X RENAME \"%s\" \"%s\"\r\n", tag, oldmbox, newmbox);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP SUBSCRIBE: adds the specified mailbox name to the server's set of
 * "active" or "subscribed" mailboxes.
 */
int
imap_subscribe(session *ssn, const char *mbox)
{

	prepare_command("%04X SUBSCRIBE \"%s\"\r\n", tag, mbox);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP UNSUBSCRIBE: removes the specified mailbox name to the server's set of
 * "active" or "subscribed" mailboxes.
 */
int
imap_unsubscribe(session *ssn, const char *mbox)
{

	prepare_command("%04X UNSUBSCRIBE \"%s\"\r\n", tag, mbox);

	return send_command(ssn, obuf.data, NULL);
}



/*
 * IMAP LIST: returns a subset of names from the complete set of all names
 * available.
 */
int
imap_list(session *ssn, const char *refer, const char *name)
{

	prepare_command("%04X LIST \"%s\" \"%s\"\r\n", tag, refer, name);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP LSUB: returns a subset of names from the set of names that the user has
 * declared as being "active" or "subscribed".
 */
int
imap_lsub(session *ssn, const char *refer, const char *name)
{

	prepare_command("%04X LSUB \"%s\" \"%s\"\r\n", tag, refer, name);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP STATUS: requests status of the indicated mailbox.
 */
int
imap_status(session *ssn, const char *mbox, const char *items)
{

	prepare_command("%04X STATUS \"%s\" (%s)\r\n", tag, mbox, items);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP APPEND: append message to the end of a mailbox.
 */
int
imap_append(session *ssn, const char *mbox, const char *flags,
    const char *date, unsigned int size)
{

	prepare_command("%04X APPEND \"%s\"%s%s%s%s%s%s {%d}\r\n", tag, mbox,
	    (flags ? " (" : ""), (flags ? flags : ""), (flags ? ")" : ""),
	    (date ? " \"" : ""), (date ? date : ""), (date ? "\"" : ""), size);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP CHECK: requests a checkpoint of the currently selected mailbox.
 */
int
imap_check(session *ssn)
{

	prepare_command("%04X CHECK\r\n", tag);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP CLOSE: deletes messages and returns to authenticated state.
 */
int
imap_close(session *ssn)
{

	prepare_command("%04X CLOSE\r\n", tag);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP EXPUNGE: permanently removes any messages with the \Deleted flag set.
 */
int
imap_expunge(session *ssn)
{

	prepare_command("%04X EXPUNGE\r\n", tag);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP SEARCH: searches the mailbox for messages that match certain criteria.
 */
int
imap_search(session *ssn, const char *charset, const char *criteria)
{

	if (charset != NULL && *charset != '\0')
		prepare_command("%04X UID SEARCH CHARSET \"%s\" %s\r\n", tag,
		    charset, criteria);
	else
		prepare_command("%04X UID SEARCH %s\r\n", tag,
		    criteria);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP FETCH: retrieves data associated with a message.
 */
int
imap_fetch(session *ssn, const char *mesg, const char *items)
{

	prepare_command("%04X UID FETCH %s %s\r\n", tag, mesg, items);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP STORE: alters data associated with a message.
 */
int
imap_store(session *ssn, const char *mesg, const char *mode,
    const char *flags)
{

	prepare_command("%04X UID STORE %s %sFLAGS.SILENT (%s)\r\n", tag,
	    mesg, (!strncasecmp(mode, "add", 3) ? "+" :
	    !strncasecmp(mode, "remove", 6) ? "-" : ""), flags);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP COPY: copy messages to mailbox.
 */
int
imap_copy(session *ssn, const char *mesg, const char *mbox)
{

	prepare_command("%04X UID COPY %s \"%s\"\r\n", tag, mesg, mbox);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP NAMESPACE: discovers the prefix and delimeter of namespaces used by the
 * server for mailboxes (RFC 2342).
 */
int
imap_namespace(session *ssn)
{

	prepare_command("%04X NAMESPACE\r\n", tag);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP IDLE: enter the idle state and waits for mailbox updates, as specified
 * in the IMAP IDLE extension (RFC 2177).
 */
int
imap_idle(session *ssn)
{
	prepare_command("%04X IDLE\r\n", tag);

	return send_command(ssn, obuf.data, NULL);
}


/*
 * IMAP DONE: ends the idle state entered through the IMAP IDLE command
 * (RFC 2177).
 */
int
imap_done(session *ssn)
{

	return imap_continuation(ssn, "DONE", strlen("DONE"));
}
