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
 * instructions.  SMCCC architectural functions, PSCI, PCI config space
 * access (DEN0115) and ACPI FFH OpRegions are clients of this interface.
 *
 * Specification references are against DEN0028 v1.7.
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
 * Arm Architecture Service function IDs (DEN0028 Chapter 7).
 *
 * VERSION and ARCH_FEATURES also serve as the discovery mechanism
 * for the remaining functions.
 */
#define	SMCCC_VERSION_FID			0x80000000
#define	SMCCC_ARCH_FEATURES_FID			0x80000001
#define	SMCCC_ARCH_SOC_ID_FID			0x80000002
#define	SMCCC_ARCH_SOC_ID64_FID			0xC0000002
#define	SMCCC_ARCH_WORKAROUND_1_FID		0x80008000
#define	SMCCC_ARCH_WORKAROUND_2_FID		0x80007FFF
#define	SMCCC_ARCH_WORKAROUND_3_FID		0x80003FFF
#define	SMCCC_ARCH_FEATURE_AVAILABILITY_FID	0xC0000003
#define	SMCCC_ARCH_WORKAROUND_4_FID		0x80000004
#define	SMCCC_ARCH_CLEAN_INV_MEMREGION_FID	0xC0000005
#define	SMCCC_ARCH_CLEAN_INV_MEMREGION_ATTR_FID	0xC0000006

/*
 * PSCI function IDs used internally by SMCCC for discovery.
 *
 * These are private to the SMCCC implementation.
 */
#define	SMCCC_PSCI_VERSION_FID		0x84000000
#define	SMCCC_PSCI_FEATURES_FID		0x8400000A

/*
 * DEN0028 return codes (§7.1).
 *
 * These are firmware return values; callers typically work with
 * DDI error codes via the wrapper functions below.
 */
#define	SMCCC_SUCCESS			0
#define	SMCCC_NOT_SUPPORTED		(-1)
#define	SMCCC_NOT_REQUIRED		(-2)
#define	SMCCC_INVALID_PARAMETER		(-3)
#define	SMCCC_RATE_LIMITED		(-4)
#define	SMCCC_BUSY			(-5)

/*
 * SMCCC_ARCH_SOC_ID type values (§7.4).
 */
#define	SMCCC_SOC_ID_VERSION		0
#define	SMCCC_SOC_ID_REVISION		1
#define	SMCCC_SOC_ID_NAME		2

/*
 * Buffer for SMCCC_ARCH_SOC_ID type 2 (SoC name).
 *
 * The name is a null-terminated UTF-8 string occupying at most
 * 136 bytes including the terminator, packed across X1-X17.
 */
#define	SMCCC_SOC_NAME_MAXSZ		136

typedef struct smccc_soc_name {
	char	sn_data[SMCCC_SOC_NAME_MAXSZ];
} smccc_soc_name_t;

/*
 * Attributes returned by SMCCC_ARCH_CLEAN_INV_MEMREGION_ATTRIBUTES (§7.11).
 */
typedef struct smccc_memregion_attr {
	boolean_t	sma_global;		/* global flush (all caches) */
	uint32_t	sma_latency_us;		/* worst-case latency (us) */
	uint64_t	sma_rate_limit;		/* max calls/sec */
	uint64_t	sma_breakeven_sz;	/* break-even size */
} smccc_memregion_attr_t;

/*
 * SMCCC_ARCH_CLEAN_INV_MEMREGION flags (§7.10).
 */
#define	SMCCC_CLEAN_INV_FLAG_DRYRUN	0x1

struct xboot_info;

extern int smccc_init(struct xboot_info *);

#if !defined(_BOOT)
extern int smccc32_call(smccc32_args_t *);
extern int smccc64_call(smccc64_args_t *);
extern boolean_t smccc_available(void);
extern uint32_t smccc_version(void);

/*
 * Arm Architecture Service wrappers (DEN0028 Chapter 7).
 *
 * Each returns DDI error codes.  Where the spec defines an overloaded
 * return value (e.g. ARCH_FEATURES), the raw firmware result is passed
 * through an out-parameter for per-function interpretation.
 */
extern int smccc_arch_features(uint32_t, int32_t *);
extern int smccc_arch_soc_id(uint32_t, uint32_t *);
extern int smccc_arch_soc_id_name(smccc_soc_name_t *);
extern int smccc_arch_workaround_1(void);
extern int smccc_arch_workaround_2(uint32_t);
extern int smccc_arch_workaround_3(void);
extern int smccc_arch_feature_availability(uint64_t, uint64_t *);
extern boolean_t smccc_arch_workaround_4_present(void);
extern int smccc_arch_clean_inv_memregion(uint64_t, uint64_t, uint64_t);
extern int smccc_arch_clean_inv_memregion_attributes(smccc_memregion_attr_t *);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SMCCC_H */
