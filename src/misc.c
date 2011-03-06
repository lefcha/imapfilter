#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "imapfilter.h"


/*
 * An implementation of strstr() with case-insensitivity.
 */
const char *
xstrcasestr(const char *haystack, const char *needle)
{
	const char *h, *n, *c;
	size_t hl, nl;

	c = haystack;
	n = needle;
	hl = strlen(haystack);
	nl = strlen(needle);

	while (hl >= nl) {
		while (tolower((int)(*c)) != tolower((int)(*needle))) {
			c++;
			hl--;
			if (hl < nl)
				return NULL;
		}

		h = c;
		n = needle;

		while (tolower((int)(*h)) == tolower((int)(*n))) {
			h++;
			n++;

			if (*n == '\0')
				return c;
		}
		c++;
		hl--;
	}

	return NULL;
}


/*
 * Copies at most size characters from the string pointed by src to the array
 * pointed by dest, always NULL terminating (unless size == 0).  Returns
 * pointer to dest.
 */
char *
xstrncpy(char *dst, const char *src, size_t len)
{
	char *d;
	const char *s;
	size_t n;

	d = dst;
	s = src;
	n = len;

	while (n != 0) {
		if ((*d++ = *s++) == '\0')
			break;
		n--;
	}

	if (n == 0 && len != 0)
		*d = '\0';

	return dst;
}
