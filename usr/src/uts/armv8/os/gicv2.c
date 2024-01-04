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
 * Copyright 2024 Michael van der Westhuizen
 */

/*
 * Arm Generic Interrupt Controller v2 Implementation
 *
 * See: IHI0048 ARM® Generic Interrupt Controller Architecture version 2.0.
 *
 * GICv2 supports up to 8 processing elements and the basic SGI, PPI and SPI
 * space, resulting in the following INTID space:
 *    0 -    7: SGI for the non-secure world
 *    8 -   15: SGI for the secure world
 *   16 -   31: PPI
 *   32 - 1019: SPI
 * 1020 - 1023: Special
 *
 * GICv2 does not support affinity routing. Our GICv2 code assumes we're
 * in the normal world, running on a GIC that implements two security states.
 * This is a somewhat safe assumption, as the inetboot (boot shim) common code
 * simply spins if entered in EL3, and we only support A profile cores.
 *
 * A note on priorities.
 *
 * A GICv2 implementation supports at least 16 (0-15) levels of priority.
 * However, only those priorities in the top-half can apply to the normal
 * world (this allows EL1-S to preempt EL1). In reality, this means that on
 * GIC implementations like that found on the Raspberry Pi4 we end up with
 * only eight usable priority mask values.
 *
 * If we find ourselves on such a GIC we use a few bits of sub-priority to
 * order (but not preempt) interrupt delivery. This is safe, but not optimal.
 * If we're on a GIC with a sufficient number of NS priority levels we simply
 * map those to our IPLs.
 *
 * This bodge is local to the GICv2 implementation, as GICv3+ must implement
 * a minimum of 32 priority levels when the implementation supports two
 * security states, which perfectly meets our requirements.
 */

#include <sys/types.h>
#include <sys/gic.h>
#include <sys/gic_reg.h>
#include <sys/avintr.h>
#include <sys/smp_impldefs.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/smp_impldefs.h>

extern char *gic_module_name;

typedef struct {
	/* Base address of the CPU interface */
	void		*gc_gicc;
	/* Base address of the distributor */
	void		*gc_gicd;
	/*
	 * Desired binary point value to support the priority scheme
	 */
	uint32_t	gc_bpr;
	/*
	 * PPI interrupt config for secondary CPUs.
	 */
	uint32_t	gc_icfgr1;
	/*
	 * Shadow copy of GICD_ISENABLER[0] used in initialization of
	 * secondary CPUs (PPI-only);
	 */
	uint32_t	gc_enabled_local;
	/*
	 * Shadow copy of  GICD_IPRIORITYR<0-7> used in initialization of
	 * secondary CPUs.
	 */
	uint32_t	gc_priority[8];
	/*
	 * Protect access to global GIC state.
	 * In the current implementation, the distributor.
	 */
	lock_t		gc_lock;
	/*
	 * Mapping from cpuid to GIC target identifier
	 */
	uint8_t		gc_target[8];
	/*
	 * CPUs for which we have initialized the GIC.  Used to limit IPIs to
	 * only those CPUs we can target.
	 */
	cpuset_t	gc_cpuset;
} gicv2_conf_t;

static gicv2_conf_t	conf;

static uint32_t standard_priorities[] = {
	[0]	= 248,
	[1]	= 240,
	[2]	= 232,
	[3]	= 224,
	[4]	= 216,	/* Disk */
	[5]	= 208,
	[6]	= 200,
	[7]	= 192,	/* NIC */
	[8]	= 184,
	[9]	= 176,
	[10]	= 168,	/* Clock */
	[11]	= 160,	/* Dispatcher */
	[12]	= 152,
	[13]	= 144,
	[14]	= 136,
	[15]	= 128,
};

#define	STANDARD_PRIORITY_PMR_MASK	0x000000F8
/*
 * Configure the priority fields with the smallest possible sub-priority.
 *
 * In the standard configuration we don't use sub-priority at all.
 */
#define	STANDARD_BPR			0x00000000

/*
 * Required BPR is 3 bits
 */
static uint32_t bodged_priorities[] = {
	[0]	= 240,	/* Real */
	[1]	= 228,	/* Fake */
	[2]	= 226,	/* Fake */
	[3]	= 225,	/* Fake */
	[4]	= 224,	/* Real, Disk */
	[5]	= 209,	/* Fake */
	[6]	= 208,	/* Real */
	[7]	= 192,	/* Real, NIC */
	[8]	= 177,	/* Fake */
	[9]	= 176,	/* Real */
	[10]	= 160,	/* Real, Clock  */
	[11]	= 144,	/* Real, Dispatcher */
	[12]	= 132,	/* Fake */
	[13]	= 130,	/* Fake */
	[14]	= 129,	/* Fake */
	[15]	= 128,	/* Real */
};

#define	BODGED_PRIORITY_PMR_MASK	0x000000F0
/*
 * Configure the priority fields with 5 bits of group priority and 3 bits of
 * subpriority. This may not actually work, as the minimums may be lower,
 * but we do the best that we can.
 *
 * Even of the minimum is lower our priority mask remains valid, it's just
 * ordering that might be affected.
 */
#define	BODGED_BPR			0x00000002

/*
 * IPL -> GIC Priority table to use.
 */
static uint32_t *gicv2_prio_map;
/*
 * Mask to apply to the GIC priority when setting the priority mask register
 * on the GIC CPU Interface.
 *
 * Do not apply this mask when setting interrupt configuration.
 */
static uint32_t gicv2_prio_pmr_mask;

#undef GIC_IPL_TO_PRIO
#define	GIC_IPL_TO_PRIO(v)		(gicv2_prio_map[((v) & 0xF)])

#define	GICV2_GICD_LOCK_INIT_HELD()	uint64_t old_daif = read_daif(); \
					set_daif(DAIF_SETCLEAR_IRQ); \
					LOCK_INIT_HELD(&conf.gc_lock)
#define	GICV2_GICD_LOCK()		uint64_t old_daif = read_daif(); \
					set_daif(DAIF_SETCLEAR_IRQ); \
					lock_set(&conf.gc_lock)
#define	GICV2_GICD_UNLOCK()		lock_clear(&conf.gc_lock); \
					write_daif(old_daif)
#define	GICV2_ASSERT_GICD_LOCK_HELD()	ASSERT(LOCK_HELD(&conf.gc_lock))

static inline uint32_t
gicc_read(gicv2_conf_t *gic, uint32_t reg)
{
	return (i_ddi_get32(NULL, (uint32_t *)(gic->gc_gicc + reg)));
}

static inline void
gicc_write(gicv2_conf_t *gic, uint32_t reg, uint32_t val)
{
	i_ddi_put32(NULL, (uint32_t *)(gic->gc_gicc + reg), val);
}

static inline uint32_t
gicd_read(gicv2_conf_t *gic, uint32_t reg)
{
	return (i_ddi_get32(NULL, (uint32_t *)(gic->gc_gicd + reg)));
}

static inline void
gicd_write(gicv2_conf_t *gic, uint32_t reg, uint32_t val)
{
	i_ddi_put32(NULL, (uint32_t *)(gic->gc_gicd + reg), val);
}

static inline uint32_t
gicd_rmw(gicv2_conf_t *gic, uint32_t reg, uint32_t clrbits, uint32_t setbits)
{
	uint32_t val;
	uint32_t *regaddr = (uint32_t *)(gic->gc_gicd + reg);

	val = (i_ddi_get32(NULL, regaddr) & (~clrbits)) | setbits;
	i_ddi_put32(NULL, regaddr, val);
	return (val);
}

/*
 * Enable IRQ in the distributor, which will now be forwarded to a cpu.
 *
 * 4.3.5 Interrupt Set-Enable Registers, GICD_ISENABLERn (Usage constraints):
 *   Whether implemented SGIs are permanently enabled, or can be enabled and
 *   disabled by writes to GICD_ISENABLER0 and GICD_ICENABLER0, is
 *   IMPLEMENTATION DEFINED.
 *
 * We never try to configure SGIs.
 */
static void
gicv2_enable_irq(int irq)
{
	if (GIC_INTID_IS_SPI(irq) || GIC_INTID_IS_PPI(irq)) {
		GICV2_ASSERT_GICD_LOCK_HELD();
		gicd_write(&conf, GICD_ISENABLERn(GICD_IENABLER_REGNUM(irq)),
		    GICD_IENABLER_REGBIT(irq));
	}
}

/*
 * Disable IRQ in the distributor, which will now cease being forwarded to a
 * cpu.
 *
 * 4.3.5 Interrupt Clear-Enable Registers, GICD_ICENABLERn (Usage constraints):
 *   Whether implemented SGIs are permanently enabled, or can be enabled and
 *   disabled by writes to GICD_ISENABLER0 and GICD_ICENABLER0, is
 *   IMPLEMENTATION DEFINED.
 *
 * We never try to configure SGIs.
 */
static void
gicv2_disable_irq(int irq)
{
	if (GIC_INTID_IS_SPI(irq) || GIC_INTID_IS_PPI(irq)) {
		GICV2_ASSERT_GICD_LOCK_HELD();
		gicd_write(&conf, GICD_ICENABLERn(GICD_IENABLER_REGNUM(irq)),
		    GICD_IENABLER_REGBIT(irq));
	}
}

/*
 * Configure whether IRQ is edge or level triggered.
 */
static void
gicv2_config_irq(uint32_t irq, bool is_edge)
{
	uint32_t v = (is_edge ?
	    GICD_ICFGR_INT_CONFIG_EDGE : GICD_ICFGR_INT_CONFIG_LEVEL);

	/*
	 * SGIs are not configurable.
	 */
	if (GIC_INTID_IS_SGI(irq))
		return;

	GICV2_GICD_LOCK();

	/*
	 * §8.9.7 Software must disable an interrupt before the value of the
	 * corresponding programmable Int_config field is changed. GIC
	 * behavior is otherwise UNPREDICTABLE.
	 */
	ASSERT(((gicd_read(&conf, GICD_ISENABLERn(GICD_IENABLER_REGNUM(irq))) &
	    GICD_IENABLER_REGBIT(irq)) == 0));

	/*
	 * GICD_ICFGR<n> is a packed field with 2 bits per interrupt, the even
	 * bit is reserved, the odd bit is 1 for edge-triggered 0 for
	 * level.
	 */
	(void) gicd_rmw(&conf,
	    GICD_ICFGRn(GICD_ICFGR_REGNUM(irq)),
	    GICD_ICFGR_REGVAL(irq, GICD_ICFGR_INT_CONFIG_MASK),
	    GICD_ICFGR_REGVAL(irq, v));
	GICV2_GICD_UNLOCK();
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
		gicc_write(&conf, GICC_PMR,
		    GIC_IPL_TO_PRIO(new_ipl) & gicv2_prio_pmr_mask);
	}

	return (new_ipl);
}

/*
 * Mask interrupts of priority lower than or equal to IPL.
 */
static void
gicv2_setlvlx(int ipl)
{
	gicc_write(&conf, GICC_PMR, GIC_IPL_TO_PRIO(ipl) & gicv2_prio_pmr_mask);
}

/*
 * Set the priority of IRQ to IPL
 * If IRQ is an SGI or PPI, shadow that priority into `ipriorityr_private`
 */
static void
gicv2_set_ipl(uint32_t irq, uint32_t ipl)
{
	uint32_t ipriorityr;
	uint32_t n;

	GICV2_ASSERT_GICD_LOCK_HELD();
	n = GICD_IPRIORITY_REGNUM(irq);
	ipriorityr = gicd_rmw(&conf,
	    GICD_IPRIORITYRn(n),
	    GICD_IPRIORITY_REGVAL(irq, GICD_IPRIORITY_REGMASK),
	    GICD_IPRIORITY_REGVAL(irq, GIC_IPL_TO_PRIO(ipl)));

	if (GIC_INTID_IS_PERCPU(irq)) {
		conf.gc_priority[n] = ipriorityr;
	}
}

/*
 * Configure non-local IRQs to be delivered through the distributor.
 *
 * XXXARM: We need interrupt redistribution.
 */
static void
gicv2_add_target(uint32_t irq)
{
	uint32_t coreMask = GICD_ITARGETSR_REGMASK; /* all 8 cpus */

	/*
	 * Each GICD_ITARGETSR<n> contains 4 8-bit fields indicating that int
	 * N is delivered to the cpus with 1 bits set in the value.
	 *
	 * We always program all interrupts to deliver to all possible CPUs,
	 * trusting RAZ/WI for those which don't exist.
	 */
	if (!GIC_INTID_IS_PERCPU(irq)) {
		GICV2_ASSERT_GICD_LOCK_HELD();
		(void) gicd_rmw(&conf,
		    GICD_ITARGETSRn(GICD_ITARGETSR_REGNUM(irq)),
		    GICD_ITARGETSR_REGVAL(irq, GICD_ITARGETSR_REGMASK),
		    GICD_ITARGETSR_REGVAL(irq, coreMask));
	}
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
	GICV2_GICD_LOCK();
	gicv2_set_ipl((uint32_t)irq, (uint32_t)ipl);
	gicv2_add_target((uint32_t)irq);
	gicv2_enable_irq((uint32_t)irq);
	if (GIC_INTID_IS_PPI(irq) && CPU->cpu_id == 0) {
		conf.gc_enabled_local |= (1U << irq);
	}
	GICV2_GICD_UNLOCK();
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
		GICV2_GICD_LOCK();
		gicv2_disable_irq((uint32_t)irq);
		gicv2_set_ipl((uint32_t)irq, 0);
		if (GIC_INTID_IS_PPI(irq) && CPU->cpu_id == 0) {
			conf.gc_enabled_local &= ~(1U << irq);
		}
		GICV2_GICD_UNLOCK();
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

	GICV2_GICD_LOCK();
	CPUSET_AND(cpuset, conf.gc_cpuset);
	while (!CPUSET_ISNULL(cpuset)) {
		uint_t cpu;
		CPUSET_FIND(cpuset, cpu);
		target |= conf.gc_target[cpu];
		CPUSET_DEL(cpuset, cpu);
	}
	dsb(ish);

	/* The third argument (NSATTR) is ignored from the non-secure world */
	gicd_write(&conf, GICD_SGIR, GICD_MAKE_SGIR_REGVAL(0, target, 0, irq));
	GICV2_GICD_UNLOCK();
}

static uint32_t
gicv2_acknowledge(void)
{
	return (gicc_read(&conf, GICC_IAR));
}

static uint32_t
gicv2_ack_to_vector(uint32_t ack)
{
	return (ack & GICC_IAR_INTID_NO_ARE);
}

static void
gicv2_eoi(uint32_t ack)
{
	gicc_write(&conf, GICC_EOIR, ack);
}

static void
gicv2_deactivate(uint32_t ack)
{
	gicc_write(&conf, GICC_DIR, ack);
}

/*
 * Return the target representing the current cpu from the GIC point of view
 * by reading the target field of a target specific interrupt.
 *
 * This sets the Nth bit for target N
 */
static uint_t
gicv2_get_target(void)
{
	GICV2_ASSERT_GICD_LOCK_HELD();
	return (1U << __builtin_ctz(
	    gicd_read(&conf, GICD_ITARGETSRn(0)) & 0xFF));
}

/*
 * Return the GICv2 PROM node, or OBP_NONODE if none was found.
 */
static pnode_t
find_gic(pnode_t nodeid, int depth)
{
	pnode_t	node;
	pnode_t	child;

	GICV2_ASSERT_GICD_LOCK_HELD();

	if (prom_is_compatible(nodeid, "arm,cortex-a15-gic") ||
	    prom_is_compatible(nodeid, "arm,gic-400")) {
		return (nodeid);
	}

	child = prom_childnode(nodeid);
	while (child > 0) {
		node = find_gic(child, depth + 1);
		if (node > 0)
			return (node);
		child = prom_nextnode(child);
	}

	return (OBP_NONODE);
}

/*
 * Map the GICv2 distributor and CPU interface MMIO regions into the device
 * arena.
 */
static int
gicv2_map(void)
{
	pnode_t		node;
	uint64_t	gicd_base;
	uint64_t	gicd_size;
	uint64_t	gicc_base;
	uint64_t	gicc_size;
	caddr_t		addr;

	GICV2_ASSERT_GICD_LOCK_HELD();

	node = find_gic(prom_rootnode(), 0);
	if (node <= 0)
		return (-1);

	if (prom_get_reg_address(node, 0, &gicd_base) != 0)
		return (-1);

	if (prom_get_reg_size(node, 0, &gicd_size) != 0)
		return (-1);

	if (prom_get_reg_address(node, 1, &gicc_base) != 0)
		return (-1);

	if (prom_get_reg_size(node, 1, &gicc_size) != 0)
		return (-1);

	addr = psm_map_phys(gicd_base, gicd_size, PROT_READ|PROT_WRITE);
	if (addr == NULL)
		return (-1);
	conf.gc_gicd = (void *)addr;

	addr = psm_map_phys(gicc_base, gicc_size, PROT_READ|PROT_WRITE);
	if (addr == NULL) {
		psm_unmap_phys((caddr_t)conf.gc_gicd, gicd_size);
		conf.gc_gicd = NULL;
		return (-1);
	}
	conf.gc_gicc = (void *)addr;

	return (0);
}

/*
 * Private function used for initializing CPUs.
 *
 * The boot processor is initialized from the tail of the main gicv2_init
 * function, which calls this function with the distributor lock held.
 *
 * Secondary CPUs enter this function via gicv2_cpu_init, which manages the
 * distributor lock.
 */
static void
gicv2_cpu_init_raw(cpu_t *cp)
{
	GICV2_ASSERT_GICD_LOCK_HELD();

	/*
	 * Disable the current CPU interface.
	 */
	gicc_write(&conf, GICC_CTLR, 0);

	/*
	 * Clear enabled/pending/active status of the CPU-specific interrupts.
	 *
	 * We'll restore the enabled state for secondary CPU PPIs below.
	 *
	 * Note that we do not attempt to disable SGIs, as that's an
	 * implementation-defined operation.
	 */
	gicd_write(&conf, GICD_ICENABLERn(0), 0xffff0000);
	gicd_write(&conf, GICD_ICPENDRn(0), 0xffffffff);
	gicd_write(&conf, GICD_ICACTIVERn(0), 0xffffffff);

	/*
	 * When initialising the boot CPU we do a bit more.
	 */
	if (cp->cpu_id == 0) {
		/*
		 * Record that we've cleared the enabled state of PPIs.
		 *
		 * As we enable PPIs on the boot CPU they are recorded into
		 * this variable. We later use this information when booting
		 * secondary CPUs.
		 */
		conf.gc_enabled_local = 0x0;

		/*
		 * Figure out how to map IPLs to GIC priorities.
		 */
		gicc_write(&conf, GICC_PMR, 0xFF);

		if ((gicc_read(&conf, GICC_PMR) & 0xf) == 0) {
			gicv2_prio_map = bodged_priorities;
			gicv2_prio_pmr_mask = BODGED_PRIORITY_PMR_MASK;
			conf.gc_bpr = BODGED_BPR;
		} else {
			gicv2_prio_map = standard_priorities;
			gicv2_prio_pmr_mask = STANDARD_PRIORITY_PMR_MASK;
			conf.gc_bpr = STANDARD_BPR;
		}

		/*
		 * Initialize interrupt priorities for per-CPU interrupts,
		 * setting them to the lowest possible priority and keeping a
		 * private copy of their priorities for use in initializing
		 * other processors.
		 */
		for (int i = 0; i < 8; ++i) {
			gicd_write(&conf, GICD_IPRIORITYRn(i), 0xffffffff);
			conf.gc_priority[i] =
			    gicd_read(&conf, GICD_IPRIORITYRn(i));
		}
	} else {
		/*
		 * Set PPIs to the configuration we set for the boot processor.
		 *
		 * Configuring PPIs is implementation-defined, so this might
		 * have no effect.
		 */
		gicd_write(&conf, GICD_ICFGRn(1), conf.gc_icfgr1);

		/*
		 * Initialize interrupt priorities for per-CPU interrupts from
		 * the shadow copy of the priority registers.
		 */
		for (int i = 0; i < 8; ++i) {
			gicd_write(&conf, GICD_IPRIORITYRn(i),
			    conf.gc_priority[i]);
		}

		/*
		 * Update enable bits for PPIs.
		 *
		 * These reflect the state of PPI on the boot processor at the
		 * time the secondary CPU comes up. No further attempt at
		 * synchronization is made.
		 */
		gicd_write(&conf, GICD_ISENABLERn(0), conf.gc_enabled_local);
	}

	/*
	 * Apply our subpriority configuration.
	 */
	gicc_write(&conf, GICC_BPR, conf.gc_bpr);

	/*
	 * Confugure the priority mask register to leave us at LOCK_LEVEL once
	 * initialized.
	 */
	gicc_write(&conf, GICC_PMR,
	    GIC_IPL_TO_PRIO(LOCK_LEVEL) & gicv2_prio_pmr_mask);

	/*
	 * Record our target for interrupt routing.
	 */
	conf.gc_target[cp->cpu_id] = gicv2_get_target();

	/*
	 * Enable the CPU interface.
	 *
	 * Note that we enable split priority drop and deactivation so that we
	 * can properly support threaded intrerrupts.
	 */
	gicc_write(&conf, GICC_CTLR,
	    GICC_CTLR_EnableGrp1 | GICC_CTLR_EOImodeNS);

	/*
	 * Finally, tell the world we're ready.
	 */
	CPUSET_ADD(conf.gc_cpuset, cp->cpu_id);
}

/*
 * Public function used for initializing secondary CPUs.
 *
 * Simply wraps the gicv2_cpu_init_raw call in shared state locks.
 */
static void
gicv2_cpu_init(cpu_t *cp)
{
	GICV2_GICD_LOCK();
	gicv2_cpu_init_raw(cp);
	GICV2_GICD_UNLOCK();
}

/*
 * Map GIC register space and perform global GIC initialization, including
 * disabling the CPU interface on the boot processor.
 *
 * Returns non-zero on error.
 */
static int
gicv2_init(void)
{
	GICV2_GICD_LOCK_INIT_HELD();

	if (gicv2_map() != 0) {
		GICV2_GICD_UNLOCK();
		return (-1);
	}

	/*
	 * Mask all interrupts on the current CPU interface, then disable it.
	 *
	 * This is the last time we should touch the GIC CPU interface in this
	 * function.
	 */
	gicc_write(&conf, GICC_CTLR, 0);

	/*
	 * Disable the distributor.
	 */
	gicd_write(&conf, GICD_CTLR, 0);

	/*
	 * Clear enabled/pending/active status of global interrupts.
	 */
	for (int i = 1; i < 32; ++i) {
		gicd_write(&conf, GICD_ICENABLERn(i), 0xffffffff);
		gicd_write(&conf, GICD_ICPENDRn(i), 0xffffffff);
		gicd_write(&conf, GICD_ICACTIVERn(i), 0xffffffff);
	}

	/*
	 * Make all hardware interrupts level triggered.
	 *
	 * GICD_ICFGRn(0) is SGI, and we can't configure those.
	 * GICD_ICFGRn(1) is PPI, configuring these is implementation-defined.
	 */
	for (int i = 1; i < 64; i++) {
		gicd_write(&conf, GICD_ICFGRn(i), 0x0);
	}

	/*
	 * Save PPI interrupt configuration so we can apply it to secondary
	 * CPUs. Configuring PPIs is implementation-defined, but we try anyway.
	 */
	conf.gc_icfgr1 = gicd_read(&conf, GICD_ICFGRn(1));

	/*
	 * Initialize interrupt priorities for global interrupts, setting them
	 * to the lowest possible priority and routing them to all possible
	 * CPUs. XXXARM: we need to implement interrupt redistribution.
	 */
	for (int i = 8; i < 256; ++i) {
		gicd_write(&conf, GICD_IPRIORITYRn(i), 0xffffffff);
		gicd_write(&conf, GICD_ITARGETSRn(i), 0xffffffff);
	}

	/*
	 * No CPUs have been configured yet.
	 */
	CPUSET_ZERO(conf.gc_cpuset);

	/*
	 * Enable the distributor.
	 */
	gicd_write(&conf, GICD_CTLR, GICD_CTLR_EnableGrp1);

	/*
	 * While we still hold the lock we initialize the boot processor.
	 */
	gicv2_cpu_init_raw(CPU);

	GICV2_GICD_UNLOCK();
	return (0);
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
		gic_ops.go_send_ipi = gicv2_send_ipi;
		gic_ops.go_init = gicv2_init;
		gic_ops.go_cpu_init = gicv2_cpu_init;
		gic_ops.go_config_irq = gicv2_config_irq;
		gic_ops.go_addspl = gicv2_addspl;
		gic_ops.go_delspl = gicv2_delspl;
		gic_ops.go_setlvl = gicv2_setlvl;
		gic_ops.go_setlvlx = gicv2_setlvlx;
		gic_ops.go_acknowledge = gicv2_acknowledge;
		gic_ops.go_ack_to_vector = gicv2_ack_to_vector;
		gic_ops.go_eoi = gicv2_eoi;
		gic_ops.go_deactivate = gicv2_deactivate;
		/*
		 * Explicitly use the default "is special" handler.
		 */
		gic_ops.go_vector_is_special = (gic_vector_is_special_t)NULL;
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
