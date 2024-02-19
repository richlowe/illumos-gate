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

	.file	"kdi_exceptions.S"

/*
 * The exception vector used when running in kmdb(8).
 */

#include <sys/asm_linkage.h>
#include <sys/privregs.h>

	#include "assym.h"

#define KDISVC_SHIFT	3
#if (1 << KDISVC_SHIFT) != KDISVC_SIZE
#error "KDISVC_SHIFT does not correspond to size of kdi_svc_t"
#endif

/* Each entry in the exception vector is 128 bytes / 32 instruction long */
#define VECTOR_ENTRY_SIZE	128

/* Fail the assembly if a vector entry is too large */
#define	CHECK_VECTOR_ENTRY(name)		\
	.if (. - name) > VECTOR_ENTRY_SIZE;	\
	.err;					\
	.endif

	.data
.unexpected_exception:	.asciz "kmdb: unexpected exception type"
.unexpected_kdisvc:	.asciz "kmdb: unexpected kdi svc #%d"

#define UNEXPECTED()					\
	clrex;						\
	adrp	x0, .unexpected_exception;		\
	add	x0, x0, :lo12:.unexpected_exception;	\
	bl	panic

/*
 * This is the AArch64 exception vector used by the hardware, each entry is 128 byte
 * aligned, and 32 instructions (128 bytes) long.
 *
 * We represent this as a single large function (exception_vector), which we can put in VBAR,
 * and a nested function per-entry to make things easier to refer to while debugging.
 *
 * It is important that ALTENTRY is used, so alignment is not altered, and the
 * vector keeps it specified shape.
 */
	.text
	.balign	2048
	ENTRY(kdi_exception_vector)
	/*
	 * From Current Exception level with SP_EL0
	 */
	.balign VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_current_el_sp0_sync)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_current_el_sp0_sync)
SET_SIZE(kmdb_from_current_el_sp0_sync)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_current_el_sp0_irq)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_current_el_sp0_irq)
SET_SIZE(kmdb_from_current_el_sp0_irq)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_current_el_sp0_fiq)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_current_el_sp0_fiq)
SET_SIZE(kmdb_from_current_el_sp0_fiq)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_current_el_sp0_error)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_current_el_sp0_error)
SET_SIZE(kmdb_from_current_el_sp0_error)

	/*
	 * From Current Exception level with SP_ELx
	 */
	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_current_el_sync)
	clrex
	// Leave debug exceptions disabled until we know whether we're
	// entering the debugger
	b	kmdb_from_current_el_sync_handler
	CHECK_VECTOR_ENTRY(kmdb_from_current_el_sync)
SET_SIZE(kmdb_from_current_el_sync)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_current_el_irq)
	msr DAIFClr, #DAIF_SETCLEAR_DEBUG
	b from_current_el_irq // Just let the kernel handle it
	CHECK_VECTOR_ENTRY(kmdb_from_current_el_irq)
SET_SIZE(kmdb_from_current_el_irq)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_current_el_fiq)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_current_el_fiq)
SET_SIZE(kmdb_from_current_el_fiq)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_current_el_error)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_current_el_error)
	SET_SIZE(kmdb_from_current_el_error)

	/*
	 * From Lower Exception level using aarch64
	 */
	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_lower_el_aarch64_sync)
	b from_lower_el_aarch64_sync // Just use the kernel handler
	CHECK_VECTOR_ENTRY(kmdb_from_lower_el_aarch64_sync)
SET_SIZE(kmdb_from_lower_el_aarch64_sync)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_lower_el_aarch64_irq)
	b from_lower_el_aarch64_irq // Kernel handler
	CHECK_VECTOR_ENTRY(kmdb_from_lower_el_aarch64_irq)
SET_SIZE(kmdb_from_lower_el_aarch64_irq)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_lower_el_aarch64_fiq)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_lower_el_aarch64_fiq)
SET_SIZE(kmdb_from_lower_el_aarch64_fiq)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_lower_el_aarch64_error)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_lower_el_aarch64_error)
	SET_SIZE(kmdb_from_lower_el_aarch64_error)

	/*
	 * From Lower Exception level using aarch32
	 */
	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_lower_el_aarch32_sync)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_lower_el_aarch32_sync)
SET_SIZE(kmdb_from_lower_el_aarch32_sync)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_lower_el_aarch32_irq)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_lower_el_aarch32_irq)
SET_SIZE(kmdb_from_lower_el_aarch32_irq)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_lower_el_aarch32_fiq)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_lower_el_aarch32_fiq)
SET_SIZE(kmdb_from_lower_el_aarch32_fiq)

	.balign	VECTOR_ENTRY_SIZE
ALTENTRY(kmdb_from_lower_el_aarch32_error)
	UNEXPECTED()
	CHECK_VECTOR_ENTRY(kmdb_from_lower_el_aarch32_error)
	SET_SIZE(kmdb_from_lower_el_aarch32_error)

	SET_SIZE(kdi_exception_vector)


	ENTRY(kmdb_from_current_el_sync_handler)
	// NB: We save registers here, here, because we no longer fit in the
	// top-level vector
	__SAVE_REGS_EL1
	__SAVE_FRAME

	mrs	x1, esr_el1
	lsr	w0, w1, #ESR_EC_SHIFT	// w0 <- EC, no need to mask.

	/*
	 * if we took a svc from the current exception level, we're trying to talk to kmdb
	 */
	// NB: This is written tortuously to keep the stack as legible as possible
	cmp	w0, #T_SVC
	b.ne	1f
	mov	x3, sp // saved registers
	bl	kdi_svc_handler
	b	2f

1:	mov	x0, sp // saved registers
	bl	kdi_cmnint
2:
	__RESTORE_REGS_EL1
	eret
	SET_SIZE(kmdb_from_current_el_sync_handler)

	// kdi_svc_handler(int32_t ec, int32_t esr, void *regs)
	ENTRY(kdi_svc_handler)
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp

	and	w20, w1, #ESR_ISS_MASK	// w20 <- svc #

	ldr	x1, =kdi_svcs	// x1 <- kernel svc table
	cmp	w20, #NKDISVCS
	b.hs	_kdisvc_ill	// invalid service#, we want to panic here, but usefully

	add	x20, x1, x20, lsl #KDISVC_SHIFT // x20 <- svc entry
	ldr	x16, [x20, #KS_CALL]

	// Parameters?, registers only
	// Note that they're shifted so that the struct regs is first, always
	// I hate that
	mov	x0, x3
	ldp	x1, x2, [x0, #REGOFF_X0]
	ldp	x3, x4, [x0, #REGOFF_X2]
	ldp	x5, x6, [x0, #REGOFF_X4]
	ldr	x7, [x0, #REGOFF_X6]

	blr	x16

	ldp	fp, lr, [sp], #16
	ret

_kdisvc_ill:
	adrp	x0, .unexpected_kdisvc
	add	x0, x0, :lo12:.unexpected_kdisvc
	mov	w1, w20
	bl	panic
	SET_SIZE(kdi_svc_handler)
