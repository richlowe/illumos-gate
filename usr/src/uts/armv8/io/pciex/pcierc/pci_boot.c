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
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2019 Joyent, Inc.
 * Copyright 2019 Western Digital Corporation
 * Copyright 2020 OmniOS Community Edition (OmniOSce) Association.
 * Copyright 2023 Oxide Computer Company
 */

/*
 * XXXPCI: The below comment hasn't been updated, about which I feel very guilty
 *
 * The reason is that it'd be really great to avoid doing any of this, and
 * drive it out of the PCIe nexus and the same configurator we use for hotplug
 * (remember that our root complexes are known to us via the PRD at the limit).
 *
 * If we can't do that, we must update this to reflect reality (and tidy up
 * the remnants of yanking it from i86pc).
 *
 * The below is lightly editted from the i86 version (removing bits that don't
 * exist at all), but some prose still refers to bits that don't exist at all.
 */
/*
 * PCI bus enumeration and device programming are done in several passes. The
 * following is a high level overview of this process.
 *
 * pci_enumerate(reprogram=0)
 *				The main entry point to PCI bus enumeration is
 *				pci_enumerate(). This function is invoked
 *				twice, once to set up the PCI portion of the
 *				device tree, and then a second time to
 *				reprogram any devices which were not set up by
 *				the system firmware. On this first call, the
 *				reprogram parameter is set to 0.
 *   pci_setup_tree()
 *	enumerate_bus_devs()
 *	    <foreach bus>
 *	        process_devfunc()
 *	            <set up most device properties>
 *				The next stage is to enumerate the bus and set
 *				up the bulk of the properties for each device.
 *				This is where the generic properties such as
 *				'device-id' are created.
 *		    <if PPB device>
 *			add_ppb_props()
 *				For a PCI-to-PCI bridge (ppb) device, any
 *				memory ranges for IO, memory or pre-fetchable
 *				memory that have been programmed by the system
 *				firmware (BIOS/EFI) are retrieved and stored in
 *				bus-specific lists (pci_bus_res[bus].io_avail,
 *				mem_avail and pmem_avail). The contents of
 *				these lists are used to set the initial 'ranges'
 *				property on the ppb device. Later, as children
 *				are found for this bridge, resources will be
 *				removed from these avail lists as necessary.
 *		    <else>
 *			<add to list of non-PPB devices for the bus>
 *				Any non-PPB device on the bus is recorded in a
 *				bus-specific list, to be set up (and possibly
 *				reprogrammed) later.
 *		    add_reg_props()
 *				The final step in this phase is to add the
 *				initial 'reg' and 'assigned-addresses'
 *				properties to all devices. At the same time,
 *				any IO or memory ranges which have been
 *				assigned to the bus are moved from the avail
 *				list to the corresponding used one. If no
 *				resources have been assigned to a device at
 *				this stage, then it is flagged for subsequent
 *				reprogramming.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/sunndi.h>
#include <sys/pci.h>
#include <sys/pci_impl.h>
#include <sys/pcie_impl.h>
#include <sys/pci_props.h>
#include <sys/memlist.h>
#include <sys/bootconf.h>
#include <sys/pci_cfgacc.h>
#include <sys/pci_cfgspace.h>
#include <sys/psw.h>
#include "../../../../../common/pci/pci_strings.h"
#include <sys/devcache.h>
#include <sys/plat/pci_prd.h>
#include <sys/obpdefs.h>

#define	dcmn_err	if (pci_boot_debug != 0) cmn_err
#define	bus_debug(bus)	(pci_boot_debug != 0 && pci_debug_bus_start != -1 && \
	    pci_debug_bus_end != -1 && (bus) >= pci_debug_bus_start && \
	    (bus) <= pci_debug_bus_end)
#define	dump_memlists(pbr, tag, bus)				\
	if (bus_debug((bus))) dump_memlists_impl(pbr, (tag), (bus))
#define	MSGHDR		"!%s[%02x/%02x/%x]: "

/*
 * Determining the size of a PCI BAR is done by writing all 1s to the base
 * register and then reading the value back. The retrieved value will either
 * be zero, indicating that the BAR is unimplemented, or a mask in which
 * the significant bits for the required memory space are 0.
 * For example, a 32-bit BAR could return 0xfff00000 which equates to a
 * length of 0x100000 (1MiB). The following macro does that conversion.
 * The input value must have already had the lower encoding bits cleared.
 */
#define	BARMASKTOLEN(value) ((((value) ^ ((value) - 1)) + 1) >> 1)

typedef enum {
	RES_IO,
	RES_MEM,
	RES_PMEM
} mem_res_t;

/*
 * In order to disable an IO or memory range on a bridge, the range's base must
 * be set to a value greater than its limit. The following values are used for
 * this purpose.
 */
#define	PPB_DISABLE_IORANGE_BASE	0x9fff
#define	PPB_DISABLE_IORANGE_LIMIT	0x1000
#define	PPB_DISABLE_MEMRANGE_BASE	0x9ff00000
#define	PPB_DISABLE_MEMRANGE_LIMIT	0x100fffff

static uchar_t max_dev_pci = 32;	/* PCI standard */
int pci_boot_maxbus;

int pci_boot_debug = 0;
int pci_debug_bus_start = 0;
int pci_debug_bus_end = 0xff;

extern dev_info_t *pcie_get_rc_dip(dev_info_t *);

/*
 * Module prototypes
 */
static void enumerate_bus_devs(dev_info_t *, uchar_t,
    struct pci_bus_resource *);
static void process_devfunc(dev_info_t *, struct pci_bus_resource *,
    uchar_t, uchar_t, uchar_t);
static void add_reg_props(dev_info_t *, dev_info_t *, struct pci_bus_resource *,
    uchar_t, uchar_t, uchar_t);
static void add_ppb_props(dev_info_t *, dev_info_t *, struct pci_bus_resource *,
    uchar_t, uchar_t, uchar_t, boolean_t, boolean_t);
static void add_bus_range_prop(struct pci_bus_resource *, int);
static void add_ranges_prop(struct pci_bus_resource *, int, boolean_t);
static void alloc_res_array(struct pci_bus_resource **);

static void
dump_memlists_impl(struct pci_bus_resource *pci_bus_res, const char *tag,
    int bus)
{
	printf("Memlist dump at %s - bus %x\n", tag, bus);
	if (pci_bus_res[bus].io_used != NULL) {
		printf("    io_used ");
		pci_memlist_dump(pci_bus_res[bus].io_used);
	}
	if (pci_bus_res[bus].io_avail != NULL) {
		printf("    io_avail ");
		pci_memlist_dump(pci_bus_res[bus].io_avail);
	}
	if (pci_bus_res[bus].mem_used != NULL) {
		printf("    mem_used ");
		pci_memlist_dump(pci_bus_res[bus].mem_used);
	}
	if (pci_bus_res[bus].mem_avail != NULL) {
		printf("    mem_avail ");
		pci_memlist_dump(pci_bus_res[bus].mem_avail);
	}
	if (pci_bus_res[bus].pmem_used != NULL) {
		printf("    pmem_used ");
		pci_memlist_dump(pci_bus_res[bus].pmem_used);
	}
	if (pci_bus_res[bus].pmem_avail != NULL) {
		printf("    pmem_avail ");
		pci_memlist_dump(pci_bus_res[bus].pmem_avail);
	}
}

/*
 * Enumerate all PCI devices
 */
void
pci_setup_tree(dev_info_t *dip)
{
	/* pci bus resource maps */
	struct pci_bus_resource *pci_bus_res = NULL;
	uint_t i;

	alloc_res_array(&pci_bus_res);

	for (i = 0; i <= pci_boot_maxbus; i++) {
		pci_bus_res[i].par_bus = (uchar_t)-1;
		pci_bus_res[i].sub_bus = i;
	}

	int busrng[] = {0, 0xff};
	int *bus_prop;
	uint_t bus_prop_sz;

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "bus-range",
	    &bus_prop, &bus_prop_sz) == DDI_SUCCESS) {
		VERIFY3U(bus_prop_sz, ==, 2);
		busrng[0] = bus_prop[0];
		busrng[1] = bus_prop[1];
		ddi_prop_free(bus_prop);
	}

	/* We have already run through this dip */
	if (pci_bus_res[busrng[0]].dip != NULL) {
		VERIFY3P(pci_bus_res[busrng[0]].dip, ==, dip);
		return;
	}

	/*
	 * The first bus is _our_ bus, others in the range
	 * are available to subordinate bridges.
	 */
	pci_bus_res[busrng[0]].dip = dip;

	for (int i = busrng[0]; i <= busrng[1]; i++) {
		enumerate_bus_devs(dip, i, pci_bus_res);
	}
}

void
pci_enumerate(dev_info_t *dip)
{
	pci_boot_maxbus = pci_prd_max_bus();
	pci_setup_tree(dip);
}

static void
set_ppb_res(dev_info_t *rcdip, uchar_t bus, uchar_t dev, uchar_t func,
    mem_res_t type, uint64_t base, uint64_t limit)
{
	char *tag;

	switch (type) {
	case RES_IO: {
		VERIFY0(base >> 32);
		VERIFY0(limit >> 32);

		pci_cfgacc_put8(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_IO_BASE_LOW,
		    (uint8_t)((base >> PCI_BCNF_IO_SHIFT) & PCI_BCNF_IO_MASK));
		pci_cfgacc_put8(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_IO_LIMIT_LOW,
		    (uint8_t)((limit >> PCI_BCNF_IO_SHIFT) & PCI_BCNF_IO_MASK));

		uint8_t val = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_IO_BASE_LOW);
		if ((val & PCI_BCNF_ADDR_MASK) == PCI_BCNF_IO_32BIT) {
			pci_cfgacc_put16(rcdip, PCI_GETBDF(bus, dev, func),
			    PCI_BCNF_IO_BASE_HI, base >> 16);
			pci_cfgacc_put16(rcdip, PCI_GETBDF(bus, dev, func),
			    PCI_BCNF_IO_LIMIT_HI, limit >> 16);
		} else {
			VERIFY0(base >> 16);
			VERIFY0(limit >> 16);
		}

		tag = "I/O";
		break;
	}

	case RES_MEM:
		VERIFY0(base >> 32);
		VERIFY0(limit >> 32);

		pci_cfgacc_put16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_MEM_BASE, (uint16_t)((base >> PCI_BCNF_MEM_SHIFT) &
		    PCI_BCNF_MEM_MASK));
		pci_cfgacc_put16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_MEM_LIMIT,
		    (uint16_t)((limit >> PCI_BCNF_MEM_SHIFT) &
		    PCI_BCNF_MEM_MASK));

		tag = "MEM";
		break;

	case RES_PMEM: {
		pci_cfgacc_put16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_PF_BASE_LOW,
		    (uint16_t)((base >> PCI_BCNF_MEM_SHIFT) &
		    PCI_BCNF_MEM_MASK));
		pci_cfgacc_put16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_PF_LIMIT_LOW,
		    (uint16_t)((limit >> PCI_BCNF_MEM_SHIFT) &
		    PCI_BCNF_MEM_MASK));

		uint16_t val = pci_cfgacc_get16(rcdip,
		    PCI_GETBDF(bus, dev, func), PCI_BCNF_PF_BASE_LOW);
		if ((val & PCI_BCNF_ADDR_MASK) == PCI_BCNF_PF_MEM_64BIT) {
			pci_cfgacc_put32(rcdip, PCI_GETBDF(bus, dev, func),
			    PCI_BCNF_PF_BASE_HIGH, base >> 32);
			pci_cfgacc_put32(rcdip, PCI_GETBDF(bus, dev, func),
			    PCI_BCNF_PF_LIMIT_HIGH, limit >> 32);
		} else {
			VERIFY0(base >> 32);
			VERIFY0(limit >> 32);
		}

		tag = "PMEM";
		break;
	}

	default:
		panic("Invalid resource type %d", type);
	}

	if (base > limit) {
		cmn_err(CE_NOTE, MSGHDR "DISABLE %4s range",
		    "ppb", bus, dev, func, tag);
	} else {
		cmn_err(CE_NOTE,
		    MSGHDR "PROGRAM %4s range 0x%lx ~ 0x%lx",
		    "ppb", bus, dev, func, tag, base, limit);
	}
}

static void
fetch_ppb_res(dev_info_t *rcdip, uchar_t bus, uchar_t dev, uchar_t func,
    mem_res_t type, uint64_t *basep, uint64_t *limitp)
{
	uint64_t val, base, limit;

	switch (type) {
	case RES_IO:
		val = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_IO_LIMIT_LOW);
		limit = ((val & PCI_BCNF_IO_MASK) << PCI_BCNF_IO_SHIFT) |
		    PCI_BCNF_IO_LIMIT_BITS;
		val = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_IO_BASE_LOW);
		base = ((val & PCI_BCNF_IO_MASK) << PCI_BCNF_IO_SHIFT);

		if ((val & PCI_BCNF_ADDR_MASK) == PCI_BCNF_IO_32BIT) {
			val = pci_cfgacc_get16(rcdip,
			    PCI_GETBDF(bus, dev, func), PCI_BCNF_IO_BASE_HI);
			base |= val << 16;
			val = pci_cfgacc_get16(rcdip,
			    PCI_GETBDF(bus, dev, func), PCI_BCNF_IO_LIMIT_HI);
			limit |= val << 16;
		}
		VERIFY0(base >> 32);
		break;

	case RES_MEM:
		val = pci_cfgacc_get16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_MEM_LIMIT);
		limit = ((val & PCI_BCNF_MEM_MASK) << PCI_BCNF_MEM_SHIFT) |
		    PCI_BCNF_MEM_LIMIT_BITS;
		val = pci_cfgacc_get16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_MEM_BASE);
		base = ((val & PCI_BCNF_MEM_MASK) << PCI_BCNF_MEM_SHIFT);
		VERIFY0(base >> 32);
		break;

	case RES_PMEM:
		val = pci_cfgacc_get16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_PF_LIMIT_LOW);
		limit = ((val & PCI_BCNF_MEM_MASK) << PCI_BCNF_MEM_SHIFT) |
		    PCI_BCNF_MEM_LIMIT_BITS;
		val = pci_cfgacc_get16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_BCNF_PF_BASE_LOW);
		base = ((val & PCI_BCNF_MEM_MASK) << PCI_BCNF_MEM_SHIFT);

		if ((val & PCI_BCNF_ADDR_MASK) == PCI_BCNF_PF_MEM_64BIT) {
			val = pci_cfgacc_get32(rcdip,
			    PCI_GETBDF(bus, dev, func), PCI_BCNF_PF_BASE_HIGH);
			base |= val << 32;
			val = pci_cfgacc_get32(rcdip,
			    PCI_GETBDF(bus, dev, func), PCI_BCNF_PF_LIMIT_HIGH);
			limit |= val << 32;
		}
		break;
	default:
		panic("Invalid resource type %d", type);
	}

	*basep = base;
	*limitp = limit;
}

/*
 * For any fixed configuration (often compatability) pci devices
 * and those with their own expansion rom, create device nodes
 * to hold the already configured device details.
 */
void
enumerate_bus_devs(dev_info_t *rcdip, uchar_t bus,
    struct pci_bus_resource *pci_bus_res)
{
	uchar_t dev, func, nfunc, header;

	if (bus_debug(bus)) {
		dcmn_err(CE_NOTE, "enumerating pci bus 0x%x", bus);
	}

	for (dev = 0; dev < max_dev_pci; dev++) {
		nfunc = 1;
		for (func = 0; func < nfunc; func++) {
			ushort_t venid = pci_cfgacc_get16(rcdip,
			    PCI_GETBDF(bus, dev, func), PCI_CONF_VENID);
			ushort_t devid = pci_cfgacc_get16(rcdip,
			    PCI_GETBDF(bus, dev, func), PCI_CONF_DEVID);
			if ((venid != 0xffff) && (venid != 0x0))
				dcmn_err(CE_CONT, "pci%x,%x at %x:%x:%x\n",
				    venid, devid, bus, dev, func);
			if ((venid == 0xffff) || (venid == 0)) {
				/* no function at this address */
				continue;
			}

			header = pci_cfgacc_get8(rcdip,
			    PCI_GETBDF(bus, dev, func), PCI_CONF_HEADER);
			if (header == 0xff) {
				dcmn_err(CE_CONT, "%x:%x:%x has no header\n",
				    bus, dev, func);
				continue; /* illegal value */
			}

			/*
			 * according to some mail from Microsoft posted
			 * to the pci-drivers alias, their only requirement
			 * for a multifunction device is for the 1st
			 * function to have to PCI_HEADER_MULTI bit set.
			 */
			if ((func == 0) && (header & PCI_HEADER_MULTI)) {
				nfunc = 8;
			}

			dcmn_err(CE_CONT, "%x:%x:%x process_devfunc\n", bus,
			    dev, func);
			process_devfunc(rcdip, pci_bus_res, bus, dev, func);
		}
	}

	/* percolate bus used resources up through parents to root */
	int	par_bus;

	par_bus = pci_bus_res[bus].par_bus;
	while (par_bus != (uchar_t)-1) {
		pci_bus_res[par_bus].io_size +=
		    pci_bus_res[bus].io_size;
		pci_bus_res[par_bus].mem_size +=
		    pci_bus_res[bus].mem_size;
		pci_bus_res[par_bus].pmem_size +=
		    pci_bus_res[bus].pmem_size;

		if (pci_bus_res[bus].io_used != NULL) {
			pci_memlist_merge(&pci_bus_res[bus].io_used,
			    &pci_bus_res[par_bus].io_used);
		}

		if (pci_bus_res[bus].mem_used != NULL) {
			pci_memlist_merge(&pci_bus_res[bus].mem_used,
			    &pci_bus_res[par_bus].mem_used);
		}

		if (pci_bus_res[bus].pmem_used != NULL) {
			pci_memlist_merge(&pci_bus_res[bus].pmem_used,
			    &pci_bus_res[par_bus].pmem_used);
		}

		pci_bus_res[par_bus].num_bridge +=
		    pci_bus_res[bus].num_bridge;

		bus = par_bus;
		par_bus = pci_bus_res[par_bus].par_bus;
	}
}

static void
set_devpm_d0(dev_info_t *rcdip, uchar_t bus, uchar_t dev, uchar_t func)
{
	uint16_t status;
	uint8_t header;
	uint8_t cap_ptr;
	uint8_t cap_id;
	uint16_t pmcsr;

	status = pci_cfgacc_get16(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_CONF_STAT);
	if (!(status & PCI_STAT_CAP))
		return;	/* No capabilities list */

	header = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_CONF_HEADER) & PCI_HEADER_TYPE_M;
	if (header == PCI_HEADER_CARDBUS) {
		cap_ptr = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_CBUS_CAP_PTR);
	} else {
		cap_ptr = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_CONF_CAP_PTR);
	}
	/*
	 * Walk the capabilities list searching for a PM entry.
	 */
	while (cap_ptr != PCI_CAP_NEXT_PTR_NULL && cap_ptr >= PCI_CAP_PTR_OFF) {
		cap_ptr &= PCI_CAP_PTR_MASK;
		cap_id = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
		    cap_ptr + PCI_CAP_ID);
		if (cap_id == PCI_CAP_ID_PM) {
			pmcsr = pci_cfgacc_get16(rcdip,
			    PCI_GETBDF(bus, dev, func), cap_ptr + PCI_PMCSR);
			pmcsr &= ~(PCI_PMCSR_STATE_MASK);
			pmcsr |= PCI_PMCSR_D0; /* D0 state */
			pci_cfgacc_put16(rcdip, PCI_GETBDF(bus, dev, func),
			    cap_ptr + PCI_PMCSR, pmcsr);
			break;
		}
		cap_ptr = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
		    cap_ptr + PCI_CAP_NEXT_PTR);
	}

}

static void
process_devfunc(dev_info_t *rcdip, struct pci_bus_resource *pci_bus_res,
    uchar_t bus, uchar_t dev, uchar_t func)
{
	pci_prop_data_t prop_data;
	pci_prop_failure_t prop_ret;
	dev_info_t *dip;
	int power[2] = {1, 1};
	pcie_req_id_t bdf;

	prop_ret = pci_prop_data_fill(rcdip, NULL, bus, dev, func, &prop_data);
	if (prop_ret != PCI_PROP_OK) {
		cmn_err(CE_WARN, MSGHDR "failed to get basic PCI data: 0x%x",
		    "pci", bus, dev, func, prop_ret);
		return;
	}

	VERIFY3P(pci_bus_res[bus].dip, !=, NULL);

	ndi_devi_alloc_sleep(pci_bus_res[bus].dip, DEVI_PSEUDO_NEXNAME,
	    DEVI_SID_NODEID, &dip);
	prop_ret = pci_prop_name_node(dip, &prop_data);
	if (prop_ret != PCI_PROP_OK) {
		cmn_err(CE_WARN, MSGHDR "failed to set node name: 0x%x; "
		    "devinfo node not created", "pci", bus, dev, func,
		    prop_ret);
		(void) ndi_devi_free(dip);
		return;
	}

	bdf = PCI_GETBDF(bus, dev, func);

	/*
	 * Only populate bus_t if this device is sitting under a PCIE root
	 * complex.  Some particular machines have both a PCIE root complex and
	 * a PCI hostbridge, in which case only devices under the PCIE root
	 * complex will have their bus_t populated.
	 */
	if (pcie_get_rc_dip(dip) != NULL) {
		(void) pcie_init_bus(dip, bdf, PCIE_BUS_INITIAL);
	}

	/*
	 * Go through and set all of the devinfo proprties on this function.
	 */
	prop_ret = pci_prop_set_common_props(dip, &prop_data);
	if (prop_ret != PCI_PROP_OK) {
		cmn_err(CE_WARN, MSGHDR "failed to set properties: 0x%x; "
		    "devinfo node not created", "pci", bus, dev, func,
		    prop_ret);
		if (pcie_get_rc_dip(dip) != NULL) {
			pcie_fini_bus(dip, PCIE_BUS_FINAL);
		}
		(void) ndi_devi_free(dip);
		return;
	}

	(void) ndi_prop_update_int_array(DDI_DEV_T_NONE, dip,
	    "power-consumption", power, 2);

	/* Set the device PM state to D0 */
	set_devpm_d0(rcdip, bus, dev, func);

	if (pci_prop_class_is_pcibridge(&prop_data)) {
		boolean_t pciex = (prop_data.ppd_flags & PCI_PROP_F_PCIE) != 0;
		boolean_t is_pci_bridge = pciex &&
		    prop_data.ppd_pcie_type == PCIE_PCIECAP_DEV_TYPE_PCIE2PCI;
		add_ppb_props(rcdip, dip, pci_bus_res, bus, dev, func, pciex,
		    is_pci_bridge);
	}

	prop_ret = pci_prop_set_compatible(dip, &prop_data);
	if (prop_ret != PCI_PROP_OK) {
		cmn_err(CE_WARN, MSGHDR "failed to set compatible property: "
		    "0x%x;  device may not bind to a driver", "pci", bus, dev,
		    func, prop_ret);
	}

	DEVI_SET_PCI(dip);
	add_reg_props(rcdip, dip, pci_bus_res, bus, dev, func);
	(void) ndi_devi_bind_driver(dip, 0);
}

/*
 * Returns:
 *	-1	Skip this BAR
 *	 0	Properties have been assigned
 */
static int
add_bar_reg_props(dev_info_t *rcdip, struct pci_bus_resource *pci_bus_res,
    uchar_t bus, uchar_t dev, uchar_t func, uint_t bar, ushort_t offset,
    pci_regspec_t *regs, pci_regspec_t *assigned, ushort_t *bar_sz)
{
	uint8_t baseclass;
	uint32_t base, devloc;
	uint16_t command = 0;
	uint64_t value;

	devloc = PCI_REG_MAKE_BDFR(bus, dev, func, 0);
	baseclass = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_CONF_BASCLASS);

	/*
	 * Determine the size of the BAR by writing 0xffffffff to the base
	 * register and reading the value back before restoring the original.
	 *
	 * For non-bridges, disable I/O and Memory access while doing this to
	 * avoid difficulty with USB emulation (see OHCI spec1.0a appendix B
	 * "Host Controller Mapping"). Doing this for bridges would have the
	 * side-effect of making the bridge transparent to secondary-bus
	 * activity (see sections 4.1-4.3 of the PCI-PCI Bridge Spec V1.2).
	 */
	base = pci_cfgacc_get32(rcdip, PCI_GETBDF(bus, dev, func), offset);

	if (baseclass != PCI_CLASS_BRIDGE) {
		command = pci_cfgacc_get16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_CONF_COMM);
		pci_cfgacc_put16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_CONF_COMM, command & ~(PCI_COMM_MAE | PCI_COMM_IO));
	}

	pci_cfgacc_put32(rcdip, PCI_GETBDF(bus, dev, func), offset, 0xffffffff);
	value = pci_cfgacc_get32(rcdip, PCI_GETBDF(bus, dev, func), offset);
	pci_cfgacc_put32(rcdip, PCI_GETBDF(bus, dev, func), offset, base);

	if (baseclass != PCI_CLASS_BRIDGE) {
		pci_cfgacc_put16(rcdip, PCI_GETBDF(bus, dev, func),
		    PCI_CONF_COMM, command);
	}

	/* I/O Space */
	if ((base & PCI_BASE_SPACE_IO) != 0) {
		struct memlist **io_avail = &pci_bus_res[bus].io_avail;
		struct memlist **io_used = &pci_bus_res[bus].io_used;
		boolean_t hard_decode = B_FALSE;
		uint_t len;

		*bar_sz = PCI_BAR_SZ_32;
		value &= PCI_BASE_IO_ADDR_M;
		len = BARMASKTOLEN(value);

		if (value == 0) {
			/* skip base regs with size of 0 */
			return (-1);
		}

		regs->pci_phys_hi = PCI_ADDR_IO | devloc;
		if (hard_decode) {
			regs->pci_phys_hi |= PCI_RELOCAT_B;
			regs->pci_phys_low = base & PCI_BASE_IO_ADDR_M;
		} else {
			regs->pci_phys_hi |= offset;
			regs->pci_phys_low = 0;
		}
		assigned->pci_phys_hi = PCI_RELOCAT_B | regs->pci_phys_hi;
		regs->pci_size_low = assigned->pci_size_low = len;

		/*
		 * 'type' holds the non-address part of the base to be re-added
		 * to any new address in the programming step below.
		 */
		base &= PCI_BASE_IO_ADDR_M;

		/*
		 * A device under a subtractive PPB can allocate resources from
		 * its parent bus if there is no resource available on its own
		 * bus.
		 */
		/* take out of the resource map of the bus */
		if (base != 0) {
			(void) pci_memlist_remove(io_avail, base, len);
			pci_memlist_insert(io_used, base, len);
		}

		dcmn_err(CE_NOTE,
		    MSGHDR "BAR%u  I/O FWINIT 0x%x ~ 0x%x",
		    "pci", bus, dev, func, bar, base, len);
		pci_bus_res[bus].io_size += len;
		assigned->pci_phys_low = base;

	} else {	/* Memory space */
		struct memlist **mem_avail = &pci_bus_res[bus].mem_avail;
		struct memlist **mem_used = &pci_bus_res[bus].mem_used;
		struct memlist **pmem_avail = &pci_bus_res[bus].pmem_avail;
		struct memlist **pmem_used = &pci_bus_res[bus].pmem_used;
		uint_t base_hi, phys_hi;
		uint64_t len, fbase;

		if ((base & PCI_BASE_TYPE_M) == PCI_BASE_TYPE_ALL) {
			*bar_sz = PCI_BAR_SZ_64;
			base_hi = pci_cfgacc_get32(rcdip,
			    PCI_GETBDF(bus, dev, func), offset + 4);
			pci_cfgacc_put32(rcdip, PCI_GETBDF(bus, dev, func),
			    offset + 4, 0xffffffff);
			value |= (uint64_t)pci_cfgacc_get32(rcdip,
			    PCI_GETBDF(bus, dev, func), offset + 4) << 32;
			pci_cfgacc_put32(rcdip, PCI_GETBDF(bus, dev, func),
			    offset + 4, base_hi);
			phys_hi = PCI_ADDR_MEM64;
			value &= PCI_BASE_M_ADDR64_M;
		} else {
			*bar_sz = PCI_BAR_SZ_32;
			base_hi = 0;
			phys_hi = PCI_ADDR_MEM32;
			value &= PCI_BASE_M_ADDR_M;
		}

		/* skip base regs with size of 0 */
		if (value == 0)
			return (-1);

		len = BARMASKTOLEN(value);
		regs->pci_size_low = assigned->pci_size_low = len & 0xffffffff;
		regs->pci_size_hi = assigned->pci_size_hi = len >> 32;

		phys_hi |= devloc | offset;
		if (base & PCI_BASE_PREF_M)
			phys_hi |= PCI_PREFETCH_B;

		regs->pci_phys_hi = assigned->pci_phys_hi = phys_hi;
		assigned->pci_phys_hi |= PCI_RELOCAT_B;

		base &= PCI_BASE_M_ADDR_M;

		fbase = (((uint64_t)base_hi) << 32) | base;

		dcmn_err(CE_NOTE,
		    MSGHDR "BAR%u %sMEM FWINIT 0x%lx ~ 0x%lx%s",
		    "pci", bus, dev, func, bar,
		    (phys_hi & PCI_PREFETCH_B) ? "P" : " ",
		    fbase, len,
		    *bar_sz == PCI_BAR_SZ_64 ? " (64-bit)" : "");

		/* take out of the resource map of the bus */
		if (fbase != 0) {
			/* remove from PMEM and MEM space */
			(void) pci_memlist_remove(mem_avail, fbase, len);
			(void) pci_memlist_remove(pmem_avail, fbase, len);
			/* only note as used in correct map */
			if ((phys_hi & PCI_PREFETCH_B) != 0)
				pci_memlist_insert(pmem_used, fbase, len);
			else
				pci_memlist_insert(mem_used, fbase, len);
		}

		if (phys_hi & PCI_PREFETCH_B)
			pci_bus_res[bus].pmem_size += len;
		else
			pci_bus_res[bus].mem_size += len;

		assigned->pci_phys_mid = base_hi;
		assigned->pci_phys_low = base;
	}

	dcmn_err(CE_NOTE, MSGHDR "BAR%u ---- %08x.%x.%x.%x.%x",
	    "pci", bus, dev, func, bar,
	    assigned->pci_phys_hi,
	    assigned->pci_phys_mid,
	    assigned->pci_phys_low,
	    assigned->pci_size_hi,
	    assigned->pci_size_low);

	return (0);
}

/*
 * Add the "reg" and "assigned-addresses" property
 */
static void
add_reg_props(dev_info_t *rcdip, dev_info_t *dip,
    struct pci_bus_resource *pci_bus_res, uchar_t bus, uchar_t dev,
    uchar_t func)
{
	uchar_t baseclass, subclass, progclass, header;
	uint_t bar, value, devloc, base;
	ushort_t bar_sz, offset, end;
	int max_basereg;

	struct memlist **io_avail, **io_used;
	struct memlist **mem_avail, **mem_used;
	struct memlist **pmem_avail;

	pci_regspec_t regs[16] = {{0}};
	pci_regspec_t assigned[15] = {{0}};
	int nreg, nasgn;

	io_avail = &pci_bus_res[bus].io_avail;
	io_used = &pci_bus_res[bus].io_used;
	mem_avail = &pci_bus_res[bus].mem_avail;
	mem_used = &pci_bus_res[bus].mem_used;
	pmem_avail = &pci_bus_res[bus].pmem_avail;

	dump_memlists(pci_bus_res, "add_reg_props start", bus);

	devloc = PCI_REG_MAKE_BDFR(bus, dev, func, 0);
	regs[0].pci_phys_hi = devloc;
	nreg = 1;	/* rest of regs[0] is all zero */
	nasgn = 0;

	baseclass = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_CONF_BASCLASS);
	subclass = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_CONF_SUBCLASS);
	progclass = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_CONF_PROGCLASS);
	header = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_CONF_HEADER) & PCI_HEADER_TYPE_M;

	switch (header) {
	case PCI_HEADER_ZERO:
		max_basereg = PCI_BASE_NUM;
		break;
	case PCI_HEADER_PPB:
		max_basereg = PCI_BCNF_BASE_NUM;
		break;
	default:
		max_basereg = 0;
		break;
	}

	end = PCI_CONF_BASE0 + max_basereg * sizeof (uint_t);
	for (bar = 0, offset = PCI_CONF_BASE0; offset < end;
	    bar++, offset += bar_sz) {
		int ret;

		ret = add_bar_reg_props(rcdip, pci_bus_res, bus, dev, func, bar,
		    offset, &regs[nreg], &assigned[nasgn], &bar_sz);

		if (bar_sz == PCI_BAR_SZ_64)
			bar++;

		if (ret == -1)		/* Skip BAR */
			continue;

		nreg++;
		nasgn++;
	}

	switch (header) {
	case PCI_HEADER_ZERO:
		offset = PCI_CONF_ROM;
		break;
	case PCI_HEADER_PPB:
		offset = PCI_BCNF_ROM;
		break;
	default: /* including PCI_HEADER_CARDBUS */
		goto done;
	}

	/*
	 * Add the expansion rom memory space
	 * Determine the size of the ROM base reg; don't write reserved bits
	 * ROM isn't in the PCI memory space.
	 */
	base = pci_cfgacc_get32(rcdip, PCI_GETBDF(bus, dev, func), offset);
	pci_cfgacc_put32(rcdip, PCI_GETBDF(bus, dev, func),
	    offset, PCI_BASE_ROM_ADDR_M);
	value = pci_cfgacc_get32(rcdip, PCI_GETBDF(bus, dev, func), offset);
	pci_cfgacc_put32(rcdip, PCI_GETBDF(bus, dev, func), offset, base);
	if (value & PCI_BASE_ROM_ENABLE)
		value &= PCI_BASE_ROM_ADDR_M;
	else
		value = 0;

	if (value != 0) {
		uint_t len;

		regs[nreg].pci_phys_hi = (PCI_ADDR_MEM32 | devloc) + offset;
		assigned[nasgn].pci_phys_hi = (PCI_RELOCAT_B |
		    PCI_ADDR_MEM32 | devloc) + offset;
		base &= PCI_BASE_ROM_ADDR_M;
		assigned[nasgn].pci_phys_low = base;
		len = BARMASKTOLEN(value);
		regs[nreg].pci_size_low = assigned[nasgn].pci_size_low = len;
		nreg++, nasgn++;
		/* take it out of the memory resource */
		if (base != 0) {
			(void) pci_memlist_remove(mem_avail, base, len);
			pci_memlist_insert(mem_used, base, len);
			pci_bus_res[bus].mem_size += len;
		}
	}

	/*
	 * Account for "legacy" (alias) video adapter resources
	 */

	/* add the three hard-decode, aliased address spaces for VGA */
	if ((baseclass == PCI_CLASS_DISPLAY && subclass == PCI_DISPLAY_VGA) ||
	    (baseclass == PCI_CLASS_NONE && subclass == PCI_NONE_VGA)) {

		/* VGA hard decode 0x3b0-0x3bb */
		regs[nreg].pci_phys_hi = assigned[nasgn].pci_phys_hi =
		    (PCI_RELOCAT_B | PCI_ALIAS_B | PCI_ADDR_IO | devloc);
		regs[nreg].pci_phys_low = assigned[nasgn].pci_phys_low = 0x3b0;
		regs[nreg].pci_size_low = assigned[nasgn].pci_size_low = 0xc;
		nreg++, nasgn++;
		(void) pci_memlist_remove(io_avail, 0x3b0, 0xc);
		pci_memlist_insert(io_used, 0x3b0, 0xc);
		pci_bus_res[bus].io_size += 0xc;

		/* VGA hard decode 0x3c0-0x3df */
		regs[nreg].pci_phys_hi = assigned[nasgn].pci_phys_hi =
		    (PCI_RELOCAT_B | PCI_ALIAS_B | PCI_ADDR_IO | devloc);
		regs[nreg].pci_phys_low = assigned[nasgn].pci_phys_low = 0x3c0;
		regs[nreg].pci_size_low = assigned[nasgn].pci_size_low = 0x20;
		nreg++, nasgn++;
		(void) pci_memlist_remove(io_avail, 0x3c0, 0x20);
		pci_memlist_insert(io_used, 0x3c0, 0x20);
		pci_bus_res[bus].io_size += 0x20;

		/* Video memory */
		regs[nreg].pci_phys_hi = assigned[nasgn].pci_phys_hi =
		    (PCI_RELOCAT_B | PCI_ALIAS_B | PCI_ADDR_MEM32 | devloc);
		regs[nreg].pci_phys_low =
		    assigned[nasgn].pci_phys_low = 0xa0000;
		regs[nreg].pci_size_low =
		    assigned[nasgn].pci_size_low = 0x20000;
		nreg++, nasgn++;
		/* remove from MEM and PMEM space */
		(void) pci_memlist_remove(mem_avail, 0xa0000, 0x20000);
		(void) pci_memlist_remove(pmem_avail, 0xa0000, 0x20000);
		pci_memlist_insert(mem_used, 0xa0000, 0x20000);
		pci_bus_res[bus].mem_size += 0x20000;
	}

	/* add the hard-decode, aliased address spaces for 8514 */
	if ((baseclass == PCI_CLASS_DISPLAY) &&
	    (subclass == PCI_DISPLAY_VGA) &&
	    (progclass & PCI_DISPLAY_IF_8514)) {

		/* hard decode 0x2e8 */
		regs[nreg].pci_phys_hi = assigned[nasgn].pci_phys_hi =
		    (PCI_RELOCAT_B | PCI_ALIAS_B | PCI_ADDR_IO | devloc);
		regs[nreg].pci_phys_low = assigned[nasgn].pci_phys_low = 0x2e8;
		regs[nreg].pci_size_low = assigned[nasgn].pci_size_low = 0x1;
		nreg++, nasgn++;
		(void) pci_memlist_remove(io_avail, 0x2e8, 0x1);
		pci_memlist_insert(io_used, 0x2e8, 0x1);
		pci_bus_res[bus].io_size += 0x1;

		/* hard decode 0x2ea-0x2ef */
		regs[nreg].pci_phys_hi = assigned[nasgn].pci_phys_hi =
		    (PCI_RELOCAT_B | PCI_ALIAS_B | PCI_ADDR_IO | devloc);
		regs[nreg].pci_phys_low = assigned[nasgn].pci_phys_low = 0x2ea;
		regs[nreg].pci_size_low = assigned[nasgn].pci_size_low = 0x6;
		nreg++, nasgn++;
		(void) pci_memlist_remove(io_avail, 0x2ea, 0x6);
		pci_memlist_insert(io_used, 0x2ea, 0x6);
		pci_bus_res[bus].io_size += 0x6;
	}

done:
	dump_memlists(pci_bus_res, "add_reg_props end", bus);

	(void) ndi_prop_update_int_array(DDI_DEV_T_NONE, dip, OBP_REG,
	    (int *)regs, nreg * sizeof (pci_regspec_t) / sizeof (int));
	(void) ndi_prop_update_int_array(DDI_DEV_T_NONE, dip,
	    "assigned-addresses",
	    (int *)assigned, nasgn * sizeof (pci_regspec_t) / sizeof (int));
}

static void
add_ppb_props(dev_info_t *rcdip, dev_info_t *dip,
    struct pci_bus_resource *pci_bus_res, uchar_t bus, uchar_t dev,
    uchar_t func, boolean_t pciex, boolean_t is_pci_bridge)
{
	char *dev_type;
	int i;
	uint_t cmd_reg;
	struct {
		uint64_t base;
		uint64_t limit;
	} io, mem, pmem;
	uchar_t secbus, subbus;
	uchar_t progclass;

	secbus = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_BCNF_SECBUS);
	subbus = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_BCNF_SUBBUS);

	ASSERT3U(secbus, <=, subbus);
	ASSERT3P(pci_bus_res[secbus].dip, ==, NULL);
	pci_bus_res[secbus].dip = dip;
	pci_bus_res[secbus].par_bus = bus;

	dump_memlists(pci_bus_res, "add_ppb_props start bus", bus);
	dump_memlists(pci_bus_res, "add_ppb_props start secbus", secbus);

	/*
	 * Check if it's a subtractive PPB.
	 */
	progclass = pci_cfgacc_get8(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_CONF_PROGCLASS);
	if (progclass == PCI_BRIDGE_PCI_IF_SUBDECODE)
		pci_bus_res[secbus].subtractive = B_TRUE;

	dev_type = (pciex && !is_pci_bridge) ? "pciex" : "pci";

	/* set up bus number hierarchy */
	pci_bus_res[secbus].sub_bus = subbus;
	/*
	 * Keep track of the largest subordinate bus number (this is essential
	 * for peer buses because there is no other way of determining its
	 * subordinate bus number).
	 */
	if (subbus > pci_bus_res[bus].sub_bus)
		pci_bus_res[bus].sub_bus = subbus;
	/*
	 * Loop through subordinate buses, initializing their parent bus
	 * field to this bridge's parent.  The subordinate buses' parent
	 * fields may very well be further refined later, as child bridges
	 * are enumerated.  (The value is to note that the subordinate buses
	 * are not peer buses by changing their par_bus fields to anything
	 * other than -1.)
	 */
	for (i = secbus + 1; i <= subbus; i++)
		pci_bus_res[i].par_bus = bus;

	/*
	 * Update the number of bridges on the bus.
	 */
	if (!is_pci_bridge)
		pci_bus_res[bus].num_bridge++;

	(void) ndi_prop_update_string(DDI_DEV_T_NONE, dip,
	    OBP_DEVICETYPE, dev_type);
	(void) ndi_prop_update_int(DDI_DEV_T_NONE, dip,
	    OBP_ADDRESS_CELLS, 3);
	(void) ndi_prop_update_int(DDI_DEV_T_NONE, dip,
	    OBP_SIZE_CELLS, 2);

	/*
	 * Collect bridge window specifications, and use them to populate
	 * the "avail" resources for the bus.  Not all of those resources will
	 * end up being available; this is done top-down, and so the initial
	 * collection of windows populates the 'ranges' property for the
	 * bus node.  Later, as children are found, resources are removed from
	 * the 'avail' list, so that it becomes the freelist for
	 * this point in the tree.  ranges may be set again after bridge
	 * reprogramming in fix_ppb_res(), in which case it's set from
	 * used + avail.
	 *
	 * According to PPB spec, the base register should be programmed
	 * with a value bigger than the limit register when there are
	 * no resources available. This applies to io, memory, and
	 * prefetchable memory.
	 */

	cmd_reg = pci_cfgacc_get16(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_CONF_COMM);
	fetch_ppb_res(rcdip, bus, dev, func, RES_IO, &io.base, &io.limit);
	fetch_ppb_res(rcdip, bus, dev, func, RES_MEM, &mem.base, &mem.limit);
	fetch_ppb_res(rcdip, bus, dev, func, RES_PMEM, &pmem.base, &pmem.limit);

	if (pci_boot_debug != 0) {
		dcmn_err(CE_NOTE, MSGHDR " I/O FWINIT 0x%lx ~ 0x%lx%s",
		    "ppb", bus, dev, func, io.base, io.limit,
		    io.base > io.limit ? " (disabled)" : "");
		dcmn_err(CE_NOTE, MSGHDR " MEM FWINIT 0x%lx ~ 0x%lx%s",
		    "ppb", bus, dev, func, mem.base, mem.limit,
		    mem.base > mem.limit ? " (disabled)" : "");
		dcmn_err(CE_NOTE, MSGHDR "PMEM FWINIT 0x%lx ~ 0x%lx%s",
		    "ppb", bus, dev, func, pmem.base, pmem.limit,
		    pmem.base > pmem.limit ? " (disabled)" : "");
	}

	/*
	 * I/O range
	 *
	 * If the command register I/O enable bit is not set then we assume
	 * that the I/O windows have been left unconfigured by system firmware.
	 * In that case we leave it disabled and additionally set base > limit
	 * to indicate there are there are no initial resources available and
	 * to trigger later reconfiguration.
	 */
	if ((cmd_reg & PCI_COMM_IO) == 0) {
		io.base = PPB_DISABLE_IORANGE_BASE;
		io.limit = PPB_DISABLE_IORANGE_LIMIT;
		set_ppb_res(rcdip, bus, dev, func, RES_IO, io.base, io.limit);
	} else if (io.base < io.limit) {
		uint64_t size = io.limit - io.base + 1;

		pci_memlist_insert(&pci_bus_res[secbus].io_avail, io.base,
		    size);
		pci_memlist_insert(&pci_bus_res[bus].io_used, io.base, size);

		if (pci_bus_res[bus].io_avail != NULL) {
			(void) pci_memlist_remove(&pci_bus_res[bus].io_avail,
			    io.base, size);
		}
	}

	/*
	 * Memory range
	 *
	 * It is possible that the mem range will also have been left
	 * unconfigured by system firmware. As for the I/O range, we check for
	 * this by looking at the relevant bit in the command register (Memory
	 * Access Enable in this case) but we also check if the base address is
	 * 0, indicating that it is still at PCIe defaults. While 0 technically
	 * could be a valid base address, it is unlikely.
	 */
	if ((cmd_reg & PCI_COMM_MAE) == 0 || mem.base == 0) {
		mem.base = PPB_DISABLE_MEMRANGE_BASE;
		mem.limit = PPB_DISABLE_MEMRANGE_LIMIT;
		set_ppb_res(rcdip, bus, dev, func, RES_MEM, mem.base,
		    mem.limit);
	} else if (mem.base < mem.limit) {
		uint64_t size = mem.limit - mem.base + 1;

		pci_memlist_insert(&pci_bus_res[secbus].mem_avail, mem.base,
		    size);
		pci_memlist_insert(&pci_bus_res[bus].mem_used, mem.base, size);
		/* remove from parent resource list */
		(void) pci_memlist_remove(&pci_bus_res[bus].mem_avail,
		    mem.base, size);
		(void) pci_memlist_remove(&pci_bus_res[bus].pmem_avail,
		    mem.base, size);
	}

	/*
	 * Prefetchable range - as per MEM range above.
	 */
	if ((cmd_reg & PCI_COMM_MAE) == 0 || pmem.base == 0) {
		pmem.base = PPB_DISABLE_MEMRANGE_BASE;
		pmem.limit = PPB_DISABLE_MEMRANGE_LIMIT;
		set_ppb_res(rcdip, bus, dev, func, RES_PMEM, pmem.base,
		    pmem.limit);
	} else if (pmem.base < pmem.limit) {
		uint64_t size = pmem.limit - pmem.base + 1;

		pci_memlist_insert(&pci_bus_res[secbus].pmem_avail,
		    pmem.base, size);
		pci_memlist_insert(&pci_bus_res[bus].pmem_used, pmem.base,
		    size);
		/* remove from parent resource list */
		(void) pci_memlist_remove(&pci_bus_res[bus].pmem_avail,
		    pmem.base, size);
		(void) pci_memlist_remove(&pci_bus_res[bus].mem_avail,
		    pmem.base, size);
	}

	/*
	 * Add VGA legacy resources to the bridge's pci_bus_res if it
	 * has VGA_ENABLE set.  Note that we put them in 'avail',
	 * because that's used to populate the ranges prop; they'll be
	 * removed from there by the VGA device once it's found.  Also,
	 * remove them from the parent's available list and note them as
	 * used in the parent.
	 */

	if (pci_cfgacc_get16(rcdip, PCI_GETBDF(bus, dev, func),
	    PCI_BCNF_BCNTRL) & PCI_BCNF_BCNTRL_VGA_ENABLE) {

		pci_memlist_insert(&pci_bus_res[secbus].io_avail, 0x3b0, 0xc);

		pci_memlist_insert(&pci_bus_res[bus].io_used, 0x3b0, 0xc);
		if (pci_bus_res[bus].io_avail != NULL) {
			(void) pci_memlist_remove(&pci_bus_res[bus].io_avail,
			    0x3b0, 0xc);
		}

		pci_memlist_insert(&pci_bus_res[secbus].io_avail, 0x3c0, 0x20);

		pci_memlist_insert(&pci_bus_res[bus].io_used, 0x3c0, 0x20);
		if (pci_bus_res[bus].io_avail != NULL) {
			(void) pci_memlist_remove(&pci_bus_res[bus].io_avail,
			    0x3c0, 0x20);
		}

		pci_memlist_insert(&pci_bus_res[secbus].mem_avail, 0xa0000,
		    0x20000);

		pci_memlist_insert(&pci_bus_res[bus].mem_used, 0xa0000,
		    0x20000);
		if (pci_bus_res[bus].mem_avail != NULL) {
			(void) pci_memlist_remove(&pci_bus_res[bus].mem_avail,
			    0xa0000, 0x20000);
		}
	}
	add_bus_range_prop(pci_bus_res, secbus);
	add_ranges_prop(pci_bus_res, secbus, B_TRUE);

	dump_memlists(pci_bus_res, "add_ppb_props end bus", bus);
	dump_memlists(pci_bus_res, "add_ppb_props end secbus", secbus);
}

/*
 *
 * Insert the "bus-range" property, indicating the buses this node is
 * responsible for.
 */
static void
add_bus_range_prop(struct pci_bus_resource *pci_bus_res, int bus)
{
	int bus_range[2];

	if (pci_bus_res[bus].dip == NULL)
		return;
	bus_range[0] = bus;
	bus_range[1] = pci_bus_res[bus].sub_bus;
	(void) ndi_prop_update_int_array(DDI_DEV_T_NONE, pci_bus_res[bus].dip,
	    "bus-range", (int *)bus_range, 2);
}

/*
 * Handle both PCI root and PCI-PCI bridge range properties;
 * the 'ppb' argument selects PCI-PCI bridges versus root.
 */
static void
memlist_to_ranges(void **rp, struct memlist *list, const int bus,
    const uint32_t type, boolean_t ppb)
{
	ppb_ranges_t *ppb_rp = *rp;
	pci_ranges_t *pci_rp = *rp;

	while (list != NULL) {
		uint32_t newtype = type;

		/*
		 * If this is in fact a 64-bit address, adjust the address
		 * type code to match.
		 */
		if (list->ml_address + (list->ml_size - 1) > UINT32_MAX) {
			if ((type & PCI_ADDR_MASK) == PCI_ADDR_IO) {
				cmn_err(CE_WARN, "Found invalid 64-bit I/O "
				    "space address 0x%lx+0x%lx on bus %x",
				    list->ml_address, list->ml_size, bus);
				list = list->ml_next;
				continue;
			}
			newtype &= ~PCI_ADDR_MASK;
			newtype |= PCI_ADDR_MEM64;
		}

		if (ppb) {
			ppb_rp->child_high = ppb_rp->parent_high = newtype;
			ppb_rp->child_mid = ppb_rp->parent_mid =
			    (uint32_t)(list->ml_address >> 32);
			ppb_rp->child_low = ppb_rp->parent_low =
			    (uint32_t)list->ml_address;
			ppb_rp->size_high = (uint32_t)(list->ml_size >> 32);
			ppb_rp->size_low = (uint32_t)list->ml_size;
			*rp = ++ppb_rp;
		} else {
			pci_rp->child_high = newtype;
			pci_rp->child_mid = pci_rp->parent_high =
			    (uint32_t)(list->ml_address >> 32);
			pci_rp->child_low = pci_rp->parent_low =
			    (uint32_t)list->ml_address;
			pci_rp->size_high = (uint32_t)(list->ml_size >> 32);
			pci_rp->size_low = (uint32_t)list->ml_size;
			*rp = ++pci_rp;
		}
		list = list->ml_next;
	}
}

static void
add_ranges_prop(struct pci_bus_resource *pci_bus_res, int bus, boolean_t ppb)
{
	int total, alloc_size;
	void	*rp, *next_rp;
	struct memlist *iolist, *memlist, *pmemlist;

	/* no devinfo node - unused bus, return */
	if (pci_bus_res[bus].dip == NULL)
		return;

	dump_memlists(pci_bus_res, "add_ranges_prop", bus);

	iolist = memlist = pmemlist = (struct memlist *)NULL;

	pci_memlist_merge(&pci_bus_res[bus].io_avail, &iolist);
	pci_memlist_merge(&pci_bus_res[bus].io_used, &iolist);
	pci_memlist_merge(&pci_bus_res[bus].mem_avail, &memlist);
	pci_memlist_merge(&pci_bus_res[bus].mem_used, &memlist);
	pci_memlist_merge(&pci_bus_res[bus].pmem_avail, &pmemlist);
	pci_memlist_merge(&pci_bus_res[bus].pmem_used, &pmemlist);

	total = pci_memlist_count(iolist);
	total += pci_memlist_count(memlist);
	total += pci_memlist_count(pmemlist);

	/* no property is created if no ranges are present */
	if (total == 0)
		return;

	alloc_size = total *
	    (ppb ? sizeof (ppb_ranges_t) : sizeof (pci_ranges_t));

	next_rp = rp = kmem_alloc(alloc_size, KM_SLEEP);

	memlist_to_ranges(&next_rp, iolist, bus,
	    PCI_ADDR_IO | PCI_RELOCAT_B, ppb);
	memlist_to_ranges(&next_rp, memlist, bus,
	    PCI_ADDR_MEM32 | PCI_RELOCAT_B, ppb);
	memlist_to_ranges(&next_rp, pmemlist, bus,
	    PCI_ADDR_MEM32 | PCI_RELOCAT_B | PCI_PREFETCH_B, ppb);

	(void) ndi_prop_update_int_array(DDI_DEV_T_NONE, pci_bus_res[bus].dip,
	    OBP_RANGES, (int *)rp, alloc_size / sizeof (int));

	kmem_free(rp, alloc_size);
	pci_memlist_free_all(&iolist);
	pci_memlist_free_all(&memlist);
	pci_memlist_free_all(&pmemlist);
}

static void
alloc_res_array(struct pci_bus_resource **pci_bus_res)
{
	uint_t array_size = 0;
	uint_t old_size;
	void *old_res;

	if (array_size > pci_boot_maxbus + 1)
		return;	/* array is big enough */

	old_size = array_size;
	old_res = *pci_bus_res;

	if (array_size == 0)
		array_size = 16;	/* start with a reasonable number */

	while (array_size <= pci_boot_maxbus + 1)
		array_size <<= 1;
	*pci_bus_res = kmem_zalloc(
	    array_size * sizeof (struct pci_bus_resource), KM_SLEEP);

	if (old_res) {	/* copy content and free old array */
		bcopy(old_res, *pci_bus_res,
		    old_size * sizeof (struct pci_bus_resource));
		kmem_free(old_res, old_size * sizeof (struct pci_bus_resource));
	}
}
