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
 * Copyright 2025 <contributor>
 */

#ifndef _HTABLE_H
#define	_HTABLE_H

/*
 * Describe the purpose of the file here.
 */

#ifdef __cplusplus
extern "C" {
#endif

extern int htables_dcmd(uintptr_t addr, uint_t flags, int argc,
	const mdb_arg_t *argv);
extern void htables_help(void);

#ifdef __cplusplus
}
#endif

#endif /* _HTABLE_H */
