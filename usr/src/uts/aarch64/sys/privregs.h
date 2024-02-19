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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_PRIVREGS_H
#define	_SYS_PRIVREGS_H

#include <sys/controlregs.h>
#include <sys/kdi_regs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file describes the cpu's privileged register set, and
 * how the machine state is saved on the stack when a trap occurs.
 */

#ifndef _ASM

/*
 * This is NOT the structure to use for general purpose debugging;
 * see /proc for that.  This is NOT the structure to use to decode
 * the ucontext or grovel about in a core file; see <sys/regset.h>.
 *
 * This structure has repercussions in other parts of the system.  Note
 * especially that the KDI register sets must remain compatible with this
 * definition.
 */
struct regs {
	/*
	 * Extra frame for debuggers to follow through high level interrupts
	 * and system traps.  Set them to 0 to terminate stacktrace.
	 */
	greg_t	r_savfp;
	greg_t	r_savpc;

	greg_t	r_x0;
#define	r_r0	r_x0
	greg_t	r_x1;
#define	r_r1	r_x1
	greg_t	r_x2;
	greg_t	r_x3;
	greg_t	r_x4;
	greg_t	r_x5;
	greg_t	r_x6;
	greg_t	r_x7;
	greg_t	r_x8;
	greg_t	r_x9;
	greg_t	r_x10;
	greg_t	r_x11;
	greg_t	r_x12;
	greg_t	r_x13;
	greg_t	r_x14;
	greg_t	r_x15;
	greg_t	r_x16;
	greg_t	r_x17;
	greg_t	r_x18;
	greg_t	r_x19;
	greg_t	r_x20;
	greg_t	r_x21;
	greg_t	r_x22;
	greg_t	r_x23;
	greg_t	r_x24;
	greg_t	r_x25;
	greg_t	r_x26;
	greg_t	r_x27;
	greg_t	r_x28;
#define	r_fp	r_x29
	greg_t	r_x29;
#define	r_lr	r_x30
	greg_t	r_x30;
	greg_t	r_sp;
	greg_t	r_pc;
	greg_t	r_spsr;

	/*
	 * The following are not required for correctness in save/restore, but
	 * are essential for debugging
	 */
	greg_t r_tp;
	greg_t r_esr;
	greg_t r_far;
	greg_t r_trapno;
};

#ifdef _KERNEL
#define	lwptoregs(lwp)	((struct regs *)((lwp)->lwp_regs))
#define	USERMODE(spsr)	(((spsr) & PSR_M_MASK) == PSR_M_EL0t)

#endif /* _KERNEL */

#else	/* !_ASM */

#if defined(_MACHDEP)

#define	__SAVE_REGS_EL0				\
	sub	sp, sp, #REG_FRAME;		\
	stp	x0, x1, [sp, #REGOFF_X0];	\
	stp	x2, x3, [sp, #REGOFF_X2];	\
	stp	x4, x5, [sp, #REGOFF_X4];	\
	stp	x6, x7, [sp, #REGOFF_X6];	\
	stp	x8, x9, [sp, #REGOFF_X8];	\
	stp	x10, x11, [sp, #REGOFF_X10];	\
	stp	x12, x13, [sp, #REGOFF_X12];	\
	stp	x14, x15, [sp, #REGOFF_X14];	\
	stp	x16, x17, [sp, #REGOFF_X16];	\
	stp	x18, x19, [sp, #REGOFF_X18];	\
	stp	x20, x21, [sp, #REGOFF_X20];	\
	stp	x22, x23, [sp, #REGOFF_X22];	\
	stp	x24, x25, [sp, #REGOFF_X24];	\
	stp	x26, x27, [sp, #REGOFF_X26];	\
	stp	x28, x29, [sp, #REGOFF_X28];	\
	mrs	x16, sp_el0;			\
	stp	x30, x16, [sp, #REGOFF_X30];	\
	mrs	x17, elr_el1;			\
	mrs	x18, spsr_el1;			\
	stp	x17, x18, [sp, #REGOFF_PC];	\
	stp	fp, x17, [sp, #REGOFF_SAVFP];

#define	__SAVE_REGS_EL1				\
	sub	sp, sp, #REG_FRAME;		\
	stp	x0, x1, [sp, #REGOFF_X0];	\
	stp	x2, x3, [sp, #REGOFF_X2];	\
	stp	x4, x5, [sp, #REGOFF_X4];	\
	stp	x6, x7, [sp, #REGOFF_X6];	\
	stp	x8, x9, [sp, #REGOFF_X8];	\
	stp	x10, x11, [sp, #REGOFF_X10];	\
	stp	x12, x13, [sp, #REGOFF_X12];	\
	stp	x14, x15, [sp, #REGOFF_X14];	\
	stp	x16, x17, [sp, #REGOFF_X16];	\
	stp	x18, x19, [sp, #REGOFF_X18];	\
	stp	x20, x21, [sp, #REGOFF_X20];	\
	stp	x22, x23, [sp, #REGOFF_X22];	\
	stp	x24, x25, [sp, #REGOFF_X24];	\
	stp	x26, x27, [sp, #REGOFF_X26];	\
	stp	x28, x29, [sp, #REGOFF_X28];	\
	add	x16, sp, #REG_FRAME;		\
	stp	x30, x16, [sp, #REGOFF_X30];	\
	mrs	x17, elr_el1;			\
	mrs	x18, spsr_el1;			\
	stp	x17, x18, [sp, #REGOFF_PC];	\
	stp	fp, x17, [sp, #REGOFF_SAVFP];	\
	mrs	x17, tpidr_el1;			\
	mrs	x18, esr_el1;			\
	stp	x17, x18, [sp, #REGOFF_TP];	\
	mrs	x17, far_el1;			\
	lsr	w18, w18, #ESR_EC_SHIFT;	\
	stp	x17, x18, [sp, #REGOFF_FAR];

#define	__SAVE_FRAME		\
	mrs	x17, elr_el1;	\
	stp	fp, x17, [sp, #REGOFF_SAVFP];

#define	__TERMINATE_FRAME	\
	stp	xzr, xzr, [sp, #REGOFF_SAVFP];

#define	__RESTORE_REGS_EL0			\
	ldp	x17, x18, [sp, #REGOFF_PC];	\
	msr	elr_el1, x17;			\
	msr	spsr_el1, x18;			\
	ldp	x30, x16, [sp, #REGOFF_X30];	\
	msr	sp_el0, x16;			\
	ldp	x0, x1, [sp, #REGOFF_X0];	\
	ldp	x2, x3, [sp, #REGOFF_X2];	\
	ldp	x4, x5, [sp, #REGOFF_X4];	\
	ldp	x6, x7, [sp, #REGOFF_X6];	\
	ldp	x8, x9, [sp, #REGOFF_X8];	\
	ldp	x10, x11, [sp, #REGOFF_X10];	\
	ldp	x12, x13, [sp, #REGOFF_X12];	\
	ldp	x14, x15, [sp, #REGOFF_X14];	\
	ldp	x16, x17, [sp, #REGOFF_X16];	\
	ldp	x18, x19, [sp, #REGOFF_X18];	\
	ldp	x20, x21, [sp, #REGOFF_X20];	\
	ldp	x22, x23, [sp, #REGOFF_X22];	\
	ldp	x24, x25, [sp, #REGOFF_X24];	\
	ldp	x26, x27, [sp, #REGOFF_X26];	\
	ldp	x28, x29, [sp, #REGOFF_X28];	\
	add	sp, sp, #REG_FRAME

#define	__RESTORE_REGS_EL1			\
	ldp	x17, x18, [sp, #REGOFF_PC];	\
	msr	elr_el1, x17;			\
	msr	spsr_el1, x18;			\
	ldp	x30, xzr, [sp, #REGOFF_X30];	\
	ldp	x0, x1, [sp, #REGOFF_X0];	\
	ldp	x2, x3, [sp, #REGOFF_X2];	\
	ldp	x4, x5, [sp, #REGOFF_X4];	\
	ldp	x6, x7, [sp, #REGOFF_X6];	\
	ldp	x8, x9, [sp, #REGOFF_X8];	\
	ldp	x10, x11, [sp, #REGOFF_X10];	\
	ldp	x12, x13, [sp, #REGOFF_X12];	\
	ldp	x14, x15, [sp, #REGOFF_X14];	\
	ldp	x16, x17, [sp, #REGOFF_X16];	\
	ldp	x18, x19, [sp, #REGOFF_X18];	\
	ldp	x20, x21, [sp, #REGOFF_X20];	\
	ldp	x22, x23, [sp, #REGOFF_X22];	\
	ldp	x24, x25, [sp, #REGOFF_X24];	\
	ldp	x26, x27, [sp, #REGOFF_X26];	\
	ldp	x28, x29, [sp, #REGOFF_X28];	\
	add	sp, sp, #REG_FRAME

#endif	/* _MACHDEP */

#endif	/* !_ASM */

#include <sys/controlregs.h>

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_PRIVREGS_H */
