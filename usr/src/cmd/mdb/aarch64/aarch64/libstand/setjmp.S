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

	.file "setjmp.S"

#include <sys/asm_linkage.h>

	ENTRY(setjmp)
	ALTENTRY(sigsetjmp)
	stp	x19, x20, [x0, #(0 * 16)]
	stp	x21, x22, [x0, #(1 * 16)]
	stp	x23, x24, [x0, #(2 * 16)]
	stp	x25, x26, [x0, #(3 * 16)]
	stp	x27, x28, [x0, #(4 * 16)]
	stp	x29, x30, [x0, #(5 * 16)]

	mov	x1, sp
	stp	x1, xzr,   [x0, #(6 * 16 + 4 * 32)]
	mov	x0, #0
	ret
	SET_SIZE(sigsetjmp)
	SET_SIZE(setjmp)

	ENTRY(longjmp)
	ALTENTRY(siglongjmp)
	ldp	x19, x20, [x0, #(0 * 16)]
	ldp	x21, x22, [x0, #(1 * 16)]
	ldp	x23, x24, [x0, #(2 * 16)]
	ldp	x25, x26, [x0, #(3 * 16)]
	ldp	x27, x28, [x0, #(4 * 16)]
	ldp	x29, x30, [x0, #(5 * 16)]
	ldp	x2, xzr,   [x0, #(6 * 16 + 4 * 32)]
	mov	sp, x2
	mov	x0, x1
	ret
	SET_SIZE(longjmp)
	SET_SIZE(siglongjmp)
