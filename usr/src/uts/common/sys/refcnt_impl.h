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

#ifndef _SYS_REFCNT_IMPL_H
#define	_SYS_REFCNT_IMPL_H

#include <sys/avl.h>
#include <sys/refcnt.h>
#include <sys/thread.h>
#include <sys/systm.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	REFCNT_STACK_DEPTH	15
#define	REFCNT_HISTORY_SIZE	128

/* Debugging flags */
#define	REFCNT_AUDIT	0x1
#define	REFCNT_LOGGING	0x2

typedef enum {
	RCE_HOLD,
	RCE_RELE
} refcnt_event_t;

typedef struct {
	kthread_t *ra_thread;

	/*
	 * XXX: hires isn't proving useful, maybe just ticks?
	 * maybe scaled down to something more useful?
	 */
	hrtime_t ra_hrtime;
	pc_t	ra_stack[REFCNT_STACK_DEPTH];
	int	ra_depth;
} reftoken_audit_t;

typedef struct {
	refcnt_event_t rh_event;
	reftoken_t *rh_token;
	reftoken_audit_t *rh_audit;
} refcnt_histent_t;

struct refcnt {
	uint64_t rc_count;
	uint32_t rc_flags;
	kmutex_t *rc_lock;
	avl_tree_t rc_holders;
	int rc_hist_next;
	refcnt_histent_t *rc_history;
};

struct reftoken {
	avl_node_t rt_node;	/* link in rc_holders */
	refcnt_t *rt_parent;	/* refcnt we were taken from */
	reftoken_audit_t *rt_audit;
};

#ifdef __cplusplus
}
#endif

#endif /* _SYS_REFCNT_IMPL_H */
