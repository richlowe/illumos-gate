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

/* Copyright 2023 Richard Lowe */

/*
 * KDI SVC software traps
 */

#include <sys/cmn_err.h>
#include <sys/kdi_svc.h>
#include <sys/trap.h>

extern int64_t kmdb_trap(void);

kdi_svc_t kdi_svcs[] = {
	[KDISVC_KMDB_TRAP] = { .ks_call = kmdb_trap },
};
