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
	iconv_t cd;
	char *inbuf, *outbuf;
	size_t inlen, outlen;
	char *c, *shift;
	unsigned char *r, *w;

	buffer_check(&nbuf, strlen(mbox)); 
	buffer_reset(&nbuf);
	xstrncpy(nbuf.data, mbox, nbuf.size);
	nbuf.len = strlen(nbuf.data);
	buffer_check(&cbuf, nbuf.len * 5);
	buffer_reset(&cbuf);

	r = (unsigned char *)nbuf.data;
	w = (unsigned char *)cbuf.data;
	inbuf = outbuf = NULL;
	inlen = outlen = 0;
	while (*r != '\0') {
		/* Skip non-printable ASCII characters. */
		if (*r < 0x20 || *r == 0x7F) {
			r++;
			continue;
		}
		/* Escape shift character for modified UTF-7. */
		if (*r == '&') {
			*w++ = '&';
			*w++ = '-';
			r++;
			continue;
		}
		/* Copy ASCII printable characters. */
		if (*r >= 0x20 && *r <= 0x7E) {
			*w++ = *r++;
			continue;
		}
		/* UTF-8 sequence will follow. */
		if (inbuf == NULL) {
			inbuf = (char *)r;
			inlen = 0;
		}
		if ((*r & 0xE0) == 0xC0) {	/* Two byte UTF-8. */
			inlen += 2;
			r += 2;
		} else if ((*r & 0xF0) == 0xE0) {	/* Three byte UTF-8. */
			inlen += 3;
			r += 3;
		} else if ((*r & 0xF8) == 0xF0) {	/* Four byte UTF-8. */
			inlen += 4;
			r += 4;
		}
		/* UTF-8 sequence has ended, convert it to UTF-7. */
		if (inbuf != NULL && (*r <= 0x7F || *r == '\0')) {
			outbuf = (char *)w;
			outlen = cbuf.size - (outbuf - cbuf.data);

			cd = iconv_open("UTF-7", "");
			if (cd == (iconv_t)-1) {
				error("converting mailbox name; %s\n",
				    strerror(errno));
				return mbox;
			}
			while (inlen > 0) {
				if (iconv(cd, &inbuf, &inlen, &outbuf, &outlen)
				    == -1) {
					if (errno == E2BIG) {
						buffer_check(&cbuf, cbuf.size *
						    2);
						break;
					} else {
						error("converting mailbox name;"
						    "%s\n", strerror(errno));
						return mbox;
					}
				} else {
					iconv(cd, NULL, NULL, &outbuf, &outlen);
				}
			}
			iconv_close(cd);

			w = (unsigned char *)outbuf;
			inbuf = outbuf = NULL;
			inlen = outlen = 0;
		}
	}

	if (*w != '\0')
		*w = '\0';

	/* Convert UTF-7 sequences to IMAP modified UTF-7. */
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
		*w++ = '-';
		*w = '\0';
	}

	debug("conversion: '%s' -> '%s'\n", nbuf.data, cbuf.data);

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

	/* Convert IMAP modified UTF-7 sequences to UTF-7. */
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

	/* Convert UTF-7 sequences to IMAP modified UTF-7. */
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

	debug("conversion: '%s' <- '%s'\n", nbuf.data, cbuf.data);

	return nbuf.data;
}
