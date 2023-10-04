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
 * Copyright 2023 Michael van der Westhuizen
 */

#include <sys/types.h>
#include <sys/gic.h>
#include <sys/cmn_err.h>
#include <sys/promif.h>
#include <sys/errno.h>
#include <sys/modctl.h>

static void stub_not_config(void);
static void stub_setlvlx(int ipl, int irq);

/*
 * Used by implementations to ensure that they only fill in gic_ops when
 * appropriate.
 */
char *gic_module_name = NULL;

/*
 * This is terrible, The interrupt trap code needs the GIC
 * CPU interface base address for GICv2. In implementations
 * that implement the system register interface we have to
 * leave this NULL so that the code in ml/exceptions.S can
 * identify the right thing to do.
 */
volatile struct gic_cpuif *gic_cpuif = NULL;

gic_ops_t gic_ops = {
	.mask_level_irq		= (mask_level_irq_t)stub_not_config,
	.unmask_level_irq	= (mask_level_irq_t)stub_not_config,
	.send_ipi		= (send_ipi_t)stub_not_config,
	.init_primary		= (init_primary_t)stub_not_config,
	.init_secondary		= (init_secondary_t)stub_not_config,
	.config_irq		= (config_irq_t)stub_not_config,
	.num_cpus		= (num_cpus_t)stub_not_config,
	.addspl			= (addspl_t)stub_not_config,
	.delspl			= (delspl_t)stub_not_config,
	.setlvl			= (setlvl_t)stub_not_config,
	.setlvlx		= stub_setlvlx
};

static void
stub_not_config(void)
{
	prom_panic("GIC not configured\n");
}

static void
stub_setlvlx(int ipl, int irq)
{
	/*
	 * Nothing to do here.
	 *
	 * setlvlx is called while locking and printing in the early kmem_init
	 * path (console_{enter,exit}, putq, cprintf) and in startup (cmn_err
	 * and mod_setup, reaching many of the same locks as in the kmem_init
	 * path).
	 *
	 * Adjusting the priority level this early is unnecessary, since
	 * interrupts are completely disabled. Locking is also unnecessary,
	 * since only one CPU is running. However, avoiding these calls in this
	 * case would overly complicate the general case of running the system,
	 * so we just allow to calls to be no-ops prior to loading a GIC
	 * implementation module.
	 */
}

void
gic_mask_level_irq(int irq)
{
	gic_ops.mask_level_irq(irq);
}

void
gic_unmask_level_irq(int irq)
{
	gic_ops.unmask_level_irq(irq);
}

void
gic_send_ipi(cpuset_t cpuset, int irq)
{
	gic_ops.send_ipi(cpuset, irq);
}

void
gic_init_primary(void)
{
	gic_ops.init_primary();
}

void
gic_init_secondary(int id)
{
	gic_ops.init_secondary(id);
}

void
gic_config_irq(uint32_t irq, bool is_edge)
{
	gic_ops.config_irq(irq, is_edge);
}

int
gic_num_cpus(void)
{
	return (gic_ops.num_cpus());
}

static int
gic_addspl(int irq, int ipl, int min_ipl, int max_ipl)
{
	return (gic_ops.addspl(irq, ipl, min_ipl, max_ipl));
}

static int
gic_delspl(int irq, int ipl, int min_ipl, int max_ipl)
{
	return (gic_ops.addspl(irq, ipl, min_ipl, max_ipl));
}

int (*addspl)(int, int, int, int) = gic_addspl;
int (*delspl)(int, int, int, int) = gic_delspl;

int
setlvl(int irq)
{
	return (gic_ops.setlvl(irq));
}

void
setlvlx(int ipl, int irq)
{
	gic_ops.setlvlx(ipl, irq);
}

/*
 * GIC Initialisation
 */
static void
set_gic_module_name(void)
{
	if (prom_has_compatible("arm,gic-400") ||
	    prom_has_compatible("arm,cortex-a15-gic")) {
		gic_module_name = "gicv2";
		return;
	}

#if XXXARM
	if (prom_has_compatible("arm,gic-v3")) {
		gic_module_name = "gicv3";
		return;
	}
#endif

	gic_module_name = NULL;
}

int
gic_init(void)
{
	set_gic_module_name();
	if (gic_module_name == NULL)
		return (ENOTSUP);

	if (modload("drv", gic_module_name) == -1)
		return (ENOENT);

	/*
	 * XXXARM: separate CPU initialisation from general initialisation
	 *
	 * This is where we would call:
	 * - gic_ops.init()
	 * - gic_ops_init_cpu(...)
	 */
	gic_ops.init_primary();
	return (0);
}
