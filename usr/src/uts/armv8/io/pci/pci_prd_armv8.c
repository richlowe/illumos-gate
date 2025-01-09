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
 * Copyright 2023 Oxide Computer Company
 * Copyright 2024 Richard Lowe
 */

/*
 * PCI Resource Discovery on armv8.  We just ask the PROM device tree.
 *
 * XXXPCI: Architecturally, I'm assuming that this module `pci_prd_armv8` is
 * devicetree based, and an acpi-based `pci_prd_sbsa` would exist.
 */

#include <sys/esunddi.h>
#include <sys/memlist.h>
#include <sys/promif.h>
#include <sys/types.h>
#include <sys/pci.h>
#include <sys/pci_impl.h>
#include <sys/plat/pci_prd.h>
#include <sys/obpdefs.h>

static pci_prd_upcalls_t *prd_upcalls;

/*
 * We always just tell the system to scan all PCI buses.
 */
uint32_t
pci_prd_max_bus(void)
{
	return (PCI_MAX_BUS_NUM - 1);
}

struct memlist *prd_pci_io_res[PCI_MAX_BUS_NUM];
struct memlist *prd_pci_mem_res[PCI_MAX_BUS_NUM];
struct memlist *prd_pci_pmem_res[PCI_MAX_BUS_NUM];
struct memlist *prd_pci_bus_res[PCI_MAX_BUS_NUM];

struct memlist *
pci_prd_find_resource(uint32_t bus, pci_prd_rsrc_t rsrc)
{
	static boolean_t initialized = B_FALSE;

	/*
	 * We can pull all of this out of the devicetree node
	 * _after firmware has initialized PCIe_.
	 *
	 * We only have this info after firmware (or something) has
	 * initialized PCIe, without that we have plain default devices with
	 * unprogrammed ranges.
	 *
	 * XXXPCI: Because our prototype is hardwired to a single, ecam-based,
	 * root complex the code below is just a complete hack lashup.
	 *
	 * XXXPCI: Another thing I'm not sure about is what to do about pci
	 * nodes flagged as disabled.  I assume we ignore them completely in
	 * all cases in the long term, but in the short term we assume our
	 * single PCIe device actually exists.
	 */

	if (!initialized) {
		initialized = B_TRUE;

		pnode_t node = prom_finddevice("/pcie");

		if (node < 0) {
			return (NULL);
		}

		pci_ranges_t *ranges;
		int length = prom_getproplen(node, OBP_RANGES);

		if (length <= 0) {
			return (NULL);
		}

		ranges = kmem_zalloc(length, KM_SLEEP);
		length /= sizeof (*ranges);

		if (prom_getprop(node, "ranges", (caddr_t)ranges) <= 0) {
			cmn_err(CE_PANIC, "No ranges property found for /pcie");
			return (NULL);
		}

		struct memlist *mlp;

		for (int i = 0; i < length; i++) {
			if ((ranges[i].child_high & PCI_ADDR_MASK) == PCI_ADDR_IO) {
				mlp = prd_pci_io_res[bus];
			} else if ((((ranges[i].child_high & PCI_ADDR_MASK) == PCI_ADDR_MEM32) ||
			    ((ranges[i].child_high & PCI_ADDR_MASK) == PCI_ADDR_MEM64)) &&
			    ((ranges[i].child_high & PCI_PREFETCH_B) == 0)) {
				mlp = prd_pci_mem_res[bus];
			} else if ((((ranges[i].child_high & PCI_ADDR_MASK) == PCI_ADDR_MEM32) ||
			    ((ranges[i].child_high & PCI_ADDR_MASK) == PCI_ADDR_MEM64)) &&
			    ((ranges[i].child_high & PCI_PREFETCH_B) != 0)) {
				mlp = prd_pci_pmem_res[bus];
			} else if ((ranges[i].child_high & PCI_ADDR_MASK) == PCI_ADDR_CONFIG) {
				continue;
			} else {
				cmn_err(CE_PANIC, "unknown PCI resources %x in "
				    "ranges of bus %d\n", ranges[i].child_high,
				    bus);
			}

			pci_memlist_insert(&mlp,
			    ((uint64_t)ranges[i].child_mid << 32) | ranges[i].child_low,
			    ((uint64_t)ranges[i].size_high << 32) | ranges[i].size_low);
		}
	}

	struct memlist *ret = NULL;

	switch (rsrc) {
	case PCI_PRD_R_BUS:
		ret = prd_pci_bus_res[bus];
		break;
	case PCI_PRD_R_MMIO:
		ret = prd_pci_mem_res[bus];
		break;
	case PCI_PRD_R_PREFETCH:
		ret = prd_pci_pmem_res[bus];
		break;
	case PCI_PRD_R_IO:
		ret = prd_pci_io_res[bus];
		break;
	default:
		cmn_err(CE_PANIC, "Unknown PRD resource request: %x", rsrc);
	}

	return (ret);
}

boolean_t
pci_prd_multi_root_ok(void)
{
	return (B_TRUE);
}

int
pci_prd_init(pci_prd_upcalls_t *upcalls)
{
	prd_upcalls = upcalls;
	return (0);
}

void
pci_prd_fini(void)
{

}

/*
 * XXX we should probably implement these soon. Punting for the moment.
 */
void
pci_prd_root_complex_iter(pci_prd_root_complex_f func, void *arg)
{

}

/*
 * We have no alternative slot naming here. So this is a no-op and thus empty
 * function.
 */
void
pci_prd_slot_name(uint32_t bus, dev_info_t *dip)
{
}

pci_prd_compat_flags_t
pci_prd_compat_flags(void)
{
	return (PCI_PRD_COMPAT_NONE);
}

static struct modlmisc pci_prd_modlmisc_armv8 = {
	.misc_modops = &mod_miscops,
	.misc_linkinfo = "armv8 PCI Resource Discovery"
};

static struct modlinkage pci_prd_modlinkage_armv8 = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &pci_prd_modlmisc_armv8, NULL }
};

int
_init(void)
{
	return (mod_install(&pci_prd_modlinkage_armv8));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&pci_prd_modlinkage_armv8, modinfop));
}

int
_fini(void)
{
	return (mod_remove(&pci_prd_modlinkage_armv8));
}
