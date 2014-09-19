/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 */

/*
 * This program tests that wcsrtombs and friends work properly.
 * In order for it to work, it requires that some additional locales
 * be installed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <note.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include "test_common.h"

int extra_debug = 0;

struct utsname un;

void
forkit(char *msg, const char *expect, void (*postfn)(void))
{
	int fd;
	FILE *f;
	pid_t pid;
	char *ptr = NULL;
	size_t cap = 0;
	int wstat;
	int rv;
	test_t t;
	char fname[32];

	(void) strcpy(fname, "/tmp/testXXXXXX");
	t = test_start(msg);

	fd = mkstemp(fname);
	if (fd < 0) {
		test_failed(t, "mkstemp failed: %s", strerror(errno));
		return;
	}

	/* don't leave it in the filesystem */
	(void) unlink(fname);

	pid = vfork();
	switch (pid) {
	case -1:
		test_failed(t, "vfork failed: %s", strerror(errno));
		return;
	case 0:
		if (dup2(fd, 1) < 0) {
			test_failed(t, "dup2 failed: %s", strerror(errno));
			exit(9);
		}
		postfn();
		exit(0);
	default:
		break;
	}

	/* parent */
	f = fdopen(fd, "r");
	if (f == NULL) {
		(void) close(fd);
		test_failed(t, "fdopen failed: %s", strerror(errno));
		(void) wait(NULL);
		return;
	}
	if (waitpid(pid, &wstat, NULL) < 0) {
		test_failed(t, "wait failed: %s", strerror(errno));
		(void) fclose(f);
		return;
	}
	if (!WIFEXITED(wstat) || WEXITSTATUS(wstat) != 0) {
		test_failed(t, "child failed: %#x", wstat);
		(void) fclose(f);
		return;
	}
	(void) lseek(fd, 0, SEEK_SET);
	if ((rv = getline(&ptr, &cap, f)) < 1) {
		test_failed(t, "child gave no data: %d", rv);
		(void) fclose(f);
		return;
	}
	(void) fclose(f);

	if (strncmp(ptr, expect, strlen(expect)) != 0) {
		test_failed(t, "%s != %s", ptr, expect);
		return;
	}

	(void) free(ptr);
	test_passed(t);
}

void
case_badf(void)
{
	int fd = -1;
	int rv;
	char *args[] = { "uname", NULL };
	char *env[] = { NULL };

	rv = fexecve(fd, args, env);
	if (rv != -1) {
		(void) printf("rv is not -1\n");
		(void) exit(0);
	}
	if (errno != EBADF) {
		(void) printf("err %d(%s) != EBADF\n", errno, strerror(errno));
		(void) exit(0);
	}
	(void) printf("GOOD\n");
	(void) exit(0);
}

void
case_notexec(void)
{
	int fd;
	int rv;
	char *args[] = { "uname", NULL };
	char *env[] = { NULL };

	fd = open("/usr/bin/uname", O_RDONLY);

	rv = fexecve(fd, args, env);
	if (rv != -1) {
		(void) printf("rv is not -1\n");
		(void) exit(0);
	}
	if (errno != EBADF) {
		(void) printf("err %d(%s) != EBADF\n", errno, strerror(errno));
		(void) exit(0);
	}
	(void) printf("GOOD\n");
	(void) exit(0);
}

void
case_uname(void)
{
	int fd;
	char *args[] = { "uname", NULL };
	char *env[] = { NULL };

	fd = open("/usr/bin/uname", O_EXEC);
	if (fd < 0) {
		(void) printf("failed to open /usr/bin/uname: %s",
		    strerror(errno));
		(void) exit(0);
	}

	(void) fexecve(fd, args, env);
	(void) printf("EXEC FAILED: %s\n", strerror(errno));
	(void) exit(0);
}

void
case_uname_r(void)
{
	int fd;
	char *args[] = { "uname", "-r", NULL };
	char *env[] = { NULL };

	fd = open("/usr/bin/uname", O_EXEC);
	if (fd < 0) {
		(void) printf("failed to open /usr/bin/uname: %s",
		    strerror(errno));
		(void) exit(0);
	}

	(void) fexecve(fd, args, env);
	(void) printf("EXEC FAILED: %s\n", strerror(errno));
	(void) exit(0);
}

void
test_fexecve_badf(void)
{
	forkit("fexecve (bad FD)", "GOOD\n", case_badf);
}

void
test_fexecve_notexec(void)
{
	forkit("fexecve (not O_EXEC)", "GOOD\n", case_notexec);
}

void
test_fexecve_uname(void)
{
	forkit("fexecve (uname)", un.sysname, case_uname);
}

void
test_fexecve_uname_r(void)
{
	forkit("fexecve (uname)", un.release, case_uname_r);
}

int
main(int argc, char **argv)
{
	int optc;

	(void) uname(&un);

	while ((optc = getopt(argc, argv, "dfD")) != EOF) {
		switch (optc) {
		case 'd':
			test_set_debug();
			break;
		case 'f':
			test_set_force();
			break;
		case 'D':
			test_set_debug();
			extra_debug++;
			break;
		default:
			(void) fprintf(stderr, "Usage: %s [-dfD]\n", argv[0]);
			exit(1);
		}
	}

	test_fexecve_badf();
	test_fexecve_notexec();
	test_fexecve_uname();
	test_fexecve_uname_r();

	exit(0);
}
