#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "imapfilter.h"
#include "buffer.h"


buffer nbuf;			/* Namespace buffer. */
buffer cbuf;			/* Conversion buffer. */


static const char base64[] = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";


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
	unsigned char *c, *out;
	unsigned char ucp[4], ucplast, ucptemp;
	int padding, shift;

	buffer_check(&nbuf, strlen(mbox)); 
	buffer_reset(&nbuf);
	xstrncpy(nbuf.data, mbox, nbuf.size);
	nbuf.len = strlen(nbuf.data);
	buffer_check(&cbuf, nbuf.len * 4);
	buffer_reset(&cbuf);

	c = (unsigned char *)nbuf.data;
	out = (unsigned char *)cbuf.data;

	memset((void *)ucp, 0, sizeof(ucp));
	ucplast = ucptemp = 0;
	padding = shift = 0;

	while (*c != '\0' || shift > 0) {
		if (shift > 0 && *c <= 0x7F) {
			/* Last character so do Base64 padding. */
			if (padding == 2) {
				*out++ = base64[ucplast << 2 & 0x3C];
			} else if (padding == 4) {
				*out++ = base64[ucplast << 4 & 0x30];
			}
			*out++ = '-';
			padding = 0;
			shift = 0;
			continue;
		}

		/* Escape shift character for modified UTF-7. */
		if (*c == '&') {
			*out++ = '&';
			*out++ = '-';
			c++;
			continue;

		/* Copy all ASCII printable characters. */
		} else if ((*c >= 0x20 && *c <= 0x7e)) {
			*out++ = *c;
			c++;
			continue;
		}

		/* Non-ASCII UTF-8 characters follow. */
		if (shift == 0)
			*out++ = '&';
		/* Convert UTF-8 characters to Unicode code point. */
		if ((*c & 0xE0) == 0xC0) {
			shift = 2;
			ucp[0] = 0x07 & *c >> 2;
			ucp[1] = (*c << 6 & 0xC0) | (*(c + 1) & 0x3F);
			c += 2;
		} else if ((*c & 0xF0) == 0xE0) {
			shift = 3;
			ucp[0] = (*c << 4 & 0xF0) | (*(c + 1) >> 2 & 0x0F);
			ucp[1] = (*(c + 1) << 6 & 0xC0) | (*(c + 2) & 0x3F);
			c += 3;
		} else if ((*c & 0xF8) == 0xF0) {
			shift = 4;
			ucptemp = ((*c << 2 & 0x1C) | (*(c + 1) >> 4 & 0x03)) -
			    0x01;
			ucp[0] = (ucptemp >> 2 & 0x03) | 0xD8;
			ucp[1] = (ucptemp << 6 & 0xC0) |
			    (*(c + 1) << 2 & 0x3C) | (*(c + 2) >> 4 & 0x03);
			ucp[2] = (*(c + 2) >> 2 & 0x03) | 0xDC;
			ucp[3] = (*(c + 2) << 6 & 0xC0) | (*(c + 3) & 0x3F);
			c += 4;
		}

		/* Convert Unicode characters to UTF-7. */
		if (padding == 0) {
			*out++ = base64[ucp[0] >> 2 & 0x3F];
			*out++ = base64[(ucp[0] << 4 & 0x30) |
			    (ucp[1] >> 4 & 0x0F)];
			if (shift == 4) {
				ucplast = ucp[3];
				*out++ = base64[(ucp[1] << 2 & 0x3C) |
				    (ucp[2] >> 6 & 0x03)];
				*out++ = base64[ucp[2] & 0x3F];
				*out++ = base64[ucp[3] >> 2 & 0x3F];
				padding = 4;
			} else {
				ucplast = ucp[1];
				padding = 2;
			}
		} else if (padding == 2) {
			*out++ = base64[(ucplast << 2 & 0x3C) |
			    (ucp[0] >> 6 & 0x03)];
			*out++ = base64[ucp[0] & 0x3F];
			*out++ = base64[ucp[1] >> 2 & 0x3F];
			if (shift == 4) {
				*out++ = base64[(ucp[1] << 4 & 0x30) |
				    (ucp[2] >> 4 & 0x0F)];
				*out++ = base64[(ucp[2] << 2 & 0x3C) |
				    (ucp[3] >> 6 & 0x03)];
				*out++ = base64[ucp[3] & 0x3F];
				padding = 0;
			} else {
				ucplast = ucp[1];
				padding = 4;
			}
		} else if (padding == 4) {
			*out++ = base64[(ucplast << 4 & 0x30) |
			    (ucp[0] >> 4 & 0x0F)];
			*out++ = base64[(ucp[0] << 2 & 0x3C) |
			    (ucp[1] >> 6 & 0x03)];
			if (shift == 4) {
				ucplast = ucp[3];
				*out++ = base64[ucp[1] & 0x3F];
				*out++ = base64[ucp[2] >> 2 & 0x3F];
				*out++ = base64[(ucp[2] << 4 & 0x30) |
				    (ucp[3] >> 4 & 0x0F)];
				padding = 2;
			} else {
				*out++ = base64[ucp[1] & 0x3F];
				padding = 0;
			}
		}
	}
	*out = '\0';

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
	unsigned char *c, *out;
	unsigned char ucp[4], ucptemp;
	unsigned char b64[6], b64last;
	int padding;
	
	buffer_check(&cbuf, strlen(mbox));
	buffer_reset(&cbuf);
	xstrncpy(cbuf.data, mbox, cbuf.size);
	cbuf.len = strlen(cbuf.data);
	buffer_check(&nbuf, cbuf.len);
	buffer_reset(&nbuf);

	c = (unsigned char *)cbuf.data;
	out = (unsigned char *)nbuf.data;

	memset((void *)ucp, 0, sizeof(ucp));
	memset((void *)b64, 0, sizeof(b64));
	ucptemp = b64last = 0;
	padding = 0;

	while (*c != '\0') {
		/* Copy all ASCII printable characters. */
		if (*c >= 0x20 && *c <= 0x7e) {
			if (*c != '&') {
				*out++ = *c++;
				continue;
			} else {
				c++;
			}
		}

		/* Write shift character for modified UTF-7. */
		if (*c == '-') {
			*out++ = '&';
			c++;
			continue;
		}

		/* UTF-7 characters follow. */
		padding = 0;
		do {
			/* Read Base64 characters. */
			b64[0] = strchr(base64, *c) - base64;
			b64[1] = strchr(base64, *(c + 1)) - base64;
			if (padding == 0 || padding == 2) {
				b64[2] = strchr(base64, *(c + 2)) - base64;
				c += 3;
			} else {
				c += 2;
			}
			/* Convert from Base64 to Unicode code point. */
			if (padding == 0) {
				ucp[0] = (b64[0] << 2 & 0xFC) |
				    (b64[1] >> 4 & 0x03);
				ucp[1] = (b64[1] << 4 & 0xF0) |
				    (b64[2] >> 2 & 0x0F);
				b64last = b64[2];
				padding = 2;
			} else if (padding == 2) {
				ucp[0] = (b64last << 6 & 0xC0) |
				    (b64[0] & 0x3F);
				ucp[1] = (b64[1] << 2 & 0xFC) |
				    (b64[2] >> 4 & 0x03);
				b64last = b64[2];
				padding = 4;
			} else if (padding == 4) {
				ucp[0] = (b64last << 4 & 0xF0) |
				    (b64[0] >> 2 & 0x0F);
				ucp[1] = (b64[0] << 6 & 0xC0) |
				    (b64[1] & 0x3F);
				padding = 0;
			}

			/* Convert from Unicode to UTF-8. */
			if (ucp[0] <= 0x07) {
				*out++ = 0xC0 | (ucp[0] << 2 & 0x1C) |
				    (ucp[1] >> 6 & 0x03);
				*out++ = 0x80 | (ucp[1] & 0x3F);
			} else if ((ucp[0] >= 0x08 && ucp[0] <= 0xD7) ||
			    ucp[0] >= 0xE0) {
				*out++ = 0xE0 | (ucp[0] >> 4 & 0x0F);
				*out++ = 0x80 | (ucp[0] << 2 & 0x1C) |
				    (ucp[1] >> 6 & 0x03);
				*out++ = 0x80 | (ucp[1] & 0x3F);
			} else if (ucp[0] >= 0xD8 && ucp[0] <= 0xDF) {
				b64[3] = strchr(base64, *c) - base64;
				b64[4] = strchr(base64, *(c + 1)) - base64;
				if (padding == 0 || padding == 2) {
					b64[5] = strchr(base64, *(c + 2)) -
					    base64;
					c += 3;
				} else {
					c += 2;
				}

				if (padding == 0) {
					ucp[2] = (b64[3] << 2 & 0xFC) |
					    (b64[4] >> 4 & 0x03);
					ucp[3] = (b64[4] << 4 & 0xF0) |
					    (b64[5] >> 2 & 0x0F);
					b64last = b64[5];
					padding = 2;
				} else if (padding == 2) {
					ucp[2] = (b64last << 6 & 0xC0) |
					    (b64[3] & 0x3F);
					ucp[3] = (b64[4] << 2 & 0xFC) |
					    (b64[5] >> 4 & 0x03);
					b64last = b64[5];
					padding = 4;
				} else if (padding == 4) {
					ucp[2] = (b64last << 4 & 0xF0) |
					    (b64[3] >> 2 & 0x0F);
					ucp[3] = (b64[3] << 6 & 0xC0) |
					    (b64[4] & 0x3F);
					padding = 0;
				}

				ucp[0] &= 0xFF - 0xDF;
				ucptemp = ((ucp[0] << 2 & 0x0C) |
				    (ucp[1] >> 6 & 0x03)) + 0x1;
				ucp[2] &= 0xFF - 0xDC;

				*out++ = 0xF0 | (ucptemp >> 2 & 0x03);
				*out++ = 0x80 | (ucptemp << 4 & 0x30) |
				    (ucp[1] >> 2 & 0x0F);
				*out++ = 0x80 | (ucp[1] << 4 & 0x30) |
				    (ucp[2] << 2 & 0x0C) |
				    (ucp[3] >> 6 & 0x03);
				*out++ = 0x80 | (ucp[3] & 0x3F);
			}
			if (*c == '-') {
				c++;
				break;
			}
		} while (*c != '-');
	}
	*out = '\0';

	debug("conversion: '%s' <- '%s'\n", nbuf.data, cbuf.data);

	return nbuf.data;
}
