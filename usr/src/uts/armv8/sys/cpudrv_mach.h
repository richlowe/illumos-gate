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
 * Copyright 2026 Michael van der Westhuizen
 */

#ifndef _SYS_CPUDRV_MACH_H
#define	_SYS_CPUDRV_MACH_H

#include <sys/cpudrv.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * aarch64 cpudrv machine-dependent definitions.
 *
 * Follows the sun4u model: no PPM, no dynamic topspeed redefinition.
 * The driver attaches to CPU device nodes, delegates idle state
 * registration to cpudrv_mach_init(), and delegates DVFS to the
 * platmod via cpudrv_mach_get_speeds/cpudrv_mach_set_speed wrappers.
 * When no platmod DVFS is available, the PM governor is not started
 * and the driver provides idle management only.
 */

/*
 * Cross-call readiness check.  On aarch64 we consider all online CPUs
 * ready for cross-calls.
 */
#define	CPUDRV_XCALL_IS_READY(cpuid)	(B_TRUE)

/*
 * No governor thread on aarch64.
 */
#define	CPUDRV_RESET_GOVERNOR_THREAD(cpupm)

/*
 * No _PPC change handler on aarch64.
 */
#define	CPUDRV_INSTALL_MAX_CHANGE_HANDLER(cpudsp)
#define	CPUDRV_UNINSTALL_MAX_CHANGE_HANDLER(cpudsp)

/*
 * Topspeed is always the head speed.
 */
#define	CPUDRV_TOPSPEED(cpupm)	(cpupm)->head_spd

/*
 * No dynamic topspeed redefinition on aarch64.
 */
#define	CPUDRV_REDEFINE_TOPSPEED(dip)

/*
 * No PPM callbacks on aarch64.
 */
#define	CPUDRV_SET_PPM_CALLBACKS()

/*
 * Speed enumeration via platmod.  cpudrv_mach_get_speeds() and
 * cpudrv_mach_free_speeds() are defined in cpudrv_mach.c and wrap
 * the platmod weak symbols with safe defaults.
 */
extern int cpudrv_mach_get_speeds(cpu_t *, int **, int *);
extern void cpudrv_mach_free_speeds(int *, int);

#define	CPUDRV_GET_SPEEDS(cpudsp, speeds, nspeeds) { \
	if (cpudrv_mach_get_speeds((cpudsp)->cp, &(speeds), \
	    (int *)&(nspeeds)) != DDI_SUCCESS) { \
		nspeeds = 0; \
	} \
}
#define	CPUDRV_FREE_SPEEDS(speeds, nspeeds) \
	cpudrv_mach_free_speeds(speeds, nspeeds)

/*
 * Idle and user watermark percentages.  These are only used by the
 * PM governor which is not active without DVFS, but must compile.
 */
#define	CPUDRV_IDLE_CNT_PERCENT(hwm, speeds, i) \
	(100 - ((100 - hwm) * speeds[i]))

#define	CPUDRV_USER_CNT_PERCENT(hwm, speeds, i) \
	((hwm * speeds[i - 1]) / speeds[i])

/*
 * pm-components property formatting.  Not used without DVFS, but
 * referenced by cpudrv_comp_create() which must compile.
 */
#define	CPUDRV_COMP_SIZE()	\
	(CPUDRV_COMP_MAX_DIG + 1 + CPUDRV_COMP_MAX_DIG + 3 + 1);
#define	CPUDRV_COMP_SPEED(cpupm, cur_spd) cur_spd->speed;
#define	CPUDRV_COMP_SPRINT(pmc, cpupm, cur_spd, comp_spd)	\
	(void) sprintf(pmc, "%d=%dMHz", cur_spd->pm_level, comp_spd);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_CPUDRV_MACH_H */
