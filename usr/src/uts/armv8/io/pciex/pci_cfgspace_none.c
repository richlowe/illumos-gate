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
 * Copyright 2025 Richard Lowe
 */

/* This is a null cfgspace backend, which shouldn't be necessary really */

#include <sys/ddi.h>
#include <sys/pci.h>
#include <sys/pci_cfgacc.h>
#include <sys/pci_cfgspace.h>
#include <sys/sunddi.h>

extern boolean_t pcie_access_check(int, int, int, int, size_t);
extern void (*pci_cfgacc_acc_p)(pci_cfgacc_req_t *);

uint8_t
pcie_cfgspace_none_read_uint8(int bus, int dev, int func, int reg) {
	return (PCI_EINVAL8);
}

void
pcie_cfgspace_none_write_uint8(int bus, int dev, int func, int reg, uint8_t val)
{
}

uint16_t
pcie_cfgspace_none_read_uint16(int bus, int dev, int func, int reg)
{
	return (PCI_EINVAL16);
}

void
pcie_cfgspace_none_write_uint16(int bus, int dev, int func, int reg, uint16_t val)
{
}

uint32_t
pcie_cfgspace_none_read_uint32(int bus, int dev, int func, int reg) {
	return (PCI_EINVAL32);
}

void
pcie_cfgspace_none_write_uint32(int bus, int dev, int func, int reg, uint32_t val)
{
}

uint64_t
pcie_cfgspace_none_read_uint64(int bus, int dev, int func, int reg)
{
	return (PCI_EINVAL64);
}

void
pcie_cfgspace_none_write_uint64(int bus, int dev, int func, int reg, uint64_t val)
{
}

void
pcie_cfgspace_none_acc(pci_cfgacc_req_t *req)
{
	VAL64(req) = PCI_EINVAL64;
}

void
pcie_cfgspace_none_init(void)
{
	cmn_err(CE_NOTE, "PCIe: No config space access");

	pci_getb_func = pcie_cfgspace_none_read_uint8;
	pci_getw_func = pcie_cfgspace_none_read_uint16;
	pci_getl_func = pcie_cfgspace_none_read_uint32;
	pci_putb_func = pcie_cfgspace_none_write_uint8;
	pci_putw_func = pcie_cfgspace_none_write_uint16;
	pci_putl_func = pcie_cfgspace_none_write_uint32;
	pci_cfgacc_acc_p = pcie_cfgspace_none_acc;
}
