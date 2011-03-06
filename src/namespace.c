#include <stdio.h>
#include <string.h>

#include "imapfilter.h"
#include "buffer.h"


buffer nbuf;			/* Namespace buffer. */


/*
 * Convert the names of personal mailboxes, using the namespace specified
 * by the mail server, from internal to mail server format.
 */
const char *
apply_namespace(const char *mbox, char *prefix, char delim)
{
	int n;
	char *c;

	if ((prefix == NULL && delim == '\0') ||
	    (prefix == NULL && delim == '/') ||
	    !strcasecmp(mbox, "INBOX"))
		return mbox;

	buffer_reset(&nbuf);

	n = snprintf(nbuf.data, nbuf.size + 1, "%s%s", (prefix ? prefix : ""),
	    mbox);
	if (n > (int)nbuf.size) {
		buffer_check(&nbuf, n);
		snprintf(nbuf.data, nbuf.size + 1, "%s%s",
		    (prefix ? prefix : ""), mbox);
	}
	c = nbuf.data;
	while ((c = strchr(c, '/')))
		*(c++) = delim;

	debug("namespace: '%s' -> '%s'\n", mbox, nbuf.data);

	return nbuf.data;
}


/*
 * Convert the names of personal mailboxes, using the namespace specified by
 * the mail server, from mail server format to internal format.
 */
const char *
reverse_namespace(const char *mbox, char *prefix, char delim)
{
	int n, o;
	char *c;

	if ((prefix == NULL && delim == '\0') ||
	    (prefix == NULL && delim == '/') ||
	    !strcasecmp(mbox, "INBOX"))
		return mbox;

	buffer_reset(&nbuf);

	o = strlen(prefix ? prefix : "");
	if (strncasecmp(mbox, (prefix ? prefix : ""), o))
		o = 0;

	n = snprintf(nbuf.data, nbuf.size + 1, "%s", mbox + o);
	if (n > (int)nbuf.size) {
		buffer_check(&nbuf, n);
		snprintf(nbuf.data, nbuf.size + 1, "%s", mbox + o);
	}
	c = nbuf.data;
	while ((c = strchr(c, delim)))
		*(c++) = '/';

	debug("namespace: '%s' <- '%s'\n", mbox, nbuf.data);

	return nbuf.data;
}
