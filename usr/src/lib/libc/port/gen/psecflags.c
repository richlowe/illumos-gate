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
 * Copyright 2014 <contributor>.  All rights reserved.
 */

#include <sys/proc.h>
#include <sys/procset.h>
#include <sys/syscall.h>

extern int __psecflagsset(procset_t *, int, uint_t);

int
psecflags(idtype_t idtype, id_t id, psecflags_cmd_t cmd, uint_t arg)
{
	procset_t procset;

	setprocset(&procset, POP_AND, idtype, id, P_ALL, 0);

	return (__psecflagsset(&procset, cmd, arg));
}
