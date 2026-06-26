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

#ifndef	_SYS_CPUIDLE_H
#define	_SYS_CPUIDLE_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CPU idle state descriptor.
 *
 * Common currency between the cpuidle framework and firmware-specific drivers
 * (cpudrv for devicetree with idle-states, and a separate cpudrv for ACPI).
 *
 * The entry method is encoded as an entry type and a PSCI power_state
 * parameter (for PSCI-based states).
 */

/* Entry types */
#define	LPI_ENTRY_WFI		0	/* ARM WFI instruction */
#define	LPI_ENTRY_PSCI		1	/* PSCI CPU_SUSPEND */

typedef struct lpi_state {
	uint32_t	ls_min_residency;	/* Min residency (us) */
	/* Worst-case wake latency (us) */
	uint32_t	ls_wake_latency;
	uint32_t	ls_ctx_loss_flags;	/* LPI_CTX_LOSS_* flags */
	/* PSCI CPU_SUSPEND power_state */
	uint32_t	ls_psci_state;
	uint8_t		ls_entry_type;		/* LPI_ENTRY_* */
	boolean_t	ls_enabled;		/* State is usable */
	/* Runtime counters - written only from the owning CPU's idle thread */
	uint64_t	ls_entries;		/* Times state was entered */
	uint64_t	ls_residency_ns;	/* Total time in this state */
} lpi_state_t;

/*
 * PSCI CPU_SUSPEND power_state field encoding (PSCI 1.0+ extended format).
 *   Bit 30: StateType  0 = standby, 1 = powerdown
 */
#define	PSCI_STATE_TYPE_POWERDOWN	(1U << 30)

/*
 * Register idle states for a CPU.
 */
extern int cpuidle_register_states(processorid_t cpuid,
    lpi_state_t *states, int nstates);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_CPUIDLE_H */
