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

/* Copyright 2023 Richard Lowe */

	.file	"kaif_invoke.S"

/*
 * Kernel function call invocation
 */

#include <sys/asm_linkage.h>

	/*
	 * This is going to be fun.  We were called with the function pointer
	 * in %x0, argc in %x1, and a pointer to an array of uintptr_t's
	 * (the arguments to be passed) in %x2.  In the worst case, we need
	 * to move the first eight arguments from the array to %x0-x7.
	 * The remaining arguments need to be copied from
	 * the array to [%sp, 0], [sp, 8], and so on.  Then we can call the
	 * function.
	 */
	ENTRY_NP(kaif_invoke)
	stp	fp, lr, [sp, #-32]!
	str	x19, [sp, #16]	// preserve callee-saved x19
	mov	fp, sp

	mov	x19, #0	// x19 <- number of args we'll put on the stack

	// Preserve our arguments, since we must clobber x0-x7
	mov	x9, x0	// x9 <- func
	mov	x10, x1	// x10 <- argc
	mov	x11, x2	// x11 <- argv

	// No arguments, just call
	cbz	x10, call

	// Need to spill arguments onto the stack?
	cmp	x10, #8
	b.le	stackdone  // If not, just fill registers

	sub	x13, x10, #8  // x13 <- argc - 8, nargs

	// x19 <- SA(nargs), keep sp aligned
	add	x19, x13, 1
	and	x19, x19, ~1

	// SA(nargs * 8) bytes of stack space
	sub	sp, sp, x19, lsl #3

	// store spilled args, lowest at sp, then sp+8, etc.
	// Remember we can't use x0-x11, nor callee-saved registers
	add	x15, x11, #(8 * 8)	// x15 <- &argv[8], src
	mov	x16, sp			// x16 <- &sp, dst

1: // loop spilling arguments
	ldr	x0, [x15], #8	// x0 <- src++
	str	x0, [x16], #8	// dst++ = x0
	sub	x13, x13, #1	// nargs--
	cbnz	x13, 1b

	// We've spilled all items onto the stack, so argc = 8
	mov	x10, #8

stackdone:
	// Now load the first 8 into their ABI registers.
	// goto (jmptab + (8 - argc))
	adr	x12, jmptab		// x12 <- jmptab
	mov	x13, #8
	sub	x10, x13, x10		// x10 <- 8 - argc
	add	x12, x12, x10, lsl #2	// x12 <- jmptab + ((8 - argc) insns (* 4))
	br	x12

jmptab:
cp8:	ldr	x7, [x11, #56]	// argv[7]
cp7:	ldr	x6, [x11, #48]	// argv[6]
cp6:	ldr	x5, [x11, #40]	// argv[5]
cp5:	ldr	x4, [x11, #32]	// argv[4]
cp4:	ldr	x3, [x11, #24]	// argv[3]
cp3:	ldr	x2, [x11, #16]	// argv[2]
cp2:	ldr	x1, [x11, #8]	// argv[1]
cp1:	ldr	x0, [x11]	// argv[0]

	// Now we have the arguments, call
call:	blr	x9

	// return SA((argc - 8) * 8) bytes of stack space
	add	sp, sp, x19, LSL #3
	ldr	x19, [sp, #16] // restore x19
	ldp	fp, lr, [sp], #32
	ret
	SET_SIZE(kaif_invoke)
