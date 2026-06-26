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
 * CPU idle framework for aarch64.
 *
 * This misc module provides a cpu_halt loop body (cpu_lpi_halt) and a
 * registration interface for firmware-specific idle state drivers.  On _init
 * it replaces the default CPU halt implementation with the cpu_lpi_halt
 * function pointer.  cpu_halt's outer loop handles the illumos-level heavy
 * lifting (haltset, disp_nrunnable and spurious-wakeup logic), this body
 * function handles "how should I halt?": state selection, idle callbacks,
 * and the actual WFI or PSCI CPU_SUSPEND.
 *
 * Per-CPU idle states are populated by firmware-specific drivers:
 * - cpudrv (devicetree): parses idle-states on DT cpu nodes
 * - cpudrv (ACPI): parses _LPI on ACPI0007 processor devices
 * Both call cpuidle_register_states() to hand their parsed state arrays
 * to this framework.
 *
 * At present we only support states with no architectural context loss.  In
 * future we will need to add resume trampolines and GIC/timer save-restore for
 * powerdown states.
 *
 * State selection is backward-looking (like i86pc): the duration of the
 * previous idle period is used to predict the next, and the deepest eligible
 * state whose minimum residency requirement is met is chosen.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/modctl.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/machcpuvar.h>
#include <sys/machsystm.h>
#include <sys/cpuidle.h>
#include <sys/cpuinfo.h>
#include <sys/cpu_event.h>
#include <sys/disp.h>
#include <sys/archsystm.h>
#include <sys/psci.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/sunddi.h>
#include <sys/systm.h>
#include <sys/kstat.h>
#include <util/qsort.h>

/* Per-CPU idle duration tracking, indexed by cpu_id */
static hrtime_t *lpi_last_idle;

/*
 * Per-CPU array of kstat pointers, one per idle state.
 *
 * Indexed as lpi_kstats[cpu_id][state_idx].
 */
static kstat_t ***lpi_kstats;

/*
 * kstat data template for a single idle state.
 */
typedef struct cpuidle_kstat {
	kstat_named_t	cik_type;		/* "WFI" or "PSCI" */
	kstat_named_t	cik_min_residency;	/* us */
	kstat_named_t	cik_wake_latency;	/* us */
	kstat_named_t	cik_ctx_loss;		/* LPI_CTX_LOSS_* flags */
	kstat_named_t	cik_entries;		/* times entered */
	kstat_named_t	cik_residency_ns;	/* total ns in this state */
} cpuidle_kstat_t;

static int
cpuidle_kstat_update(kstat_t *ksp, int rw)
{
	cpuidle_kstat_t *kd = ksp->ks_data;
	lpi_state_t *ls = ksp->ks_private;

	if (rw == KSTAT_WRITE) {
		return (EACCES);
	}

	kstat_named_setstr(&kd->cik_type,
	    ls->ls_entry_type == LPI_ENTRY_WFI ? "WFI" : "PSCI");
	kd->cik_min_residency.value.ui32 = ls->ls_min_residency;
	kd->cik_wake_latency.value.ui32 = ls->ls_wake_latency;
	kd->cik_ctx_loss.value.ui32 = ls->ls_ctx_loss_flags;
	kd->cik_entries.value.ui64 = ls->ls_entries;
	kd->cik_residency_ns.value.ui64 = ls->ls_residency_ns;

	return (0);
}

static void
cpuidle_kstat_create(processorid_t cpuid, lpi_state_t *states, int nstates)
{
	int i;
	char name[KSTAT_STRLEN];
	cpuidle_kstat_t *kd;
	kstat_t *ksp;

	ASSERT3P(cpu[cpuid], !=, NULL);

	lpi_kstats[cpuid] = kmem_zalloc(nstates * sizeof (kstat_t *),
	    KM_SLEEP);

	for (i = 0; i < nstates; i++) {
		(void) snprintf(name, sizeof (name), "state%d", i);

		kd = kmem_zalloc(sizeof (cpuidle_kstat_t), KM_SLEEP);
		kstat_named_init(&kd->cik_type, "type",
		    KSTAT_DATA_STRING);
		kstat_named_init(&kd->cik_min_residency, "min_residency_us",
		    KSTAT_DATA_UINT32);
		kstat_named_init(&kd->cik_wake_latency, "wake_latency_us",
		    KSTAT_DATA_UINT32);
		kstat_named_init(&kd->cik_ctx_loss, "ctx_loss_flags",
		    KSTAT_DATA_UINT32);
		kstat_named_init(&kd->cik_entries, "entries",
		    KSTAT_DATA_UINT64);
		kstat_named_init(&kd->cik_residency_ns, "residency_ns",
		    KSTAT_DATA_UINT64);

		ksp = kstat_create("cpuidle", cpuid, name, "misc",
		    KSTAT_TYPE_NAMED,
		    sizeof (cpuidle_kstat_t) / sizeof (kstat_named_t),
		    KSTAT_FLAG_VIRTUAL);

		if (ksp != NULL) {
			ksp->ks_data = kd;
			ksp->ks_private = &states[i];
			ksp->ks_update = cpuidle_kstat_update;
			ksp->ks_data_size += MAXNAMELEN;
			kstat_install(ksp);
			lpi_kstats[cpuid][i] = ksp;
		} else {
			kmem_free(kd, sizeof (cpuidle_kstat_t));
			cmn_err(CE_NOTE,
			    "!cpu%d: failed to create kstat %s", cpuid, name);
		}
	}
}

/*
 * State selection
 */

/*
 * Select the deepest eligible idle state for the current idle period.
 *
 * Walk from deepest to shallowest.  Skip states with context loss (for now)
 * or whose total commitment time (min_residency + wake_latency) exceeds the
 * predicted idle duration.  The commitment time is the minimum idle period
 * needed to enter the state, remain for the minimum useful residency, and
 * exit again.  For DT, wake_latency defaults to entry + exit latency, so
 * the threshold is entry + min_residency + exit.
 *
 * Always returns a valid index: index 0 (WFI) is the fallback.
 */
static int
lpi_select_state(lpi_state_t *states, int nstates, hrtime_t last_idle_ns)
{
	int i;
	hrtime_t threshold_ns;

	for (i = nstates - 1; i > 0; i--) {
		if (states[i].ls_ctx_loss_flags != 0) {
			continue;
		}

		if (!states[i].ls_enabled) {
			continue;
		}

		threshold_ns =
		    ((hrtime_t)states[i].ls_min_residency +
		    (hrtime_t)states[i].ls_wake_latency) *
		    NANOSEC / MICROSEC;

		if (last_idle_ns >= threshold_ns) {
			return (i);
		}
	}

	return (0);
}

/*
 * Loop body function
 */

/*
 * LPI-aware cpu_halt loop body.
 *
 * Called from inside cpu_halt()'s inner loop with interrupts disabled.  Selects
 * an idle state, enters cpu_idle callbacks, executes WFI or PSCI CPU_SUSPEND,
 * runs the exit callbacks, and returns.  cpu_halt handles interrupt unmasking
 * and loop re-evaluation.
 *
 * For CPUs whose idle states have not been populated (or that lack firmware
 * data entirely), falls back to a plain WFI.
 */
static void
cpu_lpi_halt(void)
{
	cpu_t *cpup = CPU;
	processorid_t cpuid = cpup->cpu_id;
	lpi_state_t *states;
	int nstates;
	int state_idx;
	int state;
	hrtime_t enter_ts;
	hrtime_t exit_ts;

	states = cpup->cpu_m.mcpu_lpi_states;
	nstates = cpup->cpu_m.mcpu_lpi_nstates;

	if (states == NULL || nstates == 0) {
		/* no idle states; plain WFI through the normal callback path */
		state = LPI_IDLE_STATE(1, 0);

		if (cpu_idle_enter(state, 0, NULL, NULL) != 0) {
			return;
		}

		enter_ts = gethrtime_unscaled();
		__asm__ __volatile__("wfi" ::: "memory");
		cpu_idle_exit(CPU_IDLE_CB_FLAG_IDLE);
		exit_ts = gethrtime_unscaled();
		lpi_last_idle[cpuid] = exit_ts - enter_ts;
		return;
	}

	/* Select idle state based on previous idle duration */
	state_idx = lpi_select_state(states, nstates,
	    lpi_last_idle[cpuid]);
	state = LPI_IDLE_STATE(state_idx + 1,
	    states[state_idx].ls_ctx_loss_flags);

	if (cpu_idle_enter(state, 0, NULL, NULL) != 0) {
		return;
	}

	enter_ts = gethrtime_unscaled();

	if (states[state_idx].ls_entry_type == LPI_ENTRY_WFI) {
		__asm__ __volatile__("wfi" ::: "memory");
	} else {
		ASSERT3U(states[state_idx].ls_entry_type, ==,
		    LPI_ENTRY_PSCI);
		(void) psci_cpu_suspend(
		    states[state_idx].ls_psci_state, 0, 0);
	}

	/*
	 * Woken by interrupt or spuriously.  Interrupts are still disabled.
	 * Run the idle exit callbacks from the idle thread.  If a pending
	 * interrupt later fires and its handler calls cpu_idle_exit(INTR),
	 * the index will already be zero and that call is a no-op.
	 */
	cpu_idle_exit(CPU_IDLE_CB_FLAG_IDLE);

	exit_ts = gethrtime_unscaled();
	lpi_last_idle[cpuid] = exit_ts - enter_ts;
	states[state_idx].ls_entries++;
	states[state_idx].ls_residency_ns += (exit_ts - enter_ts);
}

/*
 * cpuidle_register_states
 */

/*
 * Populate a synthetic WFI idle state.
 *
 * WFI is the implicit default on all aarch64 CPUs.  When firmware does
 * not supply it (unfortunately, common), the cpuidle framework synthesises
 * it so there is always a fallback.
 */
static void
cpuidle_synth_wfi(lpi_state_t *lsp)
{
	ASSERT3P(lsp, !=, NULL);

	lsp->ls_min_residency = 1;
	lsp->ls_wake_latency = 1;
	lsp->ls_ctx_loss_flags = 0;
	lsp->ls_psci_state = 0;
	lsp->ls_entry_type = LPI_ENTRY_WFI;
	lsp->ls_enabled = B_TRUE;
}

/*
 * Return B_TRUE if the states array already contains a WFI entry.
 */
static boolean_t
cpuidle_has_wfi(lpi_state_t *states, int nstates)
{
	int i;

	for (i = 0; i < nstates; i++) {
		if (states[i].ls_entry_type == LPI_ENTRY_WFI) {
			return (B_TRUE);
		}
	}

	return (B_FALSE);
}

/*
 * Comparison function for sorting idle states by min_residency ascending.
 *
 * This gives the state selector a stable invariant: index 0 is the
 * shallowest state (highly likely to be WFI, residency 1us) and the highest
 * index is the deepest.  lpi_select_state() walks from deepest to shallowest,
 * so this ordering is required for correct state selection.
 */
static int
cpuidle_state_cmp(const void *a, const void *b)
{
	const lpi_state_t *sa = a;
	const lpi_state_t *sb = b;

	if (sa->ls_min_residency < sb->ls_min_residency) {
		return (-1);
	}

	if (sa->ls_min_residency > sb->ls_min_residency) {
		return (1);
	}

	/* tiebreak: prefer lower wake latency */
	if (sa->ls_wake_latency < sb->ls_wake_latency) {
		return (-1);
	}

	if (sa->ls_wake_latency > sb->ls_wake_latency) {
		return (1);
	}

	return (0);
}

/*
 * Ensure the states array contains a WFI entry and is sorted by
 * min_residency ascending (shallowest first, deepest last).
 *
 * If the caller's array does not contain a WFI entry, one is added
 * by growing the array.  The array is then sorted so that
 * lpi_select_state() can walk from deepest to shallowest.  The sort
 * is necessary, as there is no guarantee that firmware contains
 * sorted entries.
 *
 * If the caller supplied NULL/0 (no firmware states), a single-element
 * WFI-only array is created.
 *
 * On return *statesp and *nstatesp reflect the (possibly new) array.
 */
static void
cpuidle_ensure_wfi(lpi_state_t **statesp, int *nstatesp)
{
	lpi_state_t *states;
	lpi_state_t *grown;
	int nstates;

	ASSERT3P(statesp, !=, NULL);
	ASSERT3P(nstatesp, !=, NULL);

	states = *statesp;
	nstates = *nstatesp;

	if (states == NULL || nstates == 0) {
		/* No firmware states - create a WFI-only array */
		states = kmem_zalloc(sizeof (lpi_state_t), KM_SLEEP);
		cpuidle_synth_wfi(&states[0]);
		*statesp = states;
		*nstatesp = 1;
		return;
	}

	if (!cpuidle_has_wfi(states, nstates)) {
		/* Grow the array by one and append WFI */
		grown = kmem_zalloc((nstates + 1) * sizeof (lpi_state_t),
		    KM_SLEEP);
		memcpy(grown, states, nstates * sizeof (lpi_state_t));
		kmem_free(states, nstates * sizeof (lpi_state_t));

		cpuidle_synth_wfi(&grown[nstates]);
		nstates++;

		states = grown;
	}

	/* Sort shallowest-first so the state selector can rely on order */
	qsort(states, nstates, sizeof (lpi_state_t), cpuidle_state_cmp);

	*statesp = states;
	*nstatesp = nstates;
}

/*
 * Register idle states for a CPU.
 *
 * Ensures a WFI entry exists in the array, adding one if the caller
 * did not supply it, then sorts the array by min_residency ascending.
 * Passing NULL/0 is valid and registers the CPU with WFI only.
 *
 * Takes ownership of the caller-allocated states array.  On failure
 * both the caller's and any synthesised array are freed.
 */
int
cpuidle_register_states(processorid_t cpuid, lpi_state_t *states, int nstates)
{
	cpu_t *cp;

	ASSERT(states != NULL || nstates == 0);

	/* Guarantee WFI is present and array is sorted */
	cpuidle_ensure_wfi(&states, &nstates);

	/*
	 * Store in the cpu_t.  The caller is the CPU's own driver, so the
	 * cpu_t is guaranteed to exist.
	 */
	cp = cpu[cpuid];
	ASSERT3P(cp, !=, NULL);

	cp->cpu_m.mcpu_lpi_states = states;
	cp->cpu_m.mcpu_lpi_nstates = nstates;

	cpuidle_kstat_create(cpuid, states, nstates);

	return (0);
}

/*
 * Module infrastructure
 */

static struct modlmisc modlmisc = {
	.misc_modops	= &mod_miscops,
	.misc_linkinfo	= "CPU idle framework"
};

static struct modlinkage modlinkage = {
	.ml_rev		= MODREV_1,
	.ml_linkage	= { &modlmisc, NULL }
};

int
_init(void)
{
	int err;

	lpi_last_idle = kmem_zalloc(max_ncpus * sizeof (hrtime_t), KM_SLEEP);
	lpi_kstats = kmem_zalloc(max_ncpus * sizeof (kstat_t **), KM_SLEEP);

	err = mod_install(&modlinkage);
	if (err != 0) {
		kmem_free(lpi_kstats, max_ncpus * sizeof (kstat_t **));
		lpi_kstats = NULL;
		kmem_free(lpi_last_idle, max_ncpus * sizeof (hrtime_t));
		lpi_last_idle = NULL;
		return (err);
	}

	cpu_halt_impl = cpu_lpi_halt;
	return (0);
}

int
_fini(void)
{
	/* Do not allow unload; cpu_halt_impl is in use */
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
