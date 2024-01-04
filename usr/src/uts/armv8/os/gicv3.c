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
 * Copyright 2024 Michael van der Westhuizen
 */

/*
 * Arm Generic Interrupt Controller v3 Implementation
 *
 * See IHI0069: Arm® Generic Interrupt Controller Architecture Specification
 *              GIC architecture version 3 and version 4
 *
 * For basic usage, GICv3 differs from GICv2 by introducing a new block, the
 * redistributor. In a setup with affinity routing, which we always configure,
 * the distributor remains responsible for configuring and routing shared
 * peripheral interrupts (SPIs) while responsibility for configuring
 * per-processor interrupts (software generated interrupts, or SGIs, and
 * private peripheral interrupts, or PPIs) moves to the redistributor.
 *
 * Since per-processor registers are no longer banked on a per-processor basis
 * configuration of per-processor interrupts _for other processors_ becomes
 * possible, simplifying the application of consistent configuration across
 * processors after MP startup. For this reason, interrupt configuration is
 * split into three functions per action: one for SPIs, one for per-processor
 * configuration and one to decide which implementation to use.
 *
 * Per-processor configuration is run via a helper that iterates through
 * the redistributors, applying a function to each. Interrupts are disabled
 * for the duration of the iteration and individual redistributors are locked
 * while the configuration function is applied to them. This scales badly as
 * the number of processors grows, but interrupt configuration is infrequent
 * and mostly happens at system startup.
 *
 * A major difference from GICv2 is that the CPU interface is now exposed as
 * system registers, which improves interrupt latency somewhat. A side-effect
 * of this change, alongside the introduction of extended PPI and SPI ranges
 * and a new interrupt type (locality-specific peripheral interrupt, or LPI),
 * is that the interrupt handling registers are now wider (64 bits instead of
 * 32).
 *
 * A redistributor consists of either two or four 64k frames, these are:
 * - The redistributor frame
 * - The SGI frame, badly named as this configures all per-processor interrupts
 * - The virtual LPI frame, if VLPI is supported
 * - A reserved frame, if VLPI is supported
 * Redistributor register offsets are expressed relative to these frames, so
 * the redistributor structure stores pointers into the mapped redistributor
 * regions representing these frame addresses. MMIO helpers exist for each
 * frame to make it clear which registers are being accessed.
 *
 * We always do EOImode=1, which splits interrupt lifecycle management to
 * separate the running priority drop and deactivation of the interrupt.
 * Taking this approach alleviates the strict ordering requirement imposed
 * by the running priority drop, enabling full support for threaded IRQs.
 */

#include <sys/types.h>
#include <sys/gic.h>
#include <sys/gic_reg.h>
#include <sys/avintr.h>
#include <sys/smp_impldefs.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/cpuinfo.h>
#include <sys/sysmacros.h>
#include <sys/archsystm.h>

extern char *gic_module_name;

/*
 * A redistributor region is a block of redistributor MMIO space. One finds
 * redistributors within this region by reading the TYPER register at the
 * head of each block and using that to advance a pointer through the space
 * until the GICR_TYPER.Last bit is set, indicating that we've hit the last
 * redistributor in a region.
 *
 * This structure represents the device arena mapping of the redistributor
 * regions.
 */
typedef struct {
	void		*base;
	uint64_t	size;
} gicv3_redist_region_t;

/*
 * The redistributor structure describes a redistributor in terms of the
 * redistributor frame addresses in the kernel address space.
 *
 * The structure also contains a lock, which must be held when accessing the
 * redistributor frames, a cached copy of the redistributor type register and
 * a template to be used when sending software generated interrupts to the
 * processor associated with this redistributor.
 *
 * Redistributor structures are stored in an array, indexed by CPU ID.
 */
typedef struct {
	lock_t			gr_lock;
	void			*gr_rd_base;
	void			*gr_sgi_base;
	void			*gr_vlpi_base;
	uint64_t		gr_typer;
	uint64_t		gr_sgir;
} gicv3_redistributor_t;

typedef struct {
	/* Base address of the CPU interface */
	void			*gc_gicc;
	/* Base address of the distributor */
	void			*gc_gicd;
	/* Size of the distributor mapping */
	uint64_t		gc_gicd_size;
	/* Shadow copy of the distributor type register */
	uint32_t		gc_gicd_typer;
	/* Number of interrupt sources in the traditional interrupt space */
	uint32_t		gc_maxsources;

	/* Owned mappings of the redistributor regions. */
	gicv3_redist_region_t	*gc_redist_regions;
	/* Number of redistributor regions */
	uint32_t		gc_num_redist_regions;
	/* Redistributor iteration stride, 0 if no padding pages are present */
	uint64_t		gc_redist_stride;

	/*
	 * Redistributors, indexed by CPU ID.
	 *
	 * The pointers in these structures index into the mappings owned by
	 * gc_redist_regions.
	 */
	gicv3_redistributor_t	*gc_redist;
	/* Number of redistributors in the gc_redist array */
	uint32_t		gc_num_redist;

	/* A flag indicating that we have 32 (or more) priority levels */
	uint32_t		gc_pri32;
	/* Protect access the distributor */
	lock_t		gc_dist_lock;
	/*
	 * CPUs for which we have initialized the GIC.  Used to limit IPIs to
	 * only those CPUs we can target.
	 */
	cpuset_t	gc_cpuset;
} gicv3_conf_t;

static gicv3_conf_t	conf;

#define	GICR_FRAME_SIZE		(64 * 1024)

#define	GIC_IPL_TO_PRI(ipl)	(conf.gc_pri32 ? (GIC_IPL_TO_PRIO((ipl))) : \
				(GIC_IPL_TO_PRIO16((ipl))))

#define	GICD_LOCK_INIT_HELD()	uint64_t __s = disable_interrupts(); \
				LOCK_INIT_HELD(&conf.gc_dist_lock)
#define	GICD_LOCK()		uint64_t __s = disable_interrupts(); \
				lock_set(&conf.gc_dist_lock)
#define	GICD_UNLOCK()		lock_clear(&conf.gc_dist_lock); \
				restore_interrupts(__s)

static inline uint32_t
reg_read_raw4(void *base, uint32_t reg)
{
	return (i_ddi_get32(NULL, (uint32_t *)(base + reg)));
}

static inline void
reg_write_raw4(void *base, uint32_t reg, uint32_t val)
{
	i_ddi_put32(NULL, (uint32_t *)(base + reg), val);
}

static inline uint32_t
reg_rmw_raw4(void *base, uint32_t reg, uint32_t clrbits, uint32_t setbits)
{
	uint32_t val;
	val = (reg_read_raw4(base, reg) & (~clrbits)) | setbits;
	reg_write_raw4(base, reg, val);
	return (val);
}

static inline void
reg_await_clear4(void *base, uint32_t reg, uint32_t mask)
{
	while (reg_read_raw4(base, reg) & mask)
		;
}

static inline uint64_t
reg_read_raw8(void *base, uint32_t reg)
{
	return (i_ddi_get64(NULL, (uint64_t *)(base + reg)));
}

static inline void
reg_write_raw8(void *base, uint32_t reg, uint64_t val)
{
	i_ddi_put64(NULL, (uint64_t *)(base + reg), val);
}

static inline uint32_t
gicd_read4(gicv3_conf_t *gic, uint32_t reg)
{
	return (reg_read_raw4(gic->gc_gicd, reg));
}

static inline void
gicd_write4(gicv3_conf_t *gic, uint32_t reg, uint32_t val)
{
	reg_write_raw4(gic->gc_gicd, reg, val);
}

static inline void
gicd_write8(gicv3_conf_t *gic, uint32_t reg, uint64_t val)
{
	reg_write_raw8(gic->gc_gicd, reg, val);
}

static inline uint32_t
gicd_rmw4(gicv3_conf_t *gic, uint32_t reg, uint32_t clrbits, uint32_t setbits)
{
	return (reg_rmw_raw4(gic->gc_gicd, reg, clrbits, setbits));
}

/*
 * Drain any outstanding writes to the distributor.
 *
 * Draining is necessary after writes to:
 * - GICD_CTLR[2:0] - group enables, only when disabling.
 * - GICD_CTLR[7:4] - the ARE bits, E1NWF bit and DS bit.
 * - GICD_ICENABLER<n> - write-to-clear (disable) registers.
 */
static void
gicd_drain_writes(gicv3_conf_t *gic)
{
	reg_await_clear4(gic->gc_gicd, GICD_CTLR, GICD_CTLR_RWP);
}

static inline uint32_t
gicr_rd_read4(gicv3_redistributor_t *r, uint32_t reg)
{
	return (reg_read_raw4(r->gr_rd_base, reg));
}

static inline uint32_t
gicr_rd_rmw4(gicv3_redistributor_t *r,
    uint32_t reg, uint32_t clrbits, uint32_t setbits)
{
	return (reg_rmw_raw4(r->gr_rd_base, reg, clrbits, setbits));
}

/*
 * Drain any outstanding writes to a redistributor.
 *
 * Needed after writes to:
 * - GICR_ICENABLER0
 * - GICR_CTLR.DPG1S
 * - GICR_CTLR.DPG1NS
 * - GICR_CTLR.DPG0
 * - GICR_CTLR.EnableLPIs on changing from 1 to 0 (disabling)
 * - GICR_VPROPBASER on changing Valid from 1 to 0
 */
static void
gicr_drain_writes(gicv3_redistributor_t *r)
{
	reg_await_clear4(r->gr_rd_base, GICR_CTLR, GICR_CTLR_RWP);
}

static inline uint32_t
gicr_sgi_read4(gicv3_redistributor_t *r, uint32_t reg)
{
	return (reg_read_raw4(r->gr_sgi_base, reg));
}

static inline void
gicr_sgi_write4(gicv3_redistributor_t *r, uint32_t reg, uint32_t val)
{
	reg_write_raw4(r->gr_sgi_base, reg, val);
}

static inline uint32_t
gicr_sgi_rmw4(gicv3_redistributor_t *r,
    uint32_t reg, uint32_t clrbits, uint32_t setbits)
{
	return (reg_rmw_raw4(r->gr_sgi_base, reg, clrbits, setbits));
}

/*
 * Private function used to awaken a CPU.
 *
 * For a CPU to receive interrupts the GICR_WAKER.ProcessorSleep bit must be
 * clear and the GICR_WAKER.ChildrenAsleep must have cleared (indicating that
 * the wakeup is complete).
 */
static void
gicv3_awaken_cpu(gicv3_conf_t *gc, cpu_t *cp)
{
	gicv3_redistributor_t *r;
	uint64_t s;

	VERIFY(cp->cpu_id < gc->gc_num_redist);
	r = &gc->gc_redist[cp->cpu_id];

	s = disable_interrupts();
	lock_set(&r->gr_lock);
	gicr_rd_rmw4(r, GICR_WAKER, GICR_WAKER_ProcessorSleep, 0x0);
	reg_await_clear4(r->gr_rd_base, GICR_WAKER, GICR_WAKER_ChildrenAsleep);
	lock_clear(&r->gr_lock);
	restore_interrupts(s);
}

/*
 * Private helper function used to apply a function to all redistributors.
 *
 * Redistributor configuration is treated as atomic, so interrupts are disabled
 * for the duration of all redistributor updates. Each redistributor is locked
 * prior to the passed function being applied to it, then unlocked afterwards.
 */
static void
gicv3_for_each_gicr(gicv3_conf_t *gc,
    void (*fn)(gicv3_redistributor_t *, uint32_t a0, uint32_t a1),
    uint32_t a0, uint32_t a1)
{
	gicv3_redistributor_t	*r;
	uint64_t		s;
	uint32_t		i;

	s = disable_interrupts();
	for (i = 0; i < gc->gc_num_redist; ++i) {
		r = &gc->gc_redist[i];
		VERIFY(r->gr_rd_base != NULL);
		VERIFY(r->gr_sgi_base != NULL);
		lock_set(&r->gr_lock);
		(*fn)(r, a0, a1);
		lock_clear(&r->gr_lock);
	}
	restore_interrupts(s);
}

/*
 * IRQ Configuration (level or edge triggered).
 */

/* Per-CPU interrupt configuration helper */
static void
gicv3_config_irq_percpu(gicv3_redistributor_t *r, uint32_t irq, uint32_t v)
{
	/*
	 * §12.11.8 Changing Int_config when the interrupt is
	 * individually enabled is UNPREDICTABLE.
	 */
	ASSERT(((gicr_sgi_read4(r, GICR_ISENABLER0) &
	    GICR_IENABLER_REGBIT(irq)) == 0));
	(void) gicr_sgi_rmw4(r,
	    GICR_ICFGR1,
	    GICR_ICFGR_REGVAL(irq, GICR_ICFGR_INT_CONFIG_MASK),
	    GICD_ICFGR_REGVAL(irq, v));
}

/* Shared peripheral interrupt configuration */
static void
gicv3_config_irq_spi(gicv3_conf_t *gc, uint32_t irq, uint32_t v)
{
	GICD_LOCK();
	/*
	 * §12.9.9 Changing Int_config when the interrupt is
	 * individually enabled is UNPREDICTABLE.
	 */
	ASSERT(((gicd_read4(gc,
	    GICD_ISENABLERn(GICD_IENABLER_REGNUM(irq))) &
	    GICD_IENABLER_REGBIT(irq)) == 0));
	(void) gicd_rmw4(gc,
	    GICD_ICFGRn(GICD_ICFGR_REGNUM(irq)),
	    GICD_ICFGR_REGVAL(irq, GICD_ICFGR_INT_CONFIG_MASK),
	    GICD_ICFGR_REGVAL(irq, v));
	GICD_UNLOCK();
}

static void
gicv3_config_irq(uint32_t irq, bool is_edge)
{
	const uint32_t v = (is_edge ?
	    GICD_ICFGR_INT_CONFIG_EDGE : GICD_ICFGR_INT_CONFIG_LEVEL);

	if (GIC_INTID_IS_SGI(irq)) {
		/* SGIs are not configurable */
	} else if (GIC_INTID_IS_PPI(irq)) {
		gicv3_for_each_gicr(&conf,
		    gicv3_config_irq_percpu, irq, v);
	} else if (GIC_INTID_IS_SPI(irq)) {
		gicv3_config_irq_spi(&conf, irq, v);
	}
}

/*
 * Mask interrupts of priority lower than, or equal to, IRQ.
 */
static int
gicv3_setlvl(int irq)
{
	int new_ipl;
	new_ipl = autovect[irq].avh_hi_pri;

	if (new_ipl != 0) {
		write_icc_pmr_el1(GIC_IPL_TO_PRI(new_ipl));
	}

	return (new_ipl);
}

/*
 * Mask interrupts of priority lower than or equal to IPL.
 */
static void
gicv3_setlvlx(int ipl)
{
	write_icc_pmr_el1(GIC_IPL_TO_PRI(ipl));
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
 *
 * XXXARM: We need interrupt redistribution.
 */

/* Add SPL for shared peripheral interrupts */
static void
gicv3_addspl_spi(gicv3_conf_t *gc, uint32_t irq, uint32_t ipl)
{
	GICD_LOCK();

	/*
	 * Set the priority.
	 */
	(void) gicd_rmw4(gc,
	    GICD_IPRIORITYRn(GICD_IPRIORITY_REGNUM(irq)),
	    GICD_IPRIORITY_REGVAL(irq, GICD_IPRIORITY_REGMASK),
	    GICD_IPRIORITY_REGVAL(irq, GIC_IPL_TO_PRI(ipl)));

	/*
	 * Set the target CPU.
	 */
	if ((conf.gc_gicd_typer & GICD_TYPER_No1N) == 0)
		gicd_write8(gc, GICD_IROUTERn(irq),
		    GICD_IROUTER_Interrupt_Routing_Mode);
	else
		gicd_write8(gc, GICD_IROUTERn(irq), cpu[0]->cpu_m.affinity);

	/*
	 * Enable the interrupt.
	 */
	gicd_write4(gc, GICD_ISENABLERn(GICD_IENABLER_REGNUM(irq)),
	    GICD_IENABLER_REGBIT(irq));

	GICD_UNLOCK();
}

/* Add SPL for per-CPU interrupts */
static void
gicv3_addspl_percpu(gicv3_redistributor_t *r, uint32_t irq, uint32_t ipl)
{
	/*
	 * Set the priority.
	 */
	(void) gicr_sgi_rmw4(r,
	    GICR_IPRIORITYRn(GICR_IPRIORITY_REGNUM(irq)),
	    GICR_IPRIORITY_REGVAL(irq, GICR_IPRIORITY_REGMASK),
	    GICR_IPRIORITY_REGVAL(irq, GIC_IPL_TO_PRI(ipl)));

	/*
	 * Enable the interrupt.
	 */
	gicr_sgi_write4(r, GICR_ISENABLER0, GICR_IENABLER_REGBIT(irq));
}

/* Enable an interrupt and set it's priority */
static int
gicv3_addspl(int irq, int ipl, int min_ipl __unused, int max_ipl __unused)
{
	if (GIC_INTID_IS_PERCPU(irq)) {
		gicv3_for_each_gicr(&conf,
		    gicv3_addspl_percpu, (uint32_t)irq, (uint32_t)ipl);
	} else if (GIC_INTID_IS_SPI(irq)) {
		gicv3_addspl_spi(&conf, (uint32_t)irq, (uint32_t)ipl);
	}

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

/* Delete SPL for shared peripheral interrupts */
static void
gicv3_delspl_spi(gicv3_conf_t *gc, uint32_t irq)
{
	GICD_LOCK();

	/*
	 * Disable the IRQ and drain writes.
	 */
	gicd_write4(gc, GICD_ICENABLERn(GICD_IENABLER_REGNUM(irq)),
	    GICD_IENABLER_REGBIT(irq));
	gicd_drain_writes(gc);

	/*
	 * Set the priority to lowest.
	 */
	(void) gicd_rmw4(gc,
	    GICD_IPRIORITYRn(GICD_IPRIORITY_REGNUM(irq)),
	    GICD_IPRIORITY_REGVAL(irq, GICD_IPRIORITY_REGMASK),
	    GICD_IPRIORITY_REGVAL(irq, GIC_IPL_TO_PRI(0)));

	GICD_UNLOCK();
}

/* Delete SPL for per-CPU interrupts */
static void
gicv3_delspl_percpu(gicv3_redistributor_t *r, uint32_t irq,
    uint32_t a1 __unused)
{
	/*
	 * Disable the IRQ.
	 */
	gicr_sgi_write4(r, GICR_ICENABLER0, GICR_IENABLER_REGBIT(irq));
	gicr_drain_writes(r);

	/*
	 * Set the priority to lowest.
	 */
	(void) gicr_sgi_rmw4(r,
	    GICR_IPRIORITYRn(GICR_IPRIORITY_REGNUM(irq)),
	    GICR_IPRIORITY_REGVAL(irq, GICR_IPRIORITY_REGMASK),
	    GICR_IPRIORITY_REGVAL(irq, GIC_IPL_TO_PRI(0)));
}

/* Disable an interrupt and reset it's priority */
static int
gicv3_delspl(int irq, int ipl __unused,
    int min_ipl __unused, int max_ipl __unused)
{
	if (autovect[irq].avh_hi_pri == 0) {
		if (GIC_INTID_IS_PERCPU(irq)) {
			gicv3_for_each_gicr(&conf,
			    gicv3_delspl_percpu, (uint32_t)irq, 0);
		} else if (GIC_INTID_IS_SPI(irq)) {
			gicv3_delspl_spi(&conf, (uint32_t)irq);
		}
	}

	return (0);
}

/*
 * Send an IRQ as an IPI to processors in `cpuset`.
 *
 * Processors not targetable by the GIC will be silently ignored, as will the
 * sending processor.
 *
 * §2.3.1: "If GICD_TYPER.RSS is 0 or ICC_CTLR_ELx.RSS is 0, Arm strongly
 * recommends that only values in the range 0-15 are used at affinity level
 * 0 to align with the SGI target list capability." We assert this.
 */
static void
gicv3_send_ipi(cpuset_t cpuset, int irq)
{
	boolean_t has_rss;
	uint64_t sgir;

	has_rss = (read_icc_ctlr_el1() & ICC_CTLR_EL1_RSS) ? B_TRUE : B_FALSE;
	dsb(ish);

	/*
	 * There is almost definitely a better way to do this, populating
	 * targetlist/RS and issuing SGI with CPUs clustered by AFF3-1+RS.
	 *
	 * However, this is obviously correct, which will do for now.
	 */
	CPUSET_AND(cpuset, conf.gc_cpuset);
	CPUSET_DEL(cpuset, CPU->cpu_id);
	while (!CPUSET_ISNULL(cpuset)) {
		uint_t cpun;
		CPUSET_FIND(cpuset, cpun);
		sgir = conf.gc_redist[cpun].gr_sgir;
		if (!has_rss && ICC_SGInR_EL1_HAS_RS(sgir)) {
			panic("cpu%d: Need range selector support to target "
			    "cpu%d with an SGI", CPU->cpu_id, cpun);
		}
		write_icc_sgi1r_el1(sgir | ICC_SGInR_EL1_MAKE_INTID(irq));
		CPUSET_DEL(cpuset, cpun);
	}
}

/*
 * Acknowledge receipt of an IRQ by reading the interrupt acknowledge register.
 *
 * The value returned from this function must be passed, unchanged, to
 * gicv3_eoi and gicv3_deactivate.
 *
 * To extract the INTID (vector), use gicv3_ack_to_vector.
 */
static uint64_t
gicv3_acknowledge(void)
{
	return (read_icc_iar1_el1());
}

/*
 * Extract the interrupt vector from an acknowledged IRQ.
 */
static uint32_t
gicv3_ack_to_vector(uint64_t ack)
{
	return (ack & ICC_IAR1_INTID);
}

/*
 * Invoke the running priority drop during interrupt processing.
 */
static void
gicv3_eoi(uint64_t ack)
{
	write_icc_eoir1_el1(ack);
}

/*
 * Deactivate an interrupt at the end of interrupt processing.
 */
static void
gicv3_deactivate(uint64_t ack)
{
	write_icc_dir_el1(ack);
}

/*
 * Private helper function to deallocate and unmap any allocated GIC
 * configuration.
 *
 * This function is only used for cleanup in error paths.
 */
static void
gicv3_clear_conf(gicv3_conf_t *gc)
{
	uint32_t i;

	if (gc->gc_num_redist && gc->gc_redist) {
		kmem_free(gc->gc_redist,
		    sizeof (gicv3_redistributor_t) * gc->gc_num_redist);
	}

	gc->gc_redist = NULL;
	gc->gc_num_redist = 0;

	if (gc->gc_num_redist_regions && gc->gc_redist_regions) {
		for (i = 0; i < gc->gc_num_redist_regions; ++i) {
			if (gc->gc_redist_regions[i].size) {
				psm_unmap_phys(
				    (caddr_t)gc->gc_redist_regions[i].base,
				    gc->gc_redist_regions[i].size);
			}
			gc->gc_redist_regions[i].base = NULL;
			gc->gc_redist_regions[i].size = 0;
		}

		kmem_free(gc->gc_redist_regions,
		    sizeof (gicv3_redist_region_t) * gc->gc_num_redist_regions);
	}

	gc->gc_redist_regions = NULL;
	gc->gc_num_redist_regions = 0;

	if (gc->gc_gicd && gc->gc_gicd_size)
		psm_unmap_phys((caddr_t)gc->gc_gicd, gc->gc_gicd_size);

	gc->gc_gicd = NULL;
	gc->gc_gicd_size = 0;
}

/*
 * Return the GICv3 PROM node, or OBP_NONODE if none was found.
 */
static pnode_t
find_gic(pnode_t nodeid, int depth)
{
	pnode_t	node;
	pnode_t	child;

	if (prom_is_compatible(nodeid, "arm,gic-v3"))
		return (nodeid);

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
 * Map the GICv3 distributor and redistributor MMIO regions into the device
 * arena.
 */
static int
gicv3_map(gicv3_conf_t *gc)
{
	pnode_t			node;
	uint32_t		i;
	uint64_t		gicd_base;
	uint64_t		gicd_size;
	uint64_t		gicrr_base;
	uint64_t		gicrr_size;
	uint32_t		num_redist_regions;
	caddr_t			addr;

	node = find_gic(prom_rootnode(), 0);
	if (node <= 0)
		return (-1);

	if (prom_get_reg_address(node, 0, &gicd_base) != 0)
		return (-1);

	if (prom_get_reg_size(node, 0, &gicd_size) != 0)
		return (-1);

	addr = psm_map_phys(gicd_base, gicd_size, PROT_READ|PROT_WRITE);
	if (addr == NULL)
		return (-1);
	gc->gc_gicd = (void *)addr;
	gc->gc_gicd_size = gicd_size;

	gc->gc_redist_stride =
	    prom_get_prop_u64(node, "redistributor-stride", 0);
	num_redist_regions =
	    prom_get_prop_u32(node, "#redistributor-regions", 1);
	gc->gc_redist_regions = kmem_zalloc(
	    sizeof (gicv3_redist_region_t) * num_redist_regions, KM_SLEEP);
	gc->gc_num_redist_regions = num_redist_regions;

	for (i = 0; i < gc->gc_num_redist_regions; ++i) {
		if (prom_get_reg_address(node, 1 + i, &gicrr_base) != 0 ||
		    prom_get_reg_size(node, 1 + i, &gicrr_size) != 0)
			return (-1);

		addr = psm_map_phys(gicrr_base, gicrr_size,
		    PROT_READ|PROT_WRITE);
		if (addr == NULL)
			return (-1);

		gc->gc_redist_regions[i].base = (void *)addr;
		gc->gc_redist_regions[i].size = gicrr_size;
	}

	return (0);
}

/*
 * Discover redistributors in all redistributor regions and assign them to CPUs
 * by CPU ID.
 */
static int
gicv3_assign_redistributors(gicv3_conf_t *gc)
{
	uint32_t		i;
	uint64_t		gicr_typer;
	uint32_t		num_redistributors;
	uint64_t		affinity;
	void			*gicr_rd_base;
	void			*gicr_sgi_base;
	void			*gicr_vlpi_base;
	void			*cursor;
	void			*ptr;
	struct cpuinfo		*ci;

	/*
	 * Count the number of redistributors present in our redistributor
	 * regions.
	 */
	num_redistributors = 0;
	for (i = 0; i < gc->gc_num_redist_regions; ++i) {
		cursor = gc->gc_redist_regions[i].base;
		do {
			ptr = cursor;
			gicr_typer = reg_read_raw8(cursor, GICR_TYPER);
			/* skip over the rd and sgi frames */
			cursor += (GICR_FRAME_SIZE * 2);
			/* skip over the vlpi and reserved frames if present */
			if (gicr_typer & GICR_TYPER_VLPIS)
				cursor += (GICR_FRAME_SIZE * 2);
			if (gc->gc_redist_stride)
				cursor = ptr + gc->gc_redist_stride;
			num_redistributors++;
		} while (!(gicr_typer & GICR_TYPER_Last));
	}

	/*
	 * Check that we have at least as many redistributors as we do CPUs.
	 */
	VERIFY(num_redistributors >= max_ncpus);

	/*
	 * Allocate the redistributor structures.
	 */
	gc->gc_redist = kmem_zalloc(
	    sizeof (gicv3_redistributor_t) * num_redistributors, KM_SLEEP);
	gc->gc_num_redist = num_redistributors;

	/*
	 * Iterate the redistributors again. For each one, grab the CPU ID
	 * from cpuinfo for this affinity value and populate the value at that
	 * CPU index.
	 */
	for (i = 0; i < gc->gc_num_redist_regions; ++i) {
		cursor = gc->gc_redist_regions[i].base;
		do {
			ptr = cursor;
			gicr_typer = reg_read_raw8(cursor, GICR_TYPER);

			gicr_rd_base = cursor;
			cursor += GICR_FRAME_SIZE;
			gicr_sgi_base = cursor;
			cursor += GICR_FRAME_SIZE;

			if (gicr_typer & GICR_TYPER_VLPIS) {
				gicr_vlpi_base = cursor;
				cursor += (GICR_FRAME_SIZE * 2);
			} else {
				gicr_vlpi_base = NULL;
			}

			affinity = AFF_GICR_TYPER_TO_PACKED(gicr_typer);

			ci = cpuinfo_for_affinity(
			    AFF_PACKED_TO_MPIDR(affinity));
			VERIFY(ci != NULL);
			VERIFY(ci->ci_id < gc->gc_num_redist);

			/*
			 * Initialize the redistributor record.
			 */
			LOCK_INIT_CLEAR(&gc->gc_redist[ci->ci_id].gr_lock);
			gc->gc_redist[ci->ci_id].gr_rd_base = gicr_rd_base;
			gc->gc_redist[ci->ci_id].gr_sgi_base = gicr_sgi_base;
			gc->gc_redist[ci->ci_id].gr_vlpi_base = gicr_vlpi_base;
			gc->gc_redist[ci->ci_id].gr_typer = gicr_typer;
			gc->gc_redist[ci->ci_id].gr_sgir =
			    AFF_PACKED_TO_ICC_SGInR_EL1(affinity);

			if (gc->gc_redist_stride)
				cursor = ptr + gc->gc_redist_stride;
		} while (!(gicr_typer & GICR_TYPER_Last));
	}

	/*
	 * Iterate the cpuinfo ensuring that we have a redistributor for
	 * each CPU.
	 */
	for (ci = cpuinfo_first(); ci != cpuinfo_end(); ci = cpuinfo_next(ci)) {
		VERIFY(ci->ci_id < gc->gc_num_redist);
		if (gc->gc_redist[ci->ci_id].gr_rd_base == NULL ||
		    gc->gc_redist[ci->ci_id].gr_sgi_base == NULL)
			return (-1);
	}

	return (0);
}

/*
 * Initialize a single redistributor, applying initial configuration.
 */
static void
gicv3_init_gicr(gicv3_redistributor_t *r,
    uint32_t a0 __unused, uint32_t a1 __unused)
{
	/*
	 * Clear enabled/pending/active status of the CPU-specific interrupts.
	 */
	gicr_sgi_write4(r, GICR_ICENABLER0, 0xffffffff);
	gicr_drain_writes(r);
	gicr_sgi_write4(r, GICR_ICPENDR0, 0xffffffff);
	gicr_sgi_write4(r, GICR_ICACTIVER0, 0xffffffff);

	/*
	 * Configure SGI and PPI to non-secure group 1.
	 */
	gicr_sgi_write4(r, GICR_IGROUPR0, 0xFFFFFFFF);
	gicr_sgi_write4(r, GICR_IGRPMODR0, 0x0);

	/*
	 * Initialize interrupt priorities for per-CPU interrupts,
	 * setting them to the lowest possible priority.
	 */
	for (int i = 0; i < 8; ++i)
		gicr_sgi_write4(r, GICR_IPRIORITYRn(i), 0xffffffff);

	/*
	 * Set all PPIs to level triggered (SGIs are always edge
	 * triggered).
	 */
	gicr_sgi_write4(r, GICR_ICFGR1, 0x0);
}

/*
 * Enable register access, disable FIQ bypass, disable IRQ bypass.
 */
static int
gicv3_enable_system_register_access(void)
{
	write_icc_sre_el1(ICC_SRE_EL1_SRE|ICC_SRE_EL1_DFB|ICC_SRE_EL1_DIB);
	if ((read_icc_sre_el1() &
	    (ICC_SRE_EL1_SRE|ICC_SRE_EL1_DFB|ICC_SRE_EL1_DIB))
	    != (ICC_SRE_EL1_SRE|ICC_SRE_EL1_DFB|ICC_SRE_EL1_DIB))
		return (-1);

	return (0);
}

/*
 * Public function used for initializing CPUs.
 *
 * The boot processor is initialized from the tail of the main gicv3_init
 * function once the distributor and redistributors have been configured.
 */
static void
gicv3_cpu_init(cpu_t *cp)
{
	/*
	 * Tell the hardware that this CPU is awake and wait for the wakeup to
	 * complete.
	 */
	gicv3_awaken_cpu(&conf, cp);

	/*
	 * CPU Interface Configuration
	 */

	/*
	 * First up, we want to use the system register interface.
	 */
	if (gicv3_enable_system_register_access() != 0)
		panic("cpu%d: Failed to enable the GIC system register "
		    "interface.", cp->cpu_id);

	/*
	 * We don't need subpriorities on GICv3.
	 */
	write_icc_bpr1_el1(0);

	/*
	 * Configure the priority mask register to leave us at LOCK_LEVEL once
	 * initialized.
	 */
	write_icc_pmr_el1(GIC_IPL_TO_PRI(LOCK_LEVEL));

	/*
	 * Ensure the use of split-EOI.
	 */
	write_icc_ctlr_el1(read_icc_ctlr_el1() | ICC_CTLR_EL1_EOImode);

	/*
	 * Enable non-secure group one interrupt signalling on the CPU
	 * interface.
	 */
	write_icc_igrpen1_el1(ICC_IGRPEN1_EL1_Enable);

	/*
	 * Finally, tell the world we're ready.
	 */
	CPUSET_ADD(conf.gc_cpuset, cp->cpu_id);
}

/*
 * Map GIC register space and perform global GIC initialization followed by
 * configuration of all redistributors. Finish up by configuring the CPU
 * interface for the boot processor.
 *
 * Returns non-zero on error.
 */
static int
gicv3_init(void)
{
	uint32_t	n;

	if (gicv3_enable_system_register_access() != 0)
		prom_panic("cpu0: Failed to enable the GIC system register "
		    "interface.");

	/*
	 * Global initialization involves the distributor, so lock it.
	 */
	GICD_LOCK_INIT_HELD();
	gicv3_clear_conf(&conf);

	/*
	 * Map redistributor regions.
	 */
	if (gicv3_map(&conf) != 0) {
		gicv3_clear_conf(&conf);
		GICD_UNLOCK();
		return (-1);
	}

	/*
	 * Allocate redistributors and assign pointers to them.
	 */
	if (gicv3_assign_redistributors(&conf) != 0) {
		gicv3_clear_conf(&conf);
		GICD_UNLOCK();
		return (-1);
	}

	conf.gc_gicd_typer = gicd_read4(&conf, GICD_TYPER);
	conf.gc_maxsources = GICD_TYPER_LINES(conf.gc_gicd_typer);

	/*
	 * Disable the distributor and drain writes. This is done is pieces
	 * as we want to avoid unpredictable behaviour when changing affinity
	 * routing (§2.2.3: Changing affinity routing enables).
	 *
	 * We turn on affinity routing as quickly as possible, then assert
	 * that we were able to turn it on.
	 *
	 * In an implementation that only supports one security state, ARE
	 * is the same bit as ARE_NS, so this logic holds.
	 */
	(void) gicd_rmw4(&conf, GICD_CTLR,
	    GICD_CTLR_RWP|GICD_CTLR_EnableGrp1A|GICD_CTLR_EnableGrp1, 0x0);
	gicd_drain_writes(&conf);
	(void) gicd_rmw4(&conf, GICD_CTLR,
	    GICD_CTLR_RWP|GICD_CTLR_ARE_NS, GICD_CTLR_ARE_NS);
	gicd_drain_writes(&conf);
	VERIFY((gicd_read4(&conf, GICD_CTLR) & GICD_CTLR_ARE_NS) ==
	    GICD_CTLR_ARE_NS);

	/*
	 * XXXARM: Quirks might be needed
	 *
	 * There's some nice discussion of a few bugs that affect Ampere eMAG
	 * and Rockchip rk3399 in OpenBSD's sys/arch/arm64/dev/agintc.c. Don't
	 * concern ourselves with those workarounds just yet.
	 *
	 * There's some very sneaky detection of these issues in that code,
	 * but when we look at those issues we should include refencing
	 * GICD_TYPER.SecurityExtn=1 to protect the checks.
	 */

	/*
	 * The minimum number of priority bits for a GICv3 that implements a
	 * single security state is 4. If two states are implemented the
	 * minimum is 5.
	 */
	conf.gc_pri32 =
	    ((ICC_CTLR_NUM_PRI_BITS(read_icc_ctlr_el1()) >= 5) ? 1 : 0);

	/*
	 * Disable all SPIs.
	 */
	for (n = 32; n < conf.gc_maxsources; n += 32)
		gicd_write4(&conf, GICD_ICENABLERn(n >> 5), 0xFFFFFFFF);
	gicd_drain_writes(&conf);

	/*
	 * Move all SPIs to non-secure group 1.
	 */
	for (n = 32; n < conf.gc_maxsources; n += 32) {
		gicd_write4(&conf, GICD_IGROUPRn(n >> 5), 0xFFFFFFFF);
		gicd_write4(&conf, GICD_IGRPMODRn(n >> 5), 0x0);
	}

	/*
	 * Drop all SPIs to the lowest priority.
	 */
	for (n = 32; n < conf.gc_maxsources; n += 4)
		gicd_write4(&conf, GICD_IPRIORITYRn(n >> 2), 0xFFFFFFFF);

	/*
	 * Make all SPIs level-sensitive.
	 */
	for (n = 32; n < conf.gc_maxsources; n += 16)
		gicd_write4(&conf, GICD_ICFGRn(n >> 4), 0x0);

	/*
	 * Enable the distributor.
	 */
	(void) gicd_rmw4(&conf, GICD_CTLR, GICD_CTLR_RWP,
	    GICD_CTLR_EnableGrp1A);

	/*
	 * Done touching the distributor.
	 */
	GICD_UNLOCK();

	/*
	 * Reset all of the redistributors.
	 */
	gicv3_for_each_gicr(&conf, gicv3_init_gicr, 0, 0);

	/*
	 * No CPUs have been configured yet.
	 */
	CPUSET_ZERO(conf.gc_cpuset);

	/*
	 * Initialize the boot processor.
	 */
	gicv3_cpu_init(CPU);
	return (0);
}

static struct modlmisc modlmisc = {
	&mod_miscops,
	"Generic Interrupt Controller v3"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	if (strcmp(gic_module_name, "gicv3") == 0) {
		gic_ops.go_send_ipi = gicv3_send_ipi;
		gic_ops.go_init = gicv3_init;
		gic_ops.go_cpu_init = gicv3_cpu_init;
		gic_ops.go_config_irq = gicv3_config_irq;
		gic_ops.go_addspl = gicv3_addspl;
		gic_ops.go_delspl = gicv3_delspl;
		gic_ops.go_setlvl = gicv3_setlvl;
		gic_ops.go_setlvlx = gicv3_setlvlx;
		gic_ops.go_acknowledge = gicv3_acknowledge;
		gic_ops.go_ack_to_vector = gicv3_ack_to_vector;
		gic_ops.go_eoi = gicv3_eoi;
		gic_ops.go_deactivate = gicv3_deactivate;
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
