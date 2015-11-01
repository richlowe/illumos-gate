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

/* Copyright 2014, Richard Lowe */

#ifndef _SYS_SECFLAGS_IMPL_H
#define	_SYS_SECFLAGS_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/procset.h>

typedef uint32_t secflagset_t;

typedef struct psecflags {
	secflagset_t psf_effective;
	secflagset_t psf_inherit;
} psecflags_t;

typedef struct psecflagdelta {
	secflagset_t psd_add;		/* Flags to add */
	secflagset_t psd_rem;		/* Flags to remove */
	secflagset_t psd_assign;	/* Flags to assign */
	boolean_t psd_ass_active;	/* Need to assign */
} psecflagdelta_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SECFLAGS_IMPL_H */
