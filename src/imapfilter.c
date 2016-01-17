#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <locale.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "imapfilter.h"
#include "session.h"
#include "list.h"
#include "version.h"
#include "buffer.h"
#include "pathnames.h"
#include "regexp.h"


extern buffer ibuf, obuf, nbuf, cbuf;
extern regexp responses[];
extern SSL_CTX *ssl23ctx, *tls1ctx;
#ifndef OPENSSL_NO_SSL3_METHOD
extern SSL_CTX *ssl3ctx;
#endif
#if OPENSSL_VERSION_NUMBER >= 0x01000100fL
extern SSL_CTX *tls11ctx, *tls12ctx;
#endif

options opts;			/* Program options. */
environment env;		/* Environment variables. */

list *sessions = NULL;		/* Active IMAP sessions. */

void usage(void);
void version(void);


/*
 * IMAPFilter: an IMAP mail filtering utility.
 */
int
main(int argc, char *argv[])
{
	int c;
	char *cafile = NULL, *capath = NULL;

	setlocale(LC_CTYPE, "");

	opts.verbose = 0;
	opts.interactive = 0;
	opts.log = NULL;
	opts.config = NULL;
	opts.oneline = NULL;
	opts.debug = NULL;

	opts.truststore = NULL;
	if (exists_dir("/etc/ssl/certs"))
		opts.truststore = "/etc/ssl/certs";
	else if (exists_file("/etc/ssl/cert.pem"))
		opts.truststore = "/etc/ssl/cert.pem";

	env.home = NULL;
	env.pathmax = -1;

	while ((c = getopt(argc, argv, "Vc:d:e:il:t:v?")) != -1) {
		switch (c) {
		case 'V':
			version();
			/* NOTREACHED */
			break;
		case 'c':
			opts.config = optarg;
			break;
		case 'd':
			opts.debug = optarg;
			break;
		case 'e':
			opts.oneline = optarg;
			break;
		case 'i':
			opts.interactive = 1;
			break;
		case 'l':
			opts.log = optarg;
			break;
		case 't':
			opts.truststore = optarg;
			break;
		case 'v':
			opts.verbose = 1;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
			break;
		}
	}

	get_pathmax();
	open_debug();
	create_homedir();
	catch_signals();
	open_log();
	if (opts.config == NULL)
		opts.config = get_filepath("config.lua");

	buffer_init(&ibuf, INPUT_BUF);
	buffer_init(&obuf, OUTPUT_BUF);
	buffer_init(&nbuf, NAMESPACE_BUF);
	buffer_init(&cbuf, CONVERSION_BUF);

	regexp_compile(responses);

	SSL_library_init();
	SSL_load_error_strings();
#ifndef OPENSSL_NO_SSL3_METHOD
	ssl3ctx = SSL_CTX_new(SSLv3_client_method());
#endif
	ssl23ctx = SSL_CTX_new(SSLv23_client_method());
	tls1ctx = SSL_CTX_new(TLSv1_client_method());
#if OPENSSL_VERSION_NUMBER >= 0x01000100fL
	tls11ctx = SSL_CTX_new(TLSv1_1_client_method());
	tls12ctx = SSL_CTX_new(TLSv1_2_client_method());
#endif
	if (exists_dir(opts.truststore))
		capath = opts.truststore;
	else if (exists_file(opts.truststore))
		cafile = opts.truststore;
#ifndef OPENSSL_NO_SSL3_METHOD
	SSL_CTX_load_verify_locations(ssl3ctx, cafile, capath);
#endif
	SSL_CTX_load_verify_locations(ssl23ctx, cafile, capath);
	SSL_CTX_load_verify_locations(tls1ctx, cafile, capath);
#if OPENSSL_VERSION_NUMBER >= 0x01000100fL
	SSL_CTX_load_verify_locations(tls11ctx, cafile, capath);
	SSL_CTX_load_verify_locations(tls12ctx, cafile, capath);
#endif

	start_lua();
#if LUA_VERSION_NUM < 502
	{
		list *l;
		session *s;

		l = sessions;
		while (l != NULL) {
			s = l->data;
			l = l->next;

			request_logout(s);
		}
	}
#endif
	stop_lua();

#ifndef OPENSSL_NO_SSL3_METHOD
	SSL_CTX_free(ssl3ctx);
#endif
	SSL_CTX_free(ssl23ctx);
	SSL_CTX_free(tls1ctx);
#if OPENSSL_VERSION_NUMBER >= 0x01000100fL
	SSL_CTX_free(tls11ctx);
	SSL_CTX_free(tls12ctx);
#endif
	ERR_free_strings();

	regexp_free(responses);

	buffer_free(&ibuf);
	buffer_free(&obuf);
	buffer_free(&nbuf);
	buffer_free(&cbuf);

	xfree(env.home);

	close_log();
	close_debug();

	exit(0);
}


/*
 * Print a very brief usage message.
 */
void
usage(void)
{

	fprintf(stderr, "usage: imapfilter [-iVv] [-c configfile] "
	    "[-d debugfile] [-e 'command'] [-l logfile]\n");

	exit(0);
}


/*
 * Print program's version and copyright.
 */
void
version(void)
{

	fprintf(stderr, "IMAPFilter %s  %s\n", VERSION, COPYRIGHT);

	exit(0);
}
