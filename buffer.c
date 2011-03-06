#include <stdio.h>

#include "imapfilter.h"
#include "buffer.h"


/*
 * Initialize buffer.
 */
void
buffer_init(buffer *buf, size_t n)
{

	buf->data = (char *)xmalloc((n + 1) * sizeof(char));
	*buf->data = '\0';
	buf->len = 0;
	buf->size = n;
}


/*
 * Free allocated memory of buffer.
 */
void
buffer_free(buffer *buf)
{

	if (!buf->data)
		return;

	xfree(buf->data);
	buf->data = NULL;
}


/*
 * Reset buffer.
 */
void
buffer_reset(buffer *buf)
{

	*buf->data = '\0';
	buf->len = 0;
}


/*
 * Check if the buffer has enough space to store data and reallocate memory if
 * needed.
 */
void
buffer_check(buffer *buf, size_t n)
{

	while (n > buf->size) {
		buf->size *= 2;
		buf->data = (char *)xrealloc(buf->data, buf->size + 1);
	}
}
