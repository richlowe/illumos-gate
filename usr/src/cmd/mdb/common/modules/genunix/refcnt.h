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
 * Copyright 2016, Richard Lowe
 */

#ifndef _REFCNT_H
#define	_REFCNT_H

#ifdef __cplusplus
extern "C" {
#endif

extern int reftoken_walk_init(mdb_walk_state_t *);

extern int reftoken(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int refcount(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int reflog(uintptr_t, uint_t, int, const mdb_arg_t *);

#ifdef __cplusplus
}
#endif

#endif /* _REFCNT_H */
