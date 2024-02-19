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
 * Copyright (c) 1989, 2010, Oracle and/or its affiliates. All rights reserved.
 *
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2022 Michael van der Westhuizen
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T		*/
/*	All Rights Reserved	*/

#ifndef	_SYS_REGSET_H
#define	_SYS_REGSET_H

#include <sys/feature_tests.h>

#if !defined(_ASM)
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	REG_X0		0
#define	REG_X1		1
#define	REG_X2		2
#define	REG_X3		3
#define	REG_X4		4
#define	REG_X5		5
#define	REG_X6		6
#define	REG_X7		7
#define	REG_X8		8
#define	REG_X9		9
#define	REG_X10		10
#define	REG_X11		11
#define	REG_X12		12
#define	REG_X13		13
#define	REG_X14		14
#define	REG_X15		15
#define	REG_X16		16
#define	REG_X17		17
#define	REG_X18		18
#define	REG_X19		19
#define	REG_X20		20
#define	REG_X21		21
#define	REG_X22		22
#define	REG_X23		23
#define	REG_X24		24
#define	REG_X25		25
#define	REG_X26		26
#define	REG_X27		27
#define	REG_X28		28
#define	REG_X29		29
#define	REG_FP		REG_X29
#define	REG_X30		30
#define	REG_LR		REG_X30
#define	REG_SP		31
#define	REG_PC		32
#define	REG_SPSR	33
#define	REG_TP		34

#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * A gregset_t is defined as an array type for compatibility with the reference
 * source. This is important due to differences in the way the C language
 * treats arrays and structures as parameters.
 */
#define	_NGREG	36

#if !defined(_ASM)

typedef long	greg_t;
typedef greg_t	gregset_t[_NGREG];

typedef upad128_t fpreg_t;

/*
 * Floating point definitions.
 */
typedef struct fpu {
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
	fpreg_t			d_fpregs[32];
	uint32_t		fp_cr;
	uint32_t		fp_sr;
#else
	fpreg_t			__d_fpregs[32];
	uint32_t		__fp_cr;
	uint32_t		__fp_sr;
#endif
} fpregset_t;

/*
 * Structure mcontext defines the complete hardware machine state.
 */
typedef struct {
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
	gregset_t	gregs;	/* general register set */
	fpregset_t	fpregs;	/* floating point register set */
#else
	gregset_t	__gregs;	/* general register set */
	fpregset_t	__fpregs;	/* floating point register set */
#endif
} mcontext_t;

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
/*
 * Kernel's FPU save area
 */
typedef struct {
	fpreg_t			kfpu_regs[32];
	uint32_t		kfpu_cr;
	uint32_t		kfpu_sr;
} kfpu_t;
#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REGSET_H */
