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


/*
 * p_secflags codes
 *
 * These flags indicate the extra security-related features enabled for a
 * given process.
 */
#define	PROC_SEC_ASLR		0
#define	PROC_SEC_FORBIDNULLMAP	1
#define	PROC_SEC_NOEXECSTACK	2

extern secflagset_t secflag_to_bit(uint_t);
extern boolean_t secflag_isset(secflagset_t, uint_t);
extern void secflag_clear(secflagset_t *, uint_t);
extern void secflag_set(secflagset_t *, uint_t);
extern boolean_t secflag_isempty(secflagset_t);
extern void secflag_zero(secflagset_t *);

/* All valid bits */
#define	PROC_SEC_MASK	(secflag_to_bit(PROC_SEC_ASLR) |	\
    secflag_to_bit(PROC_SEC_FORBIDNULLMAP) |			\
    secflag_to_bit(PROC_SEC_NOEXECSTACK))

#if !defined(_KERNEL)
extern boolean_t secflag_by_name(const char *, secflagset_t *);
extern const char *secflag_to_str(secflagset_t);
extern char *secflags_to_str(secflagset_t);
extern int secflags_parse(secflagset_t, const char *, psecflagdelta_t *);
extern int psecflags(idtype_t, id_t, psecflagdelta_t *);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SECFLAGS_H */
