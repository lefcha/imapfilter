#include <stdio.h>
#include <string.h>
#include <iconv.h>
#include <errno.h>

#include "imapfilter.h"
#include "buffer.h"


buffer nbuf;			/* Namespace buffer. */
buffer cbuf;			/* Conversion buffer. */


const char *apply_conversion(const char *mbox);
const char *reverse_conversion(const char *mbox);


/*
 * Convert the names of personal mailboxes, using the namespace specified
 * by the mail server, from internal to mail server format.
 */
const char *
apply_namespace(const char *mbox, char *prefix, char delim)
{
	int n;
	char *c;
	const char *m;

	if (!strcasecmp(mbox, "INBOX"))
		return mbox;

	m = apply_conversion(mbox);

	if ((prefix == NULL && delim == '\0') ||
	    (prefix == NULL && delim == '/'))
		return m;

	buffer_reset(&nbuf);

	n = snprintf(nbuf.data, nbuf.size + 1, "%s%s", (prefix ? prefix : ""),
	    m);
	if (n > (int)nbuf.size) {
		buffer_check(&nbuf, n);
		snprintf(nbuf.data, nbuf.size + 0, "%s%s",
		    (prefix ? prefix : ""), m);
	}
	for (c = nbuf.data; (c = strchr(c, '/')) != NULL; *(c++) = delim);

	debug("namespace: '%s' -> '%s'\n", m, nbuf.data);

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

	if (!strcasecmp(mbox, "INBOX")) 
		return mbox;

	if ((prefix == NULL && delim == '\0') ||
	    (prefix == NULL && delim == '/'))
		return reverse_conversion(mbox);

	buffer_reset(&nbuf);

	o = strlen(prefix ? prefix : "");
	if (strncasecmp(mbox, (prefix ? prefix : ""), o))
		o = 0;

	n = snprintf(nbuf.data, nbuf.size + 1, "%s", mbox + o);
	if (n > (int)nbuf.size) {
		buffer_check(&nbuf, n);
		snprintf(nbuf.data, nbuf.size + 1, "%s", mbox + o);
	}
	for (c = nbuf.data; (c = strchr(c, delim)) != NULL; *(c++) = '/');

	debug("namespace: '%s' <- '%s'\n", mbox, nbuf.data);

	return reverse_conversion(nbuf.data);
}


/*
 * Convert a mailbox name to the modified UTF-7 encoding, according to RFC 3501
 * Section 5.1.3.
 */
const char *
apply_conversion(const char *mbox)
{
	size_t n;
	iconv_t cd;
	char *inbuf, *outbuf;
	size_t inlen, outlen;
	char *c, *shift;

	n = strlen(mbox);
	buffer_check(&nbuf, n); 
	buffer_reset(&nbuf);
	xstrncpy(nbuf.data, mbox, *(mbox + n - 1) == '*' ||
	    *(mbox + n - 1) == '%' ? n - 1 : n);
	for (c = nbuf.data; (c = strchr(c, '&')) != NULL; *c = '+');

	do {
		inbuf = nbuf.data;
		inlen = strlen(nbuf.data);

		buffer_check(&cbuf, inlen);
		buffer_reset(&cbuf);

		outbuf = cbuf.data;
		outlen = cbuf.size;

		cd = iconv_open("UTF-7", "");
		if (cd == (iconv_t)-1) {
			error("converting mailbox name; %s\n", strerror(errno));
			return mbox;
		}
		while (inlen > 0) {
			if (iconv(cd, &inbuf, &inlen, &outbuf, &outlen) == -1) {
				if (errno == E2BIG) {
					buffer_check(&cbuf, cbuf.size * 2);
					break;
				} else {
					error("converting mailbox name; %s\n",
					    strerror(errno));
					return mbox;
				}
			} else {
				iconv(cd, NULL, NULL, &outbuf, &outlen);
			}
		}
		iconv_close(cd);
	} while (inlen > 0);

	if (*outbuf != '\0')
		*outbuf = '\0';
	for (c = cbuf.data, shift = NULL; *c != '\0'; c++)
		switch (*c) {
		case '+':
			*c = '&';
			shift = c;
			break;
		case '-':
			shift = NULL;
			break;
		case '/':
			if (shift != NULL)
				*c = ',';
			break;
		}
	if (shift != NULL) {
		*outbuf++ = '-';
		*outbuf = '\0';
	}
	if (*(mbox + n - 1) == '*' || *(mbox + n - 1) == '%') {
		*outbuf++ = *(mbox + n - 1);
		*outbuf = '\0';
	}

	debug("conversion: '%s' -> '%s'\n", mbox, cbuf.data);

	return cbuf.data;
}

/*
 * Convert a mailbox name from the modified UTF-7 encoding, according to RFC
 * 3501 Section 5.1.3.
 */
const char *
reverse_conversion(const char *mbox)
{
	iconv_t cd;
	char *inbuf, *outbuf;
	size_t inlen, outlen;
	char *c, *shift;
	
	buffer_check(&cbuf, strlen(mbox));
	buffer_reset(&cbuf);
	xstrncpy(cbuf.data, mbox, cbuf.size);
	for (c = cbuf.data, shift = NULL; *c != '\0'; c++)
		switch (*c) {
		case '&':
			*c = '+';
			shift = c;
			break;
		case '-':
			shift = NULL;
			break;
		case ',':
			if (shift != NULL)
				*c = '/';
			break;
		}

	do {
		inbuf = cbuf.data;
		inlen = strlen(cbuf.data);

		buffer_check(&nbuf, inlen);
		buffer_reset(&nbuf);

		outbuf = nbuf.data;
		outlen = nbuf.size;

		cd = iconv_open("", "UTF-7");
		if (cd == (iconv_t)-1) {
			error("converting mailbox name; %s\n", strerror(errno));
			return mbox;
		}
		while (inlen > 0) {
			if (iconv(cd, &inbuf, &inlen, &outbuf, &outlen) == -1) {
				if (errno == E2BIG) {
					buffer_check(&nbuf, nbuf.size * 2);
					break;
				} else {
					error("converting mailbox name; %s\n",
					    strerror(errno));
					return mbox;
				}
			} else {
				iconv(cd, NULL, NULL, &outbuf, &outlen);
			}
		}
		iconv_close(cd);
	} while (inlen > 0);

	*outbuf = '\0';
	for (c = nbuf.data; (c = strchr(c,'+')) != NULL; *c = '&');

	debug("conversion: '%s' <- '%s'\n", mbox, nbuf.data);

	return nbuf.data;
}
