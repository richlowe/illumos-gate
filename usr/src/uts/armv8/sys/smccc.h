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

#ifndef _SYS_SMCCC_H
#define	_SYS_SMCCC_H

/*
 * SMC Calling Convention (SMCCC) per Arm DEN0028 v1.7.
 *
 * SMCCC provides the transport layer for firmware calls via SMC or HVC
 * instructions.  PSCI, PCI config space access (DEN0115) and ACPI FFH
 * OpRegions are clients of this interface.
 *
 * SMCCC includes a number of functions, none of which are required at
 * this time.
 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Argument/result structure for SMC32 calls.
 *
 * w[0] holds the function ID on entry; results are written back in place.
 * 32-bit calls use W0..W7 per SMCCC §5.1.
 */
typedef struct smccc32_args {
	uint32_t w[8];
} smccc32_args_t;

/*
 * Argument/result structure for SMC64 calls.
 *
 * x[0] holds the function ID on entry; results are written back in place.
 * 64-bit calls use X0..X17 per SMCCC §5.2.
 */
typedef struct smccc64_args {
	uint64_t x[18];
} smccc64_args_t;

/*
 * Well-known SMCCC function IDs used during discovery.
 */
#define	SMCCC_VERSION_FID		0x80000000
#define	SMCCC_ARCH_FEATURES_FID		0x80000001

/*
 * PSCI function IDs used internally by SMCCC for discovery.
 *
 * These are private to the SMCCC implementation.
 */
#define	SMCCC_PSCI_VERSION_FID		0x84000000
#define	SMCCC_PSCI_FEATURES_FID		0x8400000A

struct xboot_info;

extern int smccc_init(struct xboot_info *);

#if !defined(_BOOT)
extern int smccc32_call(smccc32_args_t *);
extern int smccc64_call(smccc64_args_t *);
extern boolean_t smccc_available(void);
extern uint32_t smccc_version(void);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SMCCC_H */
