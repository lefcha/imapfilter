#include <stdio.h>
#include <sys/types.h>
#include <regex.h>

#include "imapfilter.h"
#include "regexp.h"


/*
 * Compile all the patterns and allocate the necessary space for the substring
 * matching.
 */
void
regexp_compile(regexp *reg)
{
	regexp *re;

	for (re = reg; re->pattern != NULL; re++) {
		re->preg = (regex_t *)xmalloc(sizeof(regex_t));
		regcomp(re->preg, re->pattern, REG_EXTENDED | REG_ICASE);
		re->nmatch = re->preg->re_nsub + 1;
		re->pmatch = (regmatch_t *)xmalloc(sizeof(regmatch_t) *
		    re->nmatch);
	}
}


/*
 * Free the compiled regular expressions and the space allocated for the
 * substring matching.
 */
void
regexp_free(regexp *reg)
{
	regexp *re;

	for (re = reg; re->pattern != NULL; re++) {
		regfree(re->preg);
		xfree(re->preg);
		xfree(re->pmatch);
	}
}
