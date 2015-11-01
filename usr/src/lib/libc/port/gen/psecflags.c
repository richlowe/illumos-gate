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

/* Copyright 2015, Richard Lowe. */

#include "lint.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/proc.h>
#include <sys/procset.h>
#include <sys/syscall.h>
#include <sys/secflags.h>

extern int __psecflagsset(procset_t *, psecflagdelta_t *);

int
psecflags(idtype_t idtype, id_t id, psecflagdelta_t *delta)
{
	procset_t procset;

	setprocset(&procset, POP_AND, idtype, id, P_ALL, 0);

	return (__psecflagsset(&procset, delta));
}

static struct flagdesc {
	secflagset_t value;
	const char *name;
} flagdescs[] = {
	{ PROC_SEC_ASLR,		"aslr" },
	{ PROC_SEC_FORBIDNULLMAP,	"forbidnullmap" },
	{ PROC_SEC_NOEXECSTACK,		"noexecstack" },
	{ 0x0, NULL }
};

/* XXX: secflagset_t as an int64 so this can fail still? */
int
secflag_by_name(const char *str)
{
	struct flagdesc *fd;

	for (fd = flagdescs; fd->name != NULL; fd++) {
		if (strcasecmp(str, fd->name) == 0)
			return (fd->value);
	}

	return (-1);
}

const char *
secflag_to_str(secflagset_t sf)
{
	struct flagdesc *fd;

	for (fd = flagdescs; fd->name != NULL; fd++) {
		if (sf == fd->value)
			return (fd->name);
	}

	return (NULL);
}

char *
secflags_to_str(secflagset_t flags)
{
	struct flagdesc *fd;
	char *buf;
	size_t buflen = 13;	/* %08#x + comma + space + NUL */

	if (flags == 0)
		return (strdup("none"));

	for (fd = flagdescs; fd->name != NULL; fd++)
		if (flags & secflag_to_bit(fd->value))
			buflen += strlen(fd->name) + 2; /* comma, space */

	if ((buf = calloc(buflen, sizeof (char))) == NULL)
		return (NULL);	/* errno set for us */

	for (fd = flagdescs; fd->name != NULL; fd++) {
		if (secflag_isset(flags, fd->value)) {
			if (buf[0] != '\0')
				(void) strlcat(buf, ", ", buflen);
			(void) strlcat(buf, fd->name, buflen);
		}

		secflag_clear(&flags, fd->value);
	}

	if (flags != 0) { 	/* unknown flags */
		char hexbuf[11]; /* %#08x */

		(void) snprintf(hexbuf, sizeof (hexbuf), "%#08x", flags);
		if (buf[0] != '\0')
			(void) strlcat(buf, ", ", buflen);
		(void) strlcat(buf, hexbuf, buflen);
	}

	return (buf);
}

int
secflags_parse(secflagset_t defaults, const char *flags, psecflagdelta_t *ret)
{
	char *flag;
	char *s, *ss;
	boolean_t current = B_FALSE;

	/* Guarantee a clean base */
	bzero(ret, sizeof (*ret));

	if ((ss = s = strdup(flags)) == NULL)
		return (-1);	/* errno set for us */


	while ((flag = strsep(&s, ",")) != NULL) {
		int sf = 0;
		boolean_t del = B_FALSE;

		if (strcasecmp(flag, "default") == 0) {
			ret->psd_add |= defaults;
			continue;
		} else if (strcasecmp(flag, "all") == 0) {
			ret->psd_add = PROC_SEC_MASK;
			continue;
		} else if (strcasecmp(flag, "none") == 0) {
			ret->psd_rem = PROC_SEC_MASK;
			continue;
		} else if (strcasecmp(flag, "current") == 0) {
			current = B_TRUE;
			continue;
		}

		if ((flag[0] == '-') || (flag[0] == '!')) {
			flag++;
			del = B_TRUE;
		} else if (flag[0] == '+') {
			flag++;
		}

		if ((sf = secflag_by_name(flag)) == -1) {
			errno = EINVAL;
			free(ss);
			return (-1);
		}

		if (del)
			secflag_set(&(ret->psd_rem), sf);
		else
			secflag_set(&(ret->psd_add), sf);
	}

	/*
	 * If we're not using the current flags, this is strict assignment.
	 * Negatives "win".
	 */
	if (!current) {
		ret->psd_assign = ret->psd_add;
		ret->psd_assign &= ~ret->psd_rem;
		ret->psd_ass_active = B_TRUE;
		secflag_zero(&(ret->psd_add));
		secflag_zero(&(ret->psd_rem));
	}

	free(ss);
	return (0);
}
