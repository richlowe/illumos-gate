/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2012 by Delphix. All rights reserved.
 */

#include <sys/zfs_context.h>
#include <sys/trackcount.h>

#ifdef	ZFS_DEBUG

#ifdef _KERNEL
int reference_tracking_enable = FALSE; /* runs out of memory too easily */
#else
int reference_tracking_enable = TRUE;
#endif
int reference_history = 3; /* tunable */

static kmem_cache_t *reference_cache;
static kmem_cache_t *reference_history_cache;

void
trackcount_init(void)
{
	reference_cache = kmem_cache_create("reference_cache",
	    sizeof (trackref_t), 0, NULL, NULL, NULL, NULL, NULL, 0);

	reference_history_cache = kmem_cache_create("reference_history_cache",
	    sizeof (uint64_t), 0, NULL, NULL, NULL, NULL, NULL, 0);
}

void
trackcount_fini(void)
{
	kmem_cache_destroy(reference_cache);
	kmem_cache_destroy(reference_history_cache);
}

void
trackcount_create(trackcount_t *rc)
{
	mutex_init(&rc->tc_mtx, NULL, MUTEX_DEFAULT, NULL);
	list_create(&rc->tc_list, sizeof (trackref_t),
	    offsetof(trackref_t, ref_link));
	list_create(&rc->rc_removed, sizeof (trackref_t),
	    offsetof(trackref_t, ref_link));
	rc->tc_count = 0;
	rc->tc_removed_count = 0;
	rc->tc_tracked = reference_tracking_enable;
}

void
trackcount_create_untracked(trackcount_t *rc)
{
	trackcount_create(rc);
	rc->tc_tracked = B_FALSE;
}

void
trackcount_destroy_many(trackcount_t *rc, uint64_t number)
{
	trackref_t *ref;

	ASSERT(rc->tc_count == number);
	while (ref = list_head(&rc->tc_list)) {
		list_remove(&rc->tc_list, ref);
		kmem_cache_free(reference_cache, ref);
	}
	list_destroy(&rc->tc_list);

	while (ref = list_head(&rc->rc_removed)) {
		list_remove(&rc->rc_removed, ref);
		kmem_cache_free(reference_history_cache, ref->ref_removed);
		kmem_cache_free(reference_cache, ref);
	}
	list_destroy(&rc->rc_removed);
	mutex_destroy(&rc->tc_mtx);
}

void
trackcount_destroy(trackcount_t *rc)
{
	trackcount_destroy_many(rc, 0);
}

int
trackcount_is_zero(trackcount_t *rc)
{
	return (rc->tc_count == 0);
}

int64_t
trackcount_count(trackcount_t *rc)
{
	return (rc->tc_count);
}

int64_t
trackcount_add_many(trackcount_t *rc, uint64_t number, void *holder)
{
	trackref_t *ref = NULL;
	int64_t count;

	if (rc->tc_tracked) {
		ref = kmem_cache_alloc(reference_cache, KM_SLEEP);
		ref->ref_holder = holder;
		ref->ref_number = number;
	}
	mutex_enter(&rc->tc_mtx);
	ASSERT(rc->tc_count >= 0);
	if (rc->tc_tracked)
		list_insert_head(&rc->tc_list, ref);
	rc->tc_count += number;
	count = rc->tc_count;
	mutex_exit(&rc->tc_mtx);

	return (count);
}

int64_t
trackcount_add(trackcount_t *rc, void *holder)
{
	return (trackcount_add_many(rc, 1, holder));
}

int64_t
trackcount_remove_many(trackcount_t *rc, uint64_t number, void *holder)
{
	trackref_t *ref;
	int64_t count;

	mutex_enter(&rc->tc_mtx);
	ASSERT(rc->tc_count >= number);

	if (!rc->tc_tracked) {
		rc->tc_count -= number;
		count = rc->tc_count;
		mutex_exit(&rc->tc_mtx);
		return (count);
	}

	for (ref = list_head(&rc->tc_list); ref;
	    ref = list_next(&rc->tc_list, ref)) {
		if (ref->ref_holder == holder && ref->ref_number == number) {
			list_remove(&rc->tc_list, ref);
			if (reference_history > 0) {
				ref->ref_removed =
				    kmem_cache_alloc(reference_history_cache,
				    KM_SLEEP);
				list_insert_head(&rc->rc_removed, ref);
				rc->tc_removed_count++;
				if (rc->tc_removed_count > reference_history) {
					ref = list_tail(&rc->rc_removed);
					list_remove(&rc->rc_removed, ref);
					kmem_cache_free(reference_history_cache,
					    ref->ref_removed);
					kmem_cache_free(reference_cache, ref);
					rc->tc_removed_count--;
				}
			} else {
				kmem_cache_free(reference_cache, ref);
			}
			rc->tc_count -= number;
			count = rc->tc_count;
			mutex_exit(&rc->tc_mtx);
			return (count);
		}
	}
	panic("No such hold %p on refcount %llx", holder,
	    (u_longlong_t)(uintptr_t)rc);
	return (-1);
}

int64_t
trackcount_remove(trackcount_t *rc, void *holder)
{
	return (trackcount_remove_many(rc, 1, holder));
}

void
trackcount_transfer(trackcount_t *dst, trackcount_t *src)
{
	int64_t count, removed_count;
	list_t list, removed;

	list_create(&list, sizeof (trackref_t),
	    offsetof(trackref_t, ref_link));
	list_create(&removed, sizeof (trackref_t),
	    offsetof(trackref_t, ref_link));

	mutex_enter(&src->tc_mtx);
	count = src->tc_count;
	removed_count = src->tc_removed_count;
	src->tc_count = 0;
	src->tc_removed_count = 0;
	list_move_tail(&list, &src->tc_list);
	list_move_tail(&removed, &src->rc_removed);
	mutex_exit(&src->tc_mtx);

	mutex_enter(&dst->tc_mtx);
	dst->tc_count += count;
	dst->tc_removed_count += removed_count;
	list_move_tail(&dst->tc_list, &list);
	list_move_tail(&dst->rc_removed, &removed);
	mutex_exit(&dst->tc_mtx);

	list_destroy(&list);
	list_destroy(&removed);
}

#endif	/* ZFS_DEBUG */
