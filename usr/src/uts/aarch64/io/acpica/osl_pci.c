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
 * ACPI CA OSL PCI configuration space access for aarch64.
 *
 * This file provides the AcpiOsReadPciConfiguration() and
 * AcpiOsWritePciConfiguration() OSL functions, backed by a list of
 * (segment, start_bus, end_bus, ecam_base, dip) tuples.
 *
 * The list is seeded from the MCFG table at module _init() time, before
 * any AML executes.  When acpidev creates root complex device nodes, it
 * registers the dip against the matching entry (or creates a new entry
 * for root complexes not described in the MCFG).
 *
 * When a dip is registered, config space access goes through the
 * pci_cfgacc_acc() path, which uses the root complex driver's
 * pcie_rc_cfgspace_acc function pointer (e.g. ECAM, bcm2711).
 *
 * When no dip is available (early boot, or root complexes that never
 * get a driver attached), config space is accessed by mapping the
 * ECAM aperture directly.
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/pci.h>
#include <sys/pci_cfgacc.h>
#include <sys/smp_impldefs.h>
#include <sys/acpi/acpi.h>

/*
 * Compute the byte offset within an ECAM aperture for a given BDF + register.
 */
#define	ECAM_OFFSET(bus, dev, func, reg)	\
	(((uint64_t)(bus) << 20) | ((uint64_t)(dev) << 15) | \
	((uint64_t)(func) << 12) | (uint64_t)(reg))

typedef struct osl_pci_rc {
	struct osl_pci_rc	*opr_next;
	uint16_t		opr_segment;
	uint8_t			opr_start_bus;
	uint8_t			opr_end_bus;
	uint64_t		opr_ecam_base;
	dev_info_t		*opr_dip;
} osl_pci_rc_t;

static kmutex_t osl_pci_lock;
static osl_pci_rc_t *osl_pci_list;

/*
 * Find the entry matching the given segment and bus number.
 * Caller must hold osl_pci_lock.
 */
static osl_pci_rc_t *
osl_pci_find(uint16_t segment, uint8_t bus)
{
	osl_pci_rc_t *rp;

	ASSERT(MUTEX_HELD(&osl_pci_lock));

	for (rp = osl_pci_list; rp != NULL; rp = rp->opr_next) {
		if (rp->opr_segment == segment &&
		    bus >= rp->opr_start_bus &&
		    bus <= rp->opr_end_bus) {
			return (rp);
		}
	}

	return (NULL);
}

/*
 * Perform a direct ECAM read when no dip is available.
 * Maps the target register, performs the access, then unmaps.
 */
static ACPI_STATUS
osl_pci_ecam_read(osl_pci_rc_t *rp, uint8_t bus, uint8_t dev, uint8_t func,
    uint32_t reg, UINT64 *value, uint32_t width)
{
	uint64_t pa;
	size_t maplen = width / 8;
	caddr_t va;

	pa = rp->opr_ecam_base +
	    ECAM_OFFSET(bus - rp->opr_start_bus, dev, func, reg);

	va = psm_map_phys((paddr_t)pa, maplen, PROT_READ);
	if (va == NULL) {
		return (AE_ERROR);
	}

	switch (width) {
	case 8:
		*value = (UINT64)*(volatile uint8_t *)va;
		break;
	case 16:
		*value = (UINT64)*(volatile uint16_t *)va;
		break;
	case 32:
		*value = (UINT64)*(volatile uint32_t *)va;
		break;
	default:
		psm_unmap_phys(va, maplen);
		return (AE_BAD_PARAMETER);
	}

	psm_unmap_phys(va, maplen);
	return (AE_OK);
}

/*
 * Perform a direct ECAM write when no dip is available.
 */
static ACPI_STATUS
osl_pci_ecam_write(osl_pci_rc_t *rp, uint8_t bus, uint8_t dev, uint8_t func,
    uint32_t reg, UINT64 value, uint32_t width)
{
	uint64_t pa;
	size_t maplen = width / 8;
	caddr_t va;

	pa = rp->opr_ecam_base +
	    ECAM_OFFSET(bus - rp->opr_start_bus, dev, func, reg);

	va = psm_map_phys((paddr_t)pa, maplen, PROT_READ | PROT_WRITE);
	if (va == NULL) {
		return (AE_ERROR);
	}

	switch (width) {
	case 8:
		*(volatile uint8_t *)va = (uint8_t)value;
		break;
	case 16:
		*(volatile uint16_t *)va = (uint16_t)value;
		break;
	case 32:
		*(volatile uint32_t *)va = (uint32_t)value;
		break;
	default:
		psm_unmap_phys(va, maplen);
		return (AE_BAD_PARAMETER);
	}

	psm_unmap_phys(va, maplen);
	return (AE_OK);
}

/*
 * Read PCI configuration space via the dip path.
 * The caller must have already called ndi_hold_devi() on rp->opr_dip.
 */
static ACPI_STATUS
osl_pci_dip_read(dev_info_t *rcdip, uint8_t bus, uint8_t dev, uint8_t func,
    uint32_t reg, UINT64 *value, uint32_t width)
{
	pci_cfgacc_req_t req;

	req.rcdip = rcdip;
	req.bdf = PCI_GETBDF(bus, dev, func);
	req.offset = (uint16_t)reg;
	req.write = B_FALSE;
	req.ioacc = B_FALSE;
	VAL64(&req) = 0;

	switch (width) {
	case 8:
		req.size = 1;
		break;
	case 16:
		req.size = 2;
		break;
	case 32:
		req.size = 4;
		break;
	default:
		return (AE_BAD_PARAMETER);
	}

	pci_cfgacc_acc(&req);

	switch (width) {
	case 8:
		*value = (UINT64)VAL8(&req);
		break;
	case 16:
		*value = (UINT64)VAL16(&req);
		break;
	case 32:
		*value = (UINT64)VAL32(&req);
		break;
	default:
		return (AE_BAD_PARAMETER);
	}

	return (AE_OK);
}

/*
 * Write PCI configuration space via the dip path.
 */
static ACPI_STATUS
osl_pci_dip_write(dev_info_t *rcdip, uint8_t bus, uint8_t dev, uint8_t func,
    uint32_t reg, UINT64 value, uint32_t width)
{
	pci_cfgacc_req_t req;

	req.rcdip = rcdip;
	req.bdf = PCI_GETBDF(bus, dev, func);
	req.offset = (uint16_t)reg;
	req.write = B_TRUE;
	req.ioacc = B_FALSE;

	switch (width) {
	case 8:
		req.size = 1;
		VAL8(&req) = (uint8_t)value;
		break;
	case 16:
		req.size = 2;
		VAL16(&req) = (uint16_t)value;
		break;
	case 32:
		req.size = 4;
		VAL32(&req) = (uint32_t)value;
		break;
	default:
		return (AE_BAD_PARAMETER);
	}

	pci_cfgacc_acc(&req);
	return (AE_OK);
}

/*
 * Seed the PCI config space routing list from the MCFG table.
 * Called from acpica _init() after AcpiInitializeTables().
 */
void
acpica_pci_cfgspace_init(void)
{
	ACPI_TABLE_MCFG *mcfg;
	ACPI_MCFG_ALLOCATION *alloc;
	const char *endp;
	ACPI_STATUS st;

	mutex_init(&osl_pci_lock, NULL, MUTEX_DEFAULT, NULL);
	osl_pci_list = NULL;

	mcfg = NULL;
	st = AcpiGetTable(ACPI_SIG_MCFG, 1, (ACPI_TABLE_HEADER **)&mcfg);
	if (ACPI_FAILURE(st)) {
		return;
	}
	ASSERT3P(mcfg, !=, NULL);

	alloc = (ACPI_MCFG_ALLOCATION *)((uintptr_t)mcfg + sizeof (*mcfg));
	endp = ((const char *)mcfg) + mcfg->Header.Length;

	while ((const char *)alloc < endp) {
		osl_pci_rc_t *rp;

		/*
		 * A truncated table may leave fewer than a full
		 * ACPI_MCFG_ALLOCATION before endp; stop rather than
		 * reading out of bounds.
		 */
		if ((const char *)alloc + sizeof (*alloc) > endp) {
			break;
		}

		if (alloc->Address == 0 ||
		    alloc->StartBusNumber > alloc->EndBusNumber) {
			alloc++;
			continue;
		}

		rp = kmem_zalloc(sizeof (osl_pci_rc_t), KM_SLEEP);
		rp->opr_segment = alloc->PciSegment;
		rp->opr_start_bus = alloc->StartBusNumber;
		rp->opr_end_bus = alloc->EndBusNumber;
		rp->opr_ecam_base = alloc->Address;
		rp->opr_dip = NULL;

		mutex_enter(&osl_pci_lock);
		rp->opr_next = osl_pci_list;
		osl_pci_list = rp;
		mutex_exit(&osl_pci_lock);

		alloc++;
	}
}

/*
 * Register a root complex dip with the PCI config space routing list.
 * Called from acpidev when a root complex device node is fully produced.
 *
 * If an MCFG entry exists for this segment/bus range, the dip is filled
 * in and the ECAM base address is verified (and updated to match the
 * dip's reg property if there is a mismatch).
 *
 * If no MCFG entry exists, a new entry is created from the dip's DDI
 * properties.
 */
void
acpica_pci_cfgspace_register(dev_info_t *dip)
{
	int segment;
	int *bus_range;
	uint_t nbus;
	uint64_t ecam_base;
	int *reg;
	uint_t nreg;
	int ac;
	osl_pci_rc_t *rp;

	segment = ddi_prop_get_int(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "linux,pci-domain", 0);

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "bus-range", &bus_range, &nbus) != DDI_PROP_SUCCESS) {
		dev_err(dip, CE_WARN,
		    "acpica_pci_cfgspace_register: missing bus-range");
		return;
	}

	if (nbus < 2) {
		dev_err(dip, CE_WARN,
		    "acpica_pci_cfgspace_register: missing bus-range");
		ddi_prop_free(bus_range);
		return;
	}

	/*
	 * Decode the ECAM base address from the reg property using the
	 * parent's #address-cells.  Walk the address cells, accumulating
	 * into a uint64_t by shifting up 32 bits before ORing each cell.
	 * This naturally handles any #address-cells value and truncates
	 * to the lower 64 bits (e.g. 96-bit PCI phys.hi/mid/lo).
	 */
	ac = ddi_prop_get_int(DDI_DEV_T_ANY, ddi_get_parent(dip), 0,
	    "#address-cells", 2);

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "reg", &reg, &nreg) != DDI_PROP_SUCCESS) {
		dev_err(dip, CE_WARN,
		    "acpica_pci_cfgspace_register: missing reg");
		ddi_prop_free(bus_range);
		return;
	}

	if (nreg < ac) {
		dev_err(dip, CE_WARN,
		    "acpica_pci_cfgspace_register: missing reg");
		ddi_prop_free(reg);
		ddi_prop_free(bus_range);
		return;
	}

	ecam_base = 0;
	for (int i = 0; i < ac; i++) {
		ecam_base = (ecam_base << 32) | (uint64_t)(uint32_t)reg[i];
	}
	ddi_prop_free(reg);

	mutex_enter(&osl_pci_lock);

	rp = osl_pci_find((uint16_t)segment, (uint8_t)bus_range[0]);
	if (rp != NULL) {
		if (rp->opr_ecam_base != ecam_base) {
			cmn_err(CE_WARN, "!acpica_pci_cfgspace_register: "
			    "segment %d ECAM base mismatch: MCFG 0x%lx, "
			    "dip 0x%lx; trusting dip",
			    segment, (unsigned long)rp->opr_ecam_base,
			    (unsigned long)ecam_base);
			rp->opr_ecam_base = ecam_base;
		}
		rp->opr_dip = dip;
	} else {
		rp = kmem_zalloc(sizeof (osl_pci_rc_t), KM_SLEEP);
		rp->opr_segment = (uint16_t)segment;
		rp->opr_start_bus = (uint8_t)bus_range[0];
		rp->opr_end_bus = (uint8_t)bus_range[1];
		rp->opr_ecam_base = ecam_base;
		rp->opr_dip = dip;
		rp->opr_next = osl_pci_list;
		osl_pci_list = rp;
	}

	mutex_exit(&osl_pci_lock);
	ddi_prop_free(bus_range);
}

/*
 * AcpiOsReadPciConfiguration - read PCI configuration space.
 */
ACPI_STATUS
AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg,
    UINT64 *Value, UINT32 Width)
{
	osl_pci_rc_t *rp;
	dev_info_t *dip;
	ACPI_STATUS rv;

	mutex_enter(&osl_pci_lock);
	rp = osl_pci_find(PciId->Segment, (uint8_t)PciId->Bus);
	if (rp == NULL) {
		mutex_exit(&osl_pci_lock);
		cmn_err(CE_WARN, "!AcpiOsReadPciConfiguration: no entry for "
		    "segment %d bus %d", PciId->Segment, PciId->Bus);
		return (AE_NOT_FOUND);
	}

	dip = rp->opr_dip;
	if (dip != NULL) {
		ndi_hold_devi(dip);
	}
	mutex_exit(&osl_pci_lock);

	if (dip != NULL) {
		if (i_ddi_attach_node_hierarchy(dip) != DDI_SUCCESS) {
			ndi_rele_devi(dip);
			/* Fall back to direct ECAM */
			return (osl_pci_ecam_read(rp, (uint8_t)PciId->Bus,
			    (uint8_t)PciId->Device,
			    (uint8_t)PciId->Function, Reg, Value, Width));
		}

		rv = osl_pci_dip_read(dip, (uint8_t)PciId->Bus,
		    (uint8_t)PciId->Device, (uint8_t)PciId->Function,
		    Reg, Value, Width);
		ndi_rele_devi(dip);
		return (rv);
	}

	return (osl_pci_ecam_read(rp, (uint8_t)PciId->Bus,
	    (uint8_t)PciId->Device, (uint8_t)PciId->Function,
	    Reg, Value, Width));
}

/*
 * AcpiOsWritePciConfiguration - write PCI configuration space.
 *
 * Gated on the acpica_write_pci_config_ok tunable, defaulting to allowed.
 */
int acpica_write_pci_config_ok = 1;

ACPI_STATUS
AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg,
    UINT64 Value, UINT32 Width)
{
	osl_pci_rc_t *rp;
	dev_info_t *dip;
	ACPI_STATUS rv;

	if (!acpica_write_pci_config_ok) {
		cmn_err(CE_NOTE, "!write to PCI cfg %x/%x/%x %x"
		    " %lx %d not permitted", PciId->Bus, PciId->Device,
		    PciId->Function, Reg, (long)Value, Width);
		return (AE_SUPPORT);
	}

	mutex_enter(&osl_pci_lock);
	rp = osl_pci_find(PciId->Segment, (uint8_t)PciId->Bus);
	if (rp == NULL) {
		mutex_exit(&osl_pci_lock);
		cmn_err(CE_WARN, "!AcpiOsWritePciConfiguration: no entry for "
		    "segment %d bus %d", PciId->Segment, PciId->Bus);
		return (AE_NOT_FOUND);
	}

	dip = rp->opr_dip;
	if (dip != NULL) {
		ndi_hold_devi(dip);
	}
	mutex_exit(&osl_pci_lock);

	if (dip != NULL) {
		if (i_ddi_attach_node_hierarchy(dip) != DDI_SUCCESS) {
			ndi_rele_devi(dip);
			return (osl_pci_ecam_write(rp, (uint8_t)PciId->Bus,
			    (uint8_t)PciId->Device,
			    (uint8_t)PciId->Function, Reg, Value, Width));
		}

		rv = osl_pci_dip_write(dip, (uint8_t)PciId->Bus,
		    (uint8_t)PciId->Device, (uint8_t)PciId->Function,
		    Reg, Value, Width);
		ndi_rele_devi(dip);
		return (rv);
	}

	return (osl_pci_ecam_write(rp, (uint8_t)PciId->Bus,
	    (uint8_t)PciId->Device, (uint8_t)PciId->Function,
	    Reg, Value, Width));
}
