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
 */

#ifndef _SYS_PLATMOD_H
#define	_SYS_PLATMOD_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/promif.h>

extern uint64_t plat_get_cpu_clock(int cpu_no);
extern int plat_pinmux_set(pnode_t);

struct gpio_ctrl;
extern int plat_gpio_direction_output(struct gpio_ctrl *, int);
extern int plat_gpio_direction_input(struct gpio_ctrl *);
extern int plat_gpio_get(struct gpio_ctrl *);
extern int plat_gpio_set(struct gpio_ctrl *, int);
extern int plat_gpio_set_pullup(struct gpio_ctrl *, int);

extern int plat_hwreset_assert(struct prom_hwreset *);
extern int plat_hwreset_deassert(struct prom_hwreset *);
extern int plat_hwreset_is_asserted(struct prom_hwreset *, boolean_t *);

extern int plat_hwclock_enable(struct prom_hwclock *);
extern int plat_hwclock_disable(struct prom_hwclock *);
extern int plat_hwclock_is_enabled(struct prom_hwclock *, boolean_t *);
extern int plat_hwclock_get_rate(struct prom_hwclock *);
extern int plat_hwclock_get_max_rate(struct prom_hwclock *);
extern int plat_hwclock_get_min_rate(struct prom_hwclock *);
extern int plat_hwclock_set_rate(struct prom_hwclock *, int rate);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PLATMOD_H */
