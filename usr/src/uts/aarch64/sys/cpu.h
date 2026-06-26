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
 * Copyright 2026 Michael van der Westhuizen
 */

#ifndef	_SYS_CPU_H
#define	_SYS_CPU_H

#include <sys/types.h>
#include <asm/cpu.h>
#ifdef __cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && !defined(_ASM)

#define	SMT_PAUSE()	\
    __asm__ __volatile__("isb":::"memory")

/*
 * Idle state for the cpu_idle_enter/cpu_idle_exit callback framework.
 *
 * IDLE_STATE_NORMAL indicates the CPU is not in an idle state.
 *
 * On aarch64, the state value encodes both the firmware-specific
 * index (1-based) in bits [31:20] and context loss flags in bits [19:0].
 *
 * This arrangement provides tracing consumers visibility into which low power
 * idle state was entered and what context will be lost.  Since
 * IDLE_STATE_NORMAL is 0, it indicates that there is no context loss.
 */
#define	IDLE_STATE_NORMAL	0

#define	LPI_STATE_IDX_SHIFT	20
#define	LPI_STATE_IDX_MASK	0xFFF00000U
#define	LPI_STATE_CTX_MASK	0x000FFFFFU

/* Compose a state value from a 1-based LPI index and context loss flags */
#define	LPI_IDLE_STATE(idx, flags)	\
	(((uint_t)(idx) << LPI_STATE_IDX_SHIFT) | \
	((uint_t)(flags) & LPI_STATE_CTX_MASK))

/* Extract components from a state value */
#define	LPI_STATE_IDX(state)	\
	(((uint_t)(state) >> LPI_STATE_IDX_SHIFT) & 0xFFFU)
#define	LPI_STATE_CTX(state)	\
	((uint_t)(state) & LPI_STATE_CTX_MASK)

/*
 * LPI context loss flags (bits [19:0] of the idle state value).
 */
#define	LPI_CTX_LOSS_TIMER	(1U << 0)	/* Timer context lost */
#define	LPI_CTX_LOSS_GICR	(1U << 1)	/* GIC Redistributor lost */
#define	LPI_CTX_LOSS_GICD	(1U << 2)	/* GIC Distributor lost */
#define	LPI_CTX_LOSS_CPU	(1U << 3)	/* CPU register state lost */

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CPU_H */
