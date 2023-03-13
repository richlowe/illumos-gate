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

#include <sys/asm_linkage.h>

	ENTRY(memset)
	mov	x8, x0
	mov	w0, w1
	mov	x1, x2
	and	w3, w0, #0xff
	add	w3, w3, w3, lsl #8
	add	w2, w3, w3, lsl #16
	mov	x0, x8
	cmp	x1, #0x1
	b.cc	1f
	and	w3, w0, #0x1
	cmp	w3, #0x0
	b.eq	1f
	strb	w2, [x0],#1
	sub	x1, x1, #0x1
1:	cmp	x1, #0x2
	b.cc	3f
	and	w3, w0, #0x2
	cmp	w3, #0x0
	b.eq	3f
	strh	w2, [x0],#2
	sub	x1, x1, #0x2
	b	3f
2:	str	w2, [x0],#4
	sub	x1, x1, #0x4
3:	cmp	x1, #0x4
	b.cs	2b
	and	x3, x1, #0x2
	cmp	x3, #0x0
	b.eq	4f
	strh	w2, [x0],#2
4:	and	x1, x1, #0x1
	cmp	x1, #0x0
	b.eq	5f
	strb	w2, [x0]
5:	mov	x0, x8
	ret
	SET_SIZE(memset)
