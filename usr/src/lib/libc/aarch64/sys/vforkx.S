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
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

	.file	"vforkx.s"

#include "SYS.h"
#include "assym.h"

/*
 * The child of vfork() will execute in the parent's address space,
 * thereby changing the stack before the parent runs again.
 * Therefore we have to be careful how we return from vfork().
 * Pity the poor debugger developer who has to deal with this kludge.
 *
 * We block all blockable signals while performing the vfork() system call
 * trap.  This enables us to set curthread->ul_vfork safely, so that we
 * don't end up in a signal handler with curthread->ul_vfork set wrong.
 */

	ENTRY_NP(vforkx)
	mov	x5, x0
	b	1f
	ENTRY_NP(vfork)
	mov	x5, #0
1:
	ldr	x0, =SIG_SETMASK
	ldr	x1, =MASKSET0
	ldr	x2, =MASKSET1
	ldr	x3, =MASKSET2
	ldr	x4, =MASKSET3
	SYSTRAP_RVAL1(lwp_sigmask)

	mov	x1, x5
	mov	x0, #2
	SYSTRAP_RVAL2(forksys)
	b.cc	3f

	mov	x5, x9		/* save the vfork() error number */

	mrs	x10, tpidr_el0
	sub	x10, x10, #UL_TCB

	ldr	x0, =SIG_SETMASK
	ldr	w1, [x10, #(UL_SIGMASK + 0 * 4)]
	ldr	w2, [x10, #(UL_SIGMASK + 1 * 4)]
	ldr	w3, [x10, #(UL_SIGMASK + 2 * 4)]
	ldr	w4, [x10, #(UL_SIGMASK + 3 * 4)]
	SYSTRAP_RVAL1(lwp_sigmask)
	mov	x0, x5
	b	__cerror

3:
	/*
	 * To determine if we are (still) a child of vfork(), the child
	 * increments curthread->ul_vfork by one and the parent decrements
	 * it by one.  If the result is zero, then we are not a child of
	 * vfork(), else we are.  We do this to deal with the case of
	 * a vfork() child calling vfork().
	 */
	mov	x5, x0		/* save the vfork() return value */
	mrs	x10, tpidr_el0
	sub	x10, x10, #UL_TCB
	cbnz	x1, child

	/* parent process */
	ldr	w2, [x10, #UL_VFORK]
	cbz	w2, update_ul_vfork
	sub	w2, w2, #1
	b	update_ul_vfork
child:
	mov	x5, #0
	ldr	w2, [x10, #UL_VFORK]
	add	w2, w2, #1

update_ul_vfork:
	str	w2, [x10, #UL_VFORK]

	/*
	 * Clear the schedctl interface in both parent and child.
	 * (The child might have modified the parent.)
	 */
	str	xzr, [x10, #UL_SCHEDCTL]
	str	xzr, [x10, #UL_SCHEDCTL_CALLED]

	ldr	x0, =SIG_SETMASK
	ldr	w1, [x10, #(UL_SIGMASK + 0 * 4)]
	ldr	w2, [x10, #(UL_SIGMASK + 1 * 4)]
	ldr	w3, [x10, #(UL_SIGMASK + 2 * 4)]
	ldr	w4, [x10, #(UL_SIGMASK + 3 * 4)]
	SYSTRAP_RVAL1(lwp_sigmask)

	mov	x0, x5
	ret
	SET_SIZE(vfork)
	SET_SIZE(vforkx)
