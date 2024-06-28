/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
#include "assym.h"

	.data
	.globl t0stack
	.type t0stack, @object
	.size t0stack, DEFAULTSTKSZ
	.align MMU_PAGESHIFT
t0stack:
	.zero DEFAULTSTKSZ

	.globl t0
	.type t0, @object
	.size t0, MMU_PAGESIZE
	.align MMU_PAGESHIFT
t0:
	.zero MMU_PAGESIZE

	ENTRY(_start)
	mov	x1, #1
	msr	SPSel, x1
	ldr	x1, =t0stack
	ldr	x2, =DEFAULTSTKSZ
	add	x1, x1, x2
	mov	x29, x1
	mov	sp, x1

	bl	kobj_start
	mov	x0, sp
	bl	mlsetup
	bl	main
1:	b	1b

	SET_SIZE(_start)

	.balign 4096
	ENTRY(secondary_vec_start)
	// enable SMP cache affinity
	mrs	x0, midr_el1
	mov	w0, w0
	lsr	w1, w0, #24	// Implementator
	cmp	w1, #MIDR_IMPL_ARM
	b.ne	not_cortex_1
	ubfx	w1, w0, #16, #4	// Architecture
	cmp	w1, #0xF
	b.ne	not_cortex_1
	ubfx	w1, w0, #4, #12	// Part
	// XXXARM: The -A72 TRM is wishy washy about whether this is
	// necessary, but it is _possible_.
	cmp	w1, #MIDR_PART_ARM_CORTEX_A72
	b.eq	cortex_a72_1
	cmp	w1, #MIDR_PART_ARM_CORTEX_A57
	b.eq	cortex_a57_1
	// NB: Explicitly _not_ A55
	cmp	w1, #MIDR_PART_ARM_CORTEX_A53
	b.eq	cortex_a53_1
	b	not_cortex_1

cortex_a72_1:
cortex_a57_1:
cortex_a53_1:
	// CPUECTLR_EL1.SMPEN -> 1
	mrs	x0, s3_1_c15_c2_1
	tbnz	x0, #6, not_cortex_1
	orr	x0, x0, #(1<<6)
	msr	s3_1_c15_c2_1, x0
	dsb	sy
	isb
not_cortex_1:
	// invalidate cache (data/inst)
	bl	dcache_invalidate_all
	ic	iallu
	dsb	ish
	isb

	tlbi vmalle1is
	dsb	ish
	isb

	// copy to secondary_vec_end from cpu_startup_data
	adr	x0, secondary_vec_end
	ldr	x1, [x0, #STARTUP_MAIR]
	msr	mair_el1, x1
	ldr	x1, [x0, #STARTUP_TCR]
	msr	tcr_el1, x1
	ldr	x1, [x0, #STARTUP_TTBR0]
	msr	ttbr0_el1, x1
	ldr	x1, [x0, #STARTUP_TTBR1]
	msr	ttbr1_el1, x1
	isb
	ldr	x1, [x0, #STARTUP_SCTLR]
	msr	sctlr_el1, x1
	isb

	mrs	x0, CurrentEL
	cmp	x0, #0xc
	b.eq	el3

	cmp	x0, #0x8
	b.eq	el2

	b	el1
el3:
	b	el3

el2:
	bl	0f
	b	el1
0:
	adr	x0, .Lcnthctl_el2_val
	ldr	x0, [x0]
	msr	cnthctl_el2, x0
	msr	cntvoff_el2, xzr

	mov	x0, #CPTR_EL2_RES1
	msr	cptr_el2, x0

	msr	vttbr_el2, xzr
	msr	vbar_el2, xzr

	mrs	x0, midr_el1
	msr	vpidr_el2, x0

	mrs	x0, mpidr_el1
	msr	vmpidr_el2, x0

	mov	x0, #HCR_RW
	msr	hcr_el2, x0

	mov	x0, #SPSR_EL2_VAL
	msr	spsr_el2, x0
	msr	elr_el2, x30
	eret
	.balign 8
.Lcnthctl_el2_val:
	.quad (CNTHCTL_EL1PCEN | CNTHCTL_EL1PCTEN)
el1:
	// invalidate cache (data/inst)
	bl	dcache_invalidate_all
	ic	iallu
	dsb	sy
	isb

	tlbi vmalle1is
	dsb	sy
	isb

	// can access kernel data
	ldr	x0, =cpu
	mov	x1, #0

1:	cmp	x1, #NCPU
	b.eq	faild
	ldr	x2, [x0]	// x2 (struct cpu *)

	cbz	x2, 2f
	ldr	x3, [x2, #CPU_AFFINITY]
	mrs	x4, mpidr_el1
	ldr	x5, =MPIDR_AFF_MASK
	and	x4, x4, x5
	cmp	x3, x4
	b.eq	cpu_found

2:	add	x0, x0, #8
	add	x1, x1, #1
	b	1b

cpu_found:
	ldr	x1, [x2, #CPU_THREAD]
	msr	tpidr_el1, x1

	ldr	x29, [x1, #T_LABEL_X29]
	ldr	x30, [x1, #T_LABEL_PC]
	ldr	x0,  [x1, #T_LABEL_SP]
	mov	sp, x0
	// return entry point
	ret

faild:
	wfi
	b	faild

dcache_invalidate_all:
	mrs	x0, clidr_el1
	and	w3, w0, #0x07000000
	lsr	w3, w3, #23
	cbz	w3, 4f
	mov	w10, #0
	mov	w8, #1
0:	add	w2, w10, w10, lsr #1
	lsr	w1, w0, w2
	and	w1, w1, #0x7
	cmp	w1, #2
	b.lt	3f
	msr	csselr_el1, x10
	isb
	mrs	x1, ccsidr_el1
	and	w2, w1, #7
	add	w2, w2, #4
	ubfx	w4, w1, #3, #10
	clz	w5, w4
	lsl	w9, w4, w5
	lsl	w16, w8, w5
1:	ubfx	w7, w1, #13, #15
	lsl	w7, w7, w2
	lsl	w17, w8, w2
2:	orr	w11, w10, w9
	orr	w11, w11, w7
	dc	isw, x11
	subs	w7, w7, w17
	b.ge	2b
	subs	x9, x9, x16
	b.ge	1b
3:	add	w10, w10, #2
	cmp	w3, w10
	dsb	ish
	b.gt	0b
4:	ret
	.ltorg

	.globl secondary_vec_end
secondary_vec_end:
	.balign 4096
SET_SIZE(secondary_vec_start)
