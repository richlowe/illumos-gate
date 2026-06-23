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

/*
 * Arm Architecture Service functions (DEN0028 Chapter 7).
 *
 * Thin wrappers around smccc32_call/smccc64_call that present typed
 * interfaces and return DDI error codes.  Each function gates on
 * smccc_available, which requires SMCCC 1.1 or later.  Callers
 * discover individual function support via smccc_arch_features before
 * use.
 *
 * There are some rules and complexities to using these functions,
 * so callers should consult DEN0028 before using these functions.
 *
 * Specification references are against DEN0028 v1.7.
 */

#include <sys/types.h>
#include <sys/smccc.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/debug.h>
#include <sys/systm.h>

/*
 * Translate a DEN0028 firmware return code to a DDI error.
 */
static int
smccc_to_ddi(int32_t fw_ret)
{
	switch (fw_ret) {
	case SMCCC_SUCCESS:
		return (DDI_SUCCESS);
	case SMCCC_NOT_SUPPORTED:
		return (DDI_ENOTSUP);
	case SMCCC_NOT_REQUIRED:
		return (DDI_ENOTSUP);
	case SMCCC_INVALID_PARAMETER:
		return (DDI_EINVAL);
	case SMCCC_RATE_LIMITED:	/* fallthrough */
	case SMCCC_BUSY:
		return (DDI_EAGAIN);
	default:
		return (DDI_FAILURE);
	}
}

/*
 * SMCCC_ARCH_FEATURES (§7.3)
 *
 * Query whether a specific Arm Architecture Service function is implemented
 * and retrieve its feature flags.
 *
 * The raw firmware return is passed through *resultp for per-function
 * interpretation: negative = not implemented, 0 = implemented,
 * positive = implemented with feature flags.
 */
int
smccc_arch_features(uint32_t arch_func_id, int32_t *resultp)
{
	smccc32_args_t args = {
		.w = { SMCCC_ARCH_FEATURES_FID, arch_func_id },
	};
	int rv;

	if (!smccc_available()) {
		return (DDI_ENOTSUP);
	}

	rv = smccc32_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if (resultp != NULL) {
		*resultp = (int32_t)args.w[0];
	}

	return (DDI_SUCCESS);
}

/*
 * SMCCC_ARCH_SOC_ID (§7.4) - types 0 (version) and 1 (revision).
 *
 * Returns the silicon provider defined SoC identification value in *valuep.
 *
 * Use SMCCC_SOC_ID_VERSION or SMCCC_SOC_ID_REVISION for soc_id_type.
 *
 * SMCCC_SOC_ID_NAME is implemented in smccc_arch_soc_id_name.
 */
int
smccc_arch_soc_id(uint32_t soc_id_type, uint32_t *valuep)
{
	smccc32_args_t args = {
		.w = { SMCCC_ARCH_SOC_ID_FID, soc_id_type },
	};
	int rv;

	VERIFY3U(soc_id_type, <=, 1);
	VERIFY3P(valuep, !=, NULL);

	if (!smccc_available()) {
		return (DDI_ENOTSUP);
	}

	rv = smccc32_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)args.w[0] < 0) {
		return (smccc_to_ddi((int32_t)args.w[0]));
	}

	*valuep = args.w[0];
	return (DDI_SUCCESS);
}

/*
 * SMCCC_ARCH_SOC_ID type 2 (§7.4) - SoC name.
 *
 * Returns the silicon provider defined SoC name as a null-terminated UTF-8
 * string in the passed buffer structure pointer.
 */
int
smccc_arch_soc_id_name(smccc_soc_name_t *namep)
{
	smccc64_args_t args = {
		.x = { SMCCC_ARCH_SOC_ID64_FID, SMCCC_SOC_ID_NAME },
	};
	int rv;

	VERIFY(namep != NULL);

	if (!smccc_available()) {
		return (DDI_ENOTSUP);
	}

	rv = smccc64_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)args.x[0] < 0) {
		return (smccc_to_ddi((int32_t)args.x[0]));
	}

	/*
	 * X1-X17 carry the UTF-8 string in LE byte order, no BOM, guaranteed
	 * NUL-terminated.
	 *
	 * 17 registers * 8 bytes = 136 bytes = SMCCC_SOC_NAME_MAXSZ.
	 */
	bcopy(&args.x[1], namep->sn_data, SMCCC_SOC_NAME_MAXSZ);
	return (DDI_SUCCESS);
}

/*
 * SMCCC_ARCH_WORKAROUND_1 (§7.5)
 *
 * Execute the firmware mitigation for CVE-2017-5715 on the calling PE.
 *
 * The firmware call has no return value.
 *
 * DO NOT JUST CALL THIS FUNCTION - read DEN0028 first.
 */
int
smccc_arch_workaround_1(void)
{
	smccc32_args_t args = {
		.w = { SMCCC_ARCH_WORKAROUND_1_FID },
	};

	if (!smccc_available()) {
		return (DDI_ENOTSUP);
	}

	return (smccc32_call(&args));
}

/*
 * SMCCC_ARCH_WORKAROUND_2 (§7.6)
 *
 * Enable or disable the firmware mitigation for CVE-2018-3639 on the calling
 * PE.  A non-zero enable value enables the mitigation.
 *
 * The firmware call has no return value.
 *
 * DO NOT JUST CALL THIS FUNCTION - read DEN0028 first.
 */
int
smccc_arch_workaround_2(uint32_t enable)
{
	smccc32_args_t args = {
		.w = { SMCCC_ARCH_WORKAROUND_2_FID, enable },
	};

	if (!smccc_available()) {
		return (DDI_ENOTSUP);
	}

	return (smccc32_call(&args));
}

/*
 * SMCCC_ARCH_WORKAROUND_3 (§7.7)
 *
 * Execute the firmware mitigation for CVE-2017-5715 and CVE-2022-23960 on the
 * calling PE.  Supersedes WORKAROUND_1.
 *
 * The firmware call has no return value.
 *
 * DO NOT JUST CALL THIS FUNCTION - read DEN0028 first.
 */
int
smccc_arch_workaround_3(void)
{
	smccc32_args_t args = {
		.w = { SMCCC_ARCH_WORKAROUND_3_FID },
	};

	if (!smccc_available()) {
		return (DDI_ENOTSUP);
	}

	return (smccc32_call(&args));
}

/*
 * SMCCC_ARCH_FEATURE_AVAILABILITY (§7.8)
 *
 * Discover architectural features enabled by EL3 firmware for use by callers.
 *
 * The bitmask_selector identifies which set of features to query (encoded as
 * the opcode of the related system register, e.g. SCR_EL3, CPTR_EL3,
 * MDCR_EL3, MPAM3_EL3).
 */
int
smccc_arch_feature_availability(uint64_t bitmask_selector, uint64_t *bitmaskp)
{
	smccc64_args_t args = {
		.x = { SMCCC_ARCH_FEATURE_AVAILABILITY_FID, bitmask_selector },
	};
	int rv;

	VERIFY(bitmaskp != NULL);

	if (!smccc_available()) {
		return (DDI_ENOTSUP);
	}

	rv = smccc64_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)args.x[0] != SMCCC_SUCCESS) {
		return (smccc_to_ddi((int32_t)args.x[0]));
	}

	*bitmaskp = args.x[1];
	return (DDI_SUCCESS);
}

/*
 * SMCCC_ARCH_WORKAROUND_4 (§7.9)
 *
 * Presence check only.  The spec states that this function is a NOP and
 * should never be invoked; its presence via SMCCC_ARCH_FEATURES signals
 * that EL3 firmware mitigates CVE-2024-7881.
 *
 * Read DEN0028 to learn how this is supposed to be used.
 */
boolean_t
smccc_arch_workaround_4_present(void)
{
	int32_t result;

	if (smccc_arch_features(SMCCC_ARCH_WORKAROUND_4_FID, &result)
	    != DDI_SUCCESS) {
		return (B_FALSE);
	}

	return (result >= 0 ? B_TRUE : B_FALSE);
}

/*
 * SMCCC_ARCH_CLEAN_INV_MEMREGION (§7.10)
 *
 * Clean and invalidate all caches (including system caches) that hold
 * copies of locations in the specified physical address range.
 *
 * flags: SMCCC_CLEAN_INV_FLAG_DRYRUN to probe without flushing.
 *
 * Returns DDI_EAGAIN for RATE_LIMITED or BUSY.
 */
int
smccc_arch_clean_inv_memregion(uint64_t base, uint64_t length, uint64_t flags)
{
	smccc64_args_t args = {
		.x = {
			SMCCC_ARCH_CLEAN_INV_MEMREGION_FID,
			base,
			length,
			flags,
		},
	};
	int rv;

	if (!smccc_available()) {
		return (DDI_ENOTSUP);
	}

	rv = smccc64_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)args.x[0] != SMCCC_SUCCESS) {
		return (smccc_to_ddi((int32_t)args.x[0]));
	}

	return (DDI_SUCCESS);
}

/*
 * SMCCC_ARCH_CLEAN_INV_MEMREGION_ATTRIBUTES (§7.11)
 *
 * Discover the attributes of the CLEAN_INV_MEMREGION function.
 * Mandatory if CLEAN_INV_MEMREGION is implemented.
 */
int
smccc_arch_clean_inv_memregion_attributes(smccc_memregion_attr_t *attrp)
{
	smccc64_args_t args = {
		.x = { SMCCC_ARCH_CLEAN_INV_MEMREGION_ATTR_FID },
	};
	int rv;

	VERIFY(attrp != NULL);

	if (!smccc_available()) {
		return (DDI_ENOTSUP);
	}

	rv = smccc64_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)args.x[0] != SMCCC_SUCCESS) {
		return (smccc_to_ddi((int32_t)args.x[0]));
	}

	/* X1: attr_flags, bit 0 = global flush */
	attrp->sma_global = (args.x[1] & 1) ? B_TRUE : B_FALSE;
	/* X2: attr_1[31:0] = worst-case latency in microseconds */
	attrp->sma_latency_us = (uint32_t)(args.x[2] & 0xffffffff);
	/* X3: attr_2 = max calls per second */
	attrp->sma_rate_limit = args.x[3];
	/* X4: attr_3 = break-even size in bytes */
	attrp->sma_breakeven_sz = args.x[4];

	return (DDI_SUCCESS);
}
