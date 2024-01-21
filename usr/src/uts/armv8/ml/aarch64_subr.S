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
 * Copyright 2024 Michael van der Westhuizen
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/asm_linkage.h>
#include <sys/errno.h>
#include "assym.h"

#define THREADP(reg)			\
	   mrs reg, tpidr_el1

/*
 * int setjmp(label_t *lp)
 * void longjmp(label_t *lp)
 *
 * Setjmp and longjmp implement non-local gotos using state vectors
 * type label_t.
 */
	ENTRY(setjmp)
	mov	x1, sp
	stp	x30, x1,  [x0, #8*LABEL_REG_PC]
	stp	x19, x20, [x0, #8*LABEL_REG_X19]
	stp	x21, x22, [x0, #8*LABEL_REG_X21]
	stp	x23, x24, [x0, #8*LABEL_REG_X23]
	stp	x25, x26, [x0, #8*LABEL_REG_X25]
	stp	x27, x28, [x0, #8*LABEL_REG_X27]
	str	x29,      [x0, #8*LABEL_REG_X29]

	mov	w0, #0
	ret
	SET_SIZE(setjmp)

	ENTRY(longjmp)
	ldp	x30, x1,  [x0, #8*LABEL_REG_PC]
	ldp	x19, x20, [x0, #8*LABEL_REG_X19]
	ldp	x21, x22, [x0, #8*LABEL_REG_X21]
	ldp	x23, x24, [x0, #8*LABEL_REG_X23]
	ldp	x25, x26, [x0, #8*LABEL_REG_X25]
	ldp	x27, x28, [x0, #8*LABEL_REG_X27]
	ldr	x29,      [x0, #8*LABEL_REG_X29]
	mov	sp, x1
	mov	w0, #1
	ret
	SET_SIZE(longjmp)

/*
 * int on_fault(label_t *ljb)
 * void no_fault(void)
 *
 * Catch lofault faults. Like setjmp except it returns one
 * if code following causes uncorrectable fault. Turned off
 * by calling no_fault().
 */
	ENTRY(on_fault)
	THREADP(x1)
	dsb	sy
	adr	x2, .Lcatch_fault
	str	x0, [x1, #T_ONFAULT]
	str	x2, [x1, #T_LOFAULT]
	b	setjmp
.Lcatch_fault:
	THREADP(x1)
	ldr	x0, [x1, #T_ONFAULT]
	dsb	sy
	mov	x2, #0
	str	x2, [x1, #T_ONFAULT]
	str	x2, [x1, #T_LOFAULT]
	b	longjmp
	SET_SIZE(on_fault)

	ENTRY(no_fault)
	THREADP(x1)
	dsb	sy
	mov	x2, #0
	str	x2, [x1, #T_ONFAULT]
	str	x2, [x1, #T_LOFAULT]
	ret
	SET_SIZE(no_fault)

/*
 * void on_trap_trampoline(void)
 * int on_trap(on_trap_data_t *otp, uint_t prot)
 *
 * Default trampoline code for on_trap() (see <sys/ontrap.h>).  We just
 * do a longjmp(&curthread->t_ontrap->ot_jmpbuf) if this is ever called.
 */
	ENTRY(on_trap_trampoline)
	THREADP(x1)
	ldr	x0, [x1, #T_ONTRAP]
	add	x0, x0, #OT_JMPBUF
	b	longjmp
	SET_SIZE(on_trap_trampoline)


	ENTRY(on_trap)
	mov	x3, #0
	dsb	sy
	strh	w1, [x0, #OT_PROT]	/* otp->ot_prot = prot */
	strh	w3, [x0, #OT_TRAP]	/* otp->ot_trap = 0 */
	adr	x1, on_trap_trampoline	/* r1 = &on_trap_trampoline */
	str	x1, [x0, #OT_TRAMPOLINE] /* otp->ot_trampoline = r1 */
	str	x3, [x0, #OT_HANDLE]	/* otp->ot_handle = NULL */
	str	x3, [x0, #OT_PAD1]	/* otp->ot_pad1 = NULL */

	THREADP(x1)

	ldr	x2, [x1, #T_ONTRAP]
	cmp	x2, x0
	b.eq	0f
	str	x2, [x0, #OT_PREV]	/*   otp->ot_prev = r1->t_ontrap */
	str	x0, [x1, #T_ONTRAP]	/*   r1->t_ontrap = otp */
	dsb	sy
0:	add	x0, x0, #OT_JMPBUF
	b	setjmp
	SET_SIZE(on_trap)

/*
 * greg_t getfp(void)
 * return the current frame pointer
 */

	ENTRY(getfp)
	mov	x0, x29
	ret
	SET_SIZE(getfp)


	ENTRY(_insque)
	ldr	x9, [x1, #(0 * 8)]	// predp->forw
	str	x1, [x0, #(1 * 8)]	// entryp->back = predp
	str	x9, [x0, #(0 * 8)]	// entryp->forw = predp->forw
	str	x0, [x1, #(0 * 8)]	// predp->forw = entryp
	str	x0, [x9, #(1 * 8)]	// predp->forw->back = entryp
	ret
	SET_SIZE(_insque)

	ENTRY(_remque)
	ldr	x9,  [x0,  #(0 * 8)]	// entryp->forw
	ldr	x10, [x0,  #(1 * 8)]	// entryp->back
	str	x9,  [x10, #(0 * 8)]	// entryp->back->forw = entryp->forw
	str	x10, [x9,  #(1 * 8)]	// entryp->forw->back = entryp->back
	ret
	SET_SIZE(_remque)

	/*
	 * dtrace_icookie_t
	 * dtrace_interrupt_disable(void)
	 */
	ENTRY(dtrace_interrupt_disable)
	mrs	x0, daif
	and	x0, x0,  #DAIF_IRQ
	msr	daifset, #DAIF_SETCLEAR_IRQ
	ret
	SET_SIZE(dtrace_interrupt_disable)

	/*
	 * void
	 * dtrace_interrupt_enable(dtrace_icookie_t cookie)
	 */
	ENTRY(dtrace_interrupt_enable)
	ands	x0, x0, #DAIF_IRQ
	b.ne	0f
	msr	daifclr, #DAIF_SETCLEAR_IRQ
0:	ret
	SET_SIZE(dtrace_interrupt_enable)

	ENTRY(dtrace_membar_consumer)
	dmb	ish
	ret
	SET_SIZE(dtrace_membar_consumer)

	ENTRY(dtrace_membar_producer)
	dmb	ish
	ret
	SET_SIZE(dtrace_membar_producer)

	ENTRY_NP(return_instr)
	ret
	SET_SIZE(return_instr)

	ENTRY_NP(ftrace_interrupt_disable)
	mrs	x0, daif
	and	x0, x0,  #DAIF_IRQ
	msr	daifset, #DAIF_SETCLEAR_IRQ
	ret
	SET_SIZE(ftrace_interrupt_disable)

	ENTRY_NP(ftrace_interrupt_enable)
	ands	x0, x0, #DAIF_IRQ
	b.ne	0f
	msr	daifclr, #DAIF_SETCLEAR_IRQ
0:	ret
	SET_SIZE(ftrace_interrupt_enable)


#define STRIDE1 512
#define STRIDE2 576
	ENTRY(prefetch_page_w)
	add	x0, x0, #STRIDE1
	prfum	pstl1keep, [x0]
	prfum	pstl1keep, [x0, #(STRIDE2 - STRIDE1)]
	ret
        SET_SIZE(prefetch_page_w)

	ENTRY(prefetch_page_r)
	add	x0, x0, #STRIDE1
	prfum	pldl1keep, [x0]
	prfum	pldl1keep, [x0, #(STRIDE2 - STRIDE1)]
	ret
        SET_SIZE(prefetch_page_r)

#define	PREFETCH_Q_LEN 8
#define	SMAP_SIZE 80
#define SMAP_STRIDE (PREFETCH_Q_LEN * SMAP_SIZE)

	ENTRY(prefetch_smap_w)
	add	x0, x0, #SMAP_STRIDE
	prfum	pstl1keep, [x0]
	ret
        SET_SIZE(prefetch_smap_w)

	ENTRY(flush_data_cache_all)
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
	dc	cisw, x11
	subs	w7, w7, w17
	b.ge	2b
	subs	x9, x9, x16
	b.ge	1b
3:	add	w10, w10, #2
	cmp	w3, w10
	dsb	sy
	b.gt	0b
4:	ret
	SET_SIZE(flush_data_cache_all)

	ENTRY(clean_data_cache_all)
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
	dc	csw, x11
	subs	w7, w7, w17
	b.ge	2b
	subs	x9, x9, x16
	b.ge	1b
3:	add	w10, w10, #2
	cmp	w3, w10
	dsb	sy
	b.gt	0b
4:	ret
	SET_SIZE(clean_data_cache_all)

	/*
	 * void
	 * switch_sp_and_call(uint64_t a0, uint64_t a1,
	 *     void (*func)(uint64_t a0, uint64_t a1), void *newsp);
	 */
	ENTRY(switch_sp_and_call)
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	/*
	 * Set up the new stack - the old is in fp.
	 */
	mov	sp, x3
	/*
	 * Call the function
	 *
	 * argument a0 is already in x0
	 * argument a1 is already in x1
	 */
	blr	x2
	mov	sp, fp
	ldp	fp, lr, [sp], #16
	ret
	SET_SIZE(switch_sp_and_call)
