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
 * CPU power management driver - aarch64 FDT machine-dependent support.
 *
 * Provides the cpudrv_mach interface for FDT (non-SBBR) platforms.
 * On attach, cpudrv_mach_init() parses the optional idle-states binding
 * (devicetree.org idle-states.yaml) and registers discovered states with
 * the cpuidle framework.
 *
 * Frequency scaling (DVFS) is not yet supported.  cpudrv_is_enabled()
 * returns B_FALSE for per-instance calls so the common cpudrv PM
 * governor is not started.  When platform DVFS is added (e.g. RPi4
 * via platmod hooks), cpudrv_is_enabled() and CPUDRV_GET_SPEEDS will
 * be updated to provide real speed levels.
 */

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/esunddi.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/cpuinfo.h>
#include <sys/cpuidle.h>
#include <sys/cpupm.h>
#include <sys/cpudrv_mach.h>
#include <sys/obpdefs.h>
#include <sys/machsystm.h>

/*
 * Check whether a dev_info node has "arm,idle-state" in its compatible
 * property.
 */
static boolean_t
cpudrv_is_idle_state(dev_info_t *dip)
{
	char **compat;
	uint_t ncompat;
	uint_t i;
	boolean_t found;

	ASSERT3P(dip, !=, NULL);

	if (ddi_prop_lookup_string_array(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, OBP_COMPATIBLE, &compat, &ncompat) !=
	    DDI_PROP_SUCCESS) {
		return (B_FALSE);
	}

	found = B_FALSE;
	for (i = 0; i < ncompat; i++) {
		if (strcmp(compat[i], "arm,idle-state") == 0) {
			found = B_TRUE;
			break;
		}
	}

	ddi_prop_free(compat);
	return (found);
}

/*
 * Check whether a state node is enabled (status absent or "okay"/"ok").
 */
static boolean_t
cpudrv_state_enabled(dev_info_t *dip)
{
	char *status;
	boolean_t enabled;

	ASSERT3P(dip, !=, NULL);

	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "status", &status) != DDI_PROP_SUCCESS) {
		/* absent status defaults to "okay" */
		return (B_TRUE);
	}

	enabled = (strcmp(status, "okay") == 0 ||
	    strcmp(status, "ok") == 0);
	ddi_prop_free(status);
	return (enabled);
}

/*
 * Parse a single idle-state node into an lpi_state_t.
 *
 * Returns B_TRUE on success, B_FALSE if the node should be skipped
 * (missing required properties, disabled, etc).
 */
static boolean_t
cpudrv_parse_one_state(dev_info_t *state_dip, lpi_state_t *lsp)
{
	int psci_param;
	int entry_lat;
	int exit_lat;
	int min_res;
	int wake_lat;

	ASSERT3P(state_dip, !=, NULL);
	ASSERT3P(lsp, !=, NULL);

	/* Required: arm,psci-suspend-param */
	psci_param = ddi_prop_get_int(DDI_DEV_T_ANY, state_dip,
	    DDI_PROP_DONTPASS, "arm,psci-suspend-param", -1);
	if (psci_param == -1) {
		return (B_FALSE);
	}

	/* Required: entry-latency-us */
	entry_lat = ddi_prop_get_int(DDI_DEV_T_ANY, state_dip,
	    DDI_PROP_DONTPASS, "entry-latency-us", -1);
	if (entry_lat == -1) {
		return (B_FALSE);
	}

	/* Required: exit-latency-us */
	exit_lat = ddi_prop_get_int(DDI_DEV_T_ANY, state_dip,
	    DDI_PROP_DONTPASS, "exit-latency-us", -1);
	if (exit_lat == -1) {
		return (B_FALSE);
	}

	/* Required: min-residency-us */
	min_res = ddi_prop_get_int(DDI_DEV_T_ANY, state_dip,
	    DDI_PROP_DONTPASS, "min-residency-us", -1);
	if (min_res == -1) {
		return (B_FALSE);
	}

	/* Optional: wakeup-latency-us (defaults to entry + exit) */
	wake_lat = ddi_prop_get_int(DDI_DEV_T_ANY, state_dip,
	    DDI_PROP_DONTPASS, "wakeup-latency-us", entry_lat + exit_lat);

	lsp->ls_psci_state = (uint32_t)psci_param;
	lsp->ls_min_residency = (uint32_t)min_res;
	lsp->ls_wake_latency = (uint32_t)wake_lat;
	lsp->ls_entry_type = LPI_ENTRY_PSCI;
	lsp->ls_enabled = B_TRUE;

	/*
	 * Context loss flags.  The devicetree binding doesn't carry explicit
	 * context loss bitmasks like ACPI _LPI - infer from the available
	 * properties.
	 */
	lsp->ls_ctx_loss_flags = 0;

	if (ddi_prop_exists(DDI_DEV_T_ANY, state_dip, DDI_PROP_DONTPASS,
	    "local-timer-stop")) {
		lsp->ls_ctx_loss_flags |= LPI_CTX_LOSS_TIMER;
	}

	if ((uint32_t)psci_param & PSCI_STATE_TYPE_POWERDOWN) {
		lsp->ls_ctx_loss_flags |= LPI_CTX_LOSS_CPU | LPI_CTX_LOSS_GICR;
	}

	return (B_TRUE);
}

/*
 * Parse all idle states referenced by this CPU's cpu-idle-states phandle
 * array.  Each phandle is a nodeid in the FDT prom emulation layer and
 * is resolved directly to a dev_info_t via e_ddi_nodeid_to_dip().
 *
 * Returns 0 on success with *statesp and *nstatesp set, -1 if no idle
 * states are available (not an error - cpuidle provides a WFI fallback).
 */
static int
cpudrv_parse_idle_states(dev_info_t *cpu_dip, lpi_state_t **statesp,
    int *nstatesp)
{
	int *phandles;
	uint_t nphandles;
	lpi_state_t *states;
	lpi_state_t *final;
	dev_info_t *state_dip;
	int nvalid;
	uint_t i;

	ASSERT3P(cpu_dip, !=, NULL);
	ASSERT3P(statesp, !=, NULL);
	ASSERT3P(nstatesp, !=, NULL);

	/* Read the cpu-idle-states phandle array from this CPU node */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, cpu_dip,
	    DDI_PROP_DONTPASS, "cpu-idle-states", &phandles,
	    &nphandles) != DDI_PROP_SUCCESS) {
		return (-1);
	}

	if (nphandles == 0) {
		ddi_prop_free(phandles);
		return (-1);
	}

	states = kmem_zalloc(nphandles * sizeof (lpi_state_t), KM_SLEEP);
	nvalid = 0;

	for (i = 0; i < nphandles; i++) {
		/* Phandle is a nodeid - resolve directly to dip */
		state_dip = e_ddi_nodeid_to_dip((pnode_t)phandles[i]);
		if (state_dip == NULL) {
			continue;
		}

		if (!cpudrv_is_idle_state(state_dip) ||
		    !cpudrv_state_enabled(state_dip)) {
			ndi_rele_devi(state_dip);
			continue;
		}

		if (cpudrv_parse_one_state(state_dip, &states[nvalid])) {
			nvalid++;
		}

		ndi_rele_devi(state_dip);
	}

	ddi_prop_free(phandles);

	if (nvalid == 0) {
		kmem_free(states, nphandles * sizeof (lpi_state_t));
		return (-1);
	}

	/* Shrink allocation to fit */
	if (nvalid < (int)nphandles) {
		final = kmem_alloc(nvalid * sizeof (lpi_state_t), KM_SLEEP);
		memcpy(final, states, nvalid * sizeof (lpi_state_t));
		kmem_free(states, nphandles * sizeof (lpi_state_t));
		states = final;
	}

	*statesp = states;
	*nstatesp = nvalid;
	return (0);
}

/*
 * Determine the cpu_id for the CPU device.
 */
boolean_t
cpudrv_get_cpu_id(dev_info_t *dip, processorid_t *cpu_id)
{
	uint64_t mpidr;
	processorid_t id;

	ASSERT3P(dip, !=, NULL);
	ASSERT3P(cpu_id, !=, NULL);

	if (mach_cpu_dip_to_mpidr(dip, &mpidr) != DDI_SUCCESS) {
		return (B_FALSE);
	}

	id = cpuinfo_id_for_mpidr(mpidr);
	if (id < 0) {
		return (B_FALSE);
	}

	*cpu_id = id;
	return (B_TRUE);
}

/*
 * Change CPU speed.  No DVFS support yet.
 */
/* ARGSUSED */
int
cpudrv_change_speed(cpudrv_devstate_t *cpudsp, cpudrv_pm_spd_t *new_spd)
{
	return (DDI_SUCCESS);
}

/*
 * All CPUs are always ready for power transitions.
 */
/* ARGSUSED */
boolean_t
cpudrv_power_ready(cpu_t *cp)
{
	return (B_TRUE);
}

/*
 * No governor thread on aarch64.
 */
/* ARGSUSED */
boolean_t
cpudrv_is_governor_thread(cpudrv_pm_t *cpupm)
{
	return (B_FALSE);
}

/*
 * Machine-dependent initialization.
 *
 * Parses idle states from the FDT cpu-idle-states binding and registers
 * them with the cpuidle framework.  If no idle states are found, cpuidle
 * provides a WFI fallback.
 */
boolean_t
cpudrv_mach_init(cpudrv_devstate_t *cpudsp)
{
	lpi_state_t *states;
	int nstates;

	ASSERT3P(cpudsp, !=, NULL);

	states = NULL;
	nstates = 0;
	(void) cpudrv_parse_idle_states(cpudsp->dip, &states, &nstates);

	/* Register with cpuidle framework (takes ownership of states) */
	(void) cpuidle_register_states(cpudsp->cpu_id, states, nstates);

	return (B_TRUE);
}

/*
 * Machine-dependent cleanup (detach).
 */
/* ARGSUSED */
boolean_t
cpudrv_mach_fini(cpudrv_devstate_t *cpudsp)
{
	return (B_TRUE);
}

/*
 * Check whether cpudrv PM is enabled.
 *
 * When called with NULL (from the global attach gate), returns B_TRUE
 * so the driver attaches and cpudrv_mach_init() can register idle
 * states.  When called with a per-instance cpudsp (from the PM setup
 * block), returns B_FALSE to skip the speed governor (no DVFS yet).
 */
/* ARGSUSED */
boolean_t
cpudrv_is_enabled(cpudrv_devstate_t *cpudsp)
{
	if (cpudsp == NULL) {
		return (B_TRUE);
	}

	return (B_FALSE);
}

/*
 * Set supported frequencies.  No DVFS yet.
 */
/* ARGSUSED */
void
cpudrv_set_supp_freqs(cpudrv_devstate_t *cpudsp)
{

}
