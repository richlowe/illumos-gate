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

#include <sys/ddi.h>
#include <sys/errno.h>
#include <sys/policy.h>
#include <sys/proc.h>
#include <sys/procset.h>
#include <sys/systm.h>
#include <sys/types.h>

struct psecargs {
	psecflags_cmd_t cmd;
	uint_t val;
};

static int
psecdo(proc_t *p, struct psecargs *args)
{
	mutex_enter(&p->p_lock);

	if (secpolicy_psecflags(CRED(), p, curproc) != 0) {
		mutex_exit(&p->p_lock);
		return (EPERM);
	}

	switch (args->cmd) {
	case PSECFLAGS_SET:
		secflag_set(p, args->val);
		break;
	case PSECFLAGS_DISABLE:
		secflag_disable(p, args->val);
		break;
	case PSECFLAGS_ENABLE:
		secflag_enable(p, args->val);
		break;
	}
	mutex_exit(&p->p_lock);

	return (0);
}

int
psecflags(procset_t *psp, psecflags_cmd_t cmd, uint_t arg)
{
	procset_t procset;
	struct psecargs args = { 0 };
	int rv = 0;

	/*
	 * We don't check the validity in 'arg' for the sake of newer
	 * software on older systems not just falling down dead.
	 *
	 * XXX: I'm not particularly confident that's not dumb.
	 */

	if (copyin(psp, &procset, sizeof (procset)) != 0)
		return (set_errno(EFAULT));

	/* secflags are per-process, procset must be in terms of processes */
	if ((procset.p_lidtype == P_LWPID) ||
	    (procset.p_ridtype == P_LWPID))
		return (set_errno(EINVAL));

	switch (cmd) {
	case PSECFLAGS_SET:
	case PSECFLAGS_DISABLE:
	case PSECFLAGS_ENABLE:
		args.cmd = cmd;
		args.val = arg;
		rv = dotoprocs(&procset, psecdo,
		    (caddr_t)&args);
		break;
	default:
		return (set_errno(EINVAL));
	}

	return (rv ? set_errno(rv) : 0);
}
