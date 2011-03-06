#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "imapfilter.h"


/*
 * A malloc() that checks the results and dies in case of error.
 */
void *
xmalloc(size_t size)
{
	void *ptr;

	ptr = (void *)malloc(size);

	if (ptr == NULL)
		fatal(ERROR_MEMALLOC,
		    "allocating memory; %s\n", strerror(errno));

	return ptr;
}


/*
 * A realloc() that checks the results and dies in case of error.
 */
void *
xrealloc(void *ptr, size_t size)
{

	ptr = (void *)realloc(ptr, size);

	if (ptr == NULL)
		fatal(ERROR_MEMALLOC,
		    "allocating memory; %s\n", strerror(errno));

	return ptr;
}


/*
 * A free() that dies if fed with NULL pointer.
 */
void
xfree(void *ptr)
{

	if (ptr == NULL)
		fatal(ERROR_MEMALLOC,
		    "NULL pointer given as argument\n");
	free(ptr);
}


/*
 * A strdup() that checks the results and dies in case of error.
 */
char *
xstrdup(const char *str)
{
	char *dup;

	dup = strdup(str);

	if (dup == NULL)
		fatal(ERROR_MEMALLOC, "allocating memory; %s\n",
		    strerror(errno));

	return dup;
}


/*
 * A strndup() implementation that also checks the results and dies in case of
 * error.
 */
char *
xstrndup(const char *str, size_t len)
{
	char *dup;

	dup = (char *)xmalloc((len + 1) * sizeof(char));

	memcpy(dup, str, len);

	dup[len] = '\0';

	return dup;
}
