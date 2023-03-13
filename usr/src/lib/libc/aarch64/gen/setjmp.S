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
 */

	.file	"setjmp.s"

#include <sys/asm_linkage.h>
#include "assym.h"

	ANSI_PRAGMA_WEAK(setjmp,function)
	ANSI_PRAGMA_WEAK(longjmp,function)

	/*
	 * XXXARM: We save the FP registers, which seems a bit unusual.
	 */
	ENTRY(setjmp)
	stp	x19, x20, [x0, #(0 * 16)]
	stp	x21, x22, [x0, #(1 * 16)]
	stp	x23, x24, [x0, #(2 * 16)]
	stp	x25, x26, [x0, #(3 * 16)]
	stp	x27, x28, [x0, #(4 * 16)]
	stp	x29, x30, [x0, #(5 * 16)]
	stp	q8,  q9,  [x0, #(6 * 16 + 0 * 32)]
	stp	q10, q11, [x0, #(6 * 16 + 1 * 32)]
	stp	q12, q13, [x0, #(6 * 16 + 2 * 32)]
	stp	q14, q15, [x0, #(6 * 16 + 3 * 32)]
	mov	x1, sp
	mrs	x3, tpidr_el0
	sub	x3, x3, #UL_TCB
	ldr	x2, [x3, #(UL_SIGLINK)]
	stp	x1, x2,   [x0, #(6 * 16 + 4 * 32)]
	mov	x0, #0
	ret
	SET_SIZE(setjmp)


	ENTRY(longjmp)
	ldp	x19, x20, [x0, #(0 * 16)]
	ldp	x21, x22, [x0, #(1 * 16)]
	ldp	x23, x24, [x0, #(2 * 16)]
	ldp	x25, x26, [x0, #(3 * 16)]
	ldp	x27, x28, [x0, #(4 * 16)]
	ldp	x29, x30, [x0, #(5 * 16)]
	ldp	q8,  q9,  [x0, #(6 * 16 + 0 * 32)]
	ldp	q10, q11, [x0, #(6 * 16 + 1 * 32)]
	ldp	q12, q13, [x0, #(6 * 16 + 2 * 32)]
	ldp	q14, q15, [x0, #(6 * 16 + 3 * 32)]
	ldp	x2, x3,   [x0, #(6 * 16 + 4 * 32)]
	mov	sp, x2
	cbnz	x3, 1f
	mrs	x4, tpidr_el0
	sub	x4, x4, #UL_TCB
	str	xzr, [x4, #(UL_SIGLINK)]
1:	mov	x0, x1
	ret
	SET_SIZE(longjmp)
