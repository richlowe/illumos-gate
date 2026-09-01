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
 * cpunex - nexus driver for the /cpus container node.
 *
 * Standard FDT /cpus nodes have #size-cells = 0 because cpu reg entries
 * are just MPIDR values with no size component.  The generic simple-bus
 * nexus cannot handle this.  This driver provides a minimal nexus that
 * sets child unit addresses directly from their reg property and avoids
 * regspec translation entirely.
 *
 * On ACPI platforms the /cpus node is synthesised by acpidev and this
 * driver serves the same purpose: bind as the nexus so that cpu leaf
 * drivers can attach to the children.
 */

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>
#include <sys/obpdefs.h>
#include <sys/cpuinfo.h>
#include <sys/machsystm.h>

static int cpunex_attach(dev_info_t *, ddi_attach_cmd_t);
static int cpunex_detach(dev_info_t *, ddi_detach_cmd_t);
static int cpunex_bus_ctl(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
    void *, void *);

/*
 * Set the unit address for a cpu child from its reg property.
 *
 * Only children with device_type = "cpu" are handled here; all other
 * children (cpu-map, idle-states, topology containers) are ignored.
 */
static int
cpunex_initchild(dev_info_t *child)
{
	char addr[20];
	uint64_t mpidr;
	int id;

	if (mach_cpu_dip_to_mpidr(child, &mpidr) != DDI_SUCCESS) {
		return (DDI_NOT_WELL_FORMED);
	}

	id = cpuinfo_id_for_mpidr(mpidr);
	if (id < 0) {
		return (DDI_FAILURE);
	}

	(void) snprintf(addr, sizeof (addr), "%llx",
	    (unsigned long long)id);
	ddi_set_name_addr(child, addr);

	return (DDI_SUCCESS);
}

static void
cpunex_uninitchild(dev_info_t *child)
{
	ddi_set_name_addr(child, NULL);
}

static int
cpunex_bus_ctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t op, void *arg, void *result)
{
	switch (op) {
	case DDI_CTLOPS_INITCHILD:
		return (cpunex_initchild((dev_info_t *)arg));
	case DDI_CTLOPS_UNINITCHILD:
		cpunex_uninitchild((dev_info_t *)arg);
		return (DDI_SUCCESS);
	case DDI_CTLOPS_REPORTDEV:
		cmn_err(CE_CONT, "?%s%d at %s%d\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip),
		    ddi_driver_name(dip), ddi_get_instance(dip));
		return (DDI_SUCCESS);
	default:
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}
}

static struct bus_ops cpunex_bus_ops = {
	.busops_rev	= BUSO_REV,
	.bus_ctl	= cpunex_bus_ctl,
	.bus_prop_op	= ddi_bus_prop_op,
};

static int
cpunex_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		return (DDI_SUCCESS);
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

static int
cpunex_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		return (DDI_FAILURE);
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

static struct dev_ops cpunex_ops = {
	.devo_rev		= DEVO_REV,
	.devo_refcnt		= 0,
	.devo_getinfo		= ddi_no_info,
	.devo_identify		= nulldev,
	.devo_probe		= nulldev,
	.devo_attach		= cpunex_attach,
	.devo_detach		= cpunex_detach,
	.devo_reset		= nodev,
	.devo_bus_ops		= &cpunex_bus_ops,
	.devo_quiesce		= ddi_quiesce_not_needed,
};

static struct modldrv modldrv = {
	.drv_modops		= &mod_driverops,
	.drv_linkinfo		= "CPU bus nexus",
	.drv_dev_ops		= &cpunex_ops
};

static struct modlinkage modlinkage = {
	.ml_rev			= MODREV_1,
	.ml_linkage		= { &modldrv, NULL }
};

int
_init(void)
{
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
