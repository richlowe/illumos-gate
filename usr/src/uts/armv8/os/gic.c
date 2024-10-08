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

#include <sys/types.h>
#include <sys/gic.h>
#include <sys/gic_reg.h>
#include <sys/cmn_err.h>
#include <sys/promif.h>
#include <sys/errno.h>
#include <sys/modctl.h>

static void stub_not_config(void);
static void stub_setlvlx(int ipl);

/*
 * Used by implementations to ensure that they only fill in gic_ops when
 * appropriate.
 */
char *gic_module_name = NULL;

gic_ops_t gic_ops = {
	.go_send_ipi		= (gic_send_ipi_t)stub_not_config,
	.go_init		= (gic_init_t)stub_not_config,
	.go_cpu_init		= (gic_cpu_init_t)stub_not_config,
	.go_config_irq		= (gic_config_irq_t)stub_not_config,
	.go_addspl		= (gic_addspl_t)stub_not_config,
	.go_delspl		= (gic_delspl_t)stub_not_config,
	.go_setlvl		= (gic_setlvl_t)stub_not_config,
	.go_setlvlx		= stub_setlvlx,
	.go_acknowledge		= (gic_acknowledge_t)stub_not_config,
	.go_ack_to_vector	= (gic_ack_to_vector_t)stub_not_config,
	.go_eoi			= (gic_eoi_t)stub_not_config,
	.go_deactivate		= (gic_deactivate_t)stub_not_config,
	.go_is_spurious		= (gic_is_spurious_t)NULL
};

static void
stub_not_config(void)
{
	prom_panic("GIC not configured\n");
}

static void
stub_setlvlx(int ipl __unused)
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
	 * so we just allow the calls to be no-ops prior to loading a GIC
	 * implementation module.
	 */
}

void
gic_send_ipi(cpuset_t cpuset, int irq)
{
	gic_ops.go_send_ipi(cpuset, irq);
}

void
gic_cpu_init(cpu_t *cp)
{
	gic_ops.go_cpu_init(cp);
}

void
gic_config_irq(uint32_t irq, bool is_edge)
{
	gic_ops.go_config_irq(irq, is_edge);
}

static int
gic_addspl(int irq, int ipl, int min_ipl, int max_ipl)
{
	return (gic_ops.go_addspl(irq, ipl, min_ipl, max_ipl));
}

static int
gic_delspl(int irq, int ipl, int min_ipl, int max_ipl)
{
	ASSERT3S(irq, <, MAX_VECT);
	return (gic_ops.go_delspl(irq, ipl, min_ipl, max_ipl));
}

int (*addspl)(int, int, int, int) = gic_addspl;
int (*delspl)(int, int, int, int) = gic_delspl;

int
setlvl(int irq)
{
	return (gic_ops.go_setlvl(irq));
}

void
setlvlx(int ipl)
{
	gic_ops.go_setlvlx(ipl);
}

uint64_t
gic_acknowledge(void)
{
	return (gic_ops.go_acknowledge());
}

uint32_t
gic_ack_to_vector(uint64_t ack)
{
	return (gic_ops.go_ack_to_vector(ack));
}

void
gic_eoi(uint64_t ack)
{
	gic_ops.go_eoi(ack);
}

void
gic_deactivate(uint64_t ack)
{
	gic_ops.go_deactivate(ack);
}

int
gic_is_spurious(uint32_t intid)
{
	if (gic_ops.go_is_spurious != NULL)
		return (gic_ops.go_is_spurious(intid));

	if (GIC_INTID_IS_SPECIAL(intid))
		return (1);

	return (0);
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

	if (prom_has_compatible("arm,gic-v3")) {
		gic_module_name = "gicv3";
		return;
	}

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

	if (gic_ops.go_init() != 0)
		return (-1);

	return (0);
}
