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

#pragma weak	plat_cpu_get_speed
#pragma weak	plat_cpu_get_speeds
#pragma weak	plat_cpu_free_speeds
#pragma weak	plat_cpu_set_speed
#pragma weak	plat_set_max_cpu_clock
#pragma weak	plat_gpio_get
#pragma weak	plat_gpio_set
#pragma weak	plat_clk_get_rate
#pragma weak	plat_clk_get_rate_by_name
#pragma weak	plat_pcie_osc_set
#pragma weak	plat_pcie_osc

/*
 * CPU frequency interfaces.
 *
 * plat_cpu_get_speed: return current CPU frequency in Hz, 0 if unknown.
 * plat_cpu_get_speeds: enumerate supported frequencies in MHz (highest
 *     first).  Returns DDI_SUCCESS with *speeds and *nspeeds filled in,
 *     or DDI_ENOTSUP if the platform has no DVFS.  Caller must pair
 *     a successful call with plat_cpu_free_speeds.
 * plat_cpu_set_speed: transition to the given frequency in MHz.
 *     Returns DDI_SUCCESS or DDI_FAILURE.
 * plat_set_max_cpu_clock: boot-time call to set CPU to maximum speed
 *     before the first frequency reading.
 */
extern uint64_t plat_cpu_get_speed(cpu_t *cp);
extern int plat_cpu_get_speeds(cpu_t *cp, int **speeds, int *nspeeds);
extern void plat_cpu_free_speeds(int *speeds, int nspeeds);
extern int plat_cpu_set_speed(cpu_t *cp, int speed);
extern void plat_set_max_cpu_clock(int cpu_no);

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
