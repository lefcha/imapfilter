#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "imapfilter.h"
#include "pathnames.h"


extern environment env;


/*
 * Create imapfilter's home directory.
 */
int
create_homedir(void)
{
	int n;
	char b;
	char *hd;

	n = snprintf(&b, 1, "%s/%s", env.home, PATHNAME_HOME);

	if (env.pathmax != -1 && n > env.pathmax)
		fatal(ERROR_PATHNAME,
		    "pathname limit %ld exceeded: %d\n", env.pathmax, n);

	hd = (char *)xmalloc((n + 1) * sizeof(char));
	snprintf(hd, n + 1, "%s/%s", env.home, PATHNAME_HOME);

	if (!exists_dir(hd)) {
		if (mkdir(hd, S_IRUSR | S_IWUSR | S_IXUSR))
			error("could not create directory %s; %s\n", hd,
			    strerror(errno));
	}
	xfree(hd);

	return 0;
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
		error("file %s not a regular file\n", fname);
		return -1;
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
		error("file %s not a directory\n", dname);
		return -1;
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
