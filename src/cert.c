#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/evp.h>

#include "imapfilter.h"
#include "session.h"


extern environment env;


int check_cert(X509 *pcert, unsigned char *pmd, unsigned int *pmdlen);
void print_cert(X509 *cert, unsigned char *md, unsigned int *mdlen);
char *get_serial(X509 *cert);
int write_cert(X509 *cert);
int mismatch_cert(void);


/*
 * Get SSL/TLS certificate check it, maybe ask user about it and act
 * accordingly.
 */
int
get_cert(session *ssn)
{
	X509 *cert;
	unsigned char md[EVP_MAX_MD_SIZE];
	unsigned int mdlen;
	long verify;

	mdlen = 0;

	if (!(cert = SSL_get_peer_certificate(ssn->sslconn)))
		return -1;

	verify = SSL_get_verify_result(ssn->sslconn);
	if (!((verify == X509_V_OK) ||
	    (verify == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) ||
	    (verify == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY)))
		goto fail;

	if (verify != X509_V_OK) {
		if (!(X509_digest(cert, EVP_md5(), md, &mdlen)))
			return -1;

		switch (check_cert(cert, md, &mdlen)) {
		case 0:
			if (isatty(STDIN_FILENO) == 0)
				fatal(ERROR_CERTIFICATE, "%s\n",
				    "can't accept certificate in "
				    "non-interactive mode");
			print_cert(cert, md, &mdlen);
			if (write_cert(cert) == -1)
				goto fail;
			break;
		case -1:
			if (isatty(STDIN_FILENO) == 0)
				fatal(ERROR_CERTIFICATE, "%s\n",
				    "certificate mismatch in non-interactive "
				    "mode");
			print_cert(cert, md, &mdlen);
			if (mismatch_cert() == -1)
				goto fail;
			break;
		}
	}

	X509_free(cert);

	return 0;

fail:
	X509_free(cert);

	return -1;
}


/*
 * Check if the SSL/TLS certificate exists in the certificates file.
 */
int
check_cert(X509 *pcert, unsigned char *pmd, unsigned int *pmdlen)
{
	int r;
	FILE *fd;
	char *certf;
	X509 *cert;
	unsigned char md[EVP_MAX_MD_SIZE];
	unsigned int mdlen;

	r = 0;
	cert = NULL;

	certf = get_filepath("certificates");
	if (!exists_file(certf)) {
		xfree(certf);
		return 0;
	}
	fd = fopen(certf, "r");
	xfree(certf);
	if (fd == NULL)
		return -1;

	while ((cert = PEM_read_X509(fd, &cert, NULL, NULL)) != NULL) {
		if (X509_subject_name_cmp(cert, pcert) != 0 ||
		    X509_issuer_and_serial_cmp(cert, pcert) != 0)
			continue;

		if (!X509_digest(cert, EVP_md5(), md, &mdlen) ||
		    *pmdlen != mdlen)
			continue;

		if (memcmp(pmd, md, mdlen) != 0) {
			r = -1;
			break;
		}
		r = 1;
		break;
	}

	fclose(fd);
	X509_free(cert);

	return r;
}


/*
 * Print information about the SSL/TLS certificate.
 */
void
print_cert(X509 *cert, unsigned char *md, unsigned int *mdlen)
{
	unsigned int i;
	char *s;

	s = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
	printf("Server certificate subject: %s\n", s);
	xfree(s);

	s = X509_NAME_oneline(X509_get_issuer_name(cert), NULL, 0);
	printf("Server certificate issuer: %s\n", s);
	xfree(s);

	s = get_serial(cert);
	printf("Server certificate serial: %s\n", s);
	xfree(s);

	printf("Server key fingerprint: ");
	for (i = 0; i < *mdlen; i++)
		printf(i != *mdlen - 1 ? "%02X:" : "%02X\n", md[i]);
}


/*
 * Extract certificate serial number as a string.
 */
char *
get_serial(X509 *cert)
{
	ASN1_INTEGER* serial;
	char *buf;
	long num;
	int  i;
	size_t len;

	serial = X509_get_serialNumber(cert);
	buf = xmalloc(LINE_MAX);
	*buf = '\0';
	if (serial->length <= (int)sizeof(long)) {
		num = ASN1_INTEGER_get(serial);
		if (serial->type == V_ASN1_NEG_INTEGER) {
			snprintf(buf, LINE_MAX, "-%lX", -num);
		} else {
			snprintf(buf, LINE_MAX, "%lX", num);
		}
	} else {
		if (serial->type == V_ASN1_NEG_INTEGER) {
			snprintf(buf, LINE_MAX, "-");
		}
		for (i = 0; i < serial->length; i++) {
			len = strlen(buf);
			snprintf(buf + len, LINE_MAX - len, "%02X",
			    serial->data[i]);
		}
	}
	return buf;
}


/*
 * Write the SSL/TLS certificate after asking the user to accept/reject it.
 */
int
write_cert(X509 *cert)
{
	FILE *fd;
	char c, buf[LINE_MAX];
	char *certf;
	char *s;

	do {
		printf("(R)eject, accept (t)emporarily or "
		    "accept (p)ermanently? ");
		if (fgets(buf, LINE_MAX, stdin) == NULL)
			return -1;
		c = tolower((int)(*buf));
	} while (c != 'r' && c != 't' && c != 'p');

	if (c == 'r')
		return -1;
	else if (c == 't')
		return 0;

	certf = get_filepath("certificates");
	create_file(certf, S_IRUSR | S_IWUSR);
	fd = fopen(certf, "a");
	xfree(certf);
	if (fd == NULL)
		return -1;

	s = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
	fprintf(fd, "Subject: %s\n", s);
	xfree(s);
	s = X509_NAME_oneline(X509_get_issuer_name(cert), NULL, 0);
	fprintf(fd, "Issuer: %s\n", s);
	xfree(s);
	s = get_serial(cert);
	fprintf(fd, "Serial: %s\n", s);
	xfree(s);

	PEM_write_X509(fd, cert);

	fprintf(fd, "\n");
	fclose(fd);

	return 0;
}


/*
 * Ask user to proceed, while a fingerprint mismatch in the SSL/TLS certificate
 * was found.
 */
int
mismatch_cert(void)
{
	char c, buf[LINE_MAX];

	do {
		printf("ATTENTION: SSL/TLS certificate fingerprint mismatch.\n"
		    "Proceed with the connection (y/n)? ");
		if (fgets(buf, LINE_MAX, stdin) == NULL)
			return -1;
		c = tolower((int)(*buf));
	} while (c != 'y' && c != 'n');

	if (c == 'y')
		return 0;
	else
		return -1;
}
