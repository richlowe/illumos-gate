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

	.file	"getcontext.s"

#include "SYS.h"
#include "assym.h"

	ANSI_PRAGMA_WEAK(getcontext, function)
	ENTRY(getcontext)
	sub	sp, sp, #(16 * 2)
	stp	x29, x30, [sp, #(0 * 16)]
	stp	x0, x1,   [sp, #(1 * 16)]
	bl	__getcontext
	ldp	x1, x2,   [sp, #(1 * 16)]
	ldp	x29, x30, [sp, #(0 * 16)]
	add	sp, sp, #(16 * 2)
	cbnz	x0, 1f

	mov	x3, sp
	str	x30, [x1, #(UC_MCONTEXT_GREGS + 8*REG_PC)]
	str	x30, [x1, #(UC_MCONTEXT_GREGS + 8*REG_X30)]
	str	x3,  [x1, #(UC_MCONTEXT_GREGS + 8*REG_SP)]
	str	xzr, [x1, #(UC_MCONTEXT_GREGS + 8*REG_X0)]
	ret

1:	mov	x0, #-1
	ret
	SET_SIZE(getcontext)

	ANSI_PRAGMA_WEAK(swapcontext, function)
	ENTRY(swapcontext)
	sub	sp, sp, #(16 * 2)
	stp	x29, x30, [sp, #(0 * 16)]
	stp	x0, x1,   [sp, #(1 * 16)]
	bl	__getcontext
	ldp	x1, x2,   [sp, #(1 * 16)]
	ldp	x29, x30, [sp, #(0 * 16)]
	add	sp, sp, #(16 * 2)
	cbnz	x0, 1f

	mov	x3, sp
	str	x30, [x1, #(UC_MCONTEXT_GREGS + 8*REG_PC)]
	str	x30, [x1, #(UC_MCONTEXT_GREGS + 8*REG_X30)]
	str	x3,  [x1, #(UC_MCONTEXT_GREGS + 8*REG_SP)]
	str	xzr, [x1, #(UC_MCONTEXT_GREGS + 8*REG_X0)]
	mov	x0, x2
	bl	setcontext

1:	mov	x0, #-1
	ret
	SET_SIZE(swapcontext)
