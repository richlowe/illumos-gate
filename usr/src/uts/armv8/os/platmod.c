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

int
plat_clk_get_rate(dev_info_t *dip, uint_t clkno)
{
	struct prom_hwclock hwclk;

	if (prom_fdt_get_clock_by_index((pnode_t)ddi_get_nodeid(dip),
	    clkno, &hwclk) != 0) {
		return (-1);
	}

	if (prom_fdt_is_compatible(hwclk.node, "fixed-clock")) {
		uint_t clock;
		if (prom_getproplen(hwclk.node, "clock-frequency") ==
		    sizeof (uint_t)) {
			prom_getprop(hwclk.node, "clock-frequency",
			    (caddr_t)&clock);
			return (ntohl(clock));
		}
	}

	return (-1);
}

int
plat_clk_get_rate_by_name(dev_info_t *dip, const char *clkname)
{
	struct prom_hwclock hwclk;

	if (prom_fdt_get_clock_by_name((pnode_t)ddi_get_nodeid(dip),
	    clkname, &hwclk) != 0) {
		return (-1);
	}

	if (prom_fdt_is_compatible(hwclk.node, "fixed-clock")) {
		uint_t clock;
		if (prom_getproplen(hwclk.node, "clock-frequency") ==
		    sizeof (uint_t)) {
			prom_getprop(hwclk.node, "clock-frequency",
			    (caddr_t)&clock);
			return (ntohl(clock));
		}
	}

	return (-1);
}
