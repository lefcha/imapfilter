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
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
extern SSL_CTX *sslctx;
#else
extern SSL_CTX *ssl23ctx;
#ifndef OPENSSL_NO_SSL3_METHOD
extern SSL_CTX *ssl3ctx;
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
extern SSL_CTX *tls1ctx;
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
extern SSL_CTX *tls11ctx;
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
extern SSL_CTX *tls12ctx;
#endif
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
	opts.dryrun = 0;
	opts.log = NULL;
	opts.config = NULL;
	opts.oneline = NULL;
	opts.debug = NULL;

	opts.truststore = NULL;
	if (exists_dir(CONFIG_SSL_CAPATH))
		capath = CONFIG_SSL_CAPATH;
	if (exists_file(CONFIG_SSL_CAFILE))
		cafile = CONFIG_SSL_CAFILE;

	env.home = NULL;
	env.pathmax = -1;

	while ((c = getopt(argc, argv, "Vc:d:e:il:nt:v?")) != -1) {
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
		case 'n':
			opts.dryrun = 1;
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
	ignore_user_signals();
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
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
	sslctx = SSL_CTX_new(TLS_method());
#else
	ssl23ctx = SSL_CTX_new(SSLv23_client_method());
#ifndef OPENSSL_NO_SSL3_METHOD
	ssl3ctx = SSL_CTX_new(SSLv3_client_method());
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
	tls1ctx = SSL_CTX_new(TLSv1_client_method());
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
	tls11ctx = SSL_CTX_new(TLSv1_1_client_method());
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
	tls12ctx = SSL_CTX_new(TLSv1_2_client_method());
#endif
#endif
	if (exists_dir(opts.truststore)) {
		capath = opts.truststore;
		cafile = NULL;
	} else if (exists_file(opts.truststore)) {
		cafile = opts.truststore;
		capath = NULL;
	}
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
	if (sslctx)
		SSL_CTX_load_verify_locations(sslctx, cafile, capath);
#else
	if (ssl23ctx)
		SSL_CTX_load_verify_locations(ssl23ctx, cafile, capath);
#ifndef OPENSSL_NO_SSL3_METHOD
	if (ssl3ctx)
		SSL_CTX_load_verify_locations(ssl3ctx, cafile, capath);
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
	if (tls1ctx)
		SSL_CTX_load_verify_locations(tls1ctx, cafile, capath);
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
	if (tls11ctx)
		SSL_CTX_load_verify_locations(tls11ctx, cafile, capath);
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
	if (tls12ctx)
		SSL_CTX_load_verify_locations(tls12ctx, cafile, capath);
#endif
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

#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
	if (sslctx)
		SSL_CTX_free(sslctx);
#else
	if (ssl23ctx)
		SSL_CTX_free(ssl23ctx);
#ifndef OPENSSL_NO_SSL3_METHOD
	if (ssl3ctx)
		SSL_CTX_free(ssl3ctx);
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
	if (tls1ctx)
		SSL_CTX_free(tls1ctx);
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
	if (tls11ctx)
		SSL_CTX_free(tls11ctx);
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
	if (tls12ctx)
		SSL_CTX_free(tls12ctx);
#endif
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

	fprintf(stderr, "usage: imapfilter [-inVv] [-c configfile] "
	    "[-d debugfile] [-e 'command']\n"
	    "\t\t  [-l logfile] [-t truststore]\n");

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
