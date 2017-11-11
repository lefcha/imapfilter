#include <signal.h>

#include "imapfilter.h"


void signal_handler(int sig);


/*
 * Catch signals that cause program's termination.
 */
void
catch_signals(void)
{

	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);
}


/*
 * Release signals and reset them to default action.
 */
void
release_signals(void)
{

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGUSR1, SIG_DFL);
	signal(SIGUSR2, SIG_DFL);
}


/*
 * Signal handler for signals that cause termination of program.
 */
void
signal_handler(int sig)
{

	if (sig == SIGUSR1 || sig == SIGUSR2)
		return;

	release_signals();

	fatal(ERROR_SIGNAL, "killed by signal %d\n", sig);
}
