/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2012, Richard Lowe.
 */

#define	FUNC(x) \
	.text; \
	.align	16; \
	.globl	x; \
	.type	x, @function; \
x:

#define	SET_SIZE(x, x_size) \
	.size	x, [.-x]; \
        .globl x_size; \
        .type  x_size, @object; \
x_size:

/*
 * Extracted versions of the functional tests
 *
 * basic			-- A regular function
 * align			-- odd number of arguments needing save-area
 *				   alignment
 * stack-spill			-- Spills to the stack, and uses another
 *				   prologue
 */
FUNC(gcc_10_align)
	stp	x29, x30, [sp, #-64]!
	mov	x29, sp
	stp	x0, x1, [sp, #16]
	stp	x2, x3, [sp, #32]
	stp	x4, x19, [sp, #48]
SET_SIZE(gcc_10_align, gcc_10_align_end)

FUNC(gcc_10_basic)
	stp	x29, x30, [sp, #-64]!
	mov	x29, sp
	stp	x0, x1, [sp, #16]
	stp	x2, x3, [sp, #32]
SET_SIZE(gcc_10_basic, gcc_10_basic_end)

FUNC(gcc_10_stack_spill)
	sub	sp, sp, #0x70
	stp	x29, x30, [sp, #16]
	add	x29, sp, #0x10
	stp	x0, x1, [sp, #32]
	stp	x2, x3, [sp, #48]
	stp	x4, x5, [sp, #64]
	stp	x6, x7, [sp, #80]
	str	x19, [sp, #96]
SET_SIZE(gcc_10_stack_spill, gcc_10_stack_spill_end)

/* A larger offset between fp and sp than `gcc_10_stack_spill` */
FUNC(gcc_10_larger_fp_off)
	sub	sp, sp, #0x70
	stp	x29, x30, [sp, #16]
	add	x29, sp, #0x100
	stp	x0, x1, [sp, #272]
	stp	x2, x3, [sp, #288]
	stp	x4, x5, [sp, #304]
	stp	x6, x7, [sp, #320]
SET_SIZE(gcc_10_larger_fp_off, gcc_10_larger_fp_off_end)
