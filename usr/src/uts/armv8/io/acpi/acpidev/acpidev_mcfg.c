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
 * Basic bootstrap of non-hotplug PCIe host bridges from the ACPI MCFG table,
 * with cross-reference to the DSDT.
 *
 * One thorny thing to keep an eye on is that in the MCFG_ALLOCATION the
 * end bus number is inclusive (it's a uint8_t), ending at 255. For objects
 * found in the DSDT the bus range is exclusive, so the end bus number is
 * 256 for all busses.
 */

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_subrdefs.h>
#include <sys/ddi_impldefs.h>
#include <sys/stdbool.h>
#include <sys/obpdefs.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/acpidev.h>
#include <sys/acpidev_gic.h>
#include <sys/acpidev_iort.h>
#include <sys/acpidev_rsc.h>
#include <sys/pci.h>
#include <sys/queue.h>

#define	SIZE_FOR_NUM_BUSSES(n)	((n) * (32lu * (8lu * 4096lu)))

typedef struct {
	ACPI_HANDLE			hdl;
	char				*objname;
	int				sta;
	acpidev_resource_handle_t	rhdl;
	struct regspec			*rs;
	uint_t				num_rs;
	size_t				rs_size;

	uint32_t			interrupt_cells;
	uint32_t			*interrupt_map_cells;
	uint_t				num_interrupt_map_cells;
	size_t				interrupt_map_cells_size;

	uint32_t			*interrupt_mask_cells;
	uint_t				num_interrupt_mask_cells;
	size_t				interrupt_mask_cells_size;

	struct rangespec		*ranges;
	uint_t				num_ranges;
	size_t				ranges_size;

	bool				is_pcie;

	int				segno;
	int				busno;
} acpi_namespace_root_complex_t;

static SLIST_HEAD(acpi_root_complexes_head, acpi_root_complex)
    acpi_root_complexes = SLIST_HEAD_INITIALIZER(acpi_root_complexes);

typedef struct acpi_root_complex {
	SLIST_ENTRY(acpi_root_complex)	entries;
	ACPI_MCFG_ALLOCATION		*mcfg;
	acpi_namespace_root_complex_t	*anrc;
	struct regspec			*rs;
	uint_t				num_rs;
	size_t				rs_size;
} acpi_root_complex_t;


static void
free_acpi_namespace_root_complex(acpi_namespace_root_complex_t *anrc)
{
	if (anrc == NULL) {
		return;
	}

	if (anrc->ranges != NULL && anrc->ranges_size != 0) {
		kmem_free(anrc->ranges, anrc->ranges_size);
	}

	if (anrc->interrupt_mask_cells != NULL &&
	    anrc->interrupt_mask_cells_size != 0) {
		kmem_free(anrc->interrupt_mask_cells,
		    anrc->interrupt_mask_cells_size);
	}

	if (anrc->interrupt_map_cells != NULL &&
	    anrc->interrupt_map_cells_size != 0) {
		kmem_free(anrc->interrupt_map_cells,
		    anrc->interrupt_map_cells_size);
	}

	if (anrc->rs != NULL && anrc->rs_size != 0) {
		kmem_free(anrc->rs, anrc->rs_size);
	}

	if (anrc->rhdl != NULL) {
		acpidev_resource_handle_free(anrc->rhdl);
	}

	if (anrc->objname != NULL) {
		acpidev_free_object_name(anrc->objname);
	}

	kmem_free(anrc, sizeof (acpi_namespace_root_complex_t));
}

static ACPI_STATUS
acpidev_pci_link_to_gic(ACPI_HANDLE hdl, ACPI_PCI_ROUTING_TABLE *prt,
    struct acpidev_gic_interrupt *gi)
{
	ACPI_HANDLE lnkh;
	ACPI_BUFFER cb;
	ACPI_RESOURCE *crp;
	ACPI_STATUS st;
	int lnk_status;

	ASSERT3P(prt, !=, NULL);
	ASSERT3P(gi, !=, NULL);

	gi->gi_gsiv = 0xffffffff;
	gi->gi_flags = 0x0;

	if (ACPI_FAILURE(st = AcpiGetHandle(hdl, prt->Source, &lnkh))) {
		cmn_err(CE_WARN, "?acpidev: failed to get PCI "
		    "Interrupt Link object for '%s'", prt->Source);
		return (st);
	}

	/*
	 * Check _STA on the PCI Interrupt Link device.  If the link is
	 * not present/enabled, skip it - the GSIV from _CRS would be
	 * meaningless.
	 */
	lnk_status = acpidev_query_device_status(lnkh);
	if (!acpidev_check_device_enabled(lnk_status)) {
		cmn_err(CE_WARN, "?acpidev: PCI Interrupt Link "
		    "'%s' is not enabled (status 0x%x)",
		    prt->Source, lnk_status);
		return (AE_NOT_FOUND);
	}

	cb.Pointer = NULL;
	cb.Length = ACPI_ALLOCATE_BUFFER;
	if (ACPI_FAILURE(st =
	    AcpiGetCurrentResources(lnkh, &cb))) {
		cmn_err(CE_WARN, "?acpidev: failed to get PCI "
		    "Interrupt Link object current resources for '%s'",
		    prt->Source);
		return (st);
	}

	for (crp = cb.Pointer;
	    crp->Type != ACPI_RESOURCE_TYPE_END_TAG;
	    crp = ACPI_NEXT_RESOURCE(crp)) {
		if (crp->Type == ACPI_RESOURCE_TYPE_IRQ) {
			if (gi->gi_gsiv != 0xffffffff) {
				cmn_err(CE_WARN, "acpidev: multiple IRQs "
				    "from %s._CRS", prt->Source);
				AcpiOsFree(cb.Pointer);
				return (AE_ERROR);
			}

			if (crp->Data.Irq.InterruptCount != 1) {
				cmn_err(CE_WARN, "acpidev: <>1 interrupt "
				    "from %s._CRS", prt->Source);
				AcpiOsFree(cb.Pointer);
				return (AE_ERROR);
			}

			gi->gi_gsiv = crp->Data.Irq.Interrupts[0];

			if (crp->Data.Irq.Triggering == ACPI_EDGE_SENSITIVE) {
				gi->gi_flags |= AGIF_EDGE;
			} else {
				gi->gi_flags |= AGIF_LEVEL;
			}

			if (crp->Data.Irq.Polarity == ACPI_ACTIVE_HIGH) {
				gi->gi_flags |= AGIF_ACTIVE_HIGH;
			} else {
				gi->gi_flags |= AGIF_ACTIVE_LOW;
			}
		} else if (crp->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ) {
			if (gi->gi_gsiv != 0xffffffff) {
				cmn_err(CE_WARN, "acpidev: multiple IRQs "
				    "from %s._CRS", prt->Source);
				AcpiOsFree(cb.Pointer);
				return (AE_ERROR);
			}

			if (crp->Data.ExtendedIrq.InterruptCount != 1) {
				cmn_err(CE_WARN, "acpidev: <>1 interrupt "
				    "from %s._CRS", prt->Source);
				AcpiOsFree(cb.Pointer);
				return (AE_ERROR);
			}

			gi->gi_gsiv = crp->Data.ExtendedIrq.Interrupts[0];

			if (crp->Data.ExtendedIrq.Triggering ==
			    ACPI_EDGE_SENSITIVE) {
				gi->gi_flags |= AGIF_EDGE;
			} else {
				gi->gi_flags |= AGIF_LEVEL;
			}

			if (crp->Data.ExtendedIrq.Polarity ==
			    ACPI_ACTIVE_HIGH) {
				gi->gi_flags |= AGIF_ACTIVE_HIGH;
			} else {
				gi->gi_flags |= AGIF_ACTIVE_LOW;
			}
		}
	}

	AcpiOsFree(cb.Pointer);

	if (gi->gi_gsiv == 0xffffffff) {
		cmn_err(CE_WARN, "?acpidev: no interrupt found "
		    "in PCI Interrupt Link _CRS for %s", prt->Source);
		return (AE_ERROR);
	}

	return (AE_OK);
}

#define	NEXT_PRT_ITEM(p)	(void *)(((char *)(p)) + (p)->Length)

typedef struct {
	uint32_t bdf;
	uint32_t pin;
	struct acpidev_gic_interrupt gi;
} pci_irq_routing_map_entry_t;

static ACPI_STATUS
acpidev_pci_get_interrupt_map(ACPI_HANDLE hdl,
    acpi_namespace_root_complex_t *anrc, dev_info_t *gdip)
{
	ACPI_PCI_ROUTING_TABLE *prtp;
	ACPI_BUFFER rb;
	ACPI_STATUS st;
	pci_irq_routing_map_entry_t *entries;
	size_t entries_size;
	uint32_t nentries;
	uint32_t eidx;
	uint32_t mindev;
	uint32_t maxdev;

	rb.Pointer = NULL;
	rb.Length = ACPI_ALLOCATE_BUFFER;
	st = AcpiGetIrqRoutingTable(hdl, &rb);
	if (st == AE_NOT_FOUND) {
		return (AE_OK);
	}
	if (ACPI_FAILURE(st)) {
		cmn_err(CE_WARN,
		    "acpidev: AcpiGetIrqRoutingTable failed: 0x%x", st);
		return (st);
	}

	nentries = 0;
	for (prtp = rb.Pointer; prtp->Length != 0; prtp = NEXT_PRT_ITEM(prtp)) {
		nentries++;
	}

	if (nentries == 0) {
		AcpiOsFree(rb.Pointer);
		return (AE_OK);
	}

	entries_size = sizeof (pci_irq_routing_map_entry_t) * nentries;
	entries = kmem_zalloc(entries_size, KM_SLEEP);
	ASSERT3P(entries, !=, NULL);
	eidx = 0;
	mindev = UINT32_MAX;
	maxdev = 0;

	for (prtp = rb.Pointer; prtp->Length != 0; prtp = NEXT_PRT_ITEM(prtp)) {
		uint32_t devno;
		pci_irq_routing_map_entry_t *entry = &entries[eidx];
		entry->gi.gi_gsiv = 0xffffffff;
		entry->gi.gi_flags = 0x0;
		/*
		 * The _PRT Address uses the _ADR format: the function
		 * number (low 16 bits) must be 0xFFFF ("any function")
		 * and the device number sits in bits [20:16] with
		 * nothing above (ACPI 6.6, §6.2.14).  A malformed
		 * Address means the firmware's routing table is
		 * untrustworthy.
		 */
		if ((prtp->Address & 0xffff) != 0xffff ||
		    (prtp->Address & 0x1f0000) !=
		    (prtp->Address & 0xffff0000)) {
			cmn_err(CE_WARN, "acpidev: malformed _PRT Address "
			    "0x%lx", prtp->Address);
			kmem_free(entries, entries_size);
			AcpiOsFree(rb.Pointer);
			return (AE_BAD_DATA);
		}
		devno = (prtp->Address >> 16) & 0x1f;
		if (devno < mindev) {
			mindev = devno;
		}
		if (devno > maxdev) {
			maxdev = devno;
		}
		entry->bdf = devno << 11;
		/*
		 * The _PRT Pin is the PCI pin number, 0-3 (INTA-INTD,
		 * ACPI 6.6 §6.2.14).  A Pin outside that range means
		 * the firmware's routing table is untrustworthy.
		 */
		if (prtp->Pin > 3) {
			cmn_err(CE_WARN, "acpidev: malformed _PRT Pin "
			    "%u", prtp->Pin);
			kmem_free(entries, entries_size);
			AcpiOsFree(rb.Pointer);
			return (AE_BAD_DATA);
		}
		entry->pin = prtp->Pin + 1;	/* pin is 0-based */

		/*
		 * NULL Source name means index is GSIV
		 */
		if (*prtp->Source != 0) {
			if (ACPI_FAILURE(st = acpidev_pci_link_to_gic(
			    hdl, prtp, &entry->gi))) {
				cmn_err(CE_WARN, "acpidev: "
				    "acpidev_pci_link_to_gic failed: %d", st);
				kmem_free(entries, entries_size);
				AcpiOsFree(rb.Pointer);
				return (st);
			}
		} else {
			entry->gi.gi_gsiv = prtp->SourceIndex;
			/*
			 * When Source is NULL, SourceIndex is a direct
			 * GSIV.  Per ACPI spec, these are global system
			 * interrupts and SPIs are level-triggered,
			 * active-high by default.
			 */
			entry->gi.gi_flags =
			    AGIF_LEVEL|AGIF_ACTIVE_HIGH;
		}

		if (++eidx >= nentries) {
			break;
		}
	}

	AcpiOsFree(rb.Pointer);

	/*
	 * Qemu sbsa-ref constructs bad mappings for device 0, but
	 * that emulation is glacially slow and can't complete a boot,
	 * so we don't attempt to work around the problem here.
	 */

	ASSERT3U(mindev, <=, maxdev);
	ASSERT3U(maxdev, <=, 0x1f);


	/*
	 * Compute interrupt-map stride from the standard cell counts:
	 *
	 *   child unit address    (#address-cells from this PCI node = 3)
	 *   child unit interrupt  (#interrupt-cells from this node = 1)
	 *   interrupt parent      (phandle = 1 cell)
	 *   parent unit address   (#address-cells from GIC; 0 when the
	 *                          GIC has no children, e.g. no ITS/v2m)
	 *   parent unit interrupt (#interrupt-cells from GIC = 3)
	 *
	 * Interrupt controllers that are not buses have zero parent
	 * address cells in interrupt-map entries and legitimately omit
	 * #address-cells from their node.  The IEEE 1275 default of 2
	 * is wrong here - default to 0 for interrupt controllers,
	 * matching the consumer in map_interrupt_map().
	 */
	uint32_t pci_addr_cells = 3;	/* PCI OF binding: always 3 */
	uint32_t pci_intr_cells = 1;	/* PCI OF binding: always 1 */
	uint32_t gic_addr_default = ddi_prop_exists(DDI_DEV_T_ANY, gdip,
	    DDI_PROP_DONTPASS, OBP_INTERRUPT_CONTROLLER) ? 0 : 2;
	uint32_t gic_addr_cells = ddi_prop_get_int(DDI_DEV_T_ANY, gdip,
	    DDI_PROP_DONTPASS, OBP_ADDRESS_CELLS, gic_addr_default);
	uint32_t gic_intr_cells = acpidev_get_gic_interrupt_cells();
	size_t map_stride = pci_addr_cells + pci_intr_cells +
	    1 /* phandle */ + gic_addr_cells + gic_intr_cells;
	size_t nmap_cells = map_stride * nentries;
	size_t map_sz = sizeof (uint32_t) * nmap_cells;
	uint32_t *map_cells = kmem_zalloc(map_sz, KM_SLEEP);

	for (eidx = 0; eidx < nentries; ++eidx) {
		uint32_t *cells = &map_cells[map_stride * eidx];
		uint32_t off = 0;

		/* Child unit address (PCI: phys hi, mid, lo) */
		cells[off++] = entries[eidx].bdf;
		cells[off++] = 0;
		cells[off++] = 0;

		/* Child unit interrupt (PCI: pin) */
		cells[off++] = entries[eidx].pin;

		/* Interrupt parent phandle */
		cells[off++] = ddi_get_nodeid(gdip);

		/*
		 * Parent unit address
		 *
		 * GIC #address-cells worth of zeroes.
		 */
		for (uint32_t a = 0; a < gic_addr_cells; a++) {
			cells[off++] = 0;
		}

		/* Parent unit interrupt (GIC #interrupt-cells) */
		st = acpidev_serialize_interrupt(&entries[eidx].gi,
		    &cells[off]);
		if (ACPI_FAILURE(st)) {
			cmn_err(CE_WARN, "acpidev: "
			    "acpidev_serialize_interrupt failed: %d", st);
			kmem_free(map_cells, map_sz);
			kmem_free(entries, entries_size);
			return (st);
		}
	}

	kmem_free(entries, entries_size);
	entries = NULL;
	entries_size = 0;

	/*
	 * dma-coherent and MSI properties are set from IORT
	 * in create_root_complex().
	 */

	uint32_t nmcells = 4;
	size_t mcells_size = sizeof (uint32_t) * nmcells;
	uint32_t *mcells = kmem_zalloc(mcells_size, KM_SLEEP);
	/*
	 * Build the interrupt-map-mask device-number field by ORing
	 * every device number in [mindev, maxdev].  This captures
	 * exactly the bits used by the actual PRT device range.
	 *
	 * When mindev is 0 the resulting mask has no bits set for the
	 * device field.  This is a dtspec deficiency, not a bug:
	 * interrupt-map-mask matching compares (child & mask) against
	 * (entry & mask), and 0 & mask is always 0, so device 0
	 * cannot be excluded from matching any entry whose masked
	 * value is also 0.  No mask value fixes this - it is inherent
	 * to mask-and-compare when the key is zero.
	 */
	uint32_t devmask = 0;
	for (uint32_t d = mindev; d <= maxdev; d++) {
		devmask |= d;
	}
	mcells[0] = (devmask & 0x1f) << 11;
	mcells[1] = 0;
	mcells[2] = 0;
	mcells[3] = 0x7;

	anrc->interrupt_cells = 1;

	anrc->interrupt_map_cells = map_cells;
	anrc->num_interrupt_map_cells = nmap_cells;
	anrc->interrupt_map_cells_size = map_sz;

	anrc->interrupt_mask_cells = mcells;
	anrc->num_interrupt_mask_cells = nmcells;
	anrc->interrupt_mask_cells_size = mcells_size;

	return (AE_OK);
}

static ACPI_STATUS
acpidev_pci_get_ranges(acpi_namespace_root_complex_t *nsdev)
{
	acpidev_resource_handle_t	rhdl;
	struct rangespec		*ranges;
	uint_t				num_ranges;
	size_t				ranges_size;
	uint_t				x;

	if (nsdev->rhdl->acpidev_range_count <= 0) {
		cmn_err(CE_CONT, "?acpidev: no ranges?\n");
		return (AE_ERROR);
	}

	rhdl = nsdev->rhdl;
	ASSERT3P(rhdl, !=, NULL);

	num_ranges = nsdev->rhdl->acpidev_range_count;
	ranges_size = sizeof (struct rangespec) * num_ranges;
	ranges = kmem_zalloc(ranges_size, KM_SLEEP);
	ASSERT3P(ranges, !=, NULL);

	for (x = 0; x < num_ranges; ++x) {
		const acpidev_ranges_t *rngp = &nsdev->rhdl->acpidev_ranges[x];
		struct rangespec *r = &ranges[x];

		if ((rngp->child_hi & ACPIDEV_REG_TYPE_M) ==
		    ACPIDEV_REG_TYPE_MEMORY) {
			r->rng_cbustype =
			    (rngp->child_hi & ACPIDEV_REG_MEM_64BIT) ?
			    PCI_ADDR_MEM64 : PCI_ADDR_MEM32;

			if ((rngp->child_hi & ACPIDEV_REG_MEM_COHERENT_M) ==
			    ACPIDEV_REG_MEM_COHERENT_PF) {
				r->rng_cbustype |= (PCI_PREFETCH_B);
			}

			/*
			 * Relocatable addresses? (n-bit)
			 */
		} else if ((rngp->child_hi & ACPIDEV_REG_TYPE_M) ==
		    ACPIDEV_REG_TYPE_IO) {
			r->rng_cbustype = PCI_ADDR_IO;
		} else {
			cmn_err(CE_WARN, "acpidev: unknown address type %x",
			    rngp->child_hi & ACPIDEV_REG_TYPE_M);
			kmem_free(ranges, ranges_size);
			return (AE_ERROR);
		}

		r->rng_coffset =
		    (((uint64_t)rngp->child_mid) << 32) | rngp->child_low;

		if ((rngp->parent_hi & ACPIDEV_REG_TYPE_M) ==
		    ACPIDEV_REG_TYPE_MEMORY) {
			r->rng_bustype = 0x0;
		} else if ((rngp->parent_hi & ACPIDEV_REG_TYPE_M) ==
		    ACPIDEV_REG_TYPE_IO) {
			r->rng_bustype = 0x1;
		} else {
			cmn_err(CE_WARN, "acpidev: unknown address type %x",
			    rngp->parent_hi & ACPIDEV_REG_TYPE_M);
			kmem_free(ranges, ranges_size);
			return (AE_ERROR);
		}

		r->rng_offset =
		    (((uint64_t)rngp->parent_mid) << 32) | rngp->parent_low;
		r->rng_size =
		    (((uint64_t)rngp->size_hi) << 32) | rngp->size_low;
	}

	nsdev->ranges = ranges;
	nsdev->num_ranges = num_ranges;
	nsdev->ranges_size = ranges_size;
	return (AE_OK);
}

static ACPI_STATUS
read_namespace_device(ACPI_HANDLE hdl, acpi_namespace_root_complex_t *nsdev,
    dev_info_t *gdip)
{
	ACPI_STATUS st;
	ACPI_BUFFER rb;
	ACPI_OBJECT ro;

	nsdev->hdl = hdl;
	nsdev->objname = acpidev_get_object_name(nsdev->hdl);

	/*
	 * If the device status indicates that it's unusable we just don't
	 * create a device tree node for it.
	 */
	nsdev->sta = acpidev_query_device_status(hdl);
	if (!acpidev_check_device_present(nsdev->sta)) {
		cmn_err(CE_CONT, "?acpidev: _STA %x: device not present\n",
		    nsdev->sta);
		return (AE_ERROR);
	}
	if (!acpidev_check_device_enabled(nsdev->sta)) {
		cmn_err(CE_CONT, "?acpidev: _STA %x: device not enabled\n",
		    nsdev->sta);
		return (AE_ERROR);
	}

	/*
	 * Get the producer resources, which gives us bus-ranges and ranges.
	 */
	st = acpidev_resource_walk(hdl,
	    METHOD_NAME__CRS, B_FALSE, &nsdev->rhdl);
	if (ACPI_FAILURE(st)) {
		cmn_err(CE_CONT,
		    "?acpidev: Failed to walk resources: %d\n", st);
		return (st);
	}
	VERIFY3P(nsdev->rhdl, !=, NULL);
	if (nsdev->rhdl->acpidev_bus_count != 1) {
		cmn_err(CE_CONT, "?acpidev: expected 1 bus range, got %d\n",
		    nsdev->rhdl->acpidev_bus_count);
		return (AE_ERROR);
	}

	/*
	 * bus_end is exclusive (one past the last bus number).  Reject
	 * degenerate ranges from broken firmware before any arithmetic
	 * that would wrap (bus_end - 1 with bus_end == 0).
	 */
	if (nsdev->rhdl->acpidev_buses[0].bus_end == 0 ||
	    nsdev->rhdl->acpidev_buses[0].bus_end <=
	    nsdev->rhdl->acpidev_buses[0].bus_start) {
		cmn_err(CE_CONT, "?acpidev: invalid bus range [%u, %u)\n",
		    nsdev->rhdl->acpidev_buses[0].bus_start,
		    nsdev->rhdl->acpidev_buses[0].bus_end);
		return (AE_ERROR);
	}

	/*
	 * Get the Configuration Base Address. When this is present we combine
	 * it with the bus-range end to create a regspec entry for the
	 * configuration space.
	 */
	rb.Pointer = &ro;
	rb.Length = sizeof (ro);
	st = AcpiEvaluateObjectTyped(hdl, METHOD_NAME__CBA, NULL, &rb,
	    ACPI_TYPE_INTEGER);
	if (ACPI_SUCCESS(st)) {
		nsdev->num_rs = 1;
		nsdev->rs_size = sizeof (struct regspec) * nsdev->num_rs;
		nsdev->rs = kmem_zalloc(nsdev->rs_size, KM_SLEEP);
		ASSERT3P(nsdev->rs, !=, NULL);
		nsdev->rs->regspec_bustype = 0x0;
		nsdev->rs->regspec_addr = ro.Integer.Value;
		nsdev->rs->regspec_size = SIZE_FOR_NUM_BUSSES(
		    nsdev->rhdl->acpidev_buses[0].bus_end);
	}

	/*
	 * Get the segment number, defaulting to 0 when not present.
	 */
	st = acpica_eval_int(hdl, METHOD_NAME__SEG, &nsdev->segno);
	if (ACPI_FAILURE(st)) {
		nsdev->segno = 0;
	}

	/*
	 * Get the declared bus number, defaulting to the start of the
	 * bus-range when not present. Then verify that the declared bus
	 * number matches the data in our bus ranges.
	 */
	st = acpica_get_busno(hdl, &nsdev->busno);
	if (ACPI_FAILURE(st)) {
		nsdev->busno = nsdev->rhdl->acpidev_buses[0].bus_start;
	}
	if (nsdev->busno != nsdev->rhdl->acpidev_buses[0].bus_start) {
		cmn_err(CE_CONT, "?acpidev: _BBN %d does not match bus range "
		    "start %u\n", nsdev->busno,
		    nsdev->rhdl->acpidev_buses[0].bus_start);
		return (AE_ERROR);
	}

	/*
	 * interrupt-map and related bits
	 */
	st = acpidev_pci_get_interrupt_map(hdl, nsdev, gdip);
	if (ACPI_FAILURE(st)) {
		cmn_err(CE_CONT,
		    "?acpidev: Failed to get the interrupt map: %d\n", st);
		return (st);
	}

	st = acpidev_pci_get_ranges(nsdev);
	if (ACPI_FAILURE(st)) {
		cmn_err(CE_CONT, "?acpidev: Failed to get ranges: %d\n", st);
		return (st);
	}

	st = AE_OK;
	return (st);
}

static ACPI_STATUS
process_namespace_device(ACPI_HANDLE hdl, UINT32 level __unused,
    void *ctx, void **rv __unused)
{
	ACPI_DEVICE_INFO *adi;
	ACPI_STATUS st;
	bool is_pcie;
	dev_info_t *gdip;
	acpi_root_complex_t *arc;
	acpi_root_complex_t *last;
	acpi_namespace_root_complex_t *nsdev;

	gdip = ctx;
	ASSERT3P(gdip, !=, NULL);

	if (ACPI_FAILURE(AcpiGetObjectInfo(hdl, &adi))) {
		return (AE_OK);
	}

	if (!(adi->Valid & ACPI_VALID_HID)) {
		AcpiOsFree(adi);
		return (AE_OK);
	}

	if (strncmp(adi->HardwareId.String, PCI_ROOT_HID_STRING,
	    sizeof (PCI_ROOT_HID_STRING)) &&
	    strncmp(adi->HardwareId.String, PCI_EXPRESS_ROOT_HID_STRING,
	    sizeof (PCI_EXPRESS_ROOT_HID_STRING))) {
		AcpiOsFree(adi);
		return (AE_OK);
	}

	is_pcie = strncmp(adi->HardwareId.String, PCI_EXPRESS_ROOT_HID_STRING,
	    sizeof (PCI_EXPRESS_ROOT_HID_STRING)) == 0;

	AcpiOsFree(adi);

	/*
	 * We have a pci or pcie host bridge device
	 */

	nsdev = kmem_zalloc(sizeof (acpi_namespace_root_complex_t), KM_SLEEP);
	ASSERT3P(nsdev, !=, NULL);
	if ((st = read_namespace_device(hdl, nsdev, gdip)) != AE_OK) {
		free_acpi_namespace_root_complex(nsdev);
		cmn_err(CE_WARN,
		    "acpidev: Failed to read ACPI namespace device");
		return (AE_OK);
	}

	nsdev->is_pcie = is_pcie;

	last = NULL;
	boolean_t has_match = B_FALSE;
	SLIST_FOREACH(arc, &acpi_root_complexes, entries) {
		last = arc;

		if (arc->mcfg == NULL || arc->mcfg->Address == 0) {
			continue;
		}

		if (nsdev->rs != NULL &&
		    nsdev->rs->regspec_addr != arc->mcfg->Address) {
			continue;
		}

		/*
		 * We've filtered by Configuration Base Address when present,
		 * now match by segment group and bus number.
		 */

		if (nsdev->segno != arc->mcfg->PciSegment) {
			cmn_err(CE_CONT, "?acpidev: mismatched segment - "
			    "%u != %u\n", nsdev->segno, arc->mcfg->PciSegment);
			continue;
		}

		if (nsdev->rhdl->acpidev_buses[0].bus_start !=
		    arc->mcfg->StartBusNumber ||
		    (nsdev->rhdl->acpidev_buses[0].bus_end - 1) !=
		    arc->mcfg->EndBusNumber) {
			cmn_err(CE_CONT, "?acpidev: mismatched bus range "
			    "- (%u, %u) != (%u, %u)\n",
			    nsdev->rhdl->acpidev_buses[0].bus_start,
			    nsdev->rhdl->acpidev_buses[0].bus_end - 1,
			    arc->mcfg->StartBusNumber,
			    arc->mcfg->EndBusNumber);
			continue;
		}

		/* we have a match */
		has_match = B_TRUE;
		arc->anrc = nsdev;
		break;
	}

	if (has_match) {
		VERIFY3P(arc->anrc, !=, NULL);
		return (AE_OK);
	}

	if (nsdev->rs == NULL || nsdev->rs->regspec_addr == 0) {
		cmn_err(CE_WARN, "acpidev: %s: skipping device node creation "
		    "due to no configuration-space base address for namespace "
		    "device without an MCFG entry.", nsdev->objname);
		free_acpi_namespace_root_complex(nsdev);
		return (AE_OK);
	}

	arc = kmem_zalloc(sizeof (acpi_root_complex_t), KM_SLEEP);
	ASSERT3P(arc, !=, NULL);
	arc->anrc = nsdev;

	if (last == NULL) {
		SLIST_INSERT_HEAD(&acpi_root_complexes, arc, entries);
	} else {
		SLIST_INSERT_AFTER(last, arc, entries);
	}

	return (AE_OK);
}

static int
create_root_complex(acpi_root_complex_t *arc)
{
	int rv;
	dev_info_t *rdip;
	acpidev_iort_rc_t *irc;
	uint32_t bus_range[2];
	char *compatible[] = { "pci-host-ecam-generic" };

	ASSERT3P(arc->anrc, !=, NULL);

	rdip = NULL;
	ndi_devi_alloc_sleep(ddi_root_node(), "pcie",
	    (pnode_t)DEVI_SID_NODEID, &rdip);
	ASSERT3P(rdip, !=, NULL);

	if ((rv = ndi_prop_update_string_array(DDI_DEV_T_NONE, rdip,
	    OBP_COMPATIBLE, compatible, 1)) != DDI_PROP_SUCCESS) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, OBP_COMPATIBLE);
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (rv);
	}

	if ((rv = ndi_prop_update_string(DDI_DEV_T_NONE, rdip,
	    OBP_DEVICETYPE, arc->anrc->is_pcie ? "pciex" : "pci")) !=
	    DDI_PROP_SUCCESS) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, OBP_DEVICETYPE);
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (rv);
	}

	/*
	 * PCI OF binding: #address-cells is always 3 (phys.hi,
	 * phys.mid, phys.lo) and #size-cells is always 2.  These
	 * are mandated by the IEEE 1275 PCI bus binding specification
	 * and are not platform-specific.
	 */
	if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    OBP_ADDRESS_CELLS, 3)) != DDI_PROP_SUCCESS) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, OBP_ADDRESS_CELLS);
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (rv);
	}

	if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    OBP_SIZE_CELLS, 2)) != DDI_PROP_SUCCESS) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, OBP_SIZE_CELLS);
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (rv);
	}

	if (arc->anrc->num_rs > 0) {
		VERIFY3P(arc->anrc->rs, !=, NULL);
		if (ACPI_FAILURE(acpidev_set_mmio_regs(rdip, arc->anrc->rs,
		    arc->anrc->num_rs))) {
			dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
			    arc->anrc->objname, OBP_REG);
			(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
			return (DDI_FAILURE);
		}
	} else {
		VERIFY3U(arc->num_rs, >, 0);
		VERIFY3P(arc->rs, !=, NULL);
		if (ACPI_FAILURE(acpidev_set_mmio_regs(rdip, arc->rs,
		    arc->num_rs))) {
			dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
			    arc->anrc->objname, OBP_REG);
			(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
			return (DDI_FAILURE);
		}
	}

	if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    "linux,pci-domain", arc->anrc->segno)) != DDI_PROP_SUCCESS) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, "linux,pci-domain");
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (rv);
	}

	if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    "pci-segment", arc->anrc->segno)) != DDI_PROP_SUCCESS) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, "pci-segment");
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (rv);
	}

	bus_range[0] = arc->anrc->rhdl->acpidev_buses[0].bus_start;
	bus_range[1] = arc->anrc->rhdl->acpidev_buses[0].bus_end - 1;
	if ((rv = ndi_prop_update_int_array(DDI_DEV_T_NONE, rdip,
	    OBP_BUS_RANGE, (int *)bus_range, 2)) != DDI_PROP_SUCCESS) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, OBP_BUS_RANGE);
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (rv);
	}

	if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    OBP_INTERRUPT_CELLS, arc->anrc->interrupt_cells)) !=
	    DDI_PROP_SUCCESS) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, OBP_INTERRUPT_CELLS);
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (rv);
	}

	if ((rv = ndi_prop_update_int_array(DDI_DEV_T_NONE, rdip,
	    OBP_INTERRUPT_MAP_MASK, (int *)arc->anrc->interrupt_mask_cells,
	    arc->anrc->num_interrupt_mask_cells)) != DDI_PROP_SUCCESS) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, OBP_INTERRUPT_MAP_MASK);
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (rv);
	}

	if ((rv = ndi_prop_update_int_array(DDI_DEV_T_NONE, rdip,
	    OBP_INTERRUPT_MAP, (int *)arc->anrc->interrupt_map_cells,
	    arc->anrc->num_interrupt_map_cells)) != DDI_PROP_SUCCESS) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, OBP_INTERRUPT_MAP);
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (rv);
	}

	if (ACPI_FAILURE(acpidev_set_standard_ranges(rdip,
	    arc->anrc->ranges, arc->anrc->num_ranges))) {
		dev_err(rdip, CE_WARN, "%s: failed to set '%s'",
		    arc->anrc->objname, OBP_RANGES);
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (DDI_FAILURE);
	}

	/*
	 * Set MSI parent and DMA coherency properties from IORT.
	 */
	irc = acpidev_iort_lookup_rc(arc->anrc->segno);
	if (irc != NULL) {
		if (irc->air_coherent) {
			(void) ddi_prop_create(DDI_DEV_T_NONE, rdip,
			    DDI_PROP_CANSLEEP | DDI_PROP_HW_DEF,
			    "dma-coherent", NULL, 0);
		}

		if (irc->air_simple) {
			dev_info_t *msi_dip;

			if (irc->air_v2m) {
				msi_dip =
				    acpidev_gic_next_v2m_frame();
			} else {
				msi_dip =
				    acpidev_gic_find_its_by_trid(
				    irc->air_its_trid);
			}
			if (msi_dip != NULL) {
				(void) ndi_prop_update_int(
				    DDI_DEV_T_NONE, rdip,
				    "msi-parent",
				    ddi_get_nodeid(msi_dip));
			}
		} else if (irc->air_nmaps > 0) {
			dev_info_t *its_dip;
			int *msi_map;
			uint_t i, ncells;

			ncells = irc->air_nmaps * 4;
			msi_map = kmem_zalloc(
			    ncells * sizeof (int), KM_SLEEP);

			for (i = 0; i < irc->air_nmaps; i++) {
				its_dip =
				    acpidev_gic_find_its_by_trid(
				    irc->air_maps[i].aim_its_trid);
				if (its_dip == NULL) {
					its_dip =
					    acpidev_gic_next_v2m_frame();
				}
				if (its_dip == NULL) {
					dev_err(rdip, CE_WARN, "%s: no ITS/v2m "
					    "target for RID range base 0x%x",
					    arc->anrc->objname,
					    irc->air_maps[i].aim_rid_base);
					kmem_free(msi_map,
					    ncells * sizeof (int));
					msi_map = NULL;
					break;
				}
				msi_map[i * 4] =
				    irc->air_maps[i].aim_rid_base;
				msi_map[i * 4 + 1] = ddi_get_nodeid(its_dip);
				msi_map[i * 4 + 2] =
				    irc->air_maps[i].aim_devid_base;
				msi_map[i * 4 + 3] =
				    irc->air_maps[i].aim_rid_count;
			}

			if (msi_map != NULL) {
				(void) ndi_prop_update_int_array(
				    DDI_DEV_T_NONE, rdip, "msi-map",
				    msi_map, ncells);
				kmem_free(msi_map, ncells * sizeof (int));
			}
		}
	}

	if (ACPI_FAILURE(acpica_tag_devinfo(rdip, arc->anrc->hdl))) {
		dev_err(rdip, CE_WARN, "%s: failed to tag ACPI devinfo",
		    arc->anrc->objname);
		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
		return (DDI_FAILURE);
	}

	acpica_pci_cfgspace_register(rdip);

	(void) ndi_devi_bind_driver(rdip, 0);
	return (DDI_SUCCESS);
}

ACPI_STATUS
acpidev_process_mcfg(dev_info_t *gdip)
{
	ACPI_TABLE_MCFG		*mcfg;
	ACPI_MCFG_ALLOCATION	*mcfg_alloc;
	const char		*mcfg_alloc_endp;
	ACPI_STATUS		st;
	uint32_t		idx;
	acpi_root_complex_t	*curr;
	acpi_root_complex_t	*prev;
	int			rv;

	SLIST_INIT(&acpi_root_complexes);

	mcfg = NULL;
	st = AcpiGetTable(ACPI_SIG_MCFG, 1, (ACPI_TABLE_HEADER **)&mcfg);
	if (st != AE_OK) {
		if (st == AE_NOT_FOUND || st == AE_NOT_EXIST) {
			return (AE_OK);
		}
		return (st);
	}
	ASSERT3P(mcfg, !=, NULL);

	mcfg_alloc = (ACPI_MCFG_ALLOCATION *)((uintptr_t)mcfg + sizeof (*mcfg));
	mcfg_alloc_endp = ((const char *)mcfg) + mcfg->Header.Length;

	curr = prev = NULL;

	idx = 0;
	while ((const char *)mcfg_alloc < mcfg_alloc_endp) {
		/*
		 * A truncated table may leave fewer than a full
		 * ACPI_MCFG_ALLOCATION before mcfg_alloc_endp; stop
		 * rather than reading out of bounds.
		 */
		if ((const char *)mcfg_alloc +
		    sizeof (*mcfg_alloc) > mcfg_alloc_endp) {
			break;
		}

		if (mcfg_alloc->Address == 0) {
			cmn_err(CE_CONT, "?acpidev: failed to process MCFG "
			    "allocation entry %u: address is zero\n", idx);
			mcfg_alloc++;
			idx++;
			continue;
		}

		if (mcfg_alloc->StartBusNumber > mcfg_alloc->EndBusNumber) {
			cmn_err(CE_CONT, "?acpidev: failed to process MCFG "
			    "allocation entry %u: start bus number is higher "
			    "than end bus number\n", idx);
			mcfg_alloc++;
			idx++;
			continue;
		}

		curr = kmem_zalloc(sizeof (acpi_root_complex_t), KM_SLEEP);
		ASSERT3P(curr, !=, NULL);
		curr->mcfg = mcfg_alloc;

		if (prev == NULL) {
			SLIST_INSERT_HEAD(&acpi_root_complexes, curr, entries);
		} else {
			SLIST_INSERT_AFTER(prev, curr, entries);
		}

		mcfg_alloc++;
		prev = curr;
		idx++;
	}

	if (ACPI_FAILURE(st = AcpiGetDevices(NULL,
	    process_namespace_device, gdip, NULL))) {
		return (st);
	}

	idx = 0;
	SLIST_FOREACH(curr, &acpi_root_complexes, entries) {
		uint32_t segno;

		if (curr->mcfg != NULL && curr->anrc == NULL) {
			cmn_err(CE_NOTE, "?MCFG entry %u has no corresponding "
			    "DSDT entry and will be skipped.", idx);
			idx++;
			continue;
		}

		segno = curr->mcfg ?
		    curr->mcfg->PciSegment : curr->anrc->segno;

		if (ddi_prop_get_int(DDI_DEV_T_ANY, ddi_root_node(),
		    DDI_PROP_DONTPASS, "acpidev-mcfg-allowed-segment-group",
		    segno) != segno) {
			cmn_err(CE_CONT, "?acpidev: skipping MCFG allocation "
			    "entry %u due to filtering\n", idx);
			idx++;
			continue;
		}

		if (curr->anrc != NULL && curr->anrc->rs == NULL &&
		    curr->mcfg != NULL) {
			/*
			 * No configuration-space base address found on the
			 * namespace object, create an alternative register
			 * specification from the available information.
			 */
			curr->num_rs = 1;
			curr->rs_size = sizeof (struct regspec) * curr->num_rs;
			curr->rs = kmem_zalloc(curr->rs_size, KM_SLEEP);
			ASSERT3P(curr->rs, !=, NULL);
			curr->rs->regspec_bustype = 0x0;
			curr->rs->regspec_addr = curr->mcfg->Address;
			if (curr->anrc->rhdl != NULL &&
			    curr->anrc->rhdl->acpidev_bus_count > 0) {
				curr->rs->regspec_size = SIZE_FOR_NUM_BUSSES(
				    curr->anrc->rhdl->acpidev_buses[0].bus_end);
			} else {
				curr->rs->regspec_size = SIZE_FOR_NUM_BUSSES(
				    ((uint32_t)curr->mcfg->EndBusNumber) + 1);
			}
		}

		rv = create_root_complex(curr);
		if (rv != DDI_SUCCESS) {
			cmn_err(CE_NOTE, "?Failed to create PCI/PCIe root "
			    "complex at index %u. %d.", idx, rv);
			idx++;
			continue;
		}

		idx++;
	}

	while (!SLIST_EMPTY(&acpi_root_complexes)) {
		curr = SLIST_FIRST(&acpi_root_complexes);
		SLIST_REMOVE_HEAD(&acpi_root_complexes, entries);
		free_acpi_namespace_root_complex(curr->anrc);
		if (curr->rs != NULL && curr->rs_size != 0) {
			kmem_free(curr->rs, curr->rs_size);
		}
		kmem_free(curr, sizeof (acpi_root_complex_t));
	}

	AcpiPutTable((ACPI_TABLE_HEADER *)mcfg);
	return (AE_OK);
}
