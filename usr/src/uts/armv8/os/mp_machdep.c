
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
 * Copyright 2024 Michael van der Westhuizen
 * Copyright 2017 Hayashi Naoyuki
 * Copyright (c) 1992, 2010, Oracle and/or its affiliates. All rights reserved.
 */
/*
 * Copyright (c) 2009-2010, Intel Corporation.
 * All rights reserved.
 */

#define	PSMI_1_7
#include <sys/smp_impldefs.h>
#include <sys/cmn_err.h>
#include <sys/clock.h>
#include <sys/debug.h>
#include <sys/cpupart.h>
#include <sys/cpuvar.h>
#include <sys/cpu_event.h>
#include <sys/cmt.h>
#include <sys/cpu.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/sysmacros.h>
#include <sys/memlist.h>
#include <sys/param.h>
#include <sys/promif.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/cpuinfo.h>
#include <sys/ddi_implfuncs.h>
#include <sys/stdbool.h>

static int mach_cpu_create_devinfo(cpu_t *cp, dev_info_t **dipp);

extern void return_instr(void);

uint_t cp_haltset_fanout = 0;
int (*addintr)(void *, int, avfunc, char *, int, caddr_t, caddr_t, uint64_t *,
    dev_info_t *) = NULL;
void (*remintr)(void *, int, avfunc, int) = NULL;
void (*setsoftint)(int, struct av_softinfo *) =
	(void (*)(int, struct av_softinfo *))return_instr;
void (*kdisetsoftint)(int, struct av_softinfo *) =
	(void (*)(int, struct av_softinfo *))return_instr;
int (*slvltovect)(int) = (int (*)(int))return_instr;

int (*psm_cpu_create_devinfo)(cpu_t *, dev_info_t **) = mach_cpu_create_devinfo;

static int
mach_softlvl_to_vect(int ipl)
{
	setsoftint = av_set_softint_pending;
	kdisetsoftint = kdi_av_set_softint_pending;

	return (-1);
}

void
cmp_set_nosteal_interval(void)
{
	/* Set the nosteal interval (used by disp_getbest()) to 100us */
	nosteal_nsec = 100000UL;
}

int
cu_plat_cpc_init(cpu_t *cp, kcpc_request_list_t *reqs, int nreqs)
{
	return (-1);
}

void
mach_cpu_pause(volatile char *safe)
{
	/*
	 * This cpu is now safe.
	 */
	*safe = PAUSE_WAIT;
	membar_enter(); /* make sure stores are flushed */

	/*
	 * Now we wait.  When we are allowed to continue, safe
	 * will be set to PAUSE_IDLE.
	 */
	while (*safe != PAUSE_IDLE)
		SMT_PAUSE();
}

/*
 * Default cpu_halt loop body: execute WFI and return.
 */
static void
cpu_halt_wfi(void)
{
	__asm__ __volatile__("wfi" ::: "memory");
}

/*
 * Pluggable cpu_halt loop body.
 *
 * This implementation hook is called with interrupts disabled from
 * the inner loop of cpu_halt.
 */
void (*cpu_halt_impl)(void) = cpu_halt_wfi;

static void
cpu_halt(void)
{
	cpu_t *cpup = CPU;
	processorid_t cpu_sid = cpup->cpu_seqid;
	cpupart_t *cp = cpup->cpu_part;
	int hset_update = 1;
	volatile int *p = &cpup->cpu_disp->disp_nrunnable;
	uint_t s;

	/*
	 * If this CPU is online then we should note our halting
	 * by adding ourselves to the partition's halted CPU
	 * bitset. This allows other CPUs to find/awaken us when
	 * work becomes available.
	 */
	if (CPU->cpu_flags & CPU_OFFLINE)
		hset_update = 0;

	/*
	 * Add ourselves to the partition's halted CPUs bitset
	 * and set our HALTED flag, if necessary.
	 *
	 * When a thread becomes runnable, it is placed on the queue
	 * and then the halted cpu bitset is checked to determine who
	 * (if anyone) should be awoken. We therefore need to first
	 * add ourselves to the halted bitset, and then check if there
	 * is any work available.  The order is important to prevent a race
	 * that can lead to work languishing on a run queue somewhere while
	 * this CPU remains halted.
	 *
	 * Either the producing CPU will see we're halted and will awaken us,
	 * or this CPU will see the work available in disp_anywork()
	 */
	if (hset_update) {
		cpup->cpu_disp_flags |= CPU_DISP_HALTED;
		membar_producer();
		bitset_atomic_add(&cp->cp_haltset, cpu_sid);
	}

	/*
	 * Check to make sure there's really nothing to do.
	 * Work destined for this CPU may become available after
	 * this check. We'll be notified through the clearing of our
	 * bit in the halted CPU bitset, and a poke.
	 */
	if (disp_anywork()) {
		if (hset_update) {
			cpup->cpu_disp_flags &= ~CPU_DISP_HALTED;
			bitset_atomic_del(&cp->cp_haltset, cpu_sid);
		}
		return;
	}

	/*
	 * We're on our way to being halted.  Wait until something becomes
	 * runnable locally or we are awakened (i.e. removed from the halt
	 * set).  Note that the call to hv_cpu_yield() can return even if we
	 * have nothing to do.
	 *
	 * Disable interrupts now, so that we'll awaken immediately
	 * after halting if someone tries to poke us between now and
	 * the time we actually halt.
	 *
	 * We check for the presence of our bit after disabling interrupts.
	 * If it's cleared, we'll return. If the bit is cleared after
	 * we check then the poke will pop us out of the halted state.
	 * Also, if the offlined CPU has been brought back on-line, then
	 * we return as well.
	 *
	 * The ordering of the poke and the clearing of the bit by cpu_wakeup
	 * is important.
	 * cpu_wakeup() must clear, then poke.
	 * cpu_halt() must disable interrupts, then check for the bit.
	 *
	 * The check for anything locally runnable is here for performance
	 * and isn't needed for correctness. disp_nrunnable ought to be
	 * in our cache still, so it's inexpensive to check, and if there
	 * is anything runnable we won't have to wait for the poke.
	 *
	 * Any interrupt will awaken the cpu from halt. Looping here
	 * will filter spurious interrupts that wake us up, but don't
	 * represent a need for us to head back out to idle().  This
	 * will enable the idle loop to be more efficient and sleep in
	 * the processor pipeline for a larger percent of the time,
	 * which returns useful cycles to the peer hardware strand
	 * that shares the pipeline.
	 */
	s = disable_interrupts();
	while (*p == 0 &&
	    ((hset_update && bitset_in_set(&cp->cp_haltset, cpu_sid)) ||
	    (!hset_update && (CPU->cpu_flags & CPU_OFFLINE)))) {
		cpu_halt_impl();
		restore_interrupts(s);
		s = disable_interrupts();
	}

	/*
	 * We're no longer halted
	 */
	restore_interrupts(s);
	if (hset_update) {
		cpup->cpu_disp_flags &= ~CPU_DISP_HALTED;
		bitset_atomic_del(&cp->cp_haltset, cpu_sid);
	}
}

static void
cpu_wakeup(cpu_t *cpu, int bound)
{
	uint_t		cpu_found;
	processorid_t	cpu_sid;
	cpupart_t	*cp;

	cp = cpu->cpu_part;
	cpu_sid = cpu->cpu_seqid;
	if (bitset_in_set(&cp->cp_haltset, cpu_sid)) {
		/*
		 * Clear the halted bit for that CPU since it will be
		 * poked in a moment.
		 */
		bitset_atomic_del(&cp->cp_haltset, cpu_sid);
		/*
		 * We may find the current CPU present in the halted cpu bitset
		 * if we're in the context of an interrupt that occurred
		 * before we had a chance to clear our bit in cpu_halt().
		 * Poking ourself is obviously unnecessary, since if
		 * we're here, we're not halted.
		 */
		if (cpu != CPU)
			poke_cpu(cpu->cpu_id);
		return;
	} else {
		/*
		 * This cpu isn't halted, but it's idle or undergoing a
		 * context switch. No need to awaken anyone else.
		 */
		if (cpu->cpu_thread == cpu->cpu_idle_thread ||
		    cpu->cpu_disp_flags & CPU_DISP_DONTSTEAL)
			return;
	}

	/*
	 * No need to wake up other CPUs if this is for a bound thread.
	 */
	if (bound)
		return;

	/*
	 * The CPU specified for wakeup isn't currently halted, so check
	 * to see if there are any other halted CPUs in the partition,
	 * and if there are then awaken one.
	 *
	 * If possible, try to select a CPU close to the target, since this
	 * will likely trigger a migration.
	 */
	do {
		cpu_found = bitset_find(&cp->cp_haltset);
		if (cpu_found == (uint_t)-1)
			return;
	} while (bitset_atomic_test_and_del(&cp->cp_haltset, cpu_found) < 0);

	if (cpu_found != CPU->cpu_seqid)
		poke_cpu(cpu_seq[cpu_found]->cpu_id);
}

/*
 * Given a dip representing a CPU, return the CPU's MPIDR.
 */
int
mach_cpu_dip_to_mpidr(dev_info_t *dip, uint64_t *mpidr)
{
	dev_info_t *pdip;
	char *device_type;
	int *reg;
	uint64_t m;
	uint_t reg_len;
	int addr_cells;
	bool is_acpi;

	ASSERT3P(dip, !=, NULL);

	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    OBP_DEVICETYPE, &device_type) != DDI_PROP_SUCCESS) {
		return (DDI_FAILURE);
	}

	is_acpi = strcmp(device_type, "acpicpu") == 0;

	if (strcmp(device_type, "cpu") != 0 && !is_acpi) {
		ddi_prop_free(device_type);
		return (DDI_FAILURE);
	}

	ddi_prop_free(device_type);

	/*
	 * ACPI CPUs have an MPIDR property.
	 */
	if (is_acpi) {
		int64_t tm = ddi_prop_get_int64(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, "mpidr", -1);

		if (tm == -1) {
			dev_err(dip, CE_WARN, "missing mpidr property");
			return (DDI_FAILURE);
		}

		if (mpidr != NULL) {
			*mpidr = (uint64_t)tm;
		}

		return (DDI_SUCCESS);
	}

	/*
	 * Not ACPI, extract from "reg".
	 */

	if ((pdip = ddi_get_parent(dip)) == NULL) {
		return (DDI_FAILURE);
	}

	addr_cells = ddi_prop_get_int(DDI_DEV_T_ANY, pdip, DDI_PROP_DONTPASS,
	    OBP_ADDRESS_CELLS, OBP_DEFAULT_ADDRESS_CELLS);

	if (addr_cells != 1 && addr_cells != 2) {
		dev_err(dip, CE_WARN,
		    "unsupported CPU %s: %d", OBP_ADDRESS_CELLS, addr_cells);
		return (DDI_FAILURE);
	}

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    OBP_REG, &reg, &reg_len) != DDI_PROP_SUCCESS) {
		dev_err(dip, CE_WARN, "no CPU %s property", OBP_REG);
		return (DDI_FAILURE);
	}

	if (addr_cells == 2 && reg_len >= 2) {
		m = ((uint64_t)(uint32_t)reg[0] << 32) | (uint32_t)reg[1];
	} else if (addr_cells == 1 && reg_len >= 1) {
		m = (uint32_t)reg[0];
	} else {
		dev_err(dip, CE_WARN, "invalid CPU %s length for %s %d",
		    OBP_REG, OBP_ADDRESS_CELLS, addr_cells);
		ddi_prop_free(reg);
		return (DDI_FAILURE);
	}

	ddi_prop_free(reg);

	if (mpidr != NULL) {
		*mpidr = m;
	}

	return (DDI_SUCCESS);
}

/*
 * Search children of the cpus nexus for a device node whose reg property
 * matches the given MPIDR.  On FDT systems the reg property of each cpu
 * node under /cpus contains the MPIDR affinity value; #address-cells
 * determines whether it is one or two 32-bit cells.
 *
 * Returns DDI_SUCCESS with *dipp set and held if found, DDI_FAILURE
 * otherwise.  The caller must hold cpu_nex_devi (ndi_devi_enter).
 */
static int
mach_cpu_find_by_mpidr(dev_info_t *cpu_nex_devi, uint64_t target_mpidr,
    dev_info_t **dipp)
{
	dev_info_t *dip;
	uint64_t mpidr;

	ASSERT3P(cpu_nex_devi, !=, NULL);
	ASSERT3P(dipp, !=, NULL);

	for (dip = ddi_get_child(cpu_nex_devi); dip != NULL;
	    dip = ddi_get_next_sibling(dip)) {
		if (mach_cpu_dip_to_mpidr(dip, &mpidr) != DDI_SUCCESS) {
			continue;
		}

		if (mpidr == target_mpidr) {
			*dipp = dip;
			(void) ndi_hold_devi(dip);
			return (DDI_SUCCESS);
		}
	}

	return (DDI_FAILURE);
}

static dev_info_t *
mach_cpu_ensure_cpus(void)
{
	int rv;
	dev_info_t *dip;
	major_t major;
	static dev_info_t *cpu_nex_devi = NULL;
	static boolean_t bound = B_FALSE;
	static kmutex_t cpu_node_lock;

	if (cpu_nex_devi == NULL) {
		mutex_enter(&cpu_node_lock);
		/* First check whether cpus exists. */
		cpu_nex_devi = ddi_find_devinfo("cpus", -1, 0);
		/* Create cpus if it doesn't exist. */
		if (cpu_nex_devi == NULL) {
			ndi_devi_enter(ddi_root_node());
			rv = ndi_devi_alloc(ddi_root_node(), "cpus",
			    (pnode_t)DEVI_SID_NODEID, &dip);
			if (rv != NDI_SUCCESS) {
				mutex_exit(&cpu_node_lock);
				cmn_err(CE_CONT,
				    "?failed to create cpu nexus device.\n");
				return (NULL);
			}

			ASSERT3P(dip, !=, NULL);
			(void) ndi_devi_online(dip, 0);
			ndi_devi_exit(ddi_root_node());
			cpu_nex_devi = dip;
		}
		mutex_exit(&cpu_node_lock);
	}

	if (bound != B_TRUE) {
		mutex_enter(&cpu_node_lock);
		if (bound != B_TRUE) {
			if ((major = ddi_name_to_major("cpunex"))
			    == DDI_MAJOR_T_NONE) {
				cmn_err(CE_CONT,
				    "?could not locate cpunex driver\n");
				mutex_exit(&cpu_node_lock);
				return (cpu_nex_devi);
			}

			(void) make_mbind("cpus", major, NULL, mb_hashtab);
			bound = B_TRUE;
		}
		mutex_exit(&cpu_node_lock);
	}

	return (cpu_nex_devi);
}

/*
 * Default handler to create device node for CPU.
 * One reference count will be held on the device node.
 *
 * If the cpus nexus already has a child whose reg property matches this
 * CPU's MPIDR (the normal case on FDT systems where /cpus/cpu@N nodes
 * are populated from the devicetree), return the existing node instead
 * of creating a duplicate.
 */
static int
mach_cpu_create_devinfo(cpu_t *cp, dev_info_t **dipp)
{
	int rv;
	dev_info_t *dip;
	dev_info_t *cpu_nex_devi;

	ASSERT3P(cp, !=, NULL);
	ASSERT3P(dipp, !=, NULL);
	*dipp = NULL;

	/*
	 * Ensure that the /cpus device tree node exists and bind the
	 * cpunex driver to the node.
	 */
	if ((cpu_nex_devi = mach_cpu_ensure_cpus()) == NULL) {
		return (DDI_FAILURE);
	}

	/*
	 * Search for an existing cpu node matching this CPU's MPIDR.
	 */
	ndi_devi_enter(cpu_nex_devi);
	if (mach_cpu_find_by_mpidr(cpu_nex_devi, cp->cpu_m.affinity,
	    &dip) == DDI_SUCCESS) {
		ndi_devi_exit(cpu_nex_devi);
		*dipp = dip;
		return (DDI_SUCCESS);
	}

	/*
	 * No existing node found.  Create a child node for this cpu.
	 */
	dip = ddi_add_child(cpu_nex_devi, "cpu", DEVI_SID_NODEID, -1);
	if (dip == NULL) {
		cmn_err(CE_CONT,
		    "?failed to create device node for cpu%d.\n", cp->cpu_id);
		rv = DDI_FAILURE;
	} else {
		*dipp = dip;
		(void) ndi_hold_devi(dip);
		rv = DDI_SUCCESS;
	}
	ndi_devi_exit(cpu_nex_devi);

	return (rv);
}

/*
 * Create cpu device node in device tree and online it.
 * Return created dip with reference count held if requested.
 *
 * On aarch64 with ACPI, acpidev_cpu creates the device nodes during boot
 * enumeration (marked offline) and hooks psm_cpu_create_devinfo to look
 * them up.  This function bridges the gap: it finds the existing node and
 * onlines it, triggering driver attachment.
 */
int
mach_cpu_create_device_node(struct cpu *cp, dev_info_t **dipp)
{
	int rv;
	dev_info_t *dip = NULL;

	ASSERT3P(cp, !=, NULL);
	ASSERT3P(psm_cpu_create_devinfo, !=, NULL);

	rv = psm_cpu_create_devinfo(cp, &dip);
	if (rv == DDI_SUCCESS) {
		/* Recursively attach driver for parent nexus device. */
		if (i_ddi_attach_node_hierarchy(ddi_get_parent(dip)) ==
		    DDI_SUCCESS) {
			/* Configure cpu itself and descendants. */
			(void) ndi_devi_online(dip,
			    NDI_ONLINE_ATTACH | NDI_CONFIG);
		}
		if (dipp != NULL) {
			*dipp = dip;
		} else {
			(void) ndi_rele_devi(dip);
		}
	}

	return (rv);
}

void
mach_init()
{
	cpuset_t cpumask;

	slvltovect = mach_softlvl_to_vect;

	idle_cpu = cpu_halt;
	disp_enq_thread = cpu_wakeup;

	/*
	 * Add all enabled CPUs to the mp_cpus cpuset.
	 */
	CPUSET_ZERO(cpumask);

	for (struct cpuinfo *ci = cpuinfo_first_enabled();
	    ci != cpuinfo_end(); ci = cpuinfo_next_enabled(ci)) {
		CPUSET_ADD(cpumask, ci->ci_id);
	}

	mp_cpus = cpumask;
}
