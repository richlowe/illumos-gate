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

#include	<link.h>
#include	<sys/asm_linkage.h>

	.file	"boot_elf.s"

	ANSI_PRAGMA_WEAK(elf_rtbndr, function)
	ENTRY(elf_rtbndr)
	/*
	 * sp + 0 = &GOT[n + 3]
	 * sp + 8 = x30
	 * x16 = &GOT[2]
	 */
	stp	x0, x1, [sp, #-16]!
	stp	x2, x3, [sp, #-16]!
	stp	x4, x5, [sp, #-16]!
	stp	x6, x7, [sp, #-16]!
	stp	x8, xzr, [sp, #-16]!
	stp	q0, q1, [sp, #-32]!
	stp	q2, q3, [sp, #-32]!
	stp	q4, q5, [sp, #-32]!
	stp	q6, q7, [sp, #-32]!

	ldr	x2, [sp, #16 * 5 + 32 * 4]
	sub	x1, x2, x16	// x1 = &GOT[n + 3] - &GOT[2] = (n + 1) * 8
	sub	x1, x1, #8	// x1 = n * 8
	lsl	x3, x1, #1	// x3 = n * 16
	add	x1, x1, x3	// x1 = n * 24

	ldr	x0, [x16, #-8]

	bl	elf_bndr
	mov	x16, x0
	ldp	q6, q7, [sp], #32
	ldp	q4, q5, [sp], #32
	ldp	q2, q3, [sp], #32
	ldp	q0, q1, [sp], #32
	ldp	x8, xzr, [sp], #16
	ldp	x6, x7, [sp], #16
	ldp	x4, x5, [sp], #16
	ldp	x2, x3, [sp], #16
	ldp	x0, x1, [sp], #16
	ldp	xzr, x30, [sp], #16

	br	x16
	SET_SIZE(elf_rtbndr)
