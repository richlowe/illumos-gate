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

	.file	"kdi_asm.S"

#include <sys/asm_linkage.h>
#include <sys/kdi_regs.h>
#include <sys/privregs.h>

#include "assym.h"

	ENTRY_NP(kmdb_enter)
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	svc	#KDISVC_KMDB_TRAP
	ldp	fp, lr, [sp], #16
	ret
	SET_SIZE(kmdb_enter)

	// kmdb_trap(void *regs, ...)
	ENTRY_NP(kmdb_trap)
	// Registers were saved in the exception vector
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	bl	kdi_cmnint // regs already in x0
	ldp	fp, lr, [sp], #16
	mov	x0, #0
	ret
	SET_SIZE(kmdb_trap)

	// Common interrupt entry kdi_cmnint(void *regs)
	ENTRY_NP(kdi_cmnint)
	ALTENTRY(kdi_master_entry)
	stp	fp, lr, [sp, #-32]!
	mov	fp, sp

	stp	x19, x20, [sp, #16]
	mov	x19, x0	       // x19 <- saved regs
	bl	kdi_get_cpusave
	mov	x20, x0		// x20 <- cpusave

	// Move to the next crumb
	bl	kdi_advance_crumb_pointer // kdi_advance_crumb_pointer(cpusave)
	ldr	x0, [x0, #KRS_CURCRUMB]
	// Add our new crumb data
	mov	x1, #KDI_CPU_STATE_MASTER
	str	x1, [x0, #KRM_CPU_STATE]
	mov	x1, sp
	str	x1, [x0, #KRM_SP]
	ldr	x1, [x19, #REGOFF_PC]
	str	x1, [x0, #KRM_PC]
	ldr	x1, [x19, #REGOFF_TRAPNO]
	str	x1, [x0, #KRM_TRAPNO]

	/*
	 * Were we in the debugger when we took the trap (i.e. was %sp in one
	 * of the debugger's memory ranges)?
	 */
	adrp	x4, kdi_memranges
	add	x4, x4, :lo12:kdi_memranges // x4 <- &kdi_memranges

	adrp	x5, kdi_nmemranges
	ldr	x5, [x5, :lo12:kdi_nmemranges] // x5 <- kdi_nmemranges

1:	ldr	x6, [x4, #MR_BASE]
	cmp	sp, x6
	b.lt	2f // below this range -- try the next one
	ldr	x6, [x4, #MR_LIM]
	cmp	sp, x6
	b.gt	2f // above this range -- try the next one
	b	3f // matched this range

2:	sub	x5, x5, 1
	cbz	x5, 4f // stack not in the debugger

	add	x4, x4, #MR_SIZE
	b	1b

3:	/*
	 * The master is still set.  That should only happen if we hit a trap
	 * while running in the debugger.  Note that it may be an intentional
	 * fault.  kmdb_dpi_handle_fault will sort it all out.
	*/
	bl	kdi_get_cpuid
	mov	x3, x0  // x3 <- cpu id
	mov	x1, x19	// x1 <- saved registers
	ldr	x2, [x1, #REGOFF_SAVFP]
	ldr	x1, [x1, #REGOFF_SAVPC]
	mrs	x0, esr_el1
	lsr	x0, x0, #ESR_EC_SHIFT

	bl	kdi_dvec_handle_fault // kdi_dvec_handle_fault(trapno, pc, sp, cpuid)
	b	5f

4:	mov	x0, x20
	mov	x1, x19
	bl	kdi_save_common_state
5:	ldp	x19, x20, [sp, #16]
	ldp	fp, lr, [sp], #32
	ret
	SET_SIZE(kdi_master_entry)
	SET_SIZE(kdi_cmnint)

	// void kdi_save_common_state(kdi_cpusave_t *, struct regs *)
	ENTRY_NP(kdi_save_common_state)
	stp	fp, lr, [sp, #-32]!
	mov	fp, sp

	str	x1, [x0, #KRS_GREGS] // Save the saved registers

	// Save the exception vector
	mrs	x1, vbar_el1
	str	x1, [x0, #KRS_EXCEPTION_VECTOR]

	// Figure out whether to handle ourselves, or trap to the
	// kernel.
	str	x0, [sp, #16] // save x0 across the call

	bl	kdi_trap_pass	// kdi_trap_pass(kdi_cpusave_t *)
	mov	x1, x0		// save return
	ldr	x0, [sp, #16]	// restore x0
	cbnz	x1, kdi_pass_to_kernel // kdi_pass_to_kernel(kdi_cpusave_t *)
	// else
	bl	kdi_debugger_entry // kdi_debugger_entry(cpusave)
	bl	kdi_resume  // get back out of kmdb
	ldp	fp, lr, [sp], #32
	ret
	SET_SIZE(kdi_save_common_state)

	ENTRY_NP(kdi_resume)
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	bl	kdi_restore_debugging_state
	ldp	fp, lr, [sp], #16
	ret
	SET_SIZE(kdi_resume)

	ENTRY_NP(kdi_slave_entry)
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp

	// Save the frame as for a normal entry, but make it clear it's a fake
	// by setting an invalid trapno
	__SAVE_REGS_EL1
	__SAVE_FRAME

	mov	x1, #~0
	str	x1, [sp, #REGOFF_TRAPNO]

	bl	kdi_get_cpusave
	bl	kdi_advance_crumb_pointer
	ldr	x2, [x0, #KRS_CURCRUMB]

	mov	x1, #KDI_CPU_STATE_SLAVE
	str	x1, [x2, #KRM_CPU_STATE]
	ldr	x1, [sp, #REGOFF_SP]
	str	x1, [x2, #KRM_SP]
	ldr	x1, [sp, #REGOFF_PC]
	str	x1, [x2, #KRM_PC]
	ldr	x1, [sp, #REGOFF_TRAPNO]
	str	x1, [x2, #KRM_TRAPNO]

	mov	x1, #KDI_CPU_STATE_SLAVE
	str	x1, [x0, #KRS_CPU_STATE]

	// cpusave in x0, regs in x1
	mov	x1, sp
	bl	kdi_save_common_state

	__RESTORE_REGS_EL1

	ldp	fp, lr, [sp], #16
	ret
	SET_SIZE(kdi_slave_entry)

	ENTRY_NP(kdi_pass_to_kernel)
	mov	x1, #KDI_CPU_STATE_NONE
	str	x1, [x0, #KRS_CPU_STATE]

	// Set up the frame as if we'd just come into the trap (again)
	// and call the kernel handler.
	ldr	x0, [x0, #KRS_GREGS]
	mov	sp, x0

	// We're going off to the normal handler, allow debug exceptions
	msr	DAIFClr, #DAIF_SETCLEAR_DEBUG
	b	from_current_el_sync_handler
	SET_SIZE(kdi_pass_to_kernel)
