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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2023 Richard Lowe
 */

	.file	"tls_get_addr.s"

/*
 * To make thread-local storage accesses as fast as possible, we
 * hand-craft the __tls_get_addr() function below, from this C code:
 * void *
 * __tls_get_addr(TLS_index *tls_index)
 * {
 *	ulwp_t *self = curthread;
 *	tls_t *tlsent = self->ul_tlsent;
 *	ulong_t moduleid;
 *	caddr_t	base;
 *
 *	if ((moduleid = tls_index->ti_moduleid) < self->ul_ntlsent &&
 *	    (base = tlsent[moduleid].tls_data) != NULL)
 *		return (base + tls_index->ti_tlsoffset);
 *
 *	return (slow_tls_get_addr(tls_index));
 * }
 */

#include "SYS.h"
#include <../assym.h>

#if SIZEOF_TLS_T == 16
#define	SHIFT 4			/* To translate moduleid into byte offset */
#else
#error tls_t changed size
#endif

	/*
	 * XXXARM: This is not actually particularly optimized.
	 * I wouldn't be surprised if the compiler actually does better,
	 * perhaps on all our platforms.
	 *
	 * Note this depends upon offsetof(tls_t, tls_data) == 0 when loading
	 * tlsent[moduleid].tls_data.  We should be able to replace it with an
	 * add-n-shift if that changes for the V2 DTV.
	 */
	ENTRY_NP(__tls_get_addr)
	mrs	x2, tpidr_el0		/* x2 <- thread pointer */
	sub	x2, x2, #UL_TCB		/* x2 <- ulwp */
	ldr	x1, [x0, #TI_MODULEID]	/* x1 <- tls moduleid */
	ldr	x3, [x2, #UL_NTLSENT]	/* x3 <- ulwp->ul_ntlsent (DTV generation) */
	cmp	x3, x1			/* if ntlsent < moduleid */
	b.ls	1f			/* ... slow path */
	ldr	x2, [x2, #UL_TLSENT]	/* x2 <- ulwp->ul_tlsent */
	lsl	x1, x1, #SHIFT		/* moduleid * sizeof(tls_t)) for byte offset */
	ldr	x1, [x2, x1]		/* x1 <- tlsent[moduleid].tls_data (tls base) */
	cbz	x1, 1f			/* ...  == NULL, slow path */
	ldr	x0, [x0, #TI_TLSOFFSET]	/* x0 <- tls offset */
	add	x0, x1, x0		/* x0 <- tls base + tls offset */
	ret
1:
	b slow_tls_get_addr		/* tail call */
	SET_SIZE(__tls_get_addr)
