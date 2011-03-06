#ifndef REGEXP_H
#define REGEXP_H


#include <stdio.h>
#include <sys/types.h>
#include <regex.h>


/* Regular expression convenience structure. */
typedef struct regexp {
	const char *pattern;	/* Regular expression pattern string. */	
	regex_t *preg;		/* Compiled regular expression. */
	size_t nmatch;		/* Number of subexpressions in pattern. */
	regmatch_t *pmatch;	/* Structure for substrings that matched. */
} regexp;


/*	regexp.c	*/
void regexp_compile(regexp *reg);
void regexp_free(regexp *reg);


#endif				/* REGEXP_H */
