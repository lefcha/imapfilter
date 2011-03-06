#ifndef BUFFER_H
#define BUFFER_H


#include <stdio.h>


/* Temporary buffer. */
typedef struct buffer {
	char *data;		/* Text or binary data. */
	size_t len;		/* Length of text or binary data. */
	size_t size;		/* Maximum size of data. */
} buffer;


/*	buffer.c	*/
void buffer_init(buffer *buf, size_t n);
void buffer_free(buffer *buf);
void buffer_reset(buffer *buf);
void buffer_check(buffer *buf, size_t n);


#endif				/* BUFFER_H */
