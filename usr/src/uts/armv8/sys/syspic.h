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
 * Copyright 2025 Michael van der Westhuizen
 */

#ifndef _SYSPIC_H
#define	_SYSPIC_H

/*
 * System Programmable Interrupt Controller interfaces
 *
 * Interfaces to process interrupts signalled by the Arm CPU.
 */

#include <sys/types.h>
#include <sys/cpuvar.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

typedef uint64_t intr_cookie_t;
typedef uint_t intr_intid_t;
typedef int intr_ipl_t;

/*
 * Perform per-CPU initialisation.
 */
extern void syspic_cpu_init(cpu_t *cp);

/*
 * Mask interrupts of priority lower than or equal to the passed intid.
 */
extern int syspic_intr_enter(intr_intid_t intid);

/*
 * Mask interrupts of priority lower than or equal to IPL.
 */
extern void syspic_intr_exit(intr_ipl_t ipl);

/*
 * Query the interrupt hardware to determine the interrupt that fired.
 *
 * Returns a cookie representing the hardware interrupt. This cookie can be
 * resolved to an interrupt vector number using `syspic_cookie_to_intid'.
 */
extern intr_cookie_t syspic_iack(void);

/*
 * Extract an interrupt ID (a vector number) from an interrupt cookie returned
 * by `syspic_iack'.
 */
extern intr_intid_t syspic_cookie_to_intid(intr_cookie_t cookie);

/*
 * Determine whether an interrupt ID represents a spurious interrupt.
 */
extern boolean_t syspic_is_spurious(intr_intid_t intid);

/*
 * Mark the end-of-interrupt processing in the interrupt controller.
 *
 * The programming model followed is aligned with the Arm split-EOI
 * functionality, meaning that EOI simply adjusts the running priority mask
 * in the hardware without affecting the state of the delivered interrupt.
 */
extern void syspic_eoi(intr_cookie_t cookie);

/*
 * Mark the end of interrupt processing for this interrupt cookie.
 *
 * This call moves the interrupt from the active or active-and-pending state to
 * the inactive or pending state.
 */
extern void syspic_intr_deactivate(intr_cookie_t cookie);

/*
 * Send an inter-processor interrupt to the target CPU.
 */
extern void syspic_send_ipi(cpuset_t cpuset, intr_intid_t intid);

/*
 * Same as `syspic_send_ipi', but more ergonomic when targeting a single CPU.
 */
extern void syspic_send_ipi_one(cpu_t *cpu, intr_intid_t intid);

/*
 * Same as `syspic_send_ipi_one', but by cpu ID.
 */
extern void syspic_send_ipi_one_id(int cpuid, intr_intid_t intid);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYSPIC_H */
