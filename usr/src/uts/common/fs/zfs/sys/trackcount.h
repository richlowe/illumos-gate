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

#ifndef	_SYS_REFCOUNT_H
#define	_SYS_REFCOUNT_H

#include <sys/inttypes.h>
#include <sys/list.h>
#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * If the reference is held only by the calling function and not any
 * particular object, use FTAG (which is a string) for the holder_tag.
 * Otherwise, use the object that holds the reference.
 */
#define	FTAG ((char *)__func__)

#ifdef	ZFS_DEBUG
typedef struct trackref {
	list_node_t ref_link;
	void *ref_holder;
	uint64_t ref_number;
	uint8_t *ref_removed;
} trackref_t;

typedef struct trackcount {
	kmutex_t tc_mtx;
	boolean_t tc_tracked;
	list_t tc_list;
	list_t rc_removed;
	uint64_t tc_count;
	uint64_t tc_removed_count;
} trackcount_t;

/* Note: trackcount_t must be initialized with trackcount_create[_untracked]() */

void trackcount_create(trackcount_t *rc);
void trackcount_create_untracked(trackcount_t *rc);
void trackcount_destroy(trackcount_t *rc);
void trackcount_destroy_many(trackcount_t *rc, uint64_t number);
int trackcount_is_zero(trackcount_t *rc);
int64_t trackcount_count(trackcount_t *rc);
int64_t trackcount_add(trackcount_t *rc, void *holder_tag);
int64_t trackcount_remove(trackcount_t *rc, void *holder_tag);
int64_t trackcount_add_many(trackcount_t *rc, uint64_t number, void *holder_tag);
int64_t trackcount_remove_many(trackcount_t *rc, uint64_t number, void *holder_tag);
void trackcount_transfer(trackcount_t *dst, trackcount_t *src);

void trackcount_init(void);
void trackcount_fini(void);

#else	/* ZFS_DEBUG */

typedef struct refcount {
	uint64_t tc_count;
} trackcount_t;

#define	trackcount_create(rc) ((rc)->tc_count = 0)
#define	trackcount_create_untracked(rc) ((rc)->tc_count = 0)
#define	trackcount_destroy(rc) ((rc)->tc_count = 0)
#define	trackcount_destroy_many(rc, number) ((rc)->tc_count = 0)
#define	trackcount_is_zero(rc) ((rc)->tc_count == 0)
#define	trackcount_count(rc) ((rc)->tc_count)
#define	trackcount_add(rc, holder) atomic_inc_64_nv(&(rc)->tc_count)
#define	trackcount_remove(rc, holder) atomic_dec_64_nv(&(rc)->tc_count)
#define	trackcount_add_many(rc, number, holder) \
	atomic_add_64_nv(&(rc)->tc_count, number)
#define	trackcount_remove_many(rc, number, holder) \
	atomic_add_64_nv(&(rc)->tc_count, -number)
#define	trackcount_transfer(dst, src) { \
	uint64_t __tmp = (src)->tc_count; \
	atomic_add_64(&(src)->tc_count, -__tmp); \
	atomic_add_64(&(dst)->tc_count, __tmp); \
}

#define	trackcount_init()
#define	trackcount_fini()

#endif	/* ZFS_DEBUG */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_REFCOUNT_H */
