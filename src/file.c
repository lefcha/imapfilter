#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "imapfilter.h"
#include "pathnames.h"

extern options opts;
extern environment env;

#define CONFIG_TEMPLATE   CONFIG_SHAREDIR "/config.lua"

/*
 * Create imapfilter's home directory.
 * if no config.lua exists, copy the config.lua file CONFIG_SHAREDIR to there, too.
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
        debug("creating directory %s", env.home);
		if (mkdir(env.home, S_IRUSR | S_IWUSR | S_IXUSR))
			error("could not create directory %s (%d: %s)\n", env.home, errno, strerror(errno));
	}

	h = get_filepath("config.lua");
	if (!exists_file(h) && exists_file(CONFIG_TEMPLATE))
    {
        debug("%s not found, initializing from %s\n", h, CONFIG_TEMPLATE);
        copy_file(CONFIG_TEMPLATE, h);
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

int
copy_file( char *from_path, char *to_path )
{
    int result;
    int from_fd, to_fd;
    struct stat from_stat;
    unsigned char * mapped_file;

    result = -1;

    if (stat(from_path, &from_stat) == -1) {
        error("could not get information about file %s (%d: %s)\n", from_path, errno, strerror(errno));
    } else if (!S_ISREG(from_stat.st_mode)) {
        error(" %s is not a normal file (%d: %s)\n", from_path, errno, strerror(errno));
    } else {
        from_fd = open(from_path, O_RDONLY, S_IRUSR | S_IWUSR);
        if (from_fd == -1) {
            error("could not open file %s (%d: %s)\n", from_path, errno, strerror(errno));
        } else {
            to_fd =
                open(to_path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
            if (to_fd == -1) {
                error("could not create file %s (%d: %s)\n", to_path, errno, strerror(errno));
            } else {
                mapped_file = mmap(NULL, from_stat.st_size, PROT_READ, MAP_SHARED, from_fd,0);
                if (mapped_file == NULL) {
                    error("unable to read from %s (%d: %s)\n", from_path, errno, strerror(errno));
                } else {
                    if (write(to_fd, mapped_file, from_stat.st_size) < from_stat.st_size) {
                        error("failed to write to %s (%d: %s)\n", to_path, errno, strerror(errno));
                    } else {
                        result = 0;
                    }
                    munmap(mapped_file, from_stat.st_size);
                }
                close(to_fd);
            }
            close(from_fd);
        }
    }
    return result;
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

void
write_pidfile(void)
{
    FILE *pidfile;

    if (opts.pidfile != NULL) {
        pidfile = fopen(opts.pidfile, "w");
        if (pidfile == NULL) {
            error("unable to write PID to \'%s\' (%d: %s)", errno, strerror(errno));
        }
        else {
            fprintf(pidfile, "%d", getpid());
            fclose(pidfile);
        }
    }
}

void
delete_pidfile(void)
{
    if (opts.pidfile != NULL) {
        if (unlink(opts.pidfile) == -1) {
            error("unable to delete PID file \'%s\' (%d: %s)", errno, strerror(errno));
        }
    }
}
