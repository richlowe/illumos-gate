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
 * Copyright 2026 Michael van der Westhuizen
 */

	.file	"smccc_impl.S"

/*
 * SMCCC low-level assembly trampolines.
 *
 * Four entry points cover the matrix of {SMC,HVC} x {32-bit,64-bit}:
 *   i_smccc_smc_call32 - SMC with w0..w7  (smccc32_args_t)
 *   i_smccc_smc_call64 - SMC with x0..x17 (smccc64_args_t)
 *   i_smccc_hvc_call32 - HVC with w0..w7  (smccc32_args_t)
 *   i_smccc_hvc_call64 - HVC with x0..x17 (smccc64_args_t)
 *
 * 32-bit trampolines take a pointer to smccc32_args_t (8 x uint32_t).
 * 64-bit trampolines take a pointer to smccc64_args_t (18 x uint64_t).
 *
 * Callee-saved x19 is used to hold the struct pointer across the call.
 *
 * This whole file is an SMCCC implementation detail and none of these
 * functions should be called from outside of smccc.c.
 */

#include <sys/asm_linkage.h>

/*
 * i_smccc_smc_call32(smccc32_args_t *args)
 *
 * 32-bit SMC: loads w0..w7 from args, issues SMC #0, stores w0..w7 back.
 */
	ENTRY(i_smccc_smc_call32)
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	stp	x19, x20, [sp, #-16]!
	mov	x19, x0			/* save args pointer */

	ldp	w0, w1, [x19, #0]
	ldp	w2, w3, [x19, #8]
	ldp	w4, w5, [x19, #16]
	ldp	w6, w7, [x19, #24]

	smc	#0

	stp	w0, w1, [x19, #0]
	stp	w2, w3, [x19, #8]
	stp	w4, w5, [x19, #16]
	stp	w6, w7, [x19, #24]

	ldp	x19, x20, [sp], #16
	ldp	x29, x30, [sp], #16
	ret
	SET_SIZE(i_smccc_smc_call32)

/*
 * i_smccc_smc_call64(smccc64_args_t *args)
 *
 * 64-bit SMC: loads x0..x17 from args, issues SMC #0, stores x0..x17 back.
 */
	ENTRY(i_smccc_smc_call64)
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	stp	x19, x20, [sp, #-16]!
	mov	x19, x0

	ldp	x0, x1, [x19, #0]
	ldp	x2, x3, [x19, #16]
	ldp	x4, x5, [x19, #32]
	ldp	x6, x7, [x19, #48]
	ldp	x8, x9, [x19, #64]
	ldp	x10, x11, [x19, #80]
	ldp	x12, x13, [x19, #96]
	ldp	x14, x15, [x19, #112]
	ldp	x16, x17, [x19, #128]

	smc	#0

	stp	x0, x1, [x19, #0]
	stp	x2, x3, [x19, #16]
	stp	x4, x5, [x19, #32]
	stp	x6, x7, [x19, #48]
	stp	x8, x9, [x19, #64]
	stp	x10, x11, [x19, #80]
	stp	x12, x13, [x19, #96]
	stp	x14, x15, [x19, #112]
	stp	x16, x17, [x19, #128]

	ldp	x19, x20, [sp], #16
	ldp	x29, x30, [sp], #16
	ret
	SET_SIZE(i_smccc_smc_call64)

/*
 * i_smccc_hvc_call32(smccc32_args_t *args)
 *
 * 32-bit HVC: loads w0..w7 from args, issues HVC #0, stores w0..w7 back.
 */
	ENTRY(i_smccc_hvc_call32)
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	stp	x19, x20, [sp, #-16]!
	mov	x19, x0

	ldp	w0, w1, [x19, #0]
	ldp	w2, w3, [x19, #8]
	ldp	w4, w5, [x19, #16]
	ldp	w6, w7, [x19, #24]

	hvc	#0

	stp	w0, w1, [x19, #0]
	stp	w2, w3, [x19, #8]
	stp	w4, w5, [x19, #16]
	stp	w6, w7, [x19, #24]

	ldp	x19, x20, [sp], #16
	ldp	x29, x30, [sp], #16
	ret
	SET_SIZE(i_smccc_hvc_call32)

/*
 * i_smccc_hvc_call64(smccc64_args_t *args)
 *
 * 64-bit HVC: loads x0..x17 from args, issues HVC #0, stores x0..x17 back.
 */
	ENTRY(i_smccc_hvc_call64)
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	stp	x19, x20, [sp, #-16]!
	mov	x19, x0

	ldp	x0, x1, [x19, #0]
	ldp	x2, x3, [x19, #16]
	ldp	x4, x5, [x19, #32]
	ldp	x6, x7, [x19, #48]
	ldp	x8, x9, [x19, #64]
	ldp	x10, x11, [x19, #80]
	ldp	x12, x13, [x19, #96]
	ldp	x14, x15, [x19, #112]
	ldp	x16, x17, [x19, #128]

	hvc	#0

	stp	x0, x1, [x19, #0]
	stp	x2, x3, [x19, #16]
	stp	x4, x5, [x19, #32]
	stp	x6, x7, [x19, #48]
	stp	x8, x9, [x19, #64]
	stp	x10, x11, [x19, #80]
	stp	x12, x13, [x19, #96]
	stp	x14, x15, [x19, #112]
	stp	x16, x17, [x19, #128]

	ldp	x19, x20, [sp], #16
	ldp	x29, x30, [sp], #16
	ret
	SET_SIZE(i_smccc_hvc_call64)
