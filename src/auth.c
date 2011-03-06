#include <stdio.h>
#include <string.h>

#include "imapfilter.h"
#include "session.h"

#ifndef NO_CRAMMD5
#include <openssl/hmac.h>
#include <openssl/evp.h>


/*
 * Authenticate to the server with the Challenge-Response Authentication
 * Mechanism (CRAM).  The authentication type associated with CRAM is
 * "CRAM-MD5".
 */
int
auth_cram_md5(session *ssn, const char *user, const char *pass)
{
	int t;
	size_t n;
	unsigned int i;
	unsigned char *chal, *resp, *out, *buf;
	unsigned char md[EVP_MAX_MD_SIZE], mdhex[EVP_MAX_MD_SIZE * 2 + 1];
	unsigned int mdlen;
	HMAC_CTX hmac;

	if ((t = imap_authenticate(ssn, "CRAM-MD5")) == -1)
		return -1;

	if (response_authenticate(ssn, t, &chal) ==
	    STATUS_RESPONSE_CONTINUE) {
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

		imap_continuation(ssn, (char *)(out), strlen((char *)(out)));

		xfree(buf);
		xfree(out);
	} else
		return -1;

	return response_authenticate(ssn, t, NULL);
}
#endif				/* NO_CRAMMD5 */
