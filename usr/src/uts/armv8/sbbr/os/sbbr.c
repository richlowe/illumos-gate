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

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/machclock.h>
#include <sys/platmod.h>
#include <sys/systm.h>
#include <sys/efi.h>
#include <sys/efirt.h>

static plat_pcie_osc_func_t plat_pcie_osc_func;

/*
 * Platform power management drivers list - empty by default
 */
char *platform_module_list[] = {
	NULL,
};

void
plat_tod_fault(enum tod_fault_type tod_bad __unused)
{
}

void
plat_pcie_osc_set(plat_pcie_osc_func_t f)
{
	plat_pcie_osc_func = f;
}

int
plat_pcie_osc(dev_info_t *dip, uint32_t support,
    uint32_t ctrl_req, uint32_t *ctrl_ret)
{
	if (plat_pcie_osc_func == NULL) {
		return (DDI_FAILURE);
	}

	if (dip == NULL) {
		return (DDI_FAILURE);
	}

	return ((*plat_pcie_osc_func)(dip, support, ctrl_req, ctrl_ret));
}

void
set_platform_defaults(void)
{
	EFI_TIME t;
	EFI_TIME_CAPABILITIES tc;

	if (efi_get_time(&t, &tc) == EFI_SUCCESS) {
		tod_module_name = "efitod";
	}
}
