#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "imapfilter.h"
#include "session.h"


#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
SSL_CTX *sslctx = NULL;
#else
SSL_CTX *ssl23ctx = NULL;
#ifndef OPENSSL_NO_SSL3_METHOD
SSL_CTX *ssl3ctx = NULL;
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
SSL_CTX *tls1ctx = NULL;
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
SSL_CTX *tls11ctx = NULL;
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
SSL_CTX *tls12ctx = NULL;
#endif
#endif


/*
 * Connect to mail server.
 */
int
open_connection(session *ssn)
{
	struct addrinfo hints, *res, *ressave;
	int n, sockfd;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	n = getaddrinfo(ssn->server, ssn->port, &hints, &res);

	if (n < 0) {
		error("gettaddrinfo; %s\n", gai_strerror(n));
		return -1;
	}

	ressave = res;

	sockfd = -1;

	while (res) {
		sockfd = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);

		if (sockfd >= 0) {
			if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
				break;

			sockfd = -1;
		}
		res = res->ai_next;
	}

	if (ressave)
		freeaddrinfo(ressave);

	if (sockfd == -1) {
		error("error while initiating connection to %s at port %s\n",
		    ssn->server, ssn->port);
		return -1;
	}

	ssn->socket = sockfd;

	if (ssn->sslproto) {
		if (open_secure_connection(ssn) == -1) {
			close_connection(ssn);
			return -1;
		}
	}

	return ssn->socket;
}


/*
 * Initialize SSL/TLS connection.
 */
int
open_secure_connection(session *ssn)
{
	int r, e;
	SSL_CTX *ctx = NULL;

#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
	if (sslctx)
		ctx = sslctx;
#else
	if (ssl23ctx)
		ctx = ssl23ctx;

	if (ssn->sslproto) {
#ifndef OPENSSL_NO_SSL3_METHOD
		if (ssl3ctx && !strcasecmp(ssn->sslproto, "ssl3"))
			ctx = ssl3ctx;
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
		if (tls1ctx && !strcasecmp(ssn->sslproto, "tls1"))
			ctx = tls1ctx;
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
		if (tls11ctx && !strcasecmp(ssn->sslproto, "tls1.1"))
			ctx = tls11ctx;
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
		if (tls12ctx && !strcasecmp(ssn->sslproto, "tls1.2"))
			ctx = tls12ctx;
#endif
	}
#endif

	if (ctx == NULL) {
		error("initiating SSL connection to %s; protocol version "
		      "not supported by current build", ssn->server);
		goto fail;
	}

	if (!(ssn->sslconn = SSL_new(ctx)))
		goto fail;

#if OPENSSL_VERSION_NUMBER >= 0x1000000fL
	if (get_option_boolean("certificates")) {
		SSL_set_hostflags(ssn->sslconn,
		    X509_CHECK_FLAG_NO_WILDCARDS);
		if (!SSL_set1_host(ssn->sslconn, ssn->server)) {
			error("failed setting hostname validation to "
			    "%s; %s\n ", ssn->server,
			    ERR_error_string(ERR_get_error(), NULL));
			goto fail;
		}
		SSL_set_verify(ssn->sslconn, SSL_VERIFY_PEER, NULL);
	}

	r = SSL_set_tlsext_host_name(ssn->sslconn, ssn->server);
	if (r == 0) {
		error("failed setting the Server Name Indication (SNI) to "
		    "%s; %s\n", ssn->server,
		    ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
#endif

	SSL_set_fd(ssn->sslconn, ssn->socket);

	for (;;) {
		if ((r = SSL_connect(ssn->sslconn)) > 0)
			break;

		switch (SSL_get_error(ssn->sslconn, r)) {
		case SSL_ERROR_ZERO_RETURN:
			error("initiating SSL connection to %s; the "
			    "connection has been closed cleanly\n",
			    ssn->server);
			goto fail;
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
		case SSL_ERROR_WANT_X509_LOOKUP:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			break;
		case SSL_ERROR_SYSCALL:
			e = ERR_get_error();
			if (e == 0 && r == 0)
				error("initiating SSL connection to %s; EOF in "
				    "violation of the protocol\n", ssn->server);
			else if (e == 0 && r == -1)
				error("initiating SSL connection to %s; %s\n",
				    ssn->server, strerror(errno));
			else
				error("initiating SSL connection to %s; %s\n",
				    ssn->server, ERR_error_string(e, NULL));
			goto fail;
		case SSL_ERROR_SSL:
			error("initiating SSL connection to %s; %s\n",
			    ssn->server, ERR_error_string(ERR_get_error(),
			    NULL));
			goto fail;
		default:
			break;
		}
	}
	if (get_option_boolean("certificates") && get_cert(ssn) == -1)
		goto fail;

	return 0;

fail:
	ssn->sslconn = NULL;

	return -1;
}


/*
 * Disconnect from mail server.
 */
int
close_connection(session *ssn)
{
	int r;

	r = 0;

	close_secure_connection(ssn);

	if (ssn->socket != -1) {
		r = close(ssn->socket);
		ssn->socket = -1;

		if (r == -1)
			error("closing socket; %s\n", strerror(errno));
	}
	return r;
}


/*
 * Shutdown SSL/TLS connection.
 */
int
close_secure_connection(session *ssn)
{

	if (ssn->sslconn) {
		SSL_shutdown(ssn->sslconn);
		SSL_free(ssn->sslconn);
		ssn->sslconn = NULL;
	}

	return 0;
}


/*
 * Read data from socket.
 */
ssize_t
socket_read(session *ssn, char *buf, size_t len, long timeout, int timeoutfail, int *interrupt)
{
	int s;
	ssize_t r;
	fd_set fds;

	struct timeval tv;

	struct timeval *tvp;

	r = 0;
	s = 1;
	tvp = NULL;

	memset(buf, 0, len + 1);

	if (timeout > 0) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		tvp = &tv;
	}

	FD_ZERO(&fds);
	FD_SET(ssn->socket, &fds);

	if (ssn->sslconn) {
		if (SSL_pending(ssn->sslconn) > 0) {
			r = socket_secure_read(ssn, buf, len);
			if (r <= 0)
				goto fail;
		} else {
			if (interrupt != NULL)
				catch_user_signals();
			if ((s = select(ssn->socket + 1, &fds, NULL, NULL, tvp)) > 0) {
				if (interrupt != NULL)
					ignore_user_signals();
				if (FD_ISSET(ssn->socket, &fds)) {
					r = socket_secure_read(ssn, buf, len);
					if (r <= 0)
						goto fail;
				}
			}
		}
	} else {
		if (interrupt != NULL)
			catch_user_signals();
		if ((s = select(ssn->socket + 1, &fds, NULL, NULL, tvp)) > 0) {
			if (interrupt != NULL)
				ignore_user_signals();
			if (FD_ISSET(ssn->socket, &fds)) {
				r = read(ssn->socket, buf, len);
				if (r == -1) {
					error("reading data; %s\n", strerror(errno));
					goto fail;
				} else if (r == 0) {
					goto fail;
				}
			}
		}
	}

	if (s == -1) {
		if (interrupt != NULL && errno == EINTR) {
			*interrupt = 1;
			return -1;
		}
		error("waiting to read from socket; %s\n", strerror(errno));
		goto fail;
	} else if (s == 0 && timeoutfail) {
		error("timeout period expired while waiting to read data\n");
		goto fail;
	}

	return r;
fail:
	close_connection(ssn);

	return -1;

}


/*
 * Read data from a TLS/SSL connection.
 */
ssize_t
socket_secure_read(session *ssn, char *buf, size_t len)
{
	int r, e;

	for (;;) {
		if ((r = (ssize_t) SSL_read(ssn->sslconn, buf, len)) > 0)
			break;

		switch (SSL_get_error(ssn->sslconn, r)) {
		case SSL_ERROR_ZERO_RETURN:
			error("reading data through SSL; the connection has "
			    "been closed cleanly\n");
			goto fail;
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
		case SSL_ERROR_WANT_X509_LOOKUP:
			break;
		case SSL_ERROR_SYSCALL:
			e = ERR_get_error();
			if (e == 0 && r == 0)
				error("reading data through SSL; EOF in "
				    "violation of the protocol\n");
			else if (e == 0 && r == -1)
				error("reading data through SSL; %s\n",
				    strerror(errno));
			else
				error("reading data through SSL; %s\n",
				    ERR_error_string(e, NULL));
			goto fail;
		case SSL_ERROR_SSL:
			error("reading data through SSL; %s\n",
			    ERR_error_string(ERR_get_error(), NULL));
			goto fail;
		default:
			break;
		}
	}

	return r;
fail:
	SSL_set_shutdown(ssn->sslconn, SSL_SENT_SHUTDOWN |
	    SSL_RECEIVED_SHUTDOWN);

	return -1;

}


/*
 * Write data to socket.
 */
ssize_t
socket_write(session *ssn, const char *buf, size_t len)
{
	int s;
	ssize_t r, t;
	fd_set fds;

	r = t = 0;
	s = 1;

	FD_ZERO(&fds);
	FD_SET(ssn->socket, &fds);

	while (len) {
		if ((s = select(ssn->socket + 1, NULL, &fds, NULL, NULL) > 0 &&
		    FD_ISSET(ssn->socket, &fds))) {
			if (ssn->sslconn) {
				r = socket_secure_write(ssn, buf, len);

				if (r <= 0)
					goto fail;
			} else {
				r = write(ssn->socket, buf, len);

				if (r == -1) {
					error("writing data; %s\n",
					    strerror(errno));
					goto fail;
				} else if (r == 0) {
					goto fail;
				}
			}

			if (r > 0) {
				len -= r;
				buf += r;
				t += r;
			}
		}
	}

	if (s == -1) {
		error("waiting to write to socket; %s\n", strerror(errno));
		goto fail;
	} else if (s == 0) {
		error("timeout period expired while waiting to write data\n");
		goto fail;
	}

	return t;
fail:
	close_connection(ssn);

	return -1;
}


/*
 * Write data to a TLS/SSL connection.
 */
ssize_t
socket_secure_write(session *ssn, const char *buf, size_t len)
{
	int r, e;

	for (;;) {
		if ((r = (ssize_t) SSL_write(ssn->sslconn, buf, len)) > 0)
			break;

		switch (SSL_get_error(ssn->sslconn, r)) {
		case SSL_ERROR_ZERO_RETURN:
			error("writing data through SSL; the connection has "
			    "been closed cleanly\n");
			goto fail;
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
		case SSL_ERROR_WANT_X509_LOOKUP:
			break;
		case SSL_ERROR_SYSCALL:
			e = ERR_get_error();
			if (e == 0 && r == 0)
				error("writing data through SSL; EOF in "
				    "violation of the protocol\n");
			else if (e == 0 && r == -1)
				error("writing data through SSL; %s\n",
				    strerror(errno));
			else
				error("writing data through SSL; %s\n",
				    ERR_error_string(e, NULL));
			goto fail;
		case SSL_ERROR_SSL:
			error("writing data through SSL; %s\n",
			    ERR_error_string(ERR_get_error(), NULL));
			goto fail;
		default:
			break;
		}
	}

	return r;
fail:
	SSL_set_shutdown(ssn->sslconn, SSL_SENT_SHUTDOWN |
	    SSL_RECEIVED_SHUTDOWN);

	return -1;
}
