#include <stdio.h>
#include <string.h>

#include "imapfilter.h"

#ifndef NO_CRAMMD5
#include <openssl/hmac.h>
#include <openssl/evp.h>


/*
 * Authenticate to the server with the Challenge-Response Authentication
 * Mechanism (CRAM).  The authentication type associated with CRAM is
 * "CRAM-MD5".
 */
unsigned char *
auth_cram_md5(const char *user, const char *pass, unsigned char *chal)
{
	size_t n;
	unsigned int i;
	unsigned char *resp, *buf, *out;
	unsigned char md[EVP_MAX_MD_SIZE], mdhex[EVP_MAX_MD_SIZE * 2 + 1];
	unsigned int mdlen;
	HMAC_CTX hmac;

	n = strlen((char *)(chal)) * 3 / 4 + 1;
	resp = (unsigned char *)xmalloc(n * sizeof(char));
	memset(resp, 0, n);

	EVP_DecodeBlock(resp, chal, strlen((char *)(chal)));

	HMAC_Init(&hmac, (const unsigned char *)pass, strlen(pass),
	    EVP_md5());
	HMAC_Update(&hmac, resp, strlen((char *)(resp)));
	HMAC_Final(&hmac, md, &mdlen);

	xfree(chal);
	xfree(resp);

	for (i = 0; i < mdlen; i++)
		snprintf((char *)(mdhex) + i * 2, mdlen * 2 - i * 2 + 1,
		    "%02x", md[i]);
	mdhex[mdlen * 2] = '\0';

	n = strlen(user) + 1 + strlen((char *)(mdhex)) + 1;
	buf = (unsigned char *)xmalloc(n * sizeof(unsigned char));
	memset(buf, 0, n);

	snprintf((char *)(buf), n, "%s %s", user, mdhex);

	n = (strlen((char *)(buf)) + 3) * 4 / 3 + 1;
	out = (unsigned char *)xmalloc(n * sizeof(unsigned char));
	memset(out, 0, n);

	EVP_EncodeBlock(out, buf, strlen((char *)(buf)));

	xfree(buf);

	return out;
}
#endif				/* NO_CRAMMD5 */
