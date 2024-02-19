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

/* Copyright 2022 Richard Lowe */
#ifndef	_MDB_KREG_H
#define	_MDB_KREG_H

#include <sys/kdi_regs.h>
#ifndef _ASM
#include <sys/types.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
typedef uint64_t kreg_t;
#endif	/* !_ASM */

/* See sys/kdi_regs.h, and sys/privregs.h, which must all agree */
#define	KREG_SAVFP	KDIREG_SAVFP
#define	KREG_SAVPC	KDIREG_SAVPC
#define	KREG_X0		KDIREG_X0
#define	KREG_X1		KDIREG_X1
#define	KREG_X2		KDIREG_X2
#define	KREG_X3		KDIREG_X3
#define	KREG_X4		KDIREG_X4
#define	KREG_X5		KDIREG_X5
#define	KREG_X6		KDIREG_X6
#define	KREG_X7		KDIREG_X7
#define	KREG_X8		KDIREG_X8
#define	KREG_X9		KDIREG_X9
#define	KREG_X10	KDIREG_X10
#define	KREG_X11	KDIREG_X11
#define	KREG_X12	KDIREG_X12
#define	KREG_X13	KDIREG_X13
#define	KREG_X14	KDIREG_X14
#define	KREG_X15	KDIREG_X15
#define	KREG_X16	KDIREG_X16
#define	KREG_X17	KDIREG_X17
#define	KREG_X18	KDIREG_X18
#define	KREG_X19	KDIREG_X19
#define	KREG_X20	KDIREG_X20
#define	KREG_X21	KDIREG_X21
#define	KREG_X22	KDIREG_X22
#define	KREG_X23	KDIREG_X23
#define	KREG_X24	KDIREG_X24
#define	KREG_X25	KDIREG_X25
#define	KREG_X26	KDIREG_X26
#define	KREG_X27	KDIREG_X27
#define	KREG_X28	KDIREG_X28
#define	KREG_X29	KDIREG_X29
#define	KREG_FP		KDIREG_FP
#define	KREG_X30	KDIREG_X30
#define	KREG_LR		KDIREG_LR
#define	KREG_SP		KDIREG_SP
#define	KREG_PC		KDIREG_PC
#define	KREG_SPSR	KDIREG_SPSR
#define	KREG_TP		KDIREG_TP
#define	KREG_TRAPNO	KDIREG_TRAPNO
#define	KREG_FAR	KDIREG_FAR
#define	KREG_ESR	KDIREG_ESR

#define	KREG_NGREG	KDIREG_NGREG

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_KREG_H */
