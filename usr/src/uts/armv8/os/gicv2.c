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
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2023 Michael van der Westhuizen
 */

#include <sys/types.h>
#include <sys/gic.h>
#include <sys/avintr.h>
#include <sys/smp_impldefs.h>
#include <sys/promif.h>

extern char *gic_module_name;

/*
 * These are shadowed copies of GICD_IPRIORITYR<0-7>, and GICD_ISENABLER[0]
 *
 * That is the priority and enabled status of SGIs and PPIs, which are CPU
 * local, and the registers for same are banked per-CPU in the distributor.
 *
 * XXXARM: It's possible these were intended to keep these in sync across all
 * CPUs, but they don't outside of initial config.
 */
static volatile uint32_t ipriorityr_private[8];
static volatile uint32_t ienable_private;

/*
 * This is used to record whether an IRQ is edge or level triggered.
 * A set bit in this mask is an edge triggered interrupt, a 0 is level
 * triggered.
 *
 * We use this in `gicv2_mask_level_irq` and `gicv2_unmask_level_irq` to decide
 * whether to enable or disable that IRQ, these functions ignore
 * edge-triggered interrupts.  This is set when `gicv2_config_irq` and never
 * altered (which makes sense).
 */
static uint32_t intr_cfg[1024 / 32];

/*
 * Protect access to global GIC state.
 * In the current implementation, the distributor.
 */
static lock_t gicv2_lock;

/* Base addresses of the distributor */
static volatile struct gic_dist *gic_dist;

/*
 * Mapping from cpuid to GIC target identifier
 * XXXARM: Another place we cap at 8 CPUs
 */
static uint8_t gicv2_target[8];

/*
 * CPUs for which we have initialized the GIC.  Used to limit IPIs to only
 * those CPUs we can target.
 */
static cpuset_t gicv2_cpuset;

/*
 * Enable IRQ in the distributor, which will now be forwarded to a cpu.
 *
 * It is IMPLEMENTATION DEFINED whether SGIs can be enabled or disabled here,
 * we never try.
 */
static void
gicv2_enable_irq(int irq)
{
	if (irq >= GIC_INTID_MIN_PPI) {
		gic_dist->isenabler[GICD_IRQ_TO_ISENABLER(irq)] =
		    GICD_IRQ_TO_ISENABLER_FIELD(irq);
	}
}

/*
 * Disable IRQ in the distributor, which will now cease being forwarded to a
 * cpu.
 *
 * It is IMPLEMENTATION DEFINED whether SGIs can be enabled or disabled here,
 * we never try.
 */
static void
gicv2_disable_irq(int irq)
{
	if (irq >= GIC_INTID_MIN_PPI) {
		gic_dist->icenabler[GICD_IRQ_TO_ISENABLER(irq)] =
		    GICD_IRQ_TO_ISENABLER_FIELD(irq);
	}
}

/*
 * If this is a level-triggered IRQ which is not an SGI, enable it
 * See comments about SGIs in enable/disable_irq
 */
static void
gicv2_unmask_level_irq(int irq)
{
	if ((irq >= GIC_INTID_MIN_PPI) &&
	    ((intr_cfg[irq / 32] & (1u << (irq % 32))) == 0)) {
		ASSERT(irq != 225);
		gicv2_enable_irq(irq);
	}
}

/*
 * If this is a level-triggered IRQ which is not an SGI, disable it
 * See comments about SGIs in enable/disable_irq
 */
static void
gicv2_mask_level_irq(int irq)
{
	if ((irq >= GIC_INTID_MIN_PPI) &&
	    ((intr_cfg[irq / 32] & (1u << (irq % 32))) == 0)) {
		ASSERT(irq != 225);
		gicv2_disable_irq(irq);
	}
}

/*
 * Configure whether IRQ is edge or level triggered.
 * Additionally record this in in `intr_cfg`.
 */
static void
gicv2_config_irq(uint32_t irq, bool is_edge)
{
	uint32_t v = (is_edge ? 0x2 : 0);

	/*
	 * §8.9.7 Software must disable an interrupt before the value of the
	 * corresponding programmable Int_config field is changed. GIC
	 * behavior is otherwise UNPREDICTABLE.
	 */
	ASSERT((gic_dist->isenabler[GICD_IRQ_TO_ISENABLER(irq)] &
	    GICD_IRQ_TO_ISENABLER_FIELD(irq)) == 0);

	/*
	 * GICD_ICFGR<n> is a packed field with 2 bits per interrupt, the even
	 * bit is reserved, the odd bit is 1 for edge-triggered 0 for
	 * level.
	 */
	gic_dist->icfgr[GICD_IRQ_TO_ICFGR(irq)] =
	    (gic_dist->icfgr[GICD_IRQ_TO_ICFGR(irq)] &
	    ~GICD_IRQ_TO_ICFGR_FIELD(irq, 0x3)) |
	    GICD_IRQ_TO_ICFGR_FIELD(irq, v);

	if (is_edge)
		intr_cfg[irq / 32] |= (1u << (irq % 32));
	else
		intr_cfg[irq / 32] &= ~(1u << (irq % 32));
}

/*
 * Mask interrupts of priority lower than or equal to IRQ.
 */
static int
gicv2_setlvl(int irq)
{
	int new_ipl;
	new_ipl = autovect[irq].avh_hi_pri;

	if (new_ipl != 0) {
		gic_cpuif->pmr = GIC_IPL_TO_PRI(new_ipl);
	}

	return (new_ipl);
}

/*
 * Unmask level IRQ if it's level triggered and mask interrupts
 * less than or equal to IPL.
 *
 * XXXARM: Is this really what setlvlx should do?
 */
static void
gicv2_setlvlx(int ipl, int irq)
{
	/*
	 * Allow code to pass a -1 IRQ to just change GICC_PMR
	 * see armv8/intr.c:do_splx(), among others.
	 */
	if (irq >= 0)
		gicv2_unmask_level_irq(irq);
	gic_cpuif->pmr = GIC_IPL_TO_PRI(ipl);
}

/*
 * Set the priority of IRQ to IPL
 * If IRQ is an SGI or PPI, shadow that priority into `ipriorityr_private`
 */
static void
gicv2_set_ipl(uint32_t irq, uint32_t ipl)
{
	uint64_t old = read_daif();
	set_daif(DAIF_SETCLEAR_IRQ);
	lock_set(&gicv2_lock);

	uint32_t ipriorityr = gic_dist->ipriorityr[GICD_IRQ_TO_IPRIORITYR(irq)];
	ipriorityr &= ~(GICD_IRQ_TO_IPRIORITYR_FIELD(irq, 0xff));
	ipriorityr |= GICD_IRQ_TO_IPRIORITYR_FIELD(irq, GIC_IPL_TO_PRI(ipl));
	gic_dist->ipriorityr[GICD_IRQ_TO_IPRIORITYR(irq)] = ipriorityr;

	if (irq <= GIC_INTID_PERCPU_MAX) {
		ipriorityr_private[GICD_IRQ_TO_IPRIORITYR(irq)] = ipriorityr;
	}

	lock_clear(&gicv2_lock);
	write_daif(old);
}

/*
 * Configure non-local IRQs to be delivered through the distributor.
 */
static void
gicv2_add_target(uint32_t irq)
{
	uint64_t old = read_daif();
	set_daif(DAIF_SETCLEAR_IRQ);
	lock_set(&gicv2_lock);
	uint32_t coreMask = 0xFF; /* all 8 (XXXARM) cpus */

	/*
	 * Each GICD_ITARGETSR<n> contains 4 8-bit fields indicating that int
	 * N is delivered to the cpus with 1 bits set in the value.
	 *
	 * XXXARM: Another place with an 8 cpu limit.
	 *
	 * We always program all interrupts to deliver to all possible CPUs,
	 * trusting RAZ/WI for those which don't exist.
	 */
	if (irq > GIC_INTID_PERCPU_MAX) {
		uint32_t tr = (gic_dist->itargetsr[GICD_IRQ_TO_ITARGETSR(irq)] &
		    ~GICD_IRQ_TO_ITARGETSR_FIELD(irq, coreMask));

		gic_dist->itargetsr[GICD_IRQ_TO_ITARGETSR(irq)] =
		    tr | GICD_IRQ_TO_ITARGETSR_FIELD(irq, coreMask);
	}

	lock_clear(&gicv2_lock);
	write_daif(old);
}

/*
 * Configure such that IRQ cannot happen at or above IPL
 *
 * There are complications here -- which this code doesn't handle -- which are
 * outlined in the pclusmp implementation, I have included that comment
 * below.
 *
 * (from i86pc/io/mp_platform_misc.c:apic_addspl_common)
 *  * Both add and delspl are complicated by the fact that different interrupts
 * may share IRQs. This can happen in two ways.
 * 1. The same H/W line is shared by more than 1 device
 * 1a. with interrupts at different IPLs
 * 1b. with interrupts at same IPL
 * 2. We ran out of vectors at a given IPL and started sharing vectors.
 * 1b and 2 should be handled gracefully, except for the fact some ISRs
 * will get called often when no interrupt is pending for the device.
 * For 1a, we handle it at the higher IPL.
 */
static int
gicv2_addspl(int irq, int ipl, int min_ipl, int max_ipl)
{
	gicv2_set_ipl((uint32_t)irq, (uint32_t)ipl);
	gicv2_add_target((uint32_t)irq);
	gicv2_enable_irq((uint32_t)irq);

	if (irq <= GIC_INTID_PERCPU_MAX)
		ienable_private |= (1u << irq);

	return (0);
}

/*
 * XXXARM: Comment taken verbatim from
 *         i86pc/io/mp_platform_misc.c:apic_delspl_common)
 *
 * Recompute mask bits for the given interrupt vector.
 * If there is no interrupt servicing routine for this
 * vector, this function should disable interrupt vector
 * from happening at all IPLs. If there are still
 * handlers using the given vector, this function should
 * disable the given vector from happening below the lowest
 * IPL of the remaining handlers.
 */
static int
gicv2_delspl(int irq, int ipl, int min_ipl, int max_ipl)
{
	if (autovect[irq].avh_hi_pri == 0) {
		gicv2_disable_irq((uint32_t)irq);
		gicv2_set_ipl((uint32_t)irq, 0);
		if (irq <= GIC_INTID_PERCPU_MAX)
			ienable_private &= ~(1u << irq);
	}

	return (0);
}

/*
 * Send an IRQ as an IPI to processors in `cpuset`.
 *
 * Processors not targetable by the GIC will be silently ignored.
 */
static void
gicv2_send_ipi(cpuset_t cpuset, int irq)
{
	uint32_t target = 0;
	CPUSET_AND(cpuset, gicv2_cpuset);
	while (!CPUSET_ISNULL(cpuset)) {
		uint_t cpu;
		CPUSET_FIND(cpuset, cpu);
		target |= gicv2_target[cpu];
		CPUSET_DEL(cpuset, cpu);
	}
	uint64_t old = read_daif();
	set_daif(DAIF_SETCLEAR_IRQ);
	dsb(ish);

	gic_dist->sgir = GICD_SGIR(0, target, 0, irq);

	write_daif(old);
}

static pnode_t
find_gic(pnode_t nodeid, int depth)
{
	if (prom_is_compatible(nodeid, "arm,cortex-a15-gic") ||
	    prom_is_compatible(nodeid, "arm,gic-400")) {
		return (nodeid);
	}

	pnode_t child = prom_childnode(nodeid);
	while (child > 0) {
		pnode_t node = find_gic(child, depth + 1);
		if (node > 0)
			return (node);
		child = prom_nextnode(child);
	}
	return (OBP_NONODE);
}

/*
 * Return the target representing the current cpu from the GIC point of view
 * by reading the target field of a target specific interrupt.
 *
 * This sets the Nth bit for target N
 *
 * XXXARM: Only works without affinity routing.
 */
static uint_t
gicv2_get_target(void)
{
	return (1 << (__builtin_ctz(gic_dist->itargetsr[0] & 0xff)));
}

/*
 * Initialize the GIC on the boot CPU, including any global initialization.
 */
static void
gicv2_init_primary(void)
{
	uint64_t old = read_daif();
	set_daif(DAIF_SETCLEAR_IRQ);

	LOCK_INIT_HELD(&gicv2_lock);

	pnode_t node = find_gic(prom_rootnode(), 0);

	if (node > 0) {
		uint64_t base;
		if (prom_get_reg_address(node, 0, &base) == 0) {
			gic_dist =
			    (struct gic_dist *)(uintptr_t)(base + SEGKPM_BASE);
		}
		if (prom_get_reg_address(node, 1, &base) == 0) {
			gic_cpuif =
			    (struct gic_cpuif *)(uintptr_t)(base + SEGKPM_BASE);
		}
	}

	/*
	 * §8.9.4 GICD_CTLR, Distributor Control Register
	 *
	 * GICD_CTRLR.DS resets as 0 if the field is R/W, but there are
	 * implications it might be DS=1 RAO/WI on some hardware.
	 *
	 * We (try to) configure it to 0, and then assert it is 0, such that
	 * the meaning of settings below contingent on DS are obvious.
	 *
	 * This is probably paranoia
	 */
	gic_dist->ctlr = (gic_dist->ctlr & ~GICD_CTLR_DS_MASK);
	ASSERT(GICD_CTLR_DS(gic_dist->ctlr) == 0);

	/*
	 * Clear enabled/pending/active status of all interrupts, and put them
	 * in group 0.
	 */
	for (int i = 0; i < 32; i++) {
		gic_dist->icenabler[i] = 0xffffffff;
		gic_dist->icpendr[i]   = 0xffffffff;
		gic_dist->icactiver[i] = 0xffffffff;
		gic_dist->igroupr[i] = 0;
	}

	/* Make all but the first 32 interrupts level triggered */
	for (int i = 1; i < 64; i++) {
		gic_dist->icfgr[i] = 0;
	}

	/*
	 * Initialize interrupt priorities.
	 *
	 * Set the CPU-specific interrupts to the lowest possible priority, and
	 * keep a private copy of that priority.
	 */
	for (int i = 0; i < 8; i++) {
		gic_dist->ipriorityr[i] = 0xffffffff;
		ipriorityr_private[i] = gic_dist->ipriorityr[i];
	}

	/*
	 * Set the rest of the interrupts to the lowest possible priority, and
	 * route them to all CPUs.
	 */
	for (int i = 8; i < 256; i++) {
		gic_dist->itargetsr[i] = 0xffffffff;
		gic_dist->ipriorityr[i] = 0xffffffff;
	}

	/* Enable group 0 interrupts, disable everything else */
	gic_cpuif->ctlr = GICC_CTLR_ENABLE_GROUP0;

	/*
	 * Configure the priority fields with 5 bits of group priority and 3
	 * bits of subpriority.
	 */
	gic_cpuif->bpr = 3;

	/* Unmask all interrupt priorities */
	gic_cpuif->pmr = 0xFF;

	/*
	 * Enable group 0 interrupts
	 * disable secure group 1 interrupts
	 * disable non-secure group 1 interrupts.
	 * disable non-secure affinity routing
	 * disable secure affinity routing
	 * enable security (don't set DS)
	 * disable 1 of N wakeup if possible.
	 *
	 * XXXARM: Read this back and see if we get values we don't expect?
	 * XXXARM: Should we be waiting for .RWP to clear?
	 */
	gic_dist->ctlr = GICD_CTLR_ENABLE_GROUP0;

	lock_clear(&gicv2_lock);
	write_daif(old);

	CPUSET_ONLY(gicv2_cpuset, 0);

	gicv2_target[0] = gicv2_get_target();
}

/*
 * Initialize the GIC for a new non-boot CPU.
 *
 * XXXARM: Can this be partially common with init_primary?
 */
static void
gicv2_init_secondary(processorid_t id)
{
	uint64_t old = read_daif();
	set_daif(DAIF_SETCLEAR_IRQ);
	lock_set(&gicv2_lock);

	/*
	 * Clear enabled/pending/active status of the CPU-specific interrupts,
	 * and put them in group 0.
	 */
	gic_dist->icenabler[0] = 0xffffffff;
	gic_dist->icpendr[0]   = 0xffffffff;
	gic_dist->icactiver[0] = 0xffffffff;
	gic_dist->igroupr[0] = 0;

	/*
	 * XXXARM: Original comment here: SGIは設定しない
	 * Google translate: "Do not configure SGI"
	 *
	 * Make interrupts 32-63 default to level triggered.
	 */
	gic_dist->icfgr[1] = 0;

	/*
	 * Initialize interrupt priorities from their global state in
	 * `ipriorityr_private`
	 *
	 * XXXARM: Only used when not affinity routing?
	 */
	for (int i = 0; i < 8; i++) {
		gic_dist->ipriorityr[i] = ipriorityr_private[i];
	}

	/*
	 * Initialize interrupt enabling for interrupts 0-32 from their global
	 * state in `ienable_private`.
	 */
	gic_dist->isenabler[0] = ienable_private;

	/* Enable group 0 interrupts, disable everything else */
	gic_cpuif->ctlr = GICC_CTLR_ENABLE_GROUP0;

	/*
	 * Configure the priority fields with 5 bits of group priority and 3
	 * bits of subpriority.
	 */
	gic_cpuif->bpr = 3;

	/* Unmask all interrupt priorities */
	gic_cpuif->pmr = 0xFF;

	CPUSET_ADD(gicv2_cpuset, id);
	gicv2_target[id] = gicv2_get_target();

	lock_clear(&gicv2_lock);
	write_daif(old);
}

/* The maximum number of CPUs supported by the GIC without affinity routing */
static int
gicv2_num_cpus(void)
{
	return (GICD_TYPER_CPUNUMBER(gic_dist->typer) + 1);
}

static struct modlmisc modlmisc = {
	&mod_miscops,
	"Generic Interrupt Controller v2"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	if (strcmp(gic_module_name, "gicv2") == 0) {
		gic_ops.mask_level_irq = gicv2_mask_level_irq;
		gic_ops.unmask_level_irq = gicv2_unmask_level_irq;
		gic_ops.send_ipi = gicv2_send_ipi;
		gic_ops.init_primary = gicv2_init_primary;
		gic_ops.init_secondary = gicv2_init_secondary;
		gic_ops.config_irq = gicv2_config_irq;
		gic_ops.num_cpus = gicv2_num_cpus;
		gic_ops.addspl = gicv2_addspl;
		gic_ops.delspl = gicv2_delspl;
		gic_ops.setlvl = gicv2_setlvl;
		gic_ops.setlvlx = gicv2_setlvlx;
	}

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
