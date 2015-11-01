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

#include <sys/ddi.h>
#include <sys/errno.h>
#include <sys/policy.h>
#include <sys/proc.h>
#include <sys/procset.h>
#include <sys/systm.h>
#include <sys/types.h>

static int
psecdo(proc_t *p, psecflagdelta_t *args)
{
	mutex_enter(&p->p_lock);

	if (secpolicy_psecflags(CRED(), p, curproc) != 0) {
		mutex_exit(&p->p_lock);
		return (EPERM);
	}

	if (args->psd_ass_active) {
		p->p_secflags.psf_inherit = args->psd_assign;
	} else {
		if (!secflag_isempty(args->psd_add)) {
			p->p_secflags.psf_inherit |= args->psd_add;
		}
		if (!secflag_isempty(args->psd_rem)) {
			p->p_secflags.psf_inherit &= ~args->psd_rem;
		}
	}

	mutex_exit(&p->p_lock);

	return (0);
}

int
psecflags(procset_t *psp, psecflagdelta_t *ap)
{
	procset_t procset;
	psecflagdelta_t args;
	int rv = 0;

	/*
	 * We don't check the validity of the bits for the sake of newer
	 * software on older systems not just falling down dead.
	 *
	 * XXX: I'm not particularly confident that's not dumb.
	 */

	if (copyin(psp, &procset, sizeof (procset)) != 0)
		return (set_errno(EFAULT));

	if (copyin(ap, &args, sizeof (psecflagdelta_t)) != 0)
		return (set_errno(EFAULT));

	/* secflags are per-process, procset must be in terms of processes */
	if ((procset.p_lidtype == P_LWPID) ||
	    (procset.p_ridtype == P_LWPID))
		return (set_errno(EINVAL));

	rv = dotoprocs(&procset, psecdo, (caddr_t)&args);

	return (rv ? set_errno(rv) : 0);
}
