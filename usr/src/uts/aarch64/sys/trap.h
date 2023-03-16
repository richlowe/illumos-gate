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
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved	*/

/*
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_TRAP_H
#define	_SYS_TRAP_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Trap type values, matching ARM exception codes
 *
 * Documented in:
 * Arm® Architecture Registers for A-profile architecture pp. 599
 *  ESR_EL1, Exception Syndrome Register (EL1)
 *
 * and pseudocode in:
 * Arm® A64 Instruction Set Architecture for A-profile architecture pp. 4508
 *   Library pseudocode for aarch64/exceptions/exceptions/AArch64.ExceptionClass
 */
#define	T_UNKNOWN		0x00	/* Unknown */
#define	T_WFx			0x01	/* WFI/WFE */
#define	T_CP15RT		0x03	/* AArch32 MCR/MRC, coproc = 0xf */
#define	T_CP15RRT		0x04	/* AArch32 MCRR/MRRC, coproc = 0xf */
#define	T_CP14RT		0x05	/* AArch32 MCR/MRC, coproc = 0xe */
#define	T_CP14DT		0x06	/* AArch32 LDC/STC, coproc = 0xe */
#define	T_SIMDFP_ACCESS		0x07	/* HCPTR-trapped access to SIMD/FPU */
#define	T_FPID			0x08	/* Access to SIMD/FPU ID register */
#define	T_PAC			0x09	/* Invalid PAC use */
#define	T_LDST64B		0x0a	/* Access to ST64B* and LD64B */
#define	T_CP14RRT		0x0c	/* AArch32 MCR or MRC access, */
					/*	coproc=0xe */
#define	T_BRANCH_TARGET		0x0d	/* BTI */
#define	T_ILLEGAL_STATE		0x0e	/* illegal execution state */
#define	T_SVC32			0x11	/* AArch32 supervisor call */
#define	T_HVC32			0x12	/* AArch32 hypervisor call */
#define	T_MONITOR_CALL32	0x13	/* AArch32 monitor call/SMC */
#define	T_SVC			0x15	/* AArch64 supervisor call */
#define	T_HVC			0x16	/* AArch64 hypervisor call */
#define	T_MONITOR_CALL		0x17	/* AArch64 monitor call/SMC */
#define	T_SYSTEM_REGISTER	0x18	/* MSR/MRS */
#define	T_SVE_ACCESS		0x19	/* HCPTR trapped access to SVE */
#define	T_ERET			0x1a	/* invalid ERET */
#define	T_TSTART_ACCESS		0x1b	/* TSTART access */
#define	T_PAC_FAIL		0x1c	/* PAC authentication failure */
#define	T_SME_ACCESS		0x1d	/* HCPTR trapped access to SME */
#define	T_GPC			0x1e	/* granule protection check */
#define	T_INSTRUCTION_ABORT	0x20	/* instruction or prefetch abort */
					/*	(lower EL) */
#define	T_INSTRUCTION_ABORT_EL	0x21	/* instruction or prefetch abort */
					/*	(same EL) */
#define	T_PC_ALIGNMENT		0x22	/* PC alignment */
#define	T_DATA_ABORT		0x24	/* data abort (lower EL) */
#define	T_NV2_DATA_ABORT	0x25	/* data abort from EL1 reported */
					/*	as from EL2 */
#define	T_SP_ALIGNMENT		0x26	/* SP alignment */
#define	T_MEMCPY_MEMSET		0x27	/* exception from CPY* / SET* */
#define	T_FP_EXCEPTION32	0x28	/* AArch32 IEEE trapped FP exception */
#define	T_FP_EXCEPTION		0x2c	/* AArch64 IEEE trapped FP exception */
#define	T_SERROR		0x2f	/* SError interrupt */
#define	T_BREAKPOINT		0x30	/* hardware breakpoint (lower EL) */
#define	T_BREAKPOINT_EL		0x31	/* hardware breakpoint (same EL) */
#define	T_SOFTWARE_STEP		0x32	/* software step (lower EL) */
#define	T_SOFTWARE_STEP_EL	0x33	/* software step (same EL) */
#define	T_WATCHPOINT		0x34	/* watchpoint (lower EL) */
#define	T_NV2_WATCHPOINT	0x35	/* watchpoint from EL1 reported */
					/*	as from EL2 */
#define	T_SOFTWARE_BREAKPOINT32	0x38	/* AArch32 software breakpoint */
#define	T_VECTOR_CATCH		0x3a	/* AArch32 Vector Catch */
#define	T_SOFTWARE_BREAKPOINT	0x3c	/* AArch64 software breakpoint */
#define	T_PMU			0x3d	/* PMU exception */

/*
 * Pseudo traps.
 */
#define	T_INTERRUPT		0x100
#define	T_FAULT			0x200
#define	T_AST			0x400
#define	T_SYSCALL		0x180

/*
 *  Definitions for fast system call subfunctions
 */
#define	T_GETHRTIME	1	/* Get high resolution time		*/
#define	T_GETHRVTIME	2	/* Get high resolution virtual time	*/
#define	T_GETHRESTIME	3	/* Get high resolution time		*/
#define	T_GETLGRP	4	/* Get home lgrpid			*/

#define	T_LASTFAST	4	/* Last valid subfunction		*/

#define	T_DTRACE_PID	0xc000
#define	T_DTRACE_RET	0xc001

/*
 * Exception Status Registers
 *
 * Arm® Architecture Registers for A-profile architecture pp. 599 (et seq)
 */
/* [55:24] Instruction Specific Syndrome 2 */
#define	ESR_ISS2_SHIFT	32
#define	ESR_ISS2_MASK	(0x3fffff << ESR_ISS2_SHIFT)
#define	ESR_ISS2(esr)	((esr & ESR_ISS2_MASK) >> ESR_ISS2_SHIFT)

/* [31:26] Exception code */
#define	ESR_EC_SHIFT	26
#define	ESR_EC_MASK	(0x3f << ESR_EC_SHIFT)
#define	ESR_EC(esr)	((esr & ESR_EC_MASK) >> ESR_EC_SHIFT)

/* [25] instruction length */
#define	ESR_IL_SHIFT	25
#define	ESR_IL_MASK	(0x1 << ESR_IL_SHIFT)
#define	ESR_IL(esr)	((esr & ESR_IL_MASK) >> ESR_IL_SHIFT)

/* [24:0] Instruction Specific Syndrome */
#define	ESR_ISS_SHIFT	0
#define	ESR_ISS_MASK	(0x1ffffff << ESR_ISS_SHIFT)
#define	ESR_ISS(esr)	((esr & ESR_ISS_MASK) >> ESR_ISS_SHIFT)

/*
 * Instruction Specific Syndromes
 *
 * Arm® Architecture Registers for A-profile architecture pp. 608 (et seq)
 */

/*
 * ISS encoding for WF* instructions pp. 609
 */
/* [24] cond valid? */
#define	ISS_WF_CV_SHIFT		24
#define	ISS_WF_CV_MASK		(0x1 << ISS_WF_CV_SHIFT)
#define	ISS_WF_CV(iss)		((iss & ISS_WF_CV_MASK) >> ISS_WF_CV_SHIFT)
/* [23:20] cond */
#define	ISS_WF_COND_SHIFT	20
#define	ISS_WF_COND_MASK	(0xf << ISS_WF_COND_SHIFT)
#define	ISS_WF_COND(iss)	((iss & ISS_WF_COND_MASK) >> ISS_WF_COND_SHIFT)

/* [9:5] register number */
#define	ISS_WF_RN_SHIFT		5
#define	ISS_WF_RN_MASK		(0x1f << ISS_WF_RN_SHIFT)
#define	ISS_WF_RN(iss)		((iss & ISS_WF_RN_MASK) >> ISS_WF_RN_SHIFT)

/* [2] RN valid? */
#define	ISS_WF_RV_SHIFT		2
#define	ISS_WF_RV_MASK		(0x1 << ISS_WF_RV_SHIFT)
#define	ISS_WF_RV(iss)		((iss & ISS_WF_RV_MASK) >> ISS_WF_RV_SHIFT)

/* [1:0] trapped instruction */
#define	ISS_WF_TI_SHIFT		0
#define	ISS_WF_TI_MASK		0x3
#define	ISS_WF_TI(iss)		((iss & ISS_WF_TI_MASK) >> ISS_WF_TI_SHIFT)

#define	ISS_WF_INSN_WFI		0x0
#define	ISS_WF_INSN_WFE		0x1
#define	ISS_WF_INSN_WFIT	0x2
#define	ISS_WF_INSN_WFET	0x4

/*
 * ISS encoding for MCR/MRC pp. 611
 */
/* [24] cond valid? */
#define	ISS_MCR_CV_SHIFT	24
#define	ISS_MCR_CV_MASK		(1 << ISS_MCR_CV_SHIFT)
#define	ISS_MCR_CV(iss)		((iss & ISS_MCR_CV_MASK) >> ISS_MCR_CV_SHIFT)

/* [23:20] cond */
#define	ISS_MCR_COND_SHIFT	20
#define	ISS_MCR_COND_MASK	(0xf << ISS_MCR_COND_SHIFT)
#define	ISS_MCR_COND(iss)	((iss & ISS_MCR_COND_MASK) & ISS_MCR_COND_SHIFT)

/* [19:17] OPC2 from insn */
#define	ISS_MCR_OPC2_SHIFT	17
#define	ISS_MCR_OPC2_MASK	(0x7 << ISS_MCR_OPC2_SHIFT)
#define	ISS_MCR_OPC2(iss)	((iss & ISS_MCR_OPC2_MASK) >> \
    ISS_MCR_OPC2_SHIFT)

/* [16:14] OPC1 from insn */
#define	ISS_MCR_OPC1_SHIFT	14
#define	ISS_MCR_OPC1_MASK	(0x7 << ISS_MCR_OPC1_SHIFT)
#define	ISS_MCR_OPC1(is)	((iss & ISS_MCR_OPC1_MASK) >> \
    ISS_MCR_OPC1_SHIFT)

/* [13:10] CRn from insn */
#define	ISS_MCR_CRN_SHIFT	10
#define	ISS_MCR_CRN_MASK	(0xf << ISS_MCR_CRN_SHIFT)
#define	ISS_MCR_CRN(iss)	((iss & ISS_MCR_CRN_MASK) >> ISS_MCR_CRN_SHIFT)

/* [9:5] Rt from insn */
#define	ISS_MCR_RT_SHIFT	5
#define	ISS_MCR_RT_MASK		(0x1f << ISS_MCR_RT_SHIFT)
#define	ISS_MCR_RT(iss)		((iss & ISS_MCR_RT_MASK) >> ISS_MCR_CRT_SHIFT)

/* [4:1] CRm from insn */
#define	ISS_MCR_CRM_SHIFT	1
#define	ISS_MCR_CRM_MASK	(0xf << ISS_MCR_CRM_SHIFT)
#define	ISS_MCR_CRM(iss)	((iss & ISS_MCR_CRM_MASK) >> ISS_MCR_CRM_SHIFT)

/* [0] direction of trapped insn */
#define	ISS_MCR_DIRECTION_SHIFT	0
#define	ISS_MCR_DIRECTION_MASK	1
#define	ISS_MCR_DIRECTION(iss)	((iss & ISS_MCR_DIRECTION_MASK) >> \
    ISS_MCR_DIRECTION_SHIFT)

#define	ISS_MCR_DIRECTION_WRITE	0
#define	ISS_MC_DIRECTION_READ	1

/*
 * ISS encoding for LD64B/ST64B* pp. 614
 * NOTE: These are values, there are no masks
 */
#define	ISS_LD64B_ST64BV	0x0 /* ST64BV trapped */
#define	ISS_LD64B_ST64BV0	0x1 /* ST64BV0 trapped */
#define	ISS_LD64B_LD64B		0x2 /* LD64B/ST64B trapped */

/*
 * ISS encoding for MCRR/MRRC access pp. 615
 */
/* [24] cond valid? */
#define	ISS_MCRR_CV_SHIFT	24
#define	ISS_MCRR_CV_MASK	(0x1 << ISS_MCRR_CV_SHIFT))
#define	ISS_MCRR_CV(iss)	((iss & ISS_MCRR_CV_MASK) >> ISS_MCRR_CV_SHIFT)

/* [23:20] cond (constant on aarch64) */
#define	ISS_MCRR_COND_SHIFT	20
#define	ISS_MCRR_COND_MASK	(0xf << ISS_MCRR_COND_SHIFT)
#define	ISS_MCRR_COND(iss)	((iss & ISS_MCRR_COND_MASK) >> \
    ISS_MCRR_COND_SHIFT)

/* [19:16] OPC1 from insn */
#define	ISS_MCRR_OPC1_SHIFT	16
#define	ISS_MCRR_OPC1_MASK	(0xf << ISS_MCRR_OPC1_SHIFT)
#define	ISS_MCRR_OPC1(iss)	((iss & ISS_MCRR_OPC1_MASK) >> \
    ISS_MCRR_OPC1_SHIFT)

/* [14:10]	RT2 from insn */
#define	ISS_MCRR_RT2_SHIFT	10
#define	ISS_MCRR_RT2_MASK	(0x1f << ISS_MCRR_RT2_SHIFT)
#define	ISS_MCRR_RT2(iss)	((iss & ISS_MCRR_RT2_MASK) >> \
    ISS_MCRR_RT2_SHIFT)

/* [9:5] RT from insn */
#define	ISS_MCRR_RT_SHIFT	5
#define	ISS_MCRR_RT_MASK	(0x1f << ISS_MCRR_RT_SHIFT)
#define	ISS_MCRR_RT(iss)	((iss & ISS_MCRR_RT_MASK) >> ISS_MCRR_RT_SHIFT)

/* [4:1] CRm from insn */
#define	ISS_MCRR_CRM_SHIFT	1
#define	ISS_MCRR_CRM_MASK	(0xf << ISS_MCRR_CRM_SHIFT)
#define	ISS_MCRR_CRM(iss)	((iss & ISS_MCRR_CRM_MASK) >> \
    ISS_MCRR_CRM_SHIFT)

/* [0] direction */
#define	ISS_MCRR_DIRECTION_SHIFT	0
#define	ISS_MCRR_DIRECTION_MASK		1
#define	ISS_MCRR_DIRECTION(iss)		((iss & ISS_MCRR_DIRECTION_MASK) >> \
    ISS_MCRR_DIRECTION_SHIFT)

#define	ISS_MCRR_DIRECTION_WRITE	0
#define	ISS_MCRR_DIRECTION_READ		1

/*
 * ISS encoding for LDC/STC pp. 618
 */
/* [24] cond valid? */
#define	ISS_LDC_CV_SHIFT	24
#define	ISS_LDC_CV_MASK		(0x1 << ISS_LDC_CV_SHIFT)
#define	ISS_LDC_CV(iss)		((iss & ISS_LDC_CV_MASK) >> ISS_LDC_CV_SHIFT)

/* [23:20] cond */
#define	ISS_LDC_COND_SHIFT	20
#define	ISS_LDC_COND_MASK	(0xf << ISS_LDC_COND_SHIFT)
#define	ISS_LDC_COND(iss)	((iss & ISS_LDC_COND_MASK) >> \
    ISS_LDC_COND_SHIFT)

/* [19:12] imm8 from insn */
#define	ISS_LDC_IMM8_SHIFT	12
#define	ISS_LDC_IMM8_MASK	(0xff << ISS_LDC_IMM8_SHIFT)
#define	ISS_LDC_IMM8(iss)	((iss & ISS_LDC_IMM8_MASK) >> \
    ISS_LDC_IMM8_SHIFT)

/* [9:5] Rn from insn */
#define	ISS_LDC_RN_SHIFT	5
#define	ISS_LDC_RN_MASK		(0x1f << ISS_LDC_RN_SHIFT)
#define	ISS_LDC_RN(iss)		((iss & ISS_LDC_RN_MASK) >> ISS_LDC_RN_SHIFT)

/* [4] offset add or sub? */
#define	ISS_LDC_OFFSET_SHIFT	4
#define	ISS_LDC_OFFSET_MASK	(1 << ISS_LDC_OFFSET_SHIFT)
#define	ISS_LDC_OFFSET(iss)	((iss & ISS_LDC_OFFSET_MASK) >> \
    ISS_LDC_OFFSET_SHIFT)

/* [3:1] addressing mode */
#define	ISS_LDC_AM_SHIFT	1
#define	ISS_LDC_AM_MASK		(7 << ISS_LDC_AM_SHIFT)
#define	ISS_LDC_AM(iss)		((iss & ISS_LDC_AM_MASK) >> ISS_LDC_AM_SHIFT)

/* [1] direction */
#define	ISS_LDC_DIRECTION_SHIFT	0
#define	ISS_LDC_DIRECTION_MASK	1
#define	ISS_LDC_DIRECTION(iss)	((iss & ISS_LDC_DIRECTION_MASK) >> \
    ISS_LDC_DIRECTION_SHIFT)

#define	ISS_LDC_OFFSET_SUB	0
#define	ISS_LDC_OFFSET_ADD	1

#define	ISS_LDC_AM_IMM_UNINDEXED	0x0
#define	ISS_LDC_AM_IMM_POST_INDEXED	0x1
#define	ISS_LDC_AM_IMM_OFFSET		0x2
#define	ISS_LDC_AM_IMM_PRE_INDEXED	0x3
/* 0x4 and 0x6 reserved */

#define	ISS_LDC_DIRECTION_WRITE	0
#define	ISS_LDC_DIRECTION_READ	1

/*
 * ISS encoding for SVE/SIMD/FP access
 */
/* [24]	cond valid? */
#define	ISS_SIMDFPACCESS_CV_SHIFT	24
#define	ISS_SIMDFPACCESS_CV_MASK	(0x1 << ISS_SIMDFPACCESS_CV_SHIFT)
#define	ISS_SIMDFPACCESS_CV(iss)	((iss & ISS_SIMDFPACCESS_CV_MASK) >> \
    ISS_SIMDFPACCESS_CV_SHIFT)

/* [23:19] cond */
#define	ISS_SIMDFPACCESS_COND_SHIFT	20
#define	ISS_SIMDFPACCESS_COND_MASK	(0xf << ISS_SIMDFPACCESS_COND_SHIFT)
#define	ISS_SIMDFPACCESS_COND(iss)	((iss & ISS_SIMDFPACCESS_COND_MASK) >> \
    ISS_SIMDFPACCESS_COND_SHIFT)

/*
 * ISS encoding for PMU exception pp. 622
 */
/* [0] synchronous? */
#define	ISS_PMU_SYNC_SHIFT	0
#define	ISS_PMU_SYNC_MASK	1
#define	ISS_PMU_SYNC(iss)	((iss & ISS_PMU_SYNC_MASK) >> \
    ISS_PMU_SYNC_SHIFT)

/*
 * ISS encoding for the Memory Copy/Set instructions pp. 622
 */
/* [24]	meminst */
#define	ISS_MEMx_MEMINST_SHIFT	24
#define	ISS_MEMx_MEMINST_MASK	(0x1 << ISS_MEMx_MEMINST_SHIFT))
#define	ISS_MEMx_MEMINST(iss)	((iss & ISS_MEMx_MEMINST) >> \
    ISS_MEMx_MEMINST_SHIFT)

/* [23] is a SETG* insn? */
#define	ISS_MEMx_ISSETG_SHIFT	23
#define	ISS_MEMx_ISSETG_MASK	(0x1 << ISS_MEMx_ISSETG_SHIFT)
#define	ISS_MEMx_ISSETG(iss)	((iss & ISS_MEMx_ISSETG_MASK) >> \
    ISS_MEMx_ISSETG_SHIFT)

/* [22:19] options from the insn */
#define	ISS_MEMx_OPTIONS_SHIFT	19
#define	ISS_MEMx_OPTIONS_MASK	(0xf << ISS_MEMx_OPTIONS_SHIFT)
#define	ISS_MEMx_OPTIONS(iss)	((iss & ISS_MEMx_OPTIONS_MASK) >> \
    ISS_MEMx_OPTIONS_SHIFT)

/* [18]	epilogue insn? (CPYE*, SETE* etc.) */
#define	ISS_MEMx_FROM_EPILOGUE_SHIFT	18
#define	ISS_MEMx_FROM_EPILOGUE_MASK	(0x1 << ISS_MEMx_FROM_EPILOGUE_SHIFT)
#define	ISS_MEMx_FROM_EPILOGUE(iss)	\
	((iss & ISS_MEMx_FROM_EPILOGUE_MASK) >> ISS_MEMx_FROM_EPILOGUE_SHIFT)

/* [17]	algorithm option? */
#define	ISS_MEMx_WRONG_OPTION_SHIFT	17
#define	ISS_MEMx_WRONG_OPTION_MASK	(0x1 << ISS_MEMx_WRONG_OPTION_SHIFT)
#define	ISS_MEMx_WRONG_OPTION(iss)	((iss & ISS_MEMx_WRONG_OPTION_MASK) >> \
    ISS_MEMx_WRONG_OPTION_SHIFT)

/* [16]	algorithm from PSTATE.C? */
#define	ISS_MEMx_OPTIONA_SHIFT	16
#define	ISS_MEMx_OPTIONA_MASK	(0x1 << ISS_MEMx_OPTIONA_SHIFT)
#define	ISS_MEMx_OPTIONA(iss)	((iss & ISS_MEMx_OPTIONA_MASK) >> \
    ISS_MEMx_OPTIONA_SHIFT)

/* [14:10] destination */
#define	ISS_MEMx_DESTREG_SHIFT	10
#define	ISS_MEMx_DESTREG_MASK	(0x1f << ISS_MEMx_DESTREG_SHIFT)
#define	ISS_MEMx_DESTREG(iss)	((iss & ISS_MEMx_DESTREG_MASK) >> \
    ISS_MEMx_DESTREG_SHIFT)

/* [9:5] source */
#define	ISS_MEMx_SRCREG_SHIFT	5
#define	ISS_MEMx_SRCREG_MASK	(0x1f << ISS_MEMx_SRCREG_SHIFT)
#define	ISS_MEMx_SRCREG(iss)	((iss & ISS_MEMx_SRCREG_MASK) >> \
    ISS_MEMx_SRCREG_SHIFT)

/* [4:0] size */
#define	ISS_MEMx_SIZEREG_SHIFT	0
#define	ISS_MEMx_SIZEREG_MASK	0x1f
#define	ISS_MEMx_SIZEREG(iss)	((iss & ISS_MEMx_SIZEREG_MASK) >> \
    ISS_MEMx_SIZEREG_SHIFT)

#define	ISS_MEMx_CPY_INST	0
#define	ISS_MEMx_SET_INST	1

/*
 * ISS encoding for HVC or SVC pp. 624
 */
/* [15:0] immediate field from instruction */
#define	ISS_VC_IMM16_SHIFT	0
#define	ISS_VC_IMM16_MASK	0xffff
#define	ISS_VC_IMM16(iss)	((iss & ISS_VC_IMM16_MASK) >> \
    ISS_VC_IMM16_SHIFT)

/*
 * ISS encoding for MSR/MRS pp. 625
 */
/* [21:20] Op0 from the insn */
#define	ISS_MSR_OP0_SHIFT	20
#define	ISS_MSR_OP0_MASK	(0x3 << ISS_MSR_OP0_SHIFT)
#define	ISS_MSR_OP0(iss)	((iss & ISS_MSR_OP0_MASK) >> ISS_MSR_OP0_SHIFT)

/* [19:17] Op2 from the insn */
#define	ISS_MSR_OP2_SHIFT	17
#define	ISS_MSR_OP2_MASK	(0x7 << ISS_MSR_OP2_SHIFT)
#define	ISS_MSR_OP2(iss)	((iss & ISS_MSR_OP2_MASK) >> ISS_MSR_OP2_SHIFT)

/* [16:14] Op1 from the insn */
#define	ISS_MSR_OP1_SHIFT	14
#define	ISS_MSR_OP1_MASK	(0x7 << ISS_MSR_OP1_SHIFT)
#define	ISS_MSR_OP1(iss)	((iss & ISS_MSR_OP1_MASK) >> ISS_MSR_OP1_SHIFT)

/* [13:10] CRn from the insn */
#define	ISS_MSR_CRN_SHIFT	10
#define	ISS_MSR_CRN_MASK	(0xf << ISS_MSR_CRN_SHIFT)
#define	ISS_MSR_CRN(iss)	((iss & ISS_MSR_CRN_MASK) >> ISS_MSR_CRN_SHIFT)

/* [9:5] Rt from the insn */
#define	ISS_MSR_RT_SHIFT	5
#define	ISS_MSR_RT_MASK		(0x1f << ISS_MSR_RT_SHIFT)
#define	ISS_MSR_RT(iss)		((iss & ISS_MSR_RT_MASK) >> ISS_MSR_RT_SHIFT)

/* [4:1] CRm from the insn */
#define	ISS_MSR_CRM_SHIFT	1
#define	ISS_MSR_CRM_MASK	(0xf << ISS_MSR_CRM_SHIFT)
#define	ISS_MSR_CRM(iss)	((iss & ISS_MSR_CRM_MASK) >> ISR_MSR_CRM_SHIFT)

/* [0] direction */
#define	ISS_MSR_DIRECTION_SHIFT	0
#define	ISS_MSR_DIRECTION_MASK	1
#define	ISS_MSR_DIRECTION(iss)	((iss & ISS_MSR_DIRECTION_MASK) >> \
    ISS_MSR_DIRECTION_SHIFT)

#define	ISS_MSR_DIRECTION_WRITE	0
#define	ISS_MSR_DIRECTION_READ	1

/*
 * ISS encoding for MSRR/MRRS pp. 628
 */
/* [21:20] Op0 from the insn */
#define	ISS_MSRR_OP0_SHIFT	20
#define	ISS_MSRR_OP0_MASK	(0x3 << ISS_MSRR_OP0_SHIFT)
#define	ISS_MSRR_OP0(iss)	((iss & ISS_MSRR_OP0_MASK) >> \
    ISS_MSRR_OP0_SHIFT)

/* [19:17] Op2 from the insn */
#define	ISS_MSRR_OP2_SHIFT	17
#define	ISS_MSRR_OP2_MASK	(0x7 << ISS_MSRR_OP2_SHIFT)
#define	ISS_MSRR_OP2(iss)	((iss & ISS_MSRR_OP2_MASK) >> \
    ISS_MSRR_OP2_SHIFT)

/* [16:14] Op1 from the insn */
#define	ISS_MSRR_OP1_SHIFT	14
#define	ISS_MSRR_OP1_MASK	(0x7 << ISS_MSRR_OP1_SHIFT)
#define	ISS_MSRR_OP1(iss)	((iss & ISS_MSRR_OP1_MASK) >> \
    ISS_MSRR_OP1_SHIFT)

/* [13:10] CRn from the insn */
#define	ISS_MSRR_CRN_SHIFT	10
#define	ISS_MSRR_CRN_MASK	(0xf << ISS_MSRR_CRN_SHIFT)
#define	ISS_MSRR_CRN(iss)	((iss & ISS_MSRR_CRN_MASK) >> \
    ISS_MSRR_CRN_SHIFT)

/* [9:6] Rt from the insn */
#define	ISS_MSRR_RT_SHIFT	6
#define	ISS_MSRR_RT_MASK	(0xf << ISS_MSRR_RT_SHIFT)
#define	ISS_MSRR_RT(iss)	((iss & ISS_MSRR_RT_MASK) >> ISS_MSRR_RT_SHIFT)

/* [4:1] CRm from the insn */
#define	ISS_MSRR_CRM_SHIFT	1
#define	ISS_MSRR_CRM_MASK	(0xf << ISS_MSRR_CRM_SHIFT)
#define	ISS_MSRR_CRM(iss)	((iss & ISS_MSRR_CRM_MASK) >> \
    ISS_MSRR_CRM_SHIFT)

/* [0] direction */
#define	ISS_MSRR_DIRECTION_SHIFT	0
#define	ISS_MSRR_DIRECTION_MASK		1
#define	ISS_MSRR_DIRECTION(iss)		((iss & ISS_MSRR_DIRECTION_MASK) >> \
    ISS_MSRR_DIRECTION_SHIFT)

#define	ISS_MSRR_DIRECTION_WRITE	0
#define	ISS_MSRR_DIRECTION_READ		1

/*
 * ISS encoding for instruction abort pp. 629
 */
/* [12:11] Synch error type */
#define	ISS_IABORT_SET_SHIFT	11
#define	ISS_IABORT_SET_MASK	(0x3 << ISS_IABORT_SET_SHIFT)
#define	ISS_IABORT_SET(iss)	((iss & ISS_IABORT_SET_MASK) >> \
    ISS_IABORT_SET_SHIFT)

/* [10] FAR not valid */
#define	ISS_IABORT_FNV_SHIFT	10
#define	ISS_IABORT_FNV_MASK	(0x1 << ISS_IABORT_FNV_SHIFT)
#define	ISS_IABORT_FNV(iss)	((iss & ISS_IABORT_FNV_MASK) >> \
    ISS_IABORT_FNV_SHIFT)

/* [9] External abort? */
#define	ISS_IABORT_EA_SHIFT	9
#define	ISS_IABORT_EA_MASK	(0x1 << ISS_IABORT_EA_SHIFT)
#define	ISS_IABORT_EA(iss)	((iss & ISS_IABORT_EA_MASK) >> \
    ISS_IABORT_EA_SHIFT)

/* [7]	S2 fault S1 page table walk? */
#define	ISS_IABORT_S1PTW_SHIFT	7
#define	ISS_IABORT_S1PTW_MASK	(0x1 << ISS_IABORT_S1PTW_SHIFT)
#define	ISS_IABORT_S1PTW(iss)	((iss & ISS_IABORT_S1PTW_MASK) >> \
    ISS_IABORT_S1PTW_SHIFT)

/* [6:0] instruction fault status code */
#define	ISS_IABORT_IFSC_SHIFT	0
#define	ISS_IABORT_IFSC_MASK	0x3f
#define	ISS_IABORT_IFSC(iss)	((iss & ISS_IABORT_IFSC_MASK) >> \
    ISS_IABORT_IFSC_SHIFT)

#define	ISS_IABORT_SET_UER	0 /* Unrecoverable */
#define	ISS_IABORT_SET_UC	2 /* Uncontainable */
#define	ISS_IABORT_SET_UEO	3 /* Restartable */

/* We spell this out, unlike other bools, to help clarify it being backwards */
#define	ISS_IABORT_FAR_VALID		0
#define	ISS_IABORT_FAR_NOT_VALID	1

typedef enum {
	ISS_IABORT_IFSC_ADDRSIZE_L0	= 0x0,	/* Address size, level 0 */
	ISS_IABORT_IFSC_ADDRSIZE_L1	= 0x1,	/* Address size, level 1 */
	ISS_IABORT_IFSC_ADDRSIZE_L2	= 0x2,	/* Address size, level 2 */
	ISS_IABORT_IFSC_ADDRSIZE_L3	= 0x3,	/* Address size, level 3 */
	ISS_IABORT_IFSC_TRANS_L0	= 0x4,	/* Translation, level 0 */
	ISS_IABORT_IFSC_TRANS_L1	= 0x5,	/* Translation, level 1 */
	ISS_IABORT_IFSC_TRANS_L2	= 0x6,	/* Translation, level 2 */
	ISS_IABORT_IFSC_TRANS_L3	= 0x7,	/* Translation, level 3 */
	ISS_IABORT_IFSC_ACCESS_L0	= 0x8,	/* Access flag, level 0 */
	ISS_IABORT_IFSC_ACCESS_L1	= 0x9,	/* Access flag, level 1 */
	ISS_IABORT_IFSC_ACCESS_L2	= 0xa,	/* Access flag, level 2 */
	ISS_IABORT_IFSC_ACCESS_L3	= 0xb,	/* Access flag, level 3 */
#define	ISS_IABORT_IFSC_ACCESS(ifsc)	((ifsc >= 0x8) && (ifsc <= 0xb))
	ISS_IABORT_IFSC_PERM_L0		= 0xc,	/* Permission, level 0 */
	ISS_IABORT_IFSC_PERM_L1		= 0xd,	/* Permission, level 1 */
	ISS_IABORT_IFSC_PERM_L2		= 0xe,	/* Permission, level 2 */
	ISS_IABORT_IFSC_PERM_L3		= 0xf,	/* Permission, level 3 */
#define	ISS_IABORT_IFSC_PERM(ifsc)	((ifsc >= 0xc) && (ifsc <= 0xf))

	ISS_IABORT_IFSC_SYNCH_EXT	= 0x10,	/* Synchronous External abort */
						/*	(not trans) */
/* It seems there is no documented 0x11 */
	ISS_IABORT_IFSC_SYNCH_EXT_LN2	= 0x12,	/* Synchronous External abort */
						/*	(not trans) level -2 */
	ISS_IABORT_IFSC_SYNCH_EXT_LN1	= 0x13,	/* Synchronous External abort */
						/*	level -1 */
	ISS_IABORT_IFSC_SYNCH_EXT_L0	= 0x14,	/* Synchronous External abort */
						/*	level 0 */
	ISS_IABORT_IFSC_SYNCH_EXT_L1	= 0x15,	/* Synchronous External abort */
						/*	level 1 */
	ISS_IABORT_IFSC_SYNCH_EXT_L2	= 0x16,	/* Synchronous External abort */
						/*	level 2 */
	ISS_IABORT_IFSC_SYNCH_EXT_L3	= 0x17,	/* Synchronous External abort */
						/*	 level 3 */
	ISS_IABORT_IFSC_PARECC		= 0x18,	/* Parity or ECC error */
						/*	(not trans) */
/* It seems there are no documented 0x19..0x1a */
	ISS_IABORT_IFSC_PARECC_LN1	= 0x1b,	/* Parity or ECC level -1 */
	ISS_IABORT_IFSC_PARECC_L0	= 0x1c,	/* Parity or ECC level 0 */
	ISS_IABORT_IFSC_PARECC_L1	= 0x1d,	/* Parity or ECC level 1 */
	ISS_IABORT_IFSC_PARECC_L2	= 0x1e,	/* Parity or ECC level 2 */
	ISS_IABORT_IFSC_PARECC_L3	= 0x1f,	/* Parity or ECC level 3 */
/* It seems there is no documented 0x20..0x21 */
	ISS_IABORT_IFSC_GPF_LN2		= 0x22,	/* Granule PF level -2 */
	ISS_IABORT_IFSC_GPF_LN1		= 0x23,	/* Granule PF level -1 */
	ISS_IABORT_IFSC_GPF_L0		= 0x24,	/* Granule PF level 0 */
	ISS_IABORT_IFSC_GPF_L1		= 0x25,	/* Granule PF level 1 */
	ISS_IABORT_IFSC_GPF_L2		= 0x26,	/* Granule PF level 2 */
	ISS_IABORT_IFSC_GPF_L3		= 0x27,	/* Granule PF level 3 */
	ISS_IABORT_IFSC_GPF		= 0x28,	/* Granule PF (not trans)  */
	ISS_IABORT_IFSC_ADDRSIZE_LN1	= 0x29,	/* Address size, level -1 */
	ISS_IABORT_IFSC_TRANS_LN2	= 0x2a,	/* Translation, level -2 */
	ISS_IABORT_IFSC_TRANS_LN1	= 0x2b,	/* Translation, level -1 */
	ISS_IABORT_IFSC_ADDRSIZE_LN2	= 0x2c,	/* Address size, level -2 */
/* It seems there are no documented 0x2d..0x2f */
	ISS_IABORT_IFSC_TLB_CONFLICT	= 0x30,	/* TLB conflict */
	ISS_IABORT_IFSC_ATOMIC_HW_UNSUP	= 0x31,	/* Unsupported atomic */
						/*	hardware update */
} iss_ifsc_t;
/*
 * ISS encoding for an SME exception pp. 633
 */
/* [2:0] SME trap code */
#define	ISS_SME_SMTC_SHIFT	0
#define	ISS_SME_SMTC_MASK	0x7
#define	ISS_SME_SMTC(iss)	((iss & ISS_SME_SMTC_MASK) >> \
    ISS_SME_SMTC_SHIFT)

/* XXXARM: SMTC values aren't here because naming is hard  */

/*
 * ISS encoding for Granule Protection Check pp. 633
 */
/* [21] GPC on stage2 trans walk? */
#define	ISS_GPC_S2PTW_SHIFT	21
#define	ISS_GPC_S2PTW_MASK	(0x1 << ISS_GPC_S2PTW_SHIFT)
#define	ISS_GPC_S2PTW(iss)	((iss & ISS_GPC_S2PTW_MASK) >> \
    ISS_GPC_S2PTW_SHIFT)

/* [20] Instruction or Data access */
#define	ISS_GPC_IND_SHIFT	20
#define	ISS_GPC_IND_MASK	(0x1 << ISS_GPC_IND_SHIFT)
#define	ISS_GPC_IND(iss)	((iss & ISS_GPC_IND_MASK) >> ISS_GPC_IND_SHIFT)

/* [19:14] GPC status code */
#define	ISS_GPC_GPCSC_SHIFT	14
#define	ISS_GPC_GPCSC_MASK	(0x3f << ISS_GPC_GPCSC_SHIFT)
#define	ISS_GPC_GPCSC(iss)	((iss & ISS_GPC_GPCSC_MASK) >> \
    ISS_GPC_GPCSC_SHIFT)

/* [13] Fault from VNCR_EL2 at EL1? */
#define	ISS_GPC_VNCR_SHIFT	13
#define	ISS_GPC_VNCR_MASK	(0x1 << ISS_GPC_VNCR_SHIFT)
#define	ISS_GPC_VNCR(iss)	((iss & ISS_GPC_VNCR_MASK) >> \
    ISS_GPC_VNCR_SHIFT)

/* [8] Cache maintenance insn? */
#define	ISS_GPC_CM_SHIFT	8
#define	ISS_GPC_CM_MASK		(0x1 << ISS_GPC_CM_SHIFT)
#define	ISS_GPC_CM(iss)		((iss & ISS_GPC_CM_MASK) >> ISS_GPC_CM_SHIFT)

/* [7] GPC on stage2 for stage 1 trans walk  */
#define	ISS_GPC_S1PTW_SHIFT	7
#define	ISS_GPC_S1PTW_MASK	(0x1 << ISS_GPC_S1PTW_SHIFT)
#define	ISS_GPC_S1PTW(iss)	((iss & ISS_GPC_S1PTW_MASK) >> \
    ISS_GPC_S1PTW_SHIFT)

/* [6] read or write access? */
#define	ISS_GPC_WNR_SHIFT	6
#define	ISS_GPC_WNR_MASK	(0x1 << ISS_GPC_WNR_SHIFT)
#define	ISS_GPC_WNR(iss)	((iss & ISS_GPC_WNR_MASK) >> ISS_GPC_WNR_SHIFT)

/* [5:0] Fault Status Code */
#define	ISS_GPC_XFSC_SHIFT	0
#define	ISS_GPC_XFSC_MASK	0x3f
#define	ISS_GPC_XFSC(iss)	((iss & ISS_GPC_XFSC_MASK) >> \
    ISS_GPC_XFSC_SHIFT)
/* XXXARM: XFSC values aren't here because naming is hard  */

#define	ISS_GPC_IND_DATA	0
#define	ISS_GPC_IND_INSN	1

#define	ISS_GPC_WNR_READ	0
#define	ISS_GPC_WNR_WRITE	1

/*
 * ISS encoding for Data Abort pp. 636
 */
/* [24] Instruction syndrome valid? */
#define	ISS_DABORT_ISV_SHIFT	24
#define	ISS_DABORT_ISV_MASK	(0x1 << ISS_DABORT_ISV_SHIFT)
#define	ISS_DABORT_ISV(iss)	((iss & ISS_DABORT_ISV_MASK) >> \
    ISS_DABORT_ISV_SHIFT)

/* [23:22] Syndrome Access Size */
#define	ISS_DABORT_SAS_SHIFT	22
#define	ISS_DABORT_SAS_MASK	(0x3 << ISS_DABORT_SAS_SHIFT)
#define	ISS_DABORT_SAS(iss)	((iss & ISS_DABORT_SAS_MASK) >> \
    ISS_DABORT_SAS_SHIFT)

/* [21] sign extension required? */
#define	ISS_DABORT_SSE_SHIFT	21
#define	ISS_DABORT_SSE_MASK	(0x1 << ISS_DABORT_SSE_SHIFT)
#define	ISS_DABORT_SSE(iss)	((iss & ISS_DABORT_SSE_MASK) >> \
    ISS_DABORT_SSE_SHIFT)

/* [20:16] syndrome register transfer */
#define	ISS_DABORT_SRT_SHIFT	16
#define	ISS_DABORT_SRT_MASK	(0x1f << ISS_DABORT_SRT_SHIFT)
#define	ISS_DABORT_SRT(iss)	((iss & ISS_DABORT_SRT_MASK) >> \
    ISS_DABORT_SRT_SHIFT)

/* [15] Bit 15 */
#define	ISS_DABORT_BIT15_SHIFT	15
#define	ISS_DABORT_BIT15_MASK	(0x1 << ISS_DABORT_BIT15_SHIFT)
#define	ISS_DABORT_BIT15(iss)	((iss & ISS_DABORT_BIT15_MASK) >> \
    ISS_DABORT_BIT15_SHIFT)

/* [14] acquire/release semantics? */
#define	ISS_DABORT_AR_SHIFT	14
#define	ISS_DABORT_AR_MASK	(0x1 << ISS_DABORT_AR_SHIFT)
#define	ISS_DABORT_AR(iss)	((iss & ISS_DABORT_AR_MASK) >> \
    ISS_DABORT_AR_SHIFT)

/* [13] fault from vncr_el2 in el1? */
#define	ISS_DABORT_VNCR_SHIFT	13
#define	ISS_DABORT_VNCR_MASK	(0x1 << ISS_DABORT_VNCR_SHIFT)
#define	ISS_DABORT_VNCR(iss)	((iss & ISS_DABORT_VNCR_MASK) >> \
    ISS_DABORT_VNCR_SHIFT)

/* [12:11] Bits 12 and 11 */
#define	ISS_DABORT_BITS_12_11_SHIFT	11
#define	ISS_DABORT_BITS_12_11_MASK	(0x3 << ISS_DABORT_BITS_12_11_SHIFT)
#define	ISS_DABORT_BITS_12_11(iss)	((iss & ISS_DABORT_BITS_12_11_MASK) >> \
    ISS_DABORT_BITS_12_11_SHIFT)

/* [10] FAR not valid? */
#define	ISS_DABORT_FNV_SHIFT	10
#define	ISS_DABORT_FNV_MASK	(0x1 << ISS_DABORT_FNV_SHIFT)
#define	ISS_DABORT_FNV(iss)	((iss & ISS_DABORT_FNV_MASK) >> \
    ISS_DABORT_FNV_SHIFT)

/* [9] External abort? */
#define	ISS_DABORT_EA_SHIFT	9
#define	ISS_DABORT_EA_MASK	(0x1 << ISS_DABORT_EA_SHIFT)
#define	ISS_DABORT_EA(iss)	((iss & ISS_DABORT_EA_MASK) >> \
    ISS_DABORT_EA_SHIFT)

/* [8] From cache maintenance? */
#define	ISS_DABORT_CM_SHIFT	8
#define	ISS_DABORT_CM_MASK	(0x1 << ISS_DABORT_CM_SHIFT)
#define	ISS_DABORT_CM(iss)	((iss & ISS_DABORT_CM_MASK) >> \
    ISS_DABORT_CM_SHIFT)

/* [7] stage 2 fault for stage 1 trans */
#define	ISS_DABORT_S1PTW_SHIFT	7
#define	ISS_DABORT_S1PTW_MASK	(0x1 << ISS_DABORT_S1PTW_SHIFT)
#define	ISS_DABORT_S1PTW(iss)	((iss & ISS_DABORT_S1PTW_MASK) >> \
    ISS_DABORT_S1PTW_SHIFT)

/* [6] write or read? */
#define	ISS_DABORT_WNR_SHIFT	6
#define	ISS_DABORT_WNR_MASK	(0x1 << ISS_DABORT_WNR_SHIFT)
#define	ISS_DABORT_WNR(iss)	((iss & ISS_DABORT_WNR_MASK) >> \
    ISS_DABORT_WNR_SHIFT)

/* [5:0] data fault status code */
#define	ISS_DABORT_DFSC_SHIFT	0
#define	ISS_DABORT_DFSC_MASK	0x3f
#define	ISS_DABORT_DFSC(iss)	((iss & ISS_DABORT_DFSC_MASK) >> \
    ISS_DABORT_DFSC_SHIFT)

#define	ISS_DABORT_SAS_BYTE		0
#define	ISS_DABORT_SAS_HALFWORD		1
#define	ISS_DABORT_SAS_WORD		2
#define	ISS_DABORT_SAS_DOUBLEWORD	3

/* When ISV == 1, BIT15 refers to width of transfer register */
#define	ISS_DABORT_SF_32BIT_REG	0
#define	ISS_DABORT_SF_64BIT_REG	1

/* When ISV == 0, BIT15 indicates if the FAR is precise */
#define	ISS_DABORT_FNP_PRECISE		0 /* FAR is address that caused abort */
/* FAR is any VA in the same granule */
#define	ISS_DABORT_FNP_IMPRECISE	1

/*
 * When DFSC = 0b00.... or 0b101011 & DFSC != 0b0000.. BITS_12_11
 * refer to Load/Store type
 */
#define	ISS_DABORT_LST_UNSPECIFIED	0
#define	ISS_DABORT_LST_ST64BV		1 /* ST64BV caused abort */
#define	ISS_DABORT_LST_XX64B		2 /* LD64B or ST64B caused abort */
#define	ISS_DABORT_LST_ST64BV0		3 /* ST64BV0 caused abort */

/* When DFSC == 0b010000 BITS12_11 refers to Synch Error Type */
/* XXXARM: This isn't what we have for IABORT SET */
#define	ISS_DABORT_SET_UER	0 /* Recoverable */
#define	ISS_DABORT_SET_UC	2 /* Uncontainable */
#define	ISS_DABORT_SET_UEO	3 /* Restartable */

/* We spell this out, unlike other bools, to help clarify it being backwards */
#define	ISS_DABORT_FAR_VALID		0
#define	ISS_DABORT_FAR_NOT_VALID	1

#define	ISS_DABORT_WNR_READ	0
#define	ISS_DABORT_WNR_WRITE	1

typedef enum {
	ISS_DABORT_DFSC_ADDRSIZE_L0	= 0x0,	/* Address size, level 0 */
	ISS_DABORT_DFSC_ADDRSIZE_L1	= 0x1,	/* Address size, level 1 */
	ISS_DABORT_DFSC_ADDRSIZE_L2	= 0x2,	/* Address size, level 2 */
	ISS_DABORT_DFSC_ADDRSIZE_L3	= 0x3,	/* Address size, level 3 */
	ISS_DABORT_DFSC_TRANS_L0	= 0x4,	/* Translation, level 0 */
	ISS_DABORT_DFSC_TRANS_L1	= 0x5,	/* Translation, level 1 */
	ISS_DABORT_DFSC_TRANS_L2	= 0x6,	/* Translation, level 2 */
	ISS_DABORT_DFSC_TRANS_L3	= 0x7,	/* Translation, level 3 */
	ISS_DABORT_DFSC_ACCESS_L0	= 0x8,	/* Access flag, level 0 */
	ISS_DABORT_DFSC_ACCESS_L1	= 0x9,	/* Access flag, level 1 */
	ISS_DABORT_DFSC_ACCESS_L2	= 0xa,	/* Access flag, level 2 */
	ISS_DABORT_DFSC_ACCESS_L3	= 0xb,	/* Access flag, level 3 */
#define	ISS_DABORT_DFSC_ACCESS(dfsc)	((dfsc >= 0x8) && (dfsc <= 0xb))
	ISS_DABORT_DFSC_PERM_L0		= 0xc,	/* Permission, level 0 */
	ISS_DABORT_DFSC_PERM_L1		= 0xd,	/* Permission, level 1 */
	ISS_DABORT_DFSC_PERM_L2		= 0xe,	/* Permission, level 2 */
	ISS_DABORT_DFSC_PERM_L3		= 0xf,	/* Permission, level 3 */
#define	ISS_DABORT_DFSC_PERM(dfsc)	((dfsc >= 0xc) && (dfsc <= 0xf))
	ISS_DABORT_DFSC_SYNCH_EXT	= 0x10,	/* Synchronous external abort */
						/*	(not trans) */
	ISS_DABORT_DFSC_SYNCH_TAG	= 0x11,	/* Synchronous tag check */
	ISS_DABORT_DFSC_SYNCH_EXT_LN2	= 0x12,	/* Synchronous external abort */
						/*	level -2 */
	ISS_DABORT_DFSC_SYNCH_EXT_LN1	= 0x13,	/* Synchronous external abort */
						/*	level -1 */
	ISS_DABORT_DFSC_SYNCH_EXT_L0	= 0x14,	/* Synchronous external abort */
						/*	level 0 */
	ISS_DABORT_DFSC_SYNCH_EXT_L1	= 0x15,	/* Synchronous external abort */
						/*	level 1 */
	ISS_DABORT_DFSC_SYNCH_EXT_L2	= 0x16,	/* Synchronous external abort */
						/*	level 2 */
	ISS_DABORT_DFSC_SYNCH_EXT_L3	= 0x17,	/* Synchronous external abort */
						/*	 level 3 */
	ISS_DABORT_DFSC_PARECC		= 0x18,	/* Parity or ECC error */
						/*	(not trans) */
/* It seems there is no 0x19 or 0x1a */
	ISS_DABORT_DFSC_PARECC_LN1	= 0x1b,	/* Parity or ECC, level -1 */
	ISS_DABORT_DFSC_PARECC_L0	= 0x1c,	/* Parity or ECC, level 0 */
	ISS_DABORT_DFSC_PARECC_L1	= 0x1d,	/* Parity or ECC, level 1 */
	ISS_DABORT_DFSC_PARECC_L2	= 0x1e,	/* Parity or ECC, level 2 */
	ISS_DABORT_DFSC_PARECC_L3	= 0x1f,	/* Parity or ECC, level 3 */
/* It seems there is no 0x20 */
	ISS_DABORT_DFSC_ALIGNMENT	= 0x21,	/* Alignment */
	ISS_DABORT_DFSC_GPF_LN2		= 0x22,	/* Granule PF, level -2 */
	ISS_DABORT_DFSC_GPF_LN1		= 0x23,	/* Granule PF, level -1 */
	ISS_DABORT_DFSC_GPF_L0		= 0x24,	/* Granule PF, level 0 */
	ISS_DABORT_DFSC_GPF_L1		= 0x25,	/* Granule PF, level 1 */
	ISS_DABORT_DFSC_GPF_L2		= 0x26,	/* Granule PF, level 2 */
	ISS_DABORT_DFSC_GPF_L3		= 0x27,	/* Granule PF, level 3 */
	ISS_DABORT_DFSC_GPF		= 0x28,	/* Granule PF (not trans) */
	ISS_DABORT_DFSC_ADDRSIZE_LN1	= 0x29,	/* Address size, level -1 */
	ISS_DABORT_DFSC_TRANS_LN2	= 0x2a,	/* Translation, level -2 */
	ISS_DABORT_DFSC_TRANS_LN1	= 0x2b,	/* Translation, level -1 */
	ISS_DABORT_DFSC_ADDRSIZE_LN2	= 0x2c,	/* Address size, level -2 */
/* It seems there is no 0x2d..0x2f */
	ISS_DABORT_DFSC_TLB_CONFLICT	= 0x30,	/* TLB conflict abort */
	ISS_DABORT_DFSC_ATOMIC_HW_UNSUP	= 0x31,	/* Unsupported atomic */
						/*	hardware update */
/* It seems there is no 0x32, or 0x33 */
	ISS_DABORT_DFSC_LOCKDOWN	= 0x34,	/* Implementation defined */
						/*	(lockdown) */
	ISS_DABORT_DFSC_ATOMIC		= 0x35,	/* Implementation defined */
						/*	(unsupported */
						/*	exclusive or atomic */
						/*	access) */
} iss_dfsc_t;

/*
 * ISS encoding for a trapped IEEE FP exception pp. 646
 */
/* [23] trapped fault valid? */
#define	ISS_FPEXC_TFV_SHIFT	23
#define	ISS_FPEXC_TFV_MASK	(0x1 << ISS_FPEXC_TFV_SHIFT)
#define	ISS_FPEXC_TVF(iss)	((iss & ISS_FPEXEC_TVF_MASK) >> \
    ISS_FPEXEC_TVF_SHIFT)

/* [10:8] (documented as) Unknown */
#define	ISS_FPEXC_VECITR_SHIFT	8
#define	ISS_FPEXC_VECITR_MASK	(0x7 << ISS_FPEXC_VECITR_SHIFT)
#define	ISS_FPEXC_VECITR(iss)	((iss & ISS_FPEXC_VECITR_MASK) >> \
    ISS_FPEXC_VECITR_SHIFT)

/* [7] Input Denormal? */
#define	ISS_FPEXC_IDF_SHIFT	7
#define	ISS_FPEXC_IDF_MASK	(0x1 << ISS_FPEXC_IDF_SHIFT)
#define	ISS_FPEXC_IDF(iss)	((iss & ISS_FPEXC_IDF_MASK) >> \
    ISS_FPEXC_IDF_SHIFT)

/* [4] Inexact? */
#define	ISS_FPEXC_IXF_SHIFT	4
#define	ISS_FPEXC_IXF_MASK	(0x1 << ISS_FPEXC_IXF_SHIFT)
#define	ISS_FPEXC_IXF(iss)	((iss & ISS_FPEXC_IXF_MASK) >> \
    ISS_FPEXC_IXF_SHIFT)

/* [3] Underflow? */
#define	ISS_FPEXC_UFF_SHIFT	3
#define	ISS_FPEXC_UFF_MASK	(0x1 << ISS_FPEXC_UFF_SHIFT)
#define	ISS_FPEXC_UFF(iss)	((iss & ISS_FPEXC_UFF_MASK) >> \
    ISS_FPEXC_UFF_SHIFT)

/* [2] Overflow? */
#define	ISS_FPEXC_OFF_SHIFT	2
#define	ISS_FPEXC_OFF_MASK	(0x1 << ISS_FPEXC_OFF_SHIFT)
#define	ISS_FPEXC_OFF(iss)	((iss & ISS_FPEXC_OFF_MASK) >> \
    ISS_FPEXC_OFF_SHIFT)

/* [1] Divide by zero? */
#define	ISS_FPEXC_DZF_SHIFT	1
#define	ISS_FPEXC_DZF_MASK	(0x1 << ISS_FPEXC_DZF_SHIFT)
#define	ISS_FPEXC_DZF(iss)	((iss & ISS_FPEXC_DZF_MASK) >> \
    ISS_FPEXC_DZF_SHIFT)

/* [0] Invalid Operation? */
#define	ISS_FPEXC_IOF_SHIFT	0
#define	ISS_FPEXC_IOF_MASK	1
#define	ISS_FPEXC_IOF(iss)	((iss & ISS_FPEXC_IOF_MASK) >> \
    ISS_FPEXC_IOF_SHIFT)

/*
 * ISS encoding for an SError interrupt pp. 648
 */
/* [24] implementation defined syndrome? */
#define	ISS_SERROR_IDS_SHIFT	24
#define	ISS_SERROR_IDS_MASK	(0x1 << ISS_SERROR_IDS_SHIFT)
#define	ISS_SERROR_IDS(iss)	((iss & ISS_SERROR_IDS_MASK) >> \
    ISS_SERROR_IDS_SHIFT)

/* [13] event synchronized and taken immediately? */
#define	ISS_SERROR_IESB_SHIFT	13
#define	ISS_SERROR_IESB_MASK	(0x1 << ISS_SERROR_IESB_SHIFT)
#define	ISS_SERROR_IESB(iss)	((iss & ISS_SERROR_IESB_MASK) >> \
    ISS_SERROR_IESB_SHIFT)

/* [12:10] asynch error type */
#define	ISS_SERROR_AET_SHIFT	10
#define	ISS_SERROR_AET_MASK	(0x7 << ISS_SERROR_AET_SHIFT)
#define	ISS_SERROR_AET(iss)	((iss & ISS_SERROR_AET_MASK) >> \
    ISS_SERROR_AET_SHIFT)

/* [9]	external abort? */
#define	ISS_SERROR_EA_SHIFT	9
#define	ISS_SERROR_EA_MASK	(0x1 << ISS_SERROR_EA_SHIFT)
#define	ISS_SERROR_EA(iss)	((iss & ISS_SERROR_EA_MASK) >> \
    ISS_SERROR_EA_SHIFT)

/* [5:0] data fault status code */
#define	ISS_SERROR_DFSC_SHIFT	0
#define	ISS_SERROR_DFSC_MASK	0x3f
#define	ISS_SERROR_DFSC(iss)	((iss & ISS_SERROR_DFSC_MASK) >> \
    ISS_SERROR_DFSC_SHIFT)

#define	ISS_SERROR_AET_UC	0 /* Uncontainable */
#define	ISS_SERROR_AET_UEU	1 /* Unrecoverable */
#define	ISS_SERROR_AET_UEO	2 /* Restartable */
#define	ISS_SERROR_AET_UER	3 /* Recoverable */
#define	ISS_SERROR_AET_CE	4 /* Corrected */

#define	ISS_SERROR_DFSC_UNCATEGORIZED	0
/* Other values not documented */
#define	ISS_SERROR_DFSC_ASYNCHRONOUS	0x11

/*
 * ISS encoding for a breakpoint or vector catch debug exception pp. 650
 */
#define	ISS_BREAKPOINT_IFSC_SHIFT	0
#define	ISS_BREAKPOINT_IFSC_MASK	0x3f
#define	ISS_BREAKPOINT_IFSC(iss)	((iss & ISS_BREAKPOINT_IFSC_MASK) >> \
    ISS_BREAKPOINT_IFSC_SHIFT)

#define	ISS_BREAKPOINT_IFSC_DEBUG	0x22 /* Debug exception */

/*
 * ISS encoding for Software Step exceptions, pp. 651
 */
/* [24] EX valid? */
#define	ISS_SOFTSTEP_ISV_SHIFT	24
#define	ISS_SOFTSTEP_ISV_MASK	(0x1 << ISS_SOFTSTEP_ISV_SHIFT)
#define	ISS_SOFTSTEP_ISV(iss)	((iss & ISS_SOFTSTEP_ISV_MASK) >> \
    ISS_SOFTSTEP_ISV_SHIFT)

/* [6] Exclusive Operation? */
#define	ISS_SOFTSTEP_EX_SHIFT	6
#define	ISS_SOFTSTEP_EX_MASK	(0x1 << ISS_SOFTSTEP_EX_SHIFT)
#define	ISS_SOFTSTEP_EX(iss)	((iss & ISS_SOFTSTEP_EX_MASK) >> \
    ISS_SOFTSTEP_EX_SHIFT)

/* [5:0] instruction fault status code */
#define	ISS_SOFTSTEP_IFSC_SHIFT	0
#define	ISS_SOFTSTEP_IFSC_MASK	0x3f
#define	ISS_SOFTSTEP_IFSC(iss)	((iss & ISS_SOFTSTEP_IFSC_MASK) >> \
    ISS_SOFTSTEP_IFSC_SHIFT)

/* Other values not documented (or documented in "Software Stepping") */
#define	ISS_SOFTSTEP_IFSC_DEBUG	0x22 /* Debug exception */

/*
 * ISS encoding for a Watchpoint exception pp. 651
 */
/* [23:18]  Watchpoint number */
#define	ISS_WATCHPOINT_WPT_SHIFT	18
#define	ISS_WATCHPOINT_WPT_MASK		(0x3f << ISS_WATCHPOINT_WPT_SHIFT)
#define	ISS_WATCHPOINT_WPT(iss)		((iss & ISS_WATCHPOINT_WPT_MASK) >> \
    ISS_WATCHPOINT_WPT_SHIFT)

/* [17] Watchpoint number valid? */
#define	ISS_WATCHPOINT_WPTV_SHIFT	17
#define	ISS_WATCHPOINT_WPTV_MASK	(0x1 << ISS_WATCHPOINT_WPTV_SHIFT)
#define	ISS_WATCHPOINT_WPTV(iss)	((iss & ISS_WATCHPOINT_WPTV_MASK) >> \
    ISS_WATCHPOINT_WPTV_SHIFT)

/* [16] Watchpoint might be false positive? */
#define	ISS_WATCHPOINT_WPF_SHIFT	16
#define	ISS_WATCHPOINT_WPF_MASK		(0x1 << ISS_WATCHPOINT_WPF_SHIFT)
#define	ISS_WATCHPOINT_WPF(iss)		((iss & ISS_WATCHPOINT_WPF_MASK) >> \
    ISS_WATCHPOINT_WPF_SHIFT)

/* [15] FAR not precise? */
#define	ISS_WATCHPOINT_FNP_SHIFT	15
#define	ISS_WATCHPOINT_FNP_MASK		(0x1 << ISS_WATCHPOINT_FNP_SHIFT)
#define	ISS_WATCHPOINT_FNP(iss)		((iss & ISS_WATCHPOINT_FNP_MASK) >> \
    ISS_WATCHPOINT_FNP_SHIFT)

/* [13] Watchpoint of vncr_el2 in el1 */
#define	ISS_WATCHPOINT_VNCR_SHIFT	13
#define	ISS_WATCHPOINT_VNCR_MASK	(0x1 << ISS_WATCHPOINT_VNCR_SHIFT)
#define	ISS_WATCHPOINT_VNCR(iss)	((iss & ISS_WATCHPOINT_VNCR_MASK) >> \
    ISS_WATCHPOINT_VNCR_SHIFT)

/* [10]	FAR not valid? */
#define	ISS_WATCHPOINT_FNV_SHIFT	10
#define	ISS_WATCHPOINT_FNV_MASK		(0x1 << ISS_WATCHPOINT_FNV_SHIFT)
#define	ISS_WATCHPOINT_FNV(iss)		((iss & ISS_WATCHPOINT_FNV_MASK) >> \
    ISS_WATCHPOINT_FNV_SHIFT)

/* [8]	From cache maintenance instruction? */
#define	ISS_WATCHPOINT_CM_SHIFT		8
#define	ISS_WATCHPOINT_CM_MASK		(0x1 << ISS_WATCHPOINT_CM_SHIFT)
#define	ISS_WATCHPOINT_CM(iss)		((iss & ISS_WATCHPOINT_CM_MASK) >> \
    ISS_WATCHPOINT_CM_SHIFT)

/* [6]	Watchpoint from write or read? */
#define	ISS_WATCHPOINT_WNR_SHIFT	6
#define	ISS_WATCHPOINT_WNR_MASK		(0x1 << ISS_WATCHPOINT_WNR_SHIFT)
#define	ISS_WATCHPOINT_WNR(iss)		((iss & ISS_WATCHPOINT_WNR_MASK) >> \
    ISS_WATCHPOINT_WNR_SHIFT)

/* [5:0] data fault status code */
#define	ISS_WATCHPOINT_DFSC_SHIFT	0
#define	ISS_WATCHPOINT_DFSC_MASK	0x3f
#define	ISS_WATCHPOINT_DFSC(iss)	((iss & ISS_WATCHPOINT_DFSC_MASK) >> \
    ISS_WATCHPOINT_DFSC_SHIFT)

/* We spell this out, unlike other bools, to help clarify it being backwards */
#define	ISS_WATCHPOINT_FNP_PRECISE	0 /* FAR is address that caused abort */
/* FAR is any VA in the same granule */
#define	ISS_WATCHPOINT_FNP_IMPRECISE	1

/* We spell this out, unlike other bools, to help clarify it being backwards */
#define	ISS_WATCHPOINT_FAR_VALID	0
#define	ISS_WATCHPOINT_FAR_NOT_VALID	1

#define	ISS_WATCHPOINT_WNR_READ		0
#define	ISS_WATCHPOINT_WNR_WRITE	1

#define	ISS_WATCHPOINT_DFSC_DEBUG	0x22 /* Debug exception */

/*
 * ISS encoding for execution of an Breakpoint instruction pp. 654
 */
/* [15:0] comment field from insn */
#define	ISS_BREAKPOINT_COMMENT_SHIFT	0
#define	ISS_BREAKPOINT_COMMENT_MASK	0xff
#define	ISS_BREAKPOINT_COMMENT(iss)	\
	((iss & ISS_BREAKPOINT_COMMENT_MASK) >> ISS_BREAKPOINT_COMMENT_SHIFT)

/*
 * ISS encoding for TSTART instruction pp. 656
 */
/* [9:5] Rd field from insn */
#define	ISS_TSTART_RD_SHIFT	5
#define	ISS_TSTART_RD_MASK	(0x1f << ISS_TSTART_RD_SHIFT)
#define	ISS_TSTART_RD(iss)	((iss & ISS_TSTART_RD_MASK) >> \
    ISS_TSTART_RD_SHIFT)

/*
 * ISS encoding for BTI instruction pp. 656
 */
/* [1:0] PSTATE.BTYPE from the BTI exception */
#define	ISS_BTI_BTYPE_SHIFT	0
#define	ISS_BTI_BTYPE_MASK	0x3
#define	ISS_BTI_BTYPE(iss)	((iss & ISS_BTI_BTYPE_MASK) >> \
    ISS_BTI_BTYPE_SHIFT)

/*
 * ISS encoding for PAC authentication failure pp. 656
 */
/* [1] Result of instruction or data key */
#define	ISS_PAC_IDKEY_SHIFT	1
#define	ISS_PAC_IDKEY_MASK	(0x1 << ISS_PAC_IDKEY_SHIFT)
#define	ISS_PAC_IDKEY(iss)	((iss & ISS_PAC_IDKEY_MASK) >> \
    ISS_PAC_IDKEY_SHIFT)

/* [0] Result of an A or B key */
#define	ISS_PAC_ABKEY_SHIFT	0
#define	ISS_PAC_ABKEY_MASK	0x1
#define	ISS_PAC_ABKEY(iss)	((iss & ISS_PAC_ABKEY_MASK) >> \
    ISS_PAC_ABKEY_SHIFT)

/*
 * Instruction Specific Syndromes (2)
 *
 * Arm® Architecture Registers for A-profile architecture pp. 599 (et seq)
 */

/*
 * ISS2 encoding for Data Abort pp. 599
 */
/* [9] permission fault caused by No Tag Access? */
#define	ISS2_DABORT_NOTAGACCESS_SHIFT	9
#define	ISS2_DABORT_NOTAGACCESS_MASK	(0x1 << ISS2_DABORT_NOTAGACCESS_SHIFT)
#define	ISS2_DABORT_NOTAGACCESS(iss2)	\
	((iss2 & ISS2_DABORT_NOTAGACCESS_MASK) >> ISS2_DABORT_NOTAGACCESS_SHIFT)

/* [6] permission fault due to overlay permissions? */
#define	ISS2_DABORT_OVERLAY_SHIFT	6
#define	ISS2_DABORT_OVERLAY_MASK	(0x1 << ISS2_DABORT_OVERLAY_SHIFT)
#define	ISS2_DABORT_OVERLAY(iss2)	((iss2 & ISS2_DABORT_OVERLAY_MASK) >> \
    ISS2_DABORT_OVERLAY_SHIFT)

/* [5] permission fault due to dirty state? */
#define	ISS2_DABORT_DIRTYBIT_SHIFT	5
#define	ISS2_DABORT_DIRTYBIT_MASK	(0x1 << ISS2_DABORT_DIRTYBIT_SHIFT)
#define	ISS2_DABORT_DIRTYBIT(iss2)	((iss2 & ISS2_DABORT_DIRTYBIT_MASK) >> \
    ISS2_DABORT_DIRTYBIT_SHIFT)

/* [4:0] specified Xs register for faults from ST64B*  */
#define	ISS2_DABORT_XS_SHIFT	0
#define	ISS2_DABORT_XS_MASK	(0x1f << ISS2_DABORT_XS_SHIFT)
#define	ISS2_DABORT_XS(iss2)	((iss2 & ISS2_DABORT_XS_MASK) >> \
    ISS2_DABORT_XS_SHIFT)

/*
 * ISS2 encoding for Instruction Abort pp. 601
 */
/* [6] permission fault due to overlay permissions? */
#define	ISS2_IABORT_OVERLAY_SHIFT	6
#define	ISS2_IABORT_OVERLAY_MASK	(0x1 << ISS2_IABORT_OVERLAY_SHIFT)
#define	ISS2_IABORT_OVERLAY(iss2)	((iss2 & ISS2_IABORT_OVERLAY_MASK) >> \
    ISS2_IABORT_OVERLAY_SHIFT)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TRAP_H */
