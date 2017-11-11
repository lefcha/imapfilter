#include <signal.h>

#include "imapfilter.h"


void signal_handler(int sig);
void user_signal_handler(int sig);


/*
 * Catch signals that cause program's termination.
 */
void
catch_signals(void)
{

	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
}


/*
 * Reset signals to default action.
 */
void
release_signals(void)
{

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
}


/*
 * Signal handler for signals that cause program's termination.
 */
void
signal_handler(int sig)
{

	release_signals();

	fatal(ERROR_SIGNAL, "killed by signal %d\n", sig);
}


/*
 * Ignore user-defined signals.
 */
void
ignore_user_signals(void)
{

	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
}


/*
 * Catch user-defined signals.
 */
void
catch_user_signals(void)
{

	signal(SIGUSR1, user_signal_handler);
	signal(SIGUSR2, user_signal_handler);
}


/*
 * Signal handler for user-defined signals.
 */
void
user_signal_handler(int sig)
{

	(void)sig;
	ignore_user_signals();
}
