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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2026 Michael van der Westhuizen
 */

/*
 * Kernel PSCI - client of the SMCCC transport layer.
 *
 * All firmware calls go through smccc32_call or smccc64_call which
 * handle conduit selection (SMC/HVC) and dispatch to the firmware.
 *
 * Public functions return DDI error codes for transport and PSCI-level
 * status.  Firmware result values (affinity states, version numbers,
 * feature flags, etc.) are returned via out parameters where needed.
 *
 * The PSCI error-to-DDI mapping is:
 *   PSCI_SUCCESS             ->  DDI_SUCCESS
 *   PSCI_NOT_SUPPORTED       ->  DDI_ENOTSUP
 *   PSCI_INVALID_PARAMETERS  ->  DDI_EINVAL
 *   PSCI_INVALID_ADDRESS     ->  DDI_EINVAL
 *   PSCI_DENIED              ->  DDI_FAILURE
 *   PSCI_ALREADY_ON          ->  DDI_EALREADY
 *   PSCI_ON_PENDING          ->  DDI_SUCCESS
 *   PSCI_INTERNAL_FAILURE    ->  DDI_FAILURE
 *   PSCI_NOT_PRESENT         ->  DDI_FAILURE
 *   PSCI_DISABLED            ->  DDI_FAILURE
 *
 * Any undocumented return code, and any return from a function not
 * documented to return on success that does not have the error bit
 * set, maps to DDI_FAILURE.
 *
 * Specification references in this module are against DEN0022F.b.
 */

#include <sys/types.h>
#include <sys/psci.h>
#include <sys/smccc.h>
#include <sys/promif.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/systm.h>
#include <sys/bootinfo.h>

/*
 * Function IDs.  SMC64 versions are preferred where available.
 */
static uint32_t psci_version_id = 0x84000000;			/* SMC32 */
static uint32_t psci_cpu_suspend_id = PSCI_CPU_SUSPEND_ID;	/* SMC64 */
static uint32_t psci_cpu_off_id = PSCI_CPU_OFF_ID;		/* SMC32 */
static uint32_t psci_cpu_on_id = PSCI_CPU_ON_ID;		/* SMC64 */
static uint32_t psci_affinity_info_id = 0xc4000004;		/* SMC64 */
static uint32_t psci_migrate_id = PSCI_MIGRATE_ID;		/* SMC64 */
static uint32_t psci_migrate_info_type_id = 0x84000006;		/* SMC32 */
static uint32_t psci_migrate_info_up_cpu_id = 0xc4000007;	/* SMC64 */
static uint32_t psci_system_off_id = 0x84000008;		/* SMC32 */
static uint32_t psci_system_reset_id = 0x84000009;		/* SMC32 */
static uint32_t psci_features_id = 0x8400000a;			/* SMC32 */
static uint32_t psci_cpu_freeze_id = 0x8400000b;		/* SMC32 */
static uint32_t psci_cpu_default_suspend_id = 0xc400000c;	/* SMC64 */
static uint32_t psci_node_hw_state_id = 0xc400000d;		/* SMC64 */
static uint32_t psci_system_suspend_id = 0xc400000e;		/* SMC64 */
static uint32_t psci_set_suspend_mode_id = 0x8400000f;		/* SMC32 */
static uint32_t psci_stat_residency_id = 0xc4000010;		/* SMC64 */
static uint32_t psci_stat_count_id = 0xc4000011;		/* SMC64 */
static uint32_t psci_system_reset2_id = 0xc4000012;		/* SMC64 */
static uint32_t psci_mem_protect_id = 0x84000013;		/* SMC32 */
static uint32_t psci_mem_protect_check_range_id = 0xc4000014;	/* SMC64 */
static uint32_t psci_system_off2_id = 0xc4000015;		/* SMC64 */

boolean_t psci_initialized = B_FALSE;

/*
 * Translate a PSCI error code (int32_t) to a DDI return value.
 */
static int
psci_to_ddi(int32_t psci_ret)
{
	switch (psci_ret) {
	case PSCI_SUCCESS:		/* fallthrough */
	case PSCI_ON_PENDING:
		return (DDI_SUCCESS);
	case PSCI_NOT_SUPPORTED:
		return (DDI_ENOTSUP);
	case PSCI_INVALID_PARAMETERS:	/* fallthrough */
	case PSCI_INVALID_ADDRESS:
		return (DDI_EINVAL);
	case PSCI_ALREADY_ON:
		return (DDI_EALREADY);
	case PSCI_DENIED:		/* fallthrough */
	case PSCI_INTERNAL_FAILURE:	/* fallthrough */
	case PSCI_NOT_PRESENT:		/* fallthrough */
	case PSCI_DISABLED:		/* fallthrough */
	default:
		return (DDI_FAILURE);
	}
}

/*
 * Common PSCI pre-call checks.
 *
 * Returns DDI_SUCCESS if the call may proceed, DDI_ETRANSPORT during
 * panic before initialization, or DDI_ENOTSUP for a zero function ID.
 */
static int
psci_check(uint32_t fid)
{
	/*
	 * We may get here very early if panicking; attempt to be useful
	 * and not panic recursively.  Unfortunately, we may also get here
	 * from `$q` under kmdb and panic rather than rebooting.
	 */
	if (!panicstr) {
		VERIFY(psci_initialized);
	}

	if (panicstr != NULL && !psci_initialized) {
		prom_printf("ERROR: attempted PSCI call before "
		    "it's initialized\n");
		return (DDI_ETRANSPORT);
	}

	if (fid == 0) {
		return (DDI_ENOTSUP);
	}

	return (DDI_SUCCESS);
}

/*
 * Internal helper: issue an SMC32 PSCI call via SMCCC.
 *
 * Returns a DDI error code for transport-level status.  On DDI_SUCCESS
 * the firmware result is stored in *resultp (if non-NULL).
 */
static int
psci_call32(uint32_t fid, uint32_t a1, uint32_t a2, uint32_t a3,
    uint32_t *resultp)
{
	int rv;
	smccc32_args_t args = {
		.w = { fid, a1, a2, a3 }
	};

	rv = psci_check(fid);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	rv = smccc32_call(&args);
	if (rv != DDI_SUCCESS) {
		prom_printf("ERROR: SMCCC transport failed for PSCI "
		    "call 0x%x (rv=%d)\n", fid, rv);
		return (DDI_ETRANSPORT);
	}

	if (resultp != NULL) {
		*resultp = args.w[0];
	}

	return (DDI_SUCCESS);
}

/*
 * Internal helper: issue an SMC64 PSCI call via SMCCC.
 *
 * Returns a DDI error code for transport-level status.  On DDI_SUCCESS
 * the firmware result is stored in *resultp (if non-NULL).
 */
static int
psci_call64(uint32_t fid, uint64_t a1, uint64_t a2, uint64_t a3,
    uint64_t *resultp)
{
	int rv;
	smccc64_args_t args = {
		.x = { fid, a1, a2, a3 }
	};

	rv = psci_check(fid);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	rv = smccc64_call(&args);
	if (rv != DDI_SUCCESS) {
		prom_printf("ERROR: SMCCC transport failed for PSCI "
		    "call 0x%x (rv=%d)\n", fid, rv);
		return (DDI_ETRANSPORT);
	}

	if (resultp != NULL) {
		*resultp = args.x[0];
	}

	return (DDI_SUCCESS);
}

int
psci_init(struct xboot_info *xbp)
{
	if (psci_initialized == B_TRUE) {
		return (DDI_SUCCESS);
	}

	/*
	 * The version field is:
	 * - [30:16]: Major version
	 * - [15:0] : Minor version
	 *
	 * If bit 31 is set, then the version represents an error value (and
	 * should not have been passed to unix).
	 */
	if (xbp == NULL || xbp->bi_psci_version & 0x80000000) {
		return (DDI_FAILURE);
	}

	/*
	 * Identifier overrides are only valid (and are optional) prior
	 * to PSCI 1.0.
	 */
	if (((xbp->bi_psci_version & 0x7FFF0000) >> 16) == 0) {
		if (xbp->bi_psci_cpu_suspend_id != 0) {
			psci_cpu_suspend_id = xbp->bi_psci_cpu_suspend_id;
		}

		if (xbp->bi_psci_cpu_off_id != 0) {
			psci_cpu_off_id = xbp->bi_psci_cpu_off_id;
		}

		if (xbp->bi_psci_cpu_on_id != 0) {
			psci_cpu_on_id = xbp->bi_psci_cpu_on_id;
		}

		if (xbp->bi_psci_migrate_id != 0) {
			psci_migrate_id = xbp->bi_psci_migrate_id;
		}
	}

	psci_initialized = B_TRUE;
	return (DDI_SUCCESS);
}

/*
 * PSCI_VERSION (§5.1.1)
 */
int
psci_version(uint32_t *versionp)
{
	uint32_t result;
	int rv;

	rv = psci_call32(psci_version_id, 0, 0, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	/*
	 * Bit 31 set indicates an error from the firmware.
	 */
	if (result & 0x80000000) {
		return (DDI_FAILURE);
	}

	if (versionp != NULL) {
		*versionp = result;
	}

	return (DDI_SUCCESS);
}

/*
 * CPU_SUSPEND (§5.1.2)
 *
 * Returns on wakeup from standby; does not return on powerdown (resumes
 * at entry_point_address).
 */
int
psci_cpu_suspend(uint32_t power_state, uint64_t entry_point_address,
    uint64_t context_id)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_cpu_suspend_id, power_state,
	    entry_point_address, context_id, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	return (psci_to_ddi((int32_t)result));
}

/*
 * CPU_OFF (§5.1.3)
 *
 * Does not return on success.
 */
int
psci_cpu_off(void)
{
	uint32_t result;
	int rv;

	rv = psci_call32(psci_cpu_off_id, 0, 0, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	/*
	 * CPU_OFF does not return on success.  Any return is an error.
	 */
	return (psci_to_ddi((int32_t)result));
}

/*
 * CPU_ON (§5.1.4)
 *
 * ALREADY_ON is mapped to DDI_EALREADY so that callers (mp_startup)
 * can distinguish it from SUCCESS - the core is already running and
 * will not go through the entry point as expected.
 *
 * ON_PENDING is mapped to DDI_SUCCESS - the call is async and a prior
 * CPU_ON is still in flight, which is functionally equivalent.
 */
int
psci_cpu_on(uint64_t target_cpu, uint64_t entry_point_address,
    uint64_t context_id)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_cpu_on_id, target_cpu,
	    entry_point_address, context_id, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	return (psci_to_ddi((int32_t)result));
}

/*
 * AFFINITY_INFO (§5.1.5)
 *
 * Mixes positive status values (0=ON, 1=OFF, 2=ON_PENDING) with
 * negative error codes, so the affinity state is returned via *statep.
 */
int
psci_affinity_info(uint64_t target_affinity, uint32_t lowest_affinity_level,
    uint32_t *statep)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_affinity_info_id, target_affinity,
	    lowest_affinity_level, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)result < 0) {
		return (psci_to_ddi((int32_t)result));
	}

	/*
	 * Documented values are 0 (ON), 1 (OFF), 2 (ON_PENDING).
	 * Any undocumented non-negative value is treated as DDI_FAILURE.
	 */
	if ((uint32_t)result > 2) {
		return (DDI_FAILURE);
	}

	if (statep != NULL) {
		*statep = (uint32_t)result;
	}

	return (DDI_SUCCESS);
}

/*
 * MIGRATE (§5.1.6)
 */
int
psci_migrate(uint64_t target_cpu)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_migrate_id, target_cpu, 0, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	return (psci_to_ddi((int32_t)result));
}

/*
 * MIGRATE_INFO_TYPE (§5.1.7)
 *
 * Mixes positive type values (0/1/2) with NOT_SUPPORTED.
 */
int
psci_migrate_info_type(uint32_t *typep)
{
	uint32_t result;
	int rv;

	rv = psci_call32(psci_migrate_info_type_id, 0, 0, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)result < 0) {
		return (psci_to_ddi((int32_t)result));
	}

	if (result > 2) {
		return (DDI_FAILURE);
	}

	if (typep != NULL) {
		*typep = result;
	}

	return (DDI_SUCCESS);
}

/*
 * MIGRATE_INFO_UP_CPU (§5.1.8)
 *
 * Returns an MPIDR value; only valid if MIGRATE_INFO_TYPE returns 0 or 1.
 */
int
psci_migrate_info_up_cpu(uint64_t *mpidp)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_migrate_info_up_cpu_id, 0, 0, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if (mpidp != NULL) {
		*mpidp = result;
	}

	return (DDI_SUCCESS);
}

/*
 * SYSTEM_OFF (§5.1.9)
 *
 * Does not return.
 */
void
psci_system_off(void)
{
	(void) psci_call32(psci_system_off_id, 0, 0, 0, NULL);
}

/*
 * SYSTEM_RESET (§5.1.11)
 *
 * Does not return.
 */
void
psci_system_reset(void)
{
	(void) psci_call32(psci_system_reset_id, 0, 0, 0, NULL);

	/* If we've got here we were asked to reset and could not, spin */
	for (;;) {
		__asm__("wfi");
	}
}

/*
 * PSCI_FEATURES (§5.1.15)
 *
 * NOT_SUPPORTED is an error; bit[31]=0 with bits[30:0] as feature flags.
 */
int
psci_features(uint32_t psci_func_id, uint32_t *flagsp)
{
	uint32_t result;
	int rv;

	rv = psci_call32(psci_features_id, psci_func_id, 0, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)result < 0) {
		return (psci_to_ddi((int32_t)result));
	}

	if (flagsp != NULL) {
		*flagsp = result;
	}

	return (DDI_SUCCESS);
}

/*
 * CPU_FREEZE (§5.1.16)
 *
 * Does not return on success.
 */
int
psci_cpu_freeze(void)
{
	uint32_t result;
	int rv;

	rv = psci_call32(psci_cpu_freeze_id, 0, 0, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	return (psci_to_ddi((int32_t)result));
}

/*
 * CPU_DEFAULT_SUSPEND (§5.1.17)
 */
int
psci_cpu_default_suspend(uint64_t entry_point_address, uint64_t context_id)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_cpu_default_suspend_id, entry_point_address,
	    context_id, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	return (psci_to_ddi((int32_t)result));
}

/*
 * NODE_HW_STATE (§5.1.18)
 *
 * Mixes positive state values (0=HW_ON, 1=HW_OFF, 2=HW_STANDBY) with
 * negative error codes.
 */
int
psci_node_hw_state(uint64_t target_cpu, uint32_t power_level,
    uint32_t *statep)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_node_hw_state_id, target_cpu, power_level,
	    0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)result < 0) {
		return (psci_to_ddi((int32_t)result));
	}

	if ((uint32_t)result > 2) {
		return (DDI_FAILURE);
	}

	if (statep != NULL) {
		*statep = (uint32_t)result;
	}

	return (DDI_SUCCESS);
}

/*
 * SYSTEM_SUSPEND (§5.1.19)
 *
 * Does not return on success.
 */
int
psci_system_suspend(uint64_t entry_point_address, uint64_t context_id)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_system_suspend_id, entry_point_address,
	    context_id, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	return (psci_to_ddi((int32_t)result));
}

/*
 * PSCI_SET_SUSPEND_MODE (§5.1.20)
 */
int
psci_set_suspend_mode(uint32_t mode)
{
	uint32_t result;
	int rv;

	rv = psci_call32(psci_set_suspend_mode_id, mode, 0, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	return (psci_to_ddi((int32_t)result));
}

/*
 * PSCI_STAT_RESIDENCY (§5.1.21)
 *
 * Returns microseconds spent in a given power state.
 */
int
psci_stat_residency(uint64_t target_cpu, uint32_t power_state,
    uint64_t *residencyp)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_stat_residency_id, target_cpu, power_state,
	    0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if (residencyp != NULL) {
		*residencyp = result;
	}

	return (DDI_SUCCESS);
}

/*
 * PSCI_STAT_COUNT (§5.1.22)
 *
 * Returns count of entries into a given power state.
 */
int
psci_stat_count(uint64_t target_cpu, uint32_t power_state,
    uint64_t *countp)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_stat_count_id, target_cpu, power_state,
	    0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if (countp != NULL) {
		*countp = result;
	}

	return (DDI_SUCCESS);
}

/*
 * SYSTEM_OFF2 (§5.1.10)
 *
 * PSCI 1.3.  Does not return on success.
 */
int
psci_system_off2(uint32_t type, uint64_t cookie)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_system_off2_id, type, cookie, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	/*
	 * SYSTEM_OFF2 does not return on success.  Any return is an error.
	 */
	return (psci_to_ddi((int32_t)result));
}

/*
 * SYSTEM_RESET2 (§5.1.12)
 *
 * PSCI 1.1.  Does not return on success.
 */
int
psci_system_reset2(uint32_t reset_type, uint64_t cookie)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_system_reset2_id, reset_type, cookie, 0,
	    &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	/*
	 * SYSTEM_RESET2 does not return on success.  Any return is an error.
	 */
	return (psci_to_ddi((int32_t)result));
}

/*
 * MEM_PROTECT (§5.1.13)
 *
 * PSCI 1.1.  On success returns previous state (0=disabled, 1=enabled).
 */
int
psci_mem_protect(uint32_t enable, uint32_t *prev_statep)
{
	uint32_t result;
	int rv;

	rv = psci_call32(psci_mem_protect_id, enable, 0, 0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)result < 0) {
		return (psci_to_ddi((int32_t)result));
	}

	if (result > 1) {
		return (DDI_FAILURE);
	}

	if (prev_statep != NULL) {
		*prev_statep = result;
	}

	return (DDI_SUCCESS);
}

/*
 * MEM_PROTECT_CHECK_RANGE (§5.1.14)
 *
 * PSCI 1.1.
 */
int
psci_mem_protect_check_range(uint64_t base, uint64_t length)
{
	uint64_t result;
	int rv;

	rv = psci_call64(psci_mem_protect_check_range_id, base, length,
	    0, &result);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	return (psci_to_ddi((int32_t)result));
}
