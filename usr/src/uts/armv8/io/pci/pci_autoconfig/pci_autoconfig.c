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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2022 Oxide Computer Company
 * Copyright 2024 Richard Lowe
 */

/*
 * Enumerate PCI devices
 */

#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/ddi_subrdefs.h>
#include <sys/modctl.h>
#include <sys/promif.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/obpdefs.h>

#include <sys/pci.h>
#include <sys/pci_cfgspace.h>
#include <sys/pcie.h>
#include <sys/pcie_impl.h>
#include <sys/plat/pci_prd.h>

extern int pci_boot_debug;
extern dev_info_t *pci_boot_bus_to_dip(uint32_t);

boolean_t
create_pcie_root_bus(uchar_t bus, dev_info_t *dip)
{
	(void) ndi_prop_update_string(DDI_DEV_T_NONE, dip,
	    OBP_DEVICETYPE, "pciex");
	(void) ndi_prop_update_string(DDI_DEV_T_NONE, dip,
	    OBP_COMPATIBLE, "pciex_root_complex");

	pcie_rc_init_bus(dip);

	return (B_TRUE);
}

void
pci_enumerate(int reprogram)
{
	extern void pci_setup_tree(void);
	extern void pci_reprogram(void);
	extern int pci_boot_maxbus;

	if (reprogram) {
		pci_reprogram();
		return;
	} else {
		pci_boot_maxbus = pci_prd_max_bus();
		pci_setup_tree();
	}
}

static struct modlmisc modlmisc = {
	&mod_miscops, "PCI Enumeration"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

static pci_prd_upcalls_t pci_upcalls = {
	.pru_bus2dip_f = pci_boot_bus_to_dip
};

int
_init(void)
{
	int	err;

	if ((err = pci_prd_init(&pci_upcalls)) != 0) {
		return (err);
	}

	if ((err = mod_install(&modlinkage)) != 0) {
		pci_prd_fini();
		return (err);
	}

	impl_bus_add_probe(pci_enumerate);
	return (0);
}

int
_fini(void)
{
	int	err;

	if ((err = mod_remove(&modlinkage)) != 0)
		return (err);

	impl_bus_delete_probe(pci_enumerate);
	pci_prd_fini();
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
