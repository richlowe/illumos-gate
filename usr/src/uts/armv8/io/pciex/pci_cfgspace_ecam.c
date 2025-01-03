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
 */

/*
 * This file provides a means of accessing PCIe extended configuration space
 * over memory mapped I/O. Traditionally this was always accessed over the
 * various I/O ports; however, we instead opt to leverage facilities in the CPU
 * to set up memory-mapped I/O. To do this we basically do an initial mapping
 * that we use prior to VM in whatever VA space that we can get. After which,
 * we will unmap that and leverage addresses from the device arena once that has
 * been set up.
 *
 * Configuration space is accessed by constructing and addresss that has the
 * bits arranged in the following pattern to indicate what the bus, device,
 * function, and register is:
 *
 *	bus[7:0]	addr[27:20]
 *	dev[4:0]	addr[19:15]
 *	func[2:0]	addr[14:12]
 *	reg[11:0]	addr[11:0]
 *
 * The CPU does not generally support 64-bit accesses, which means that a 64-bit
 * access requires us to write the lower 32-bits followed by the uppwer 32-bits.
 */

#include <sys/ddi.h>
#include <sys/esunddi.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
/* XXXPCI: for psm_map* */
#include <sys/smp_impldefs.h>

#include <sys/machparam.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <sys/mman.h>
#include <sys/bootconf.h>
#include <sys/spl.h>
#include <sys/pci.h>
#include <sys/pcie.h>
#include <sys/pcie_impl.h>
#include <sys/pci_cfgacc.h>
#include <sys/pci_cfgspace.h>
#include <sys/machsystm.h>
#include <sys/sysmacros.h>

/*
 * The pci_cfgacc_req
 */
extern void (*pci_cfgacc_acc_p)(pci_cfgacc_req_t *req);

/*
 * This contains the base virtual address for PCIe configuration space.
 *
 * XXXPCI: This doesn't actually work generally on ARM platforms, we may (in
 * theory at least) have a heterogenous collection of PCIe hardware, each with
 * its own base address.  This is the simplification we made for the
 * prototype alluded to above.
 */
uintptr_t pcie_cfgspace_ecam_vaddr;

extern boolean_t pcie_access_check(int, int, int, int, size_t);

static uintptr_t
pcie_bdfr_to_addr(int bus, int dev, int func, int reg)
{
	uintptr_t bdfr = PCIE_CADDR_ECAM(bus, dev, func, reg);

	return (bdfr + pcie_cfgspace_ecam_vaddr);
}

uint8_t
pcie_cfgspace_ecam_read_uint8(int bus, int dev, int func, int reg)
{
	volatile uint8_t *u8p;
	uint8_t rv;

	if (!pcie_access_check(bus, dev, func, reg, sizeof (rv))) {
		return (PCI_EINVAL8);
	}

	u8p = (uint8_t *)pcie_bdfr_to_addr(bus, dev, func, reg);
	return (*u8p);
}

void
pcie_cfgspace_ecam_write_uint8(int bus, int dev, int func, int reg, uint8_t val)
{
	volatile uint8_t *u8p;

	if (!pcie_access_check(bus, dev, func, reg, sizeof (val))) {
		return;
	}

	u8p = (uint8_t *)pcie_bdfr_to_addr(bus, dev, func, reg);
	*u8p = val;
}

uint16_t
pcie_cfgspace_ecam_read_uint16(int bus, int dev, int func, int reg)
{
	volatile uint16_t *u16p;
	uint16_t rv;

	if (!pcie_access_check(bus, dev, func, reg, sizeof (rv))) {
		return (PCI_EINVAL16);
	}

	u16p = (uint16_t *)pcie_bdfr_to_addr(bus, dev, func, reg);
	return (*u16p);
}

void
pcie_cfgspace_ecam_write_uint16(int bus, int dev, int func, int reg,
    uint16_t val)
{
	volatile uint16_t *u16p;

	if (!pcie_access_check(bus, dev, func, reg, sizeof (val))) {
		return;
	}

	u16p = (uint16_t *)pcie_bdfr_to_addr(bus, dev, func, reg);
	*u16p = val;
}

uint32_t
pcie_cfgspace_ecam_read_uint32(int bus, int dev, int func, int reg)
{
	volatile uint32_t *u32p;
	uint32_t rv;

	if (!pcie_access_check(bus, dev, func, reg, sizeof (rv))) {
		return (PCI_EINVAL32);
	}

	u32p = (uint32_t *)pcie_bdfr_to_addr(bus, dev, func, reg);
	return (*u32p);
}

void
pcie_cfgspace_ecam_write_uint32(int bus, int dev, int func, int reg,
    uint32_t val)
{
	volatile uint32_t *u32p;

	if (!pcie_access_check(bus, dev, func, reg, sizeof (val))) {
		return;
	}

	u32p = (uint32_t *)pcie_bdfr_to_addr(bus, dev, func, reg);
	*u32p = val;
}

/*
 * XXX Historically only 32-bit accesses were done to configuration space.
 */
uint64_t
pcie_cfgspace_ecam_read_uint64(int bus, int dev, int func, int reg)
{
	volatile uint64_t *u64p;
	uint64_t rv;

	if (!pcie_access_check(bus, dev, func, reg, sizeof (rv))) {
		return (PCI_EINVAL64);
	}

	u64p = (uint64_t *)pcie_bdfr_to_addr(bus, dev, func, reg);
	return (*u64p);
}

void
pcie_cfgspace_ecam_write_uint64(int bus, int dev, int func, int reg,
    uint64_t val)
{
	volatile uint64_t *u64p;

	if (!pcie_access_check(bus, dev, func, reg, sizeof (val))) {
		return;
	}

	u64p = (uint64_t *)pcie_bdfr_to_addr(bus, dev, func, reg);
	*u64p = val;
}

/*
 * This is an entry point that expects accesses in a different pattern from the
 * traditional function pointers used above.
 */
void
pcie_cfgspace_ecam_acc(pci_cfgacc_req_t *req)
{
	int bus, dev, func, reg;

	bus = PCI_CFGACC_BUS(req);
	dev = PCI_CFGACC_DEV(req);
	func = PCI_CFGACC_FUNC(req);
	reg = req->offset;

	switch (req->size) {
	case PCI_CFG_SIZE_BYTE:
		if (req->write) {
			pcie_cfgspace_ecam_write_uint8(bus, dev, func, reg,
			    VAL8(req));
		} else {
			VAL8(req) = pcie_cfgspace_ecam_read_uint8(bus, dev,
			    func, reg);
		}
		break;
	case PCI_CFG_SIZE_WORD:
		if (req->write) {
			pcie_cfgspace_ecam_write_uint16(bus, dev, func, reg,
			    VAL16(req));
		} else {
			VAL16(req) = pcie_cfgspace_ecam_read_uint16(bus, dev,
			    func, reg);
		}
		break;
	case PCI_CFG_SIZE_DWORD:
		if (req->write) {
			pcie_cfgspace_ecam_write_uint32(bus, dev, func, reg,
			    VAL32(req));
		} else {
			VAL32(req) = pcie_cfgspace_ecam_read_uint32(bus, dev,
			    func, reg);
		}
		break;
	case PCI_CFG_SIZE_QWORD:
		if (req->write) {
			pcie_cfgspace_ecam_write_uint64(bus, dev, func, reg,
			    VAL64(req));
		} else {
			VAL64(req) = pcie_cfgspace_ecam_read_uint64(bus, dev,
			    func, reg);
		}
		break;
	default:
		if (!req->write) {
			VAL64(req) = PCI_EINVAL64;
		}
		break;
	}
}

int
pcie_cfgspace_ecam_init(uintptr_t addr, size_t size)
{
	cmn_err(CE_NOTE, "PCIe: Using /pcie@%lx-+%lx for config space",
	    addr, size);

	pcie_cfgspace_ecam_vaddr = (uintptr_t)psm_map_phys(addr,
	    size, PROT_READ|PROT_WRITE);
	if (pcie_cfgspace_ecam_vaddr == 0) {
		cmn_err(CE_WARN, "PCIe: Failed to map config space");
		return (-1);
	}

	pci_getb_func = pcie_cfgspace_ecam_read_uint8;
	pci_getw_func = pcie_cfgspace_ecam_read_uint16;
	pci_getl_func = pcie_cfgspace_ecam_read_uint32;
	pci_putb_func = pcie_cfgspace_ecam_write_uint8;
	pci_putw_func = pcie_cfgspace_ecam_write_uint16;
	pci_putl_func = pcie_cfgspace_ecam_write_uint32;
	pci_cfgacc_acc_p = pcie_cfgspace_ecam_acc;

	return (0);
}
