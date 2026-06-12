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

#ifndef _SYS_PLATMOD_H
#define	_SYS_PLATMOD_H

/*
 * Platform interfaces for the armv8 platform.
 *
 * These interfaces are incredibly volatile and should be expected to
 * churn for the foreseeable future.
 *
 * See also: uts/aarch64/sys/platform_module.h
 */

#include <sys/types.h>
#include <sys/cpuvar.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL)

struct gpio_ctrl;
struct prom_hwclock;

#pragma weak	plat_get_cpu_clock
#pragma weak	plat_set_max_cpu_clock
#pragma weak	plat_set_cpu_supp_freqs
#pragma weak	plat_gpio_get
#pragma weak	plat_gpio_set
#pragma weak	plat_clk_get_rate
#pragma weak	plat_clk_get_rate_by_name
#pragma weak	plat_pcie_osc_set
#pragma weak	plat_pcie_osc

/*
 * Called in mp_startup.c from init_cpu_info (twice).
 */
extern uint64_t plat_get_cpu_clock(int cpu_no);
extern void plat_set_max_cpu_clock(int cpu_no);
extern void plat_set_cpu_supp_freqs(cpu_t *cp);

/*
 * Called in bcm2711-emmc2.c to drive the GPIO regulator when switching to 1v8.
 */
struct gpio_ctrl;
extern int plat_gpio_get(struct gpio_ctrl *);
extern int plat_gpio_set(struct gpio_ctrl *, int);

extern int plat_clk_get_rate(dev_info_t *, uint_t);
extern int plat_clk_get_rate_by_name(dev_info_t *, const char *);

/*
 * PCIe _OSC negotiation.
 *
 * plat_pcie_osc: Evaluate PCIe Host Bridge _OSC for a given bridge.
 *   dip:         bridge dev_info_t
 *   support:     Support Field bits to declare (DWORD 2)
 *   ctrl_req:    Control Field bits to request (DWORD 3)
 *   ctrl_ret:    on success, granted Control Field bits
 *
 *   Returns DDI_SUCCESS/DDI_FAILURE.
 *
 * plat_pcie_osc_set: Called by ACPI layer to register the _OSC
 *   evaluator.  When the weak symbol is absent (DT platforms),
 *   pcie_osc.c falls through to grant all requested bits.
 */
typedef int (*plat_pcie_osc_func_t)(dev_info_t *dip,
    uint32_t support, uint32_t ctrl_req, uint32_t *ctrl_ret);

extern void plat_pcie_osc_set(plat_pcie_osc_func_t);
extern int plat_pcie_osc(dev_info_t *dip,
    uint32_t support, uint32_t ctrl_req, uint32_t *ctrl_ret);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PLATMOD_H */
