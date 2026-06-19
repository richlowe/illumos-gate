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

#include <sys/systm.h>
#include <sys/machclock.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/byteorder.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/sysmacros.h>
#include <sys/efi.h>
#include <sys/efirt.h>

/*
 * All platmod functions are weak and are only present when required.
 * The function calls have been converted to use methods
 *	if (&plat_func)
 *		plat_func(args);
 */

/*
 * Platform power management drivers list - empty by default
 */
char *platform_module_list[] = {
	NULL
};

void
plat_tod_fault(enum tod_fault_type tod_bad __unused)
{
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

static int
plat_devi_is_compatible(dev_info_t *dip, const char **compats, uint_t ncompats)
{
	char	**cmpats;
	uint_t	ncmpats;
	uint_t	n;
	uint_t	i;

	if (ddi_prop_lookup_string_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    OBP_COMPATIBLE, &cmpats, &ncmpats) == DDI_PROP_SUCCESS) {
		for (n = 0; n < ncmpats; ++n) {
			for (i = 0; i < ncompats; ++i) {
				if (strcmp(cmpats[n], compats[i]) == 0) {
					ddi_prop_free(cmpats);
					return (1);
				}
			}
		}

		ddi_prop_free(cmpats);
	}

	return (0);
}

static int
plat_devi_is_fixed_clock(dev_info_t *dip) {
	static const char *fixed_clocks[] = {
		"fixed-clock",
	};
	static const uint_t num_fixed_clocks = ARRAY_SIZE(fixed_clocks);

	return (plat_devi_is_compatible(dip, fixed_clocks, num_fixed_clocks));
}

int
plat_clk_get_rate(dev_info_t *dip, uint_t clkno)
{
	struct prom_hwclock hwclk;
	dev_info_t *cdip;

	if (prom_fdt_get_clock_by_index((pnode_t)ddi_get_nodeid(dip),
	    clkno, &hwclk) != 0) {
		return (-1);
	}

	if ((cdip = e_ddi_nodeid_to_dip((pnode_t)hwclk.node)) == NULL) {
		return (-1);
	}

	if (plat_devi_is_fixed_clock(cdip)) {
		int freq = ddi_prop_get_int(DDI_DEV_T_ANY, cdip,
		    DDI_PROP_DONTPASS, "clock-frequency", -1);

		if (freq != -1) {
			ddi_release_devi(cdip);
			return (freq);
		}
	}

	ddi_release_devi(cdip);
	return (-1);
}

int
plat_clk_get_rate_by_name(dev_info_t *dip, const char *clkname)
{
	struct prom_hwclock hwclk;
	dev_info_t *cdip;

	if (prom_fdt_get_clock_by_name((pnode_t)ddi_get_nodeid(dip),
	    clkname, &hwclk) != 0) {
		return (-1);
	}

	if ((cdip = e_ddi_nodeid_to_dip((pnode_t)hwclk.node)) == NULL) {
		return (-1);
	}

	if (plat_devi_is_fixed_clock(cdip)) {
		int freq = ddi_prop_get_int(DDI_DEV_T_ANY, cdip,
		    DDI_PROP_DONTPASS, "clock-frequency", -1);

		if (freq != -1) {
			ddi_release_devi(cdip);
			return (freq);
		}
	}

	ddi_release_devi(cdip);
	return (-1);
}
