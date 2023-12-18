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
 * Copyright 2023 Michael van der Westhuizen
 */

#ifndef _PSCIINFO_H
#define	_PSCIINFO_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Default PSCI function identifiers for PSCI 0.2 and later.
 */
#define	PSCI_CPU_SUSPEND_ID	0xC4000001
#define	PSCI_CPU_OFF_ID		0x84000002
#define	PSCI_CPU_ON_ID		0xC4000003
#define	PSCI_MIGRATE_ID		0xC4000005

typedef enum {
	/*
	 * Used internally to indicate that the PSCI information has not
	 * been initialised or was not found (or explicitly marked as not
	 * implemented) in the firmware tables.
	 *
	 * In devicetree there would simply be no PSCI node if PSCI was not
	 * implemented, while in ACPI the Arm Boot Architecture Flags would
	 * not have the PSCI_COMPLIANT bit set.
	 */
	PSCI_NOT_IMPLEMENTED	= 0,
	/*
	 * In the ACPI world the PSCI version can only be determined at runtime
	 * by calling the PSCI_VERSION function.
	 */
	PSCI_VERSION_DEFERRED	= 1,
	/*
	 * PSCI 0.1 will contain function identifier values for the four
	 * supported functions.
	 *
	 * There is no support for overriding function identifier values in
	 * ACPI.
	 */
	PSCI_VERSION_0_1	= 2,
	/*
	 * PSCI 0.2 and later do not allow function identifier value overrides.
	 */
	PSCI_VERSION_0_2	= 3,
	PSCI_VERSION_1_0	= 4,
	PSCI_VERSION_1_1	= 5,
	PSCI_VERSION_1_2	= 6,
	/*
	 * PSCI 1.3 is still being standardized (as of 2023-12-18).
	 */
	PSCI_VERSION_1_3	= 7,
} psci_version_t;

/*
 * The PSCI conduit indicates the trap type that the PSCI code should use to
 * access PSCI.
 *
 * We use these values to choose between Hypervisor calls (HVC) and Secure
 * Monitor calls (SMC).  The conduit is always provided by firmware, either
 * through the mandatory method property in the PSCI devicetree node or via the
 * PSCI_USE_HVC bit in the Arm Boot Architecture Flags (these are in the FADT
 * field called ARM_BOOT_ARCH).
 */
typedef enum {
	/*
	 * PSCI calls are made via "hvc #0".
	 */
	PSCI_CONDUIT_HVC	= 0,
	/*
	 * PSCI calls are made via "smc #0".
	 */
	PSCI_CONDUIT_SMC	= 1,
} psci_conduit_t;

struct psciinfo {
	psci_version_t		pi_version;
	psci_conduit_t		pi_conduit;
	/*
	 * Only populated from firmware in the PSCI 0.1 case and onlu ever on
	 * devicetree machines.
	 */
	uint32_t		pi_cpu_suspend_id;
	uint32_t		pi_cpu_off_id;
	uint32_t		pi_cpu_on_id;
	uint32_t		pi_migrate_id;
};

extern void psciinfo_init(void);
extern const struct psciinfo *psciinfo_get(void);

#ifdef __cplusplus
}
#endif

#endif /* _PSCIINFO_H */
