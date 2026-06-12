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
 * Copyright 2024 Richard Lowe
 * Copyright 2026 Michael van der Westhuizen
 */

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/kmem.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/pcie_impl.h>
#include <sys/pcie_aarch64.h>

#include <sys/plat/pci_prd.h>

void
pcie_init_plat(dev_info_t *dip)
{
	pcie_bus_t	*bus_p = PCIE_DIP2BUS(dip);
	bus_p->bus_plat_private =
	    kmem_zalloc(sizeof (pcie_aarch64_priv_t), KM_SLEEP);
}

void
pcie_fini_plat(dev_info_t *dip)
{
	pcie_bus_t	*bus_p = PCIE_DIP2BUS(dip);
	kmem_free(bus_p->bus_plat_private, sizeof (pcie_aarch64_priv_t));
}

int
pcie_plat_pwr_setup(dev_info_t *dip __unused)
{
	return (DDI_SUCCESS);
}

/*
 * Undo whatever is done in pcie_plat_pwr_setup
 */
void
pcie_plat_pwr_teardown(dev_info_t *dip __unused)
{
}
