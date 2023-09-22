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
 * Copyright 2023 Brian McKenzie
 */

/*
 * Prototypes and definitions for the generic timer interface.
 */

#ifndef __SYS_ARCH_TIMER_H
#define	__SYS_ARCH_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#define	CNT_CVAL_MAX	0xfffffffffffffffful

typedef enum arch_timer_type {
	TMR_PHYS,
	TMR_VIRT,
} arch_timer_t;

extern void arch_timer_mask_irq(void);
extern void arch_timer_unmask_irq(void);
extern void arch_timer_enable(void);
extern void arch_timer_disable(void);
extern void arch_timer_set_cval(uint64_t cval);
extern uint64_t arch_timer_freq(void);
extern uint64_t arch_timer_count(void);
extern void arch_timer_select(arch_timer_t type);
extern void arch_timer_udelay(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_ARCH_TIMER_H */
