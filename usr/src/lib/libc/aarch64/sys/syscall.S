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

	.file	"syscall.s"

#include "SYS.h"

	ANSI_PRAGMA_WEAK(syscall,function)

	ENTRY(syscall)
	mov	w9, w0
	mov	x0, x1
	mov	x1, x2
	mov	x2, x3
	mov	x3, x4
	mov	x4, x5
	mov	x5, x6
	mov	x6, x7
	ldr	x7, [sp]
	svc	#0
	SYSCERROR
	RET
	SET_SIZE(syscall)

	ENTRY(_syscall6)
	mov	w9, w0
	mov	x0, x1
	mov	x1, x2
	mov	x2, x3
	mov	x3, x4
	mov	x4, x5
	mov	x5, x6
	svc	#0
	SYSCERROR
	RET
	SET_SIZE(_syscall6)

	ENTRY(__systemcall)
	stp	x0, x1, [sp, #(-8 * 2)]!
	mov	w9, w1
	mov	x0, x2
	mov	x1, x3
	mov	x2, x4
	mov	x3, x5
	mov	x4, x6
	mov	x5, x7
	ldr	x6, [sp, #(8 * 2)]
	ldr	x7, [sp, #(8 * 3)]
	svc	#0
	ldp	x2, x3, [sp], #(8 * 2)
	b.cs	1f
	str	x0, [x2, #(0 * 8)]
	str	x1, [x2, #(1 * 8)]
	mov	x0, #0
	RET
1:
	mov	x3, #-1
	str	x3, [x2, #(0 * 8)]
	str	x3, [x2, #(1 * 8)]
	mov	x0, x9
	RET
	SET_SIZE(__systemcall)

	ENTRY(__systemcall6)
	stp	x0, x1, [sp, #(-8 * 2)]!
	mov	w9, w1
	mov	x0, x2
	mov	x1, x3
	mov	x2, x4
	mov	x3, x5
	mov	x4, x6
	mov	x5, x7
	svc	#0
	ldp	x2, x3, [sp], #(8 * 2)
	b.cs	1f
	str	x0, [x2, #(0 * 8)]
	str	x1, [x2, #(1 * 8)]
	mov	x0, #0
	RET
1:
	mov	x3, #-1
	str	x3, [x2, #(0 * 8)]
	str	x3, [x2, #(1 * 8)]
	mov	x0, x9
	RET
	SET_SIZE(__systemcall6)
