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
 * Copyright 2022 Richard Lowe
 */

	.file	"vpanic.s"

#include <sys/asm_linkage.h>
#include <sys/privregs.h>
#include <sys/panic.h>

#include "assym.h"

/*
 * The panic() and cmn_err() functions invoke vpanic() as a common entry point
 * into the panic code implemented in panicsys().  vpanic() is responsible
 * for passing through the format string and arguments, and constructing a
 * regs structure on the stack into which it saves the current register
 * values.  If we are not dying due to a fatal trap, these registers will
 * then be preserved in panicbuf as the current processor state.  Before
 * invoking panicsys(), vpanic() activates the first panic trigger (see
 * common/os/panic.c) and switches to the panic_stack if successful.  Note that
 * DTrace takes a slightly different panic path if it must panic from probe
 * context.  Instead of calling panic, it calls into dtrace_vpanic(), which
 * sets up the initial stack as vpanic does, calls dtrace_panic_trigger(), and
 * branches back into vpanic().
 */

/*
 * r0..r18, r29, r30
 * Must be an even number to keep sp correctly aligned
 */
#define NREGS (21 + 1)
#define STACK_RESERVATION (NREGS * 8)

	ENTRY_NP(vpanic)
	/* Prologue also saves x29 and x30, sp (in fp) for us */
	stp x29, x30, [sp, #-STACK_RESERVATION]!
	mov x29, sp

	/*
	 * Push all caller-saved regs to somewhere safe.
	 * That's x0...x18, x29, x30, sp
	 * (the latter 3 being saved by the prologue already)
	 *
	 * This leaves our stack with, fp, lr, <all these saved registers> and the
	 * original sp in our fp.
	 */
	stp x0, x1, [sp, #0x10]
	stp x2, x3, [sp, #0x20]
	stp x4, x5, [sp, #0x30]
	stp x6, x7, [sp, #0x40]
	stp x8, x9, [sp, #0x50]
	stp x10, x11, [sp, #0x60]
	stp x12, x13, [sp, #0x70]
	stp x14, x15, [sp, #0x80]
	stp x16, x17, [sp, #0x90]
	str x18, [sp, #0x100]

	adrp x0, panic_quiesce
	add x0, x0, :lo12:panic_quiesce
	bl panic_trigger

vpanic_common:
	/*
	 * From this point on we use 4 registers as temps.
	 * These must all be caller-saved registers we saved above.
	 *
	 * x8  - the original sp, pointing to our caller-saved registers
	 * x9  - temp used for moving values
	 * x10 - temp used for moving values
	 * x11 - result of `panic_trigger`
	 */

	/* Save the current stack pointer */
	mov x8, sp

	/*
	 * The panic_trigger result is in %x0 from the call above, and
	 * dtrace_panic places it in %x0 before branching here.
	 */
	mov x11, x0		 /* return value of panic_trigger for later */
	/*
	 * If panic_trigger() was successful, we are the first to initiate a
	 * panic: we now switch to the reserved panic_stack before continuing.
	 */
	cmp x0, #0
	b.eq 1f

	/*
	 * If panic_trigger() returned non-0, we are the first to initiate a
	 * panic: we now switch to the reserved panic_stack before continuing.
	 */
	adrp x9, panic_stack
	add x9, x9, :lo12:panic_stack
	mov sp, x9
	add sp, sp, #PANICSTKSIZE

1:				/* panic_trigger failed, we're not the first */
	sub sp, sp, #REG_FRAME

	/*
	 * Now that we've got everything set up, store the register values as
	 * they were when we entered vpanic() to the designated location in
	 * the regs structure we allocated on the stack.
	 */
	ldp x9, x10, [x8, #0x10]
	stp x9, x10, [sp, #REGOFF_X0]
	ldp x9, x10, [x8, #0x20]
	stp x9, x10, [sp, #REGOFF_X2]
	ldp x9, x10, [x8, #0x30]
	stp x9, x10, [sp, #REGOFF_X4]
	ldp x9, x10, [x8, #0x40]
	stp x9, x10, [sp, #REGOFF_X6]
	ldp x9, x10, [x8, #0x50]
	stp x9, x10, [sp, #REGOFF_X8]
	ldp x9, x10, [x8, #0x60]
	stp x9, x10, [sp, #REGOFF_X10]
	ldp x9, x10, [x8, #0x70]
	stp x9, x10, [sp, #REGOFF_X12]
	ldp x9, x10, [x8, #0x80]
	stp x9, x10, [sp, #REGOFF_X14]
	ldp x9, x10, [x8, #0x90]
	stp x9, x10, [sp, #REGOFF_X16]
	ldr x9, [x8, #0x100]
	str x9, [sp, #REGOFF_X18]

	/* x29 and x30 were saved to the old sp, as part of the prologue */
	ldp x9, x10, [x8]
	stp x9, x10, [sp, #REGOFF_X29]

	/* Now save the registers we haven't had to preserve */
	stp x19, x20, [sp, #REGOFF_X19]
	stp x21, x22, [sp, #REGOFF_X21]
	stp x23, x24, [sp, #REGOFF_X23]
	stp x25, x26, [sp, #REGOFF_X25]
	stp x27, x28, [sp, #REGOFF_X27]
	/* x29, x30 were already saved */

	/* Saved earlier, to refer to our saved pieces */
	str x8, [sp, #REGOFF_SP]

	/*
	 * XXXARM It seems like this should be our original lr,
	 * but that's not what x86 does
	 */
	adr x9, vpanic
	str x9, [sp, #REGOFF_PC]

	mov x9, #0
	str x9, [sp, #REGOFF_SPSR]

	/* Setup to call panicsys(fmt, ap, &regs, on_panic_stack) */
	ldp x0, x1, [sp, #REGOFF_X0]  /* format, ap */
	mov x2, sp		      /* &struct regs */
	mov x3, x11		      /* return value of panic_trigger */
	bl panicsys

	/* epilogue though if we return from here, we're in _so_ much trouble */
	add sp, sp, #REG_FRAME
	/*  Back onto our original stack */
	mov sp, x29
	ldp x29, x30, [sp], #STACK_RESERVATION
	ret
	SET_SIZE(vpanic)

/*
 * DTrace panics specially from probe context, see the comment at
 * the top of this file
 */
	ENTRY_NP(dtrace_vpanic)
	stp x29, x30, [sp, #-STACK_RESERVATION]!
	mov x29, sp

	/*
	 * NB: This must be exactly the same as in `vpanic`,
	 * since we restore them after jumping to `vpanic_common`.
	 * See the comments there.
	 */
	stp x0, x1, [sp, #0x10]
	stp x2, x3, [sp, #0x20]
	stp x4, x5, [sp, #0x30]
	stp x6, x7, [sp, #0x40]
	stp x8, x9, [sp, #0x50]
	stp x10, x11, [sp, #0x60]
	stp x12, x13, [sp, #0x70]
	stp x14, x15, [sp, #0x80]
	stp x16, x17, [sp, #0x90]
	str x18, [sp, #0x100]

	adrp x0, panic_quiesce
	add x0, x0, :lo12:panic_quiesce
	bl dtrace_panic_trigger
	b vpanic_common
	SET_SIZE(dtrace_vpanic)
