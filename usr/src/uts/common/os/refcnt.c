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

/*
 * Generic, debuggable, reference counts
 */

#include <sys/atomic.h>
#include <sys/avl.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/inttypes.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/thread.h>
#include <sys/time.h>
#include <sys/types.h>

#include <sys/refcnt_impl.h>

/* XXX: Condition this on DEBUG in future */
uint32_t refcnt_debug = REFCNT_AUDIT | REFCNT_LOGGING;

static int
refcnt_compare(const void *first, const void *second)
{
	uintptr_t a = (uintptr_t)first;
	uintptr_t b = (uintptr_t)second;

	if (a < b)
		return (-1);
	else
		return (a > b);
}

static reftoken_audit_t *
refcnt_audit_new(void)
{
	reftoken_audit_t *ra;

	ra = kmem_zalloc(sizeof (reftoken_audit_t), KM_SLEEP);
	ra->ra_thread = curthread;
	ra->ra_hrtime = gethrtime();
	ra->ra_depth = getpcstack(ra->ra_stack,
	    REFCNT_STACK_DEPTH);

	return (ra);
}

static void
refcnt_histent_create(refcnt_t *rc, reftoken_t *rt, reftoken_audit_t *ra,
	refcnt_event_t re)
{
	refcnt_histent_t *rh;

	if (rc->rc_hist_next >= REFCNT_HISTORY_SIZE)
		rc->rc_hist_next = 0;

	rh = &rc->rc_history[rc->rc_hist_next++];


	/*
	 * If the slot is already used, and is a release, we know we must be
	 * the last reference to it.
	 *
	 * If it is held, we need to keep it until it _is_ released.
	 */
	if ((rh->rh_token != NULL) && (rh->rh_event == RCE_RELE)) {
		kmem_free(rh->rh_audit, sizeof (reftoken_audit_t));
		kmem_free(rh->rh_token, sizeof (reftoken_t));
	}

	rh->rh_event = re;
	rh->rh_token = rt;
	rh->rh_audit = ra;
}

refcnt_t *
refcnt_init(kmutex_t *mtx)
{
	refcnt_t *rc;

	rc = kmem_zalloc(sizeof (refcnt_t), KM_SLEEP);

	rc->rc_count = 0;
	rc->rc_flags = refcnt_debug;
	rc->rc_lock = mtx;

	if (rc->rc_flags & REFCNT_AUDIT) {
		avl_create(&rc->rc_holders,
		    refcnt_compare,
		    sizeof (reftoken_t),
		    offsetof(reftoken_t, rt_node));
	}

	if (rc->rc_flags & REFCNT_LOGGING)
		rc->rc_history = kmem_zalloc(sizeof (refcnt_histent_t) *
		    REFCNT_HISTORY_SIZE, KM_SLEEP);

	return (rc);
}

void
refcnt_destroy(refcnt_t *rc)
{
	if (rc->rc_count != 0)
		cmn_err(CE_PANIC, "destroying refcount with "
		    "outstanding references: %p (%" PRIu64 " refs)", rc,
		    rc->rc_count);

	if (rc->rc_flags & REFCNT_AUDIT)
		avl_destroy(&rc->rc_holders);

	/*
	 * We know there are no outstanding references to tokens except from
	 * the log, because if there were, we'd have panicked above.
	 */
	if (rc->rc_flags & REFCNT_LOGGING) {
		for (int i = 0; i < REFCNT_HISTORY_SIZE; i++) {
			kmem_free(rc->rc_history[i].rh_audit,
			    sizeof (reftoken_audit_t));
			kmem_free(rc->rc_history[i].rh_token,
			    sizeof (reftoken_t));
		}
		kmem_free(rc->rc_history, sizeof (refcnt_histent_t) *
		    REFCNT_HISTORY_SIZE);
	}

	kmem_free(rc, sizeof (refcnt_t));
}

reftoken_t *
refcnt_hold_locked(refcnt_t *rc)
{
	reftoken_t *rt;

	VERIFY(MUTEX_HELD(rc->rc_lock));

	atomic_inc_64(&rc->rc_count);

	if (rc->rc_flags != 0) {
		rt = kmem_zalloc(sizeof (reftoken_t), KM_SLEEP);
		rt->rt_parent = rc;

		if ((rc->rc_flags & REFCNT_AUDIT) ||
		    (rc->rc_flags & REFCNT_LOGGING)) {
			rt->rt_audit = refcnt_audit_new();

			if (rc->rc_flags & REFCNT_AUDIT)
				avl_add(&rc->rc_holders, rt);

			if (rc->rc_flags & REFCNT_LOGGING)
				refcnt_histent_create(rc, rt, rt->rt_audit,
					RCE_HOLD);
		}
	} else {
		rt = (reftoken_t *)-1;
	}

	VERIFY(rc->rc_count != 0);
	return (rt);

}

refcnt_state_t
refcnt_rele(refcnt_t *rc, reftoken_t *rt)
{
	if (rc->rc_flags & REFCNT_AUDIT) {
		avl_index_t in;

		if (avl_find(&rc->rc_holders, rt, &in) == NULL) {
			cmn_err(CE_PANIC, "releasing unheld refcount token, "
			    "cnt: %p token: %p", rc, rt);
		}

		avl_remove(&rc->rc_holders, rt);

		/* If logging, we'll need the audit info for the log still */
		if (!(rc->rc_flags & REFCNT_LOGGING))
			kmem_free(rt->rt_audit, sizeof (reftoken_audit_t));
	}

	if (rc->rc_flags & REFCNT_LOGGING)
		refcnt_histent_create(rc, rt, refcnt_audit_new(),
			RCE_RELE);
	else
		kmem_free(rt, sizeof (reftoken_t));

	/*
	 * We intentionally do this here, so that users of the debugging flags
	 * get the extra help
	 */
	VERIFY(rc->rc_count > 0);

	if (atomic_dec_64_nv(&rc->rc_count) == 0)
		return (RS_FREEABLE);

	return (RS_HELD);
}

boolean_t
refcnt_isheld(refcnt_t *rc)
{
	return (rc->rc_count != 0);
}

uint64_t
refcnt_count(refcnt_t *rc)
{
	return (rc->rc_count);
}
