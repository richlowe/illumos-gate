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
 */

#include <sys/types.h>
#include <sys/psci.h>
#include <sys/smccc.h>
#include <sys/promif.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/systm.h>
#include <sys/bootinfo.h>

static uint32_t pcsi_version_id = 0x84000000;
static uint32_t psci_cpu_suspend_id = PSCI_CPU_SUSPEND_ID;
static uint32_t psci_cpu_off_id = PSCI_CPU_OFF_ID;
static uint32_t psci_cpu_on_id = PSCI_CPU_ON_ID;
static uint32_t psci_affinity_info_id = 0xc4000004;
static uint32_t psci_migrate_id = PSCI_MIGRATE_ID;
static uint32_t psci_migrate_info_type_id = 0x84000006;
static uint32_t psci_migrate_info_up_cpu_id = 0xc4000007;
static uint32_t psci_system_off_id = 0x84000008;
static uint32_t psci_system_reset_id = 0x84000009;
static uint32_t psci_features_id = 0x8400000a;
static uint32_t psci_cpu_freeze_id = 0x8400000b;
static uint32_t psci_cpu_default_suspend_id = 0xc400000c;
static uint32_t psci_node_hw_state_id = 0xc400000d;
static uint32_t psci_system_suspend_id = 0xc400000e;
static uint32_t psci_set_suspend_mode_id = 0x8400000f;
static uint32_t psci_stat_residency_id = 0xc4000010;
static uint32_t psci_stat_count_id = 0xc4000011;

boolean_t psci_initialized = B_FALSE;

/*
 * Internal helper: issue an SMC32 PSCI call via SMCCC.
 *
 * Wraps smccc32_call with PSCI-specific initialization checks and
 * panic-path handling.  Returns the firmware result from w[0].
 */
static uint32_t
psci_call32(uint32_t fid, uint32_t a1, uint32_t a2, uint32_t a3)
{
	int rv;
	smccc32_args_t args = {
		.w = { fid, a1, a2, a3 }
	};

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
		return (0);
	}

	rv = smccc32_call(&args);
	if (rv != DDI_SUCCESS) {
		prom_printf("ERROR: SMCCC transport failed for PSCI "
		    "call 0x%x (rv=%d)\n", fid, rv);
		return (0);
	}

	return (args.w[0]);
}

/*
 * Internal helper: issue an SMC64 PSCI call via SMCCC.
 *
 * Wraps smccc64_call with PSCI-specific initialization checks and
 * panic-path handling.  Returns the firmware result from x[0].
 */
static uint64_t
psci_call64(uint32_t fid, uint64_t a1, uint64_t a2, uint64_t a3)
{
	int rv;
	smccc64_args_t args = {
		.x = { fid, a1, a2, a3 }
	};

	if (!panicstr) {
		VERIFY(psci_initialized);
	}

	if (panicstr != NULL && !psci_initialized) {
		prom_printf("ERROR: attempted PSCI call before "
		    "it's initialized\n");
		return (0);
	}

	rv = smccc64_call(&args);
	if (rv != DDI_SUCCESS) {
		prom_printf("ERROR: SMCCC transport failed for PSCI "
		    "call 0x%x (rv=%d)\n", fid, rv);
		return (0);
	}

	return (args.x[0]);
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
	 * should not have been passed to UNIX).
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

uint32_t
psci_version(void)
{
	return (psci_call32(pcsi_version_id, 0, 0, 0));
}

int32_t
psci_cpu_suspend(uint32_t power_state, uint64_t entry_point_address,
    uint64_t context_id)
{
	return (psci_call64(psci_cpu_suspend_id, power_state,
	    entry_point_address, context_id));
}

int32_t
psci_cpu_off(void)
{
	return (psci_call32(psci_cpu_off_id, 0, 0, 0));
}

int32_t
psci_cpu_on(uint64_t target_cpu, uint64_t entry_point_address,
    uint64_t context_id)
{
	return (psci_call64(psci_cpu_on_id, target_cpu, entry_point_address,
	    context_id));
}

int32_t
psci_affinity_info(uint64_t target_affinity, uint32_t lowest_affinity_level)
{
	return (psci_call64(psci_affinity_info_id, target_affinity,
	    lowest_affinity_level, 0));
}

int32_t
psci_migrate(uint64_t target_cpu)
{
	return (psci_call64(psci_migrate_id, target_cpu, 0, 0));
}

int32_t
psci_migrate_info_type(void)
{
	return (psci_call32(psci_migrate_info_type_id, 0, 0, 0));
}

uint64_t
psci_migrate_info_up_cpu(void)
{
	return (psci_call64(psci_migrate_info_up_cpu_id, 0, 0, 0));
}

void
psci_system_off(void)
{
	psci_call32(psci_system_off_id, 0, 0, 0);
}

void
psci_system_reset(void)
{
	psci_call32(psci_system_reset_id, 0, 0, 0);

	/* If we've got here we were asked to reset and could not, spin. */
	for (;;) {
		__asm__("wfi");
	}
}

int32_t
psci_features(uint32_t psci_func_id)
{
	return (psci_call32(psci_features_id, psci_func_id, 0, 0));
}

int32_t
psci_cpu_freeze(void)
{
	return (psci_call32(psci_cpu_freeze_id, 0, 0, 0));
}

int32_t
psci_cpu_default_suspend(uint64_t entry_point_address, uint64_t context_id)
{
	return (psci_call64(psci_cpu_default_suspend_id, entry_point_address,
	    context_id, 0));
}

int32_t
psci_node_hw_state(uint64_t target_cpu, uint32_t power_level)
{
	return (psci_call64(psci_node_hw_state_id, target_cpu, power_level,
	    0));
}

int32_t
psci_system_suspend(uint64_t entry_point_address, uint64_t context_id)
{
	return (psci_call64(psci_system_suspend_id, entry_point_address,
	    context_id, 0));
}

int32_t
psci_set_suspend_mode(uint32_t mode)
{
	return (psci_call32(psci_set_suspend_mode_id, mode, 0, 0));
}

uint64_t
psci_stat_residency(uint64_t target_cpu, uint32_t power_state)
{
	return (psci_call64(psci_stat_residency_id, target_cpu,
	    power_state, 0));
}

uint64_t
psci_stat_count(uint64_t target_cpu, uint32_t power_state)
{
	return (psci_call64(psci_stat_count_id, target_cpu, power_state, 0));
}
