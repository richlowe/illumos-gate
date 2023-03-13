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

	.file	"_stack_grow.s"
#include "SYS.h"
#include "assym.h"

	ENTRY(_stack_grow)
	mrs	x3, tpidr_el0
	sub	x3, x3, #UL_TCB
	ldr	x1, [x3, #(UL_USTACK + SS_SP)]
	ldr	x2, [x3, #(UL_USTACK + SS_SIZE)]
	sub	x3, x0, x1
	cmp	x2, x3
	b.hs	1f
	cbnz	x2, 2f
1:	ret

2:	sub	x3, sp, x1
	cmp	x2, x3
	b.lo	3f
	sub	sp, x1, #STACK_ALIGN
3:	strb	wzr, [x1, #-1]
	SYSTRAP_RVAL1(lwp_self)
	mov	w1, #SIGSEGV
	SYSTRAP_RVAL1(lwp_kill)
	ret

	SET_SIZE(_stack_grow)
