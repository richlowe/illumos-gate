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

#ifndef _SYS_KDI_SVC_H
#define	_SYS_KDI_SVC_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	NKDISVCS	1

typedef struct kdi_svc {
	int64_t	(*ks_call)();	/* the actual call */
} kdi_svc_t;

extern kdi_svc_t kdi_svcs[];

#define	KDISVC_KMDB_TRAP		0x0 /* Enter kmdb(8) */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_KDI_SVC_H */
