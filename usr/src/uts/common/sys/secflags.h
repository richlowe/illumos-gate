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

#ifndef _SYS_SECFLAGS_H
#define	_SYS_SECFLAGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

typedef struct psecflags {
	uint_t psf_effective;
	uint_t psf_inherit;
} psecflags_t;

/*
 * p_secflags codes
 *
 * These flags indicate the extra security-related features enabled for a
 * given process.
 */
#define	PROC_SEC_ASLR	0x00000001
/* All valid bits */
#define	PROC_SEC_MASK	(PROC_SEC_ASLR)

/* psecflags(2) commands */
typedef enum {
	PSECFLAGS_SET,
	PSECFLAGS_DISABLE,
	PSECFLAGS_ENABLE
} psecflags_cmd_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SECFLAGS_H */
