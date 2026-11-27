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

#ifndef _SYS_ACPICPPC_H
#define	_SYS_ACPICPPC_H

/*
 * Collaborative Processor Performance Control (CPPC) - ACPI 6.5 §8.4.7.
 *
 * Evaluates per-CPU _CPC ACPI objects to discover performance capabilities
 * and provides the DVFS interface used by the SBBR cpudrv_mach driver.
 *
 * Three register transport types are supported:
 * - PCC (0x0A): Platform Communications Channel shared memory.
 *   The PCC subspace index is encoded in the Access Size field of
 *   the GAS, and the Address field is the byte offset within that
 *   subspace's shared memory region.
 * - SystemMemory (0x00): Direct MMIO at the physical address in
 *   the GAS Address field.
 * - FFH (0x7F): Functional Fixed Hardware.  On aarch64, per DEN0048D:
 *   - address 0x0 = AMEVCNTR0_EL0[0] (core cycle counter, delivered)
 *   - address 0x1 = AMEVCNTR0_EL0[1] (const freq counter, reference)
 *
 * Performance Domain Coordination
 *
 * The driver constructs performance domains from _PSD objects.  Each
 * domain groups the CPUs that share a hardware performance control
 * relationship and records the coordination type:
 * - HW_ALL (0xFD): Hardware-coordinated.  Each CPU gets its own
 *   single-member domain; the firmware handles cross-CPU coordination
 *   when the OS writes a Desired Performance value.
 * - SW_ALL (0xFC): All CPUs in the domain must be programmed with
 *   the same Desired Performance value.  The driver computes the
 *   maximum requested level across the domain and writes it to
 *   every member's register.
 * - SW_ANY (0xFE): Any CPU in the domain may write on behalf of
 *   the domain.  The driver computes the domain maximum and writes
 *   to the requesting CPU only; the platform applies domain-wide.
 *
 * SW_ALL and SW_ANY domains are constructed and operational, but
 * have not been validated on hardware and emit a CE_WARN at attach
 * time.  HW_ALL is the tested path.
 */

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cpu;	/* forward declaration */

/*
 * Initialize a CPU's CPPC state from its _CPC ACPI object.
 *
 * Called from cpudrv_mach_init during ACPI0007 device attach.
 * Evaluates _CPC on the given ACPI handle, parses all 23 entries,
 * and allocates per-CPU state in the machcpu.  Also evaluates _PSD
 * to determine the coordination domain; the CPU is placed into an
 * appropriate domain before CPPC is marked active.
 *
 * Returns DDI_SUCCESS if _CPC was parsed and CPPC is available for
 * this CPU, or DDI_FAILURE if _CPC is absent, malformed, or the
 * domain could not be joined.  DDI_FAILURE is not fatal - it simply
 * means DVFS is not available on this CPU.
 */
extern int cppc_cpu_init(struct cpu *cp, void *acpi_hdl);

/*
 * Returns B_TRUE if CPPC was successfully initialized for this CPU
 * and DVFS is available.
 */
extern boolean_t cppc_cpu_available(struct cpu *cp);

/*
 * Retrieve the set of supported speeds (in MHz) for this CPU.
 *
 * The speed table is synthesized from the CPPC performance range
 * using anchor points at Highest, Nominal, LowestNonlinear, and
 * Lowest performance levels, with interpolated midpoints between
 * adjacent anchors when the gap is large enough.  See the block
 * comment in cppc.c for the full derivation.
 *
 * On success, *speeds is allocated via kmem_alloc and *nspeeds is
 * set to the number of entries.  The array is sorted in descending
 * order (highest frequency first).  The caller must free it with
 * cppc_free_speeds().
 *
 * Returns DDI_SUCCESS or DDI_FAILURE.
 */
extern int cppc_get_speeds(struct cpu *cp, int **speeds, int *nspeeds);

/*
 * Free a speed table returned by cppc_get_speeds().
 */
extern void cppc_free_speeds(int *speeds, int nspeeds);

/*
 * Set the desired CPU speed in MHz.
 *
 * Converts the MHz value to an abstract CPPC performance level,
 * aggregates across the coordination domain (max policy), and
 * writes the domain target to the appropriate DesiredPerformance
 * register(s) via the appropriate transport (PCC, SystemMemory).
 *
 * Returns DDI_SUCCESS or DDI_FAILURE.
 */
extern int cppc_set_speed(struct cpu *cp, int speed_mhz);

/*
 * Get the current delivered CPU speed in MHz.
 *
 * Where possible (FFH or PCC delivered/reference counters), this
 * returns the actual delivered frequency.  Otherwise, returns the
 * last requested speed.
 *
 * Returns the speed in MHz, or 0 on error.
 */
extern uint64_t cppc_get_speed(struct cpu *cp);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_ACPICPPC_H */
