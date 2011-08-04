#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <regex.h>

#include "imapfilter.h"
#include "session.h"
#include "buffer.h"
#include "regexp.h"


extern options opts;

buffer ibuf;			/* Input buffer. */
enum {				/* Server data responses to be parsed;
				 * regular expressions index. */
	DATA_RESPONSE_TAGGED,
	DATA_RESPONSE_CAPABILITY,
	DATA_RESPONSE_AUTHENTICATE,
	DATA_RESPONSE_NAMESPACE,
	DATA_RESPONSE_STATUS,
	DATA_RESPONSE_STATUS_MESSAGES,
	DATA_RESPONSE_STATUS_RECENT,
	DATA_RESPONSE_STATUS_UNSEEN,
	DATA_RESPONSE_STATUS_UIDNEXT,
	DATA_RESPONSE_EXAMINE_EXISTS,
	DATA_RESPONSE_EXAMINE_RECENT,
	DATA_RESPONSE_LIST,
	DATA_RESPONSE_SEARCH,
	DATA_RESPONSE_FETCH,
	DATA_RESPONSE_FETCH_FLAGS,
	DATA_RESPONSE_FETCH_DATE,
	DATA_RESPONSE_FETCH_SIZE,
	DATA_RESPONSE_FETCH_STRUCTURE,
	DATA_RESPONSE_FETCH_BODY,
	DATA_RESPONSE_IDLE,
};
regexp responses[] = {		/* Server data responses to be parsed;
				 * regular expressions patterns. */
	{ "([0-9A-F]{4,4}) (OK|NO|BAD) .*\r\n", NULL, 0, NULL },
	{ "\\* CAPABILITY (.*)\r\n", NULL, 0, NULL },
	{ "\\+ ([^ ]*)\r\n", NULL, 0, NULL },
	{ "\\* NAMESPACE (NIL|\\(\\(\"([^ ]*)\" \"(.)\"\\).*\\)) "
	  "(NIL|\\(.*\\)) (NIL|\\(.*\\))\r\n", NULL, 0, NULL },
	{ "\\* STATUS .* \\(([0-9A-Z ]*)\\) *\r\n", NULL, 0, NULL },
	{ "MESSAGES ([0-9]+)", NULL, 0, NULL },
	{ "RECENT ([0-9]+)", NULL, 0, NULL },
	{ "UNSEEN ([0-9]+)", NULL, 0, NULL },
	{ "UIDNEXT ([0-9]+)", NULL, 0, NULL },
	{ "\\* ([0-9]+) EXISTS\r\n", NULL, 0, NULL },
	{ "\\* ([0-9]+) RECENT\r\n", NULL, 0, NULL },
	{ "\\* (LIST|LSUB) \\((.*)\\) (\".\"|NIL) "
	  "(\"(.+)\"|(.+)|\\{([0-9]+)\\}\r\n(.*))\r\n", NULL, 0, NULL },
	{ "\\* SEARCH ?([0-9 ]*)\r\n", NULL, 0, NULL },
	{ "\\* [0-9]+ FETCH \\((.*)\\)\r\n", NULL, 0, NULL },
	{ "FLAGS \\((.*)\\)", NULL, 0, NULL },
	{ "INTERNALDATE \"(.*)\"", NULL, 0, NULL },
	{ "RFC822.SIZE ([0-9]+)", NULL, 0, NULL },
	{ "BODYSTRUCTURE (\\(.+\\))", NULL, 0, NULL },
	{ "\\* [0-9]+ FETCH \\(.*BODY\\[.*\\] (\\{([0-9]+)\\}\r\n|\"(.*)\")",
	  NULL, 0, NULL },
	{ "\\* [0-9]+ (RECENT|EXISTS)\r\n", NULL, 0, NULL },
	{ NULL, NULL, 0, NULL }
};


int receive_response(session *ssn, char *buf, long timeout, int timeoutfail);

int check_tag(char *buf, session *ssn, int tag);
int check_bye(char *buf);
int check_continuation(char *buf);
int check_trycreate(char *buf);


/*
 * Read data the server sent.
 */
int
receive_response(session *ssn, char *buf, long timeout, int timeoutfail)
{
	ssize_t n;

	if ((n = socket_read(ssn, buf, INPUT_BUF, timeout ? timeout :
	    (long)(get_option_number("timeout")), timeoutfail)) == -1)
		return -1;


	if (opts.debug > 0) {
		int i;
		
		debug("getting response (%d):\n\n", ssn->socket);

		for (i = 0; i < n; i++)
			debugc(buf[i]);

		debug("\n");
	}

	return n;
}


/*
 * Search for tagged response in the data that the server sent.
 */
int
check_tag(char *buf, session *ssn, int tag)
{
	int r;
	char t[4 + 1];
	regexp *re;

	r = STATUS_RESPONSE_NONE;

	snprintf(t, sizeof(t), "%04X", tag);

	re = &responses[DATA_RESPONSE_TAGGED];

	if (!regexec(re->preg, buf, re->nmatch, re->pmatch, 0)) {
		if (!strncasecmp(buf + re->pmatch[1].rm_so, t,
		    strlen(t))) {
			if (!strncasecmp(buf + re->pmatch[2].rm_so,
			    "OK", strlen("OK")))
				r = STATUS_RESPONSE_OK;
			else if (!strncasecmp(buf + re->pmatch[2].rm_so,
			    "NO", strlen("NO")))
				r = STATUS_RESPONSE_NO;
			else if (!strncasecmp(buf + re->pmatch[2].rm_so,
			    "BAD", strlen("BAD")))
				r = STATUS_RESPONSE_BAD;
		}
	}

	if (r != STATUS_RESPONSE_NONE)
		verbose("S (%d): %s", ssn->socket, buf + re->pmatch[0].rm_so);

	if (r == STATUS_RESPONSE_NO || r == STATUS_RESPONSE_BAD)
		error("IMAP (%d): %s", ssn->socket, buf + re->pmatch[0].rm_so);

	return r;
}


/*
 * Check if server sent a BYE response (connection is closed immediately).
 */
int
check_bye(char *buf)
{

	if (xstrcasestr(buf, "* BYE") &&
	    !xstrcasestr(buf, " LOGOUT "))
		return 1;
	else
		return 0;
}


/*
 * Check if server sent a PREAUTH response (connection already authenticated
 * by external means).
 */
int
check_preauth(char *buf)
{

	if (xstrcasestr(ibuf.data, "* PREAUTH"))
		return 1;
	else
		return 0;
}


/*
 * Check if the server sent a continuation request.
 */
int
check_continuation(char *buf)
{

	if ((buf[0] == '+' && buf[1] == ' ') || xstrcasestr(buf, "\r\n+ "))
		return 1;
	else
		return 0;
}


/*
 * Check if the server sent a TRYCREATE response.
 */
int
check_trycreate(char *buf)
{

	if (xstrcasestr(buf, "[TRYCREATE]"))
		return 1;
	else
		return 0;
}


/*
 * Get server data and make sure there is a tagged response inside them.
 */
int
response_generic(session *ssn, int tag)
{
	int r;
	ssize_t n;

	if (tag == -1)
		return -1;

	buffer_reset(&ibuf);

	do {
		buffer_check(&ibuf, ibuf.len + INPUT_BUF);
		if ((n = receive_response(ssn, ibuf.data + ibuf.len, 0, 1)) == -1)
			return -1;
		ibuf.len += n;

		if (check_bye(ibuf.data))
			return -1;
	} while ((r = check_tag(ibuf.data, ssn, tag)) == STATUS_RESPONSE_NONE);

	if (r == STATUS_RESPONSE_NO &&
	    (check_trycreate(ibuf.data) || get_option_boolean("create")))
		return STATUS_RESPONSE_TRYCREATE;

	return r;
}


/*
 * Get server data and make sure there is a continuation response inside them.
 */
int
response_continuation(session *ssn)
{
	ssize_t n;

	buffer_reset(&ibuf);

	do {
		buffer_check(&ibuf, ibuf.len + INPUT_BUF);
		if ((n = receive_response(ssn, ibuf.data + ibuf.len, 0, 1)) == -1)
			return -1;
		ibuf.len += n;

		if (check_bye(ibuf.data))
			return -1;
	} while (!check_continuation(ibuf.data));

	return STATUS_RESPONSE_CONTINUE;
}


/*
 * Process the greeting that server sends during connection.
 */
int
response_greeting(session *ssn)
{

	buffer_reset(&ibuf);

	if (receive_response(ssn, ibuf.data, 0, 1) == -1)
		return -1;

	verbose("S (%d): %s", ssn->socket, ibuf.data);

	if (check_bye(ibuf.data))
		return -1;

	if (check_preauth(ibuf.data))
		return STATUS_RESPONSE_PREAUTH;

	return STATUS_RESPONSE_NONE;
}


/*
 * Process the data that server sent due to IMAP CAPABILITY client request.
 */
int
response_capability(session *ssn, int tag)
{
	int r;
	char *s;
	regexp *re;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	ssn->protocol = PROTOCOL_NONE;

	re = &responses[DATA_RESPONSE_CAPABILITY];

	if (!regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0)) {
		s = xstrndup(ibuf.data + re->pmatch[1].rm_so,
		    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		if (xstrcasestr(s, "IMAP4rev1"))
			ssn->protocol = PROTOCOL_IMAP4REV1;
		else if (xstrcasestr(s, "IMAP4"))
			ssn->protocol = PROTOCOL_IMAP4;
		else {
			error("server supports neither the IMAP4rev1 nor the "
			    "IMAP4 protocol\n");
			return -1;
		}

		ssn->capabilities = CAPABILITY_NONE;

		if (xstrcasestr(s, "NAMESPACE"))
			ssn->capabilities |= CAPABILITY_NAMESPACE;
#ifndef NO_CRAMMD5
		if (xstrcasestr(s, "AUTH=CRAM-MD5"))
			ssn->capabilities |= CAPABILITY_CRAMMD5;
#endif
#ifndef NO_SSLTLS
		if (xstrcasestr(s, "STARTTLS"))
			ssn->capabilities |= CAPABILITY_STARTTLS;
#endif
		if (xstrcasestr(s, "CHILDREN"))
			ssn->capabilities |= CAPABILITY_CHILDREN;

		if (xstrcasestr(s, "IDLE"))
			ssn->capabilities |= CAPABILITY_IDLE;

		xfree(s);
	}

	return r;
}


#ifndef NO_CRAMMD5
/*
 * Process the data that server sent due to IMAP AUTHENTICATE client request.
 */
int
response_authenticate(session *ssn, int tag, unsigned char **cont)
{
	int r;
	regexp *re;

	re = &responses[DATA_RESPONSE_AUTHENTICATE];

	if ((r = response_continuation(ssn)) == STATUS_RESPONSE_CONTINUE &&
	    !regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0))
		*cont = (unsigned char *)xstrndup(ibuf.data + re->pmatch[1].rm_so,
		    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

	return r;
}
#endif


/*
 * Process the data that server sent due to IMAP NAMESPACE client request.
 */
int
response_namespace(session *ssn, int tag)
{
	int r, n;
	regexp *re;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	ssn->ns.prefix = NULL;
	ssn->ns.delim = '\0';

	re = &responses[DATA_RESPONSE_NAMESPACE];

	if (!regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0)) {
		n = re->pmatch[2].rm_eo - re->pmatch[2].rm_so;
		if (n > 0)
			ssn->ns.prefix = xstrndup(ibuf.data +
			    re->pmatch[2].rm_so, n);
		ssn->ns.delim = *(ibuf.data + re->pmatch[3].rm_so);
	}
	debug("namespace (%d): '%s' '%c'\n", ssn->socket,
	    (ssn->ns.prefix ? ssn->ns.prefix : ""), ssn->ns.delim);

	return r;
}


/*
 * Process the data that server sent due to IMAP STATUS client request.
 */
int
response_status(session *ssn, int tag, unsigned int *exist,
    unsigned int *recent, unsigned int *unseen, unsigned int *uidnext)
{
	int r;
	char *s;
	regexp *re;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	re = &responses[DATA_RESPONSE_STATUS];

	if (!regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0)) {
		s = xstrndup(ibuf.data + re->pmatch[1].rm_so,
		    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		re = &responses[DATA_RESPONSE_STATUS_MESSAGES];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0))
			*exist = strtol(s + re->pmatch[1].rm_so, NULL, 10);

		re = &responses[DATA_RESPONSE_STATUS_RECENT];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0))
			*recent = strtol(s + re->pmatch[1].rm_so, NULL, 10);

		re = &responses[DATA_RESPONSE_STATUS_UNSEEN];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0))
			*unseen = strtol(s + re->pmatch[1].rm_so, NULL, 10);

		re = &responses[DATA_RESPONSE_STATUS_UIDNEXT];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0))
			*uidnext = strtol(s + re->pmatch[1].rm_so, NULL, 10);

		xfree(s);
	}

	return r;
}


/*
 * Process the data that server sent due to IMAP EXAMINE client request.
 */
int
response_examine(session *ssn, int tag, unsigned int *exist,
    unsigned int *recent)
{
	int r;
	regexp *re;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	re = &responses[DATA_RESPONSE_EXAMINE_EXISTS];
	if (!regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0))
		*exist = strtol(ibuf.data + re->pmatch[1].rm_so, NULL, 10);

	re = &responses[DATA_RESPONSE_EXAMINE_RECENT];
	if (!regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0))
		*recent = strtol(ibuf.data + re->pmatch[1].rm_so, NULL, 10);

	return r;
}


/*
 * Process the data that server sent due to IMAP SELECT client request.
 */
int
response_select(session *ssn, int tag)
{
	int r;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	if (xstrcasestr(ibuf.data, "[READ-ONLY]"))
		return STATUS_RESPONSE_READONLY;

	return r;
}


/*
 * Process the data that server sent due to IMAP LIST or IMAP LSUB client
 * request.
 */
int
response_list(session *ssn, int tag, char **mboxs, char **folders)
{
	int r, n;
	char *b, *a, *s, *m, *f;
	const char *v;
	regexp *re;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	m = *mboxs = (char *)xmalloc((ibuf.len + 1) * sizeof(char));
	f = *folders = (char *)xmalloc((ibuf.len + 1) * sizeof(char));
	*m = *f = '\0';

	re = &responses[DATA_RESPONSE_LIST];

	b = ibuf.data;
	while (!regexec(re->preg, b, re->nmatch, re->pmatch, 0)) {
		a = xstrndup(b + re->pmatch[2].rm_so,
		    re->pmatch[2].rm_eo - re->pmatch[2].rm_so);

		if (re->pmatch[5].rm_so != -1 && re->pmatch[5].rm_so != -1)
			s = xstrndup(b + re->pmatch[5].rm_so,
			    re->pmatch[5].rm_eo - re->pmatch[5].rm_so);
		else if (re->pmatch[6].rm_so != -1 &&
		    re->pmatch[6].rm_so != -1)
			s = xstrndup(b + re->pmatch[6].rm_so,
			    re->pmatch[6].rm_eo - re->pmatch[6].rm_so);
		else
			s = xstrndup(b + re->pmatch[8].rm_so, strtoul(b +
			    re->pmatch[7].rm_so, NULL, 10));

		v = reverse_namespace(s, ssn->ns.prefix, ssn->ns.delim);
		n = strlen(v);

		if (!xstrcasestr(a, "\\NoSelect")) {
			xstrncpy(m, v, ibuf.len - (m - *mboxs));
			m += n;
			xstrncpy(m, "\n", ibuf.len - (m - *mboxs));
			m += strlen("\n");
		}

		if (!xstrcasestr(a, "\\NoInferiors") &&
		    (!(ssn->capabilities & CAPABILITY_CHILDREN) ||
		    ((ssn->capabilities & CAPABILITY_CHILDREN) &&
		    (xstrcasestr(a, "\\HasChildren")) &&
		    !xstrcasestr(a, "\\HasNoChildren")))) {
			xstrncpy(f, v, ibuf.len - (f - *folders));
			f += n;
			xstrncpy(f, "\n", ibuf.len - (f - *folders));
			f += strlen("\n");
		}

		b += re->pmatch[0].rm_eo;

		xfree(a);
		xfree(s);
	}

	return r;
}


/*
 * Process the data that server sent due to IMAP SEARCH client request.
 */
int
response_search(session *ssn, int tag, char **mesgs)
{
	int r;
	unsigned int min;
	regexp *re;
	char *b, *m;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	re = &responses[DATA_RESPONSE_SEARCH];

	b = ibuf.data;
	m = NULL;
	while (!regexec(re->preg, b, re->nmatch, re->pmatch, 0)) {
		if (!*mesgs) {
			m = *mesgs = (char *)xmalloc((ibuf.len + 1) *
			    sizeof(char));
			*m = '\0';
		}

		min = (unsigned int)(re->pmatch[1].rm_eo - re->pmatch[1].rm_so) < ibuf.len ?
		    (unsigned int)(re->pmatch[1].rm_eo - re->pmatch[1].rm_so) :
		    ibuf.len;

		xstrncpy(m, b + re->pmatch[1].rm_so, min);
		m += min;
		xstrncpy(m++, " ", ibuf.len - min);

		b += re->pmatch[0].rm_eo;
	}

	return r;
}


/*
 * Process the data that server sent due to IMAP FETCH FAST client request.
 */
int
response_fetchfast(session *ssn, int tag, char **flags, char **date,
    char **size)
{
	int r;
	char *s;
	regexp *re;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	re = &responses[DATA_RESPONSE_FETCH];
	if (!regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0)) { 
		s = xstrndup(ibuf.data + re->pmatch[1].rm_so,
		    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		re = &responses[DATA_RESPONSE_FETCH_FLAGS];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0))
			*flags = xstrndup(s + re->pmatch[1].rm_so,
			    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		re = &responses[DATA_RESPONSE_FETCH_DATE];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0))
			*date = xstrndup(s + re->pmatch[1].rm_so,
			    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		re = &responses[DATA_RESPONSE_FETCH_SIZE];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0))
			*size = xstrndup(s + re->pmatch[1].rm_so,
			    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		xfree(s);
	}

	return r;
}


/*
 * Process the data that server sent due to IMAP FETCH FLAGS client request.
 */
int
response_fetchflags(session *ssn, int tag, char **flags)
{
	int r;
	char *s;
	regexp *re;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	re = &responses[DATA_RESPONSE_FETCH];
	if (!regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0)) { 
		s = xstrndup(ibuf.data + re->pmatch[1].rm_so,
		    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		re = &responses[DATA_RESPONSE_FETCH_FLAGS];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0))
			*flags = xstrndup(s + re->pmatch[1].rm_so,
			    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		xfree(s);
	}

	return r;
}


/*
 * Process the data that server sent due to IMAP FETCH INTERNALDATE client
 * request.
 */
int
response_fetchdate(session *ssn, int tag, char **date)
{
	int r;
	char *s;
	regexp *re;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	re = &responses[DATA_RESPONSE_FETCH];
	if (!regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0)) { 
		s = xstrndup(ibuf.data + re->pmatch[1].rm_so,
		    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		re = &responses[DATA_RESPONSE_FETCH_DATE];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0))
			*date = xstrndup(s + re->pmatch[1].rm_so,
			    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		xfree(s);
	}

	return r;
}


/*
 * Process the data that server sent due to IMAP FETCH RFC822.SIZE client
 * request.
 */
int
response_fetchsize(session *ssn, int tag, char **size)
{
	int r;
	char *s;
	regexp *re;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	re = &responses[DATA_RESPONSE_FETCH];
	if (!regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0)) { 
		s = xstrndup(ibuf.data + re->pmatch[1].rm_so,
		    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		re = &responses[DATA_RESPONSE_FETCH_SIZE];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0))
			*size = xstrndup(s + re->pmatch[1].rm_so,
			    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		xfree(s);
	}

	return r;
}


/*
 * Process the data that server sent due to IMAP FETCH BODYSTRUCTURE client
 * request.
 */
int
response_fetchstructure(session *ssn, int tag, char **structure)
{
	int r;
	char *s;
	regexp *re;

	if ((r = response_generic(ssn, tag)) == -1)
		return -1;

	re = &responses[DATA_RESPONSE_FETCH];
	if (!regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0)) { 
		s = xstrndup(ibuf.data + re->pmatch[1].rm_so,
		    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);

		re = &responses[DATA_RESPONSE_FETCH_STRUCTURE];
		if (!regexec(re->preg, s, re->nmatch, re->pmatch, 0)) {
			*structure = xstrndup(s + re->pmatch[1].rm_so,
			    re->pmatch[1].rm_eo - re->pmatch[1].rm_so);
		}
		xfree(s);
	}

	return r;
}


/*
 * Process the data that server sent due to IMAP FETCH BODY[] client request,
 * ie. FETCH BODY[HEADER], FETCH BODY[TEXT], FETCH BODY[HEADER.FIELDS
 * (<fields>)], FETCH BODY[<part>].
 */
int
response_fetchbody(session *ssn, int tag, char **body, size_t *len)
{
	int r, match;
	unsigned int offset;
	ssize_t n;
	regexp *re;

	if (tag == -1)
		return -1;

	buffer_reset(&ibuf);

	match = -1;
	offset = 0;
	
	re = &responses[DATA_RESPONSE_FETCH_BODY];

	do {
		buffer_check(&ibuf, ibuf.len + INPUT_BUF);
		if ((n = receive_response(ssn, ibuf.data + ibuf.len, 0, 1)) == -1)
			return -1;
		ibuf.len += n;

		if (match != 0) {
			match = regexec(re->preg, ibuf.data, re->nmatch,
			    re->pmatch, 0);
			
			if (match == 0 && re->pmatch[2].rm_so != -1 &&
			    re->pmatch[2].rm_eo != -1) {
				*len = strtoul(ibuf.data + re->pmatch[2].rm_so,
				    NULL, 10);
				offset = re->pmatch[0].rm_eo + *len;
			}
		}

		if (offset != 0 && ibuf.len >= offset) {
			if (check_bye(ibuf.data + offset))
				return -1;
		}
	} while (ibuf.len < offset || (r = check_tag(ibuf.data + offset, ssn,
	    tag)) == STATUS_RESPONSE_NONE);

	if (match == 0) {
		if (re->pmatch[2].rm_so != -1 &&
		    re->pmatch[2].rm_eo != -1) {
			*body = ibuf.data + re->pmatch[0].rm_eo;
		} else {
			*body = ibuf.data + re->pmatch[3].rm_so;
			*len = re->pmatch[3].rm_eo - re->pmatch[3].rm_so;
		}
	}

	return r;
}


/*
 * Process the data that server sent due to IMAP IDLE client request.
 */
int
response_idle(session *ssn, int tag)
{
	regexp *re;

	re = &responses[DATA_RESPONSE_IDLE];

	do {
		buffer_reset(&ibuf);

		switch (receive_response(ssn, ibuf.data,
		    get_option_number("keepalive") * 60, 0)) {
		case -1:
			return -1;
			break; /* NOTREACHED */
		case 0:
			return STATUS_RESPONSE_TIMEOUT;
			break; /* NOTREACHED */
		}

		verbose("S (%d): %s", ssn->socket, ibuf.data);

		if (check_bye(ibuf.data))
			return -1;

	} while (regexec(re->preg, ibuf.data, re->nmatch, re->pmatch, 0));

	return STATUS_RESPONSE_UNTAGGED;
}
