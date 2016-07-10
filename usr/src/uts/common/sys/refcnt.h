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
 * Copyright 2016, Richard Lowe.
 */

#ifndef _SYS_REFCNT_H
#define	_SYS_REFCNT_H

#include <sys/mutex.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct refcnt refcnt_t;
typedef struct reftoken reftoken_t;

typedef enum {
	RS_HELD = 0,
	RS_FREEABLE
} refcnt_state_t;

extern refcnt_t *refcnt_init(kmutex_t *);
extern void refcnt_destroy(refcnt_t *);
extern reftoken_t *refcnt_hold_locked(refcnt_t *);
extern refcnt_state_t refcnt_rele(refcnt_t *, reftoken_t *);
extern boolean_t refcnt_isheld(refcnt_t *);
extern uint64_t refcnt_count(refcnt_t *);
#ifdef __cplusplus
}
#endif

#endif /* _SYS_REFCNT_H */
