#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "imapfilter.h"
#include "pathnames.h"


extern options opts;
extern environment env;


/*
 * Create imapfilter's home directory.
 */
void
create_homedir(void)
{
	int n;
	char *h, *i;

	h = getenv("HOME");
	i = getenv("IMAPFILTER_HOME");

	if (i == NULL)
		n = strlen(h ? h : "") + strlen(h ? "/" : "") +
		    strlen(".imapfilter");
	else
		n = strlen(i);

	if (env.pathmax != -1 && n > env.pathmax)
		fatal(ERROR_PATHNAME,
		    "pathname limit %ld exceeded: %d\n", env.pathmax, n);

	env.home = (char *)xmalloc((n + 1) * sizeof(char));
	if (i == NULL)
		snprintf(env.home, n + 1, "%s%s%s", h ? h : "", h ? "/" : "",
		    ".imapfilter");
	else
		snprintf(env.home, n + 1, "%s", i);

	if (!exists_dir(env.home)) {
		if (mkdir(env.home, S_IRUSR | S_IWUSR | S_IXUSR))
			error("could not create directory %s; %s\n", env.home,
			    strerror(errno));
	}
}


/*
 * Check if a file exists.
 */
int
exists_file(char *fname)
{
	struct stat fs;

	if (access(fname, F_OK))
		return 0;

	stat(fname, &fs);
	if (!S_ISREG(fs.st_mode)) {
		return 0;
	}

	return 1;
}


/*
 * Check if a directory exists.
 */
int
exists_dir(char *dname)
{
	struct stat ds;

	if (access(dname, F_OK))
		return 0;

	stat(dname, &ds);
	if (!S_ISDIR(ds.st_mode)) {
		return 0;
	}

	return 1;
}


/*
 * Create a file with the specified permissions.
 */
int
create_file(char *fname, mode_t mode)
{
	int fd;

	fd = 0;

	if (!exists_file(fname)) {
		fd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, mode);
		if (fd == -1) {
			error("could not create file %s; %s\n", fname,
			    strerror(errno));
			return -1;
		}
		close(fd);
	}

	return 0;
}


/*
 * Get the system's maximum number of bytes in a pathname.
 */
int
get_pathmax(void)
{
	int n;

	errno = 0;

	n = pathconf("/", _PC_PATH_MAX);

	if (n == -1 && errno != 0) {
		error("getting PATH_MAX limit; %s\n", strerror(errno));
		return -1;
	}
	env.pathmax = n;

	return 0;
}


/*
 * Get the path of a file inside the configuration directory.
 */
char *
get_filepath(char *fname)
{
	int n;
	char *fp;

	n = strlen(env.home) + strlen("/") + strlen(fname);
	if (env.pathmax != -1 && n > env.pathmax)
		fatal(ERROR_PATHNAME,
		    "pathname limit %ld exceeded: %d\n", env.pathmax, n);

	fp = (char *)xmalloc((n + 1) * sizeof(char));
	snprintf(fp, n + 1, "%s/%s", env.home, fname);

	return fp;
}


/*
 * Write PID to user specified file.
 */
void
write_pidfile(void)
{
	FILE *fp;

	if (opts.pidfile == NULL)
		return;

	if ((fp = fopen(opts.pidfile, "w")) == NULL) {
	    error("could not write PID to file %s; %s\n", opts.pidfile, strerror(errno));
	    return;
	}

	fprintf(fp, "%d\n", getpid());
	fclose(fp);
}


/*
 * Delete user specified file, which holds the PID written earlier.
 */
void
remove_pidfile(void)
{

	if (opts.pidfile == NULL)
		return;

	if (unlink(opts.pidfile) == -1)
		error("could not delete PID file %s; %s\n", opts.pidfile, strerror(errno));
}
