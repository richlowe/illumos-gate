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
 * GIC device tree node creation from MADT.
 */

#include <sys/types.h>
#include <sys/stddef.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_subrdefs.h>
#include <sys/ddi_impldefs.h>
#include <sys/stdbool.h>
#include <sys/obpdefs.h>
#include <sys/gic_reg.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/acpidev.h>
#include <sys/acpidev_gic.h>
#include <sys/smp_impldefs.h>
#include <sys/list.h>

#define	ACPI_GIC_INTERRUPT_CELLS	3

/*
 * ITS device registry - tracks translation IDs to dev_info_t mappings
 * so that IORT parsing can resolve ITS references.  Populated during
 * GIC device tree creation, queried by acpidev_iort.c.
 * No locking needed - populated single-threaded at boot before MCFG runs.
 */
struct its_entry {
	uint32_t	ite_trid;
	dev_info_t	*ite_dip;
	list_node_t	ite_node;
};

static list_t its_list;

/*
 * GICv2m MSI frame registry - tracks MSI frame dev_info_t nodes so that
 * root complexes on GICv2 systems can be assigned an msi-parent via
 * round-robin distribution across the available frames.
 *
 * Same boot-time single-threaded population as the ITS registry.
 */
struct v2m_entry {
	dev_info_t	*v2me_dip;
	list_node_t	v2me_node;
};

static list_t v2m_list;
static uint32_t v2m_count = 0;
static uint32_t v2m_rr_idx = 0;
static bool gic_registries_initialized = false;

struct gicc_item {
	ACPI_MADT_GENERIC_INTERRUPT	*gicc;
	list_node_t			node;
};

struct gicd_item {
	ACPI_MADT_GENERIC_DISTRIBUTOR	*gicd;
	list_node_t			node;
};

struct gv2m_item {
	ACPI_MADT_GENERIC_MSI_FRAME	*gv2m;
	list_node_t			node;
};

struct gicr_item {
	ACPI_MADT_GENERIC_REDISTRIBUTOR	*gicr;
	list_node_t			node;
};

struct gits_item {
	ACPI_MADT_GENERIC_TRANSLATOR	*gits;
	list_node_t			node;
};

struct madt_gic {
	ACPI_TABLE_MADT			*mg_madt;

	list_t				mg_gicc;
	list_t				mg_gicd;
	list_t				mg_gv2m;
	list_t				mg_gicr;
	list_t				mg_gits;

	uint32_t			mg_ngicc;
	uint32_t			mg_ngicd;
	uint32_t			mg_ngv2m;
	uint32_t			mg_ngicr;
	uint32_t			mg_ngits;
};

static uint32_t gic_version;

ACPI_STATUS
acpidev_set_up_root_node(void)
{
	int rv = DDI_SUCCESS;

	if (!ddi_prop_exists(DDI_DEV_T_ANY, ddi_root_node(),
	    DDI_PROP_DONTPASS, OBP_ADDRESS_CELLS)) {
		if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, ddi_root_node(),
		    OBP_ADDRESS_CELLS, ACPIDEV_ROOT_ADDRESS_CELLS)) !=
		    DDI_PROP_SUCCESS) {
			goto out;
		}
	}

	if (!ddi_prop_exists(DDI_DEV_T_ANY, ddi_root_node(),
	    DDI_PROP_DONTPASS, OBP_SIZE_CELLS)) {
		if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, ddi_root_node(),
		    OBP_SIZE_CELLS, ACPIDEV_ROOT_SIZE_CELLS)) !=
		    DDI_PROP_SUCCESS) {
			goto out;
		}
	}

out:
	if (rv == DDI_SUCCESS) {
		return (AE_OK);
	}

	return (AE_ERROR);
}

ACPI_STATUS
acpidev_set_system_pic(dev_info_t *dip)
{
	if (ndi_prop_update_int(DDI_DEV_T_NONE, ddi_root_node(),
	    OBP_INTERRUPT_PARENT, ddi_get_nodeid(dip)) != DDI_PROP_SUCCESS) {
		return (AE_ERROR);
	}

	return (AE_OK);
}

uint32_t
acpidev_get_gic_interrupt_cells(void)
{
	return (ACPI_GIC_INTERRUPT_CELLS);
}

/*
 * GICv3:
 *
 * #interrupt-cells: a single cell with a value of at least 3.  If the
 * system requires describing PPI affinity, then the value must be at least 4.
 * XX we will only ever have 3.
 *
 * The 1st cell is the interrupt type; 0 for SPI interrupts, 1 for PPI
 * interrupts. Other values are reserved for future use.
 *
 * The 2nd cell contains the interrupt number for the interrupt type.  SPI
 * interrupts are in the range [0-987]. PPI interrupts are in the range [0-15].
 *
 * The 3rd cell is the flags, encoded as follows:
 *   bits[3:0] trigger type and level flags.
 *     1 = edge triggered
 *     4 = level triggered
 *
 * GICv2:
 *
 * #interrupt-cells : Specifies the number of cells needed to encode an
 * interrupt source.  The type shall be a <u32> and the value shall be 3.
 *
 * The 1st cell is the interrupt type; 0 for SPI interrupts, 1 for PPI
 * interrupts.
 *
 * The 2nd cell contains the interrupt number for the interrupt type.  SPI
 * interrupts are in the range [0-987].  PPI interrupts are in the range [0-15].
 *
 * The 3rd cell is the flags, encoded as follows:
 *   bits[3:0] trigger type and level flags.
 *     1 = low-to-high edge triggered
 *     2 = high-to-low edge triggered (invalid for SPIs)
 *     4 = active high level-sensitive
 *     8 = active low level-sensitive (invalid for SPIs).
 *   bits[15:8] PPI interrupt cpu mask.  Each bit corresponds to each of
 *   the 8 possible cpus attached to the GIC.  A bit set to '1' indicated
 *   the interrupt is wired to that CPU.  Only valid for PPI interrupts.
 *   NOTE: just leave this out.
 */

ACPI_STATUS
acpidev_serialize_interrupt(
    const struct acpidev_gic_interrupt *g, uint32_t *cells)
{
	uint32_t i;

	ASSERT3P(g, !=, NULL);
	ASSERT3P(cells, !=, NULL);

	if (g->gi_flags & AGIF_EDGE) {
		if ((g->gi_flags & (AGIF_FALLING|AGIF_EDGE)) !=
		    g->gi_flags) {
			cmn_err(CE_WARN, "acpidev: bad interrupt flags %u. "
			    "Edge must be rising or falling.", g->gi_flags);
			return (AE_SUPPORT);
		}
	} else {
		if ((g->gi_flags & (AGIF_ACTIVE_LOW)) != g->gi_flags) {
			cmn_err(CE_WARN, "acpidev: bad interrupt flags %u. "
			    "Level must be active low or active high.",
			    g->gi_flags);
			return (AE_SUPPORT);
		}
	}

	if (gic_version == 2 && GIC_INTID_IS_SPI(g->gi_gsiv)) {
		if (((g->gi_flags & (AGIF_EDGE|AGIF_FALLING)) ==
		    (AGIF_EDGE|AGIF_FALLING)) ||
		    ((g->gi_flags & (AGIF_LEVEL|AGIF_ACTIVE_LOW)) ==
		    (AGIF_LEVEL|AGIF_ACTIVE_LOW))) {
			cmn_err(CE_WARN, "acpidev: bad interrupt flags %u. "
			    "SPI cannot be edge falling or level active low.",
			    g->gi_flags);
			return (AE_SUPPORT);
		}
	}

	/*
	 * The bindings make no reference to extended SPI or PPI
	 * interrupt identifiers, so don't try to support them.
	 *
	 * We also fail on LPI or SGI, since those can't be encoded
	 * based on the bindings.
	 */
	if (!GIC_INTID_IS_PPI(g->gi_gsiv) &&
	    !GIC_INTID_IS_SPI(g->gi_gsiv)) {
		cmn_err(CE_WARN, "acpidev: bad interrupt %u. "
		    "Bindings do not specify how to encode anything other than "
		    "basic PPI or SPI.", g->gi_gsiv);
		return (AE_SUPPORT);
	}

	i = 0;
	cells[i++] = GIC_INTID_IS_PPI(g->gi_gsiv) ? 1 : 0;
	cells[i++] = GIC_INTID_IS_PPI(g->gi_gsiv) ?
	    (g->gi_gsiv - GIC_INTID_PPI_MIN) :
	    (g->gi_gsiv - GIC_INTID_SPI_MIN);

	if (g->gi_flags & AGIF_EDGE) {
		if (g->gi_flags & AGIF_FALLING) {
			/* high-to-low edge triggered */
			if (gic_version == 2) {
				cells[i++] = 0x2;
			} else {
				cells[i++] = 0x1;
			}
		} else {
			/* low-to-high edge triggered */
			if (gic_version == 2) {
				cells[i++] = 0x1;
			} else {
				cells[i++] = 0x1;
			}
		}
	} else {
		if (g->gi_flags & AGIF_ACTIVE_LOW) {
			/* active low level-sensitive */
			if (gic_version == 2) {
				cells[i++] = 0x8;
			} else {
				cells[i++] = 0x4;
			}
		} else {
			/* active high level-sensitive */
			if (gic_version == 2) {
				cells[i++] = 0x4;
			} else {
				cells[i++] = 0x4;
			}
		}
	}

	return (AE_OK);
}

ACPI_STATUS
acpidev_set_gsiv_interrupts(dev_info_t *rdip,
    const struct acpidev_gic_interrupt *gi, size_t ngi)
{
	size_t gii;
	size_t i;
	size_t ncells;
	size_t nalloc;
	uint32_t *cells;
	ACPI_STATUS st;

	for (gii = 0; gii < ngi; ++gii) {
		const struct acpidev_gic_interrupt *g = &gi[gii];
		ASSERT3P(g, !=, NULL);

		if (g->gi_flags & AGIF_EDGE) {
			if ((g->gi_flags & (AGIF_FALLING|AGIF_EDGE)) !=
			    g->gi_flags) {
				return (AE_SUPPORT);
			}
		} else {
			if ((g->gi_flags & (AGIF_ACTIVE_LOW)) != g->gi_flags) {
				return (AE_SUPPORT);
			}
		}

		/*
		 * The bindings make no reference to extended SPI or PPI
		 * interrupt identifiers, so don't try to support them.
		 *
		 * We also fail on LPI or SGI, since those can't be encoded
		 * based on the bindings.
		 */
		if (!GIC_INTID_IS_PPI(g->gi_gsiv) &&
		    !GIC_INTID_IS_SPI(g->gi_gsiv)) {
			return (AE_SUPPORT);
		}
	}

	/*
	 * OK, things seem to be legitimate, let's get on with encoding the
	 * interrupts.
	 */
	st = AE_OK;
	ncells = ngi * ACPI_GIC_INTERRUPT_CELLS;
	nalloc = sizeof (uint32_t) * ncells;
	cells = kmem_zalloc(nalloc, KM_SLEEP);
	ASSERT3P(cells, !=, NULL);

	i = 0;

	for (gii = 0; gii < ngi; ++gii) {
		const struct acpidev_gic_interrupt *g = &gi[gii];
		ASSERT3P(g, !=, NULL);
		ASSERT3U(ncells - i, >=, 3);

		st = acpidev_serialize_interrupt(g, &cells[i]);
		if (ACPI_FAILURE(st)) {
			kmem_free(cells, nalloc);
			return (st);
		}
		i += ACPI_GIC_INTERRUPT_CELLS;
	}

	VERIFY3U(i, ==, ncells);

	if (ndi_prop_update_int_array(DDI_DEV_T_NONE, rdip, OBP_INTERRUPTS,
	    (int *)cells, ncells) != DDI_PROP_SUCCESS) {
		st = AE_ERROR;
	}

	kmem_free(cells, nalloc);
	return (st);
}

ACPI_STATUS
acpidev_set_standard_ranges(dev_info_t *rdip, struct rangespec *rs, size_t nrs)
{
	size_t rsi;
	size_t i;
	size_t ncells;
	size_t nalloc;
	uint32_t *cells;
	ACPI_STATUS st;
	size_t stride;
	int cac;
	int pac;
	int csc;

	cac = ddi_prop_get_int(DDI_DEV_T_ANY, rdip, 0,
	    OBP_ADDRESS_CELLS, OBP_DEFAULT_ADDRESS_CELLS);
	pac = ddi_prop_get_int(DDI_DEV_T_ANY, ddi_get_parent(rdip), 0,
	    OBP_ADDRESS_CELLS, OBP_DEFAULT_ADDRESS_CELLS);
	csc = ddi_prop_get_int(DDI_DEV_T_ANY, rdip, 0,
	    OBP_SIZE_CELLS, OBP_DEFAULT_SIZE_CELLS);
	VERIFY3U(cac, >=, 1);
	VERIFY3U(cac, <=, 3);
	VERIFY3U(pac, >=, 1);
	VERIFY3U(pac, <=, 3);
	VERIFY3U(csc, >=, 1);
	VERIFY3U(csc, <=, 2);
	stride = (size_t)(cac + pac + csc);

	st = AE_OK;
	ncells = nrs * stride;
	nalloc = sizeof (uint32_t) * ncells;
	cells = kmem_zalloc(nalloc, KM_SLEEP);
	ASSERT3P(cells, !=, NULL);

	i = 0;

	for (rsi = 0; rsi < nrs; ++rsi) {
		const struct rangespec *r = &rs[rsi];
		ASSERT3P(r, !=, NULL);
		ASSERT3U(ncells - i, >=, stride);

		switch (cac) {
		case 3:
			cells[i++] = r->rng_cbustype & 0xffffffff;
			/* fallthrough */
		case 2:
			cells[i++] = (r->rng_coffset >> 32) & 0xffffffff;
			/* fallthrough */
		case 1:
			cells[i++] = r->rng_coffset & 0xffffffff;
			break;
		default:
			cmn_err(CE_PANIC,
			    "!acpidev: invalid child #address-cells (%d)", cac);
		}

		switch (pac) {
		case 3:
			cells[i++] = r->rng_bustype & 0xffffffff;
			/* fallthrough */
		case 2:
			cells[i++] = (r->rng_offset >> 32) & 0xffffffff;
			/* fallthrough */
		case 1:
			cells[i++] = r->rng_offset & 0xffffffff;
			break;
		default:
			cmn_err(CE_PANIC, "!acpidev: "
			    "invalid parent #address-cells (%d)", pac);
		}

		switch (csc) {
		case 2:
			cells[i++] = (r->rng_size >> 32) & 0xffffffff;
			/* fallthrough */
		case 1:
			cells[i++] = r->rng_size & 0xffffffff;
			break;
		default:
			cmn_err(CE_PANIC,
			    "!acpidev: invalid child #size-cells (%d)", csc);
		}
	}

	VERIFY3U(i, ==, ncells);

	if (ndi_prop_update_int_array(DDI_DEV_T_NONE, rdip, OBP_RANGES,
	    (int *)cells, ncells) != DDI_PROP_SUCCESS) {
		st = AE_ERROR;
	}

	kmem_free(cells, nalloc);
	return (st);
}

ACPI_STATUS
acpidev_set_mmio_regs(dev_info_t *rdip, struct regspec *rs, size_t nrs)
{
	size_t rsi;
	size_t i;
	size_t ncells;
	size_t nalloc;
	uint32_t *cells;
	ACPI_STATUS st;
	size_t stride;
	int ac;
	int sc;

	ac = ddi_prop_get_int(DDI_DEV_T_ANY, ddi_get_parent(rdip), 0,
	    OBP_ADDRESS_CELLS, OBP_DEFAULT_ADDRESS_CELLS);
	sc = ddi_prop_get_int(DDI_DEV_T_ANY, ddi_get_parent(rdip), 0,
	    OBP_SIZE_CELLS, OBP_DEFAULT_SIZE_CELLS);
	VERIFY3U(ac, >=, 1);
	VERIFY3U(ac, <=, 3);
	VERIFY3U(sc, >=, 1);
	VERIFY3U(sc, <=, 2);
	stride = (size_t)(ac + sc);

	st = AE_OK;
	ncells = nrs * stride;
	nalloc = sizeof (uint32_t) * ncells;
	cells = kmem_zalloc(nalloc, KM_SLEEP);
	ASSERT3P(cells, !=, NULL);

	i = 0;

	for (rsi = 0; rsi < nrs; ++rsi) {
		const struct regspec *r = &rs[rsi];
		ASSERT3P(r, !=, NULL);
		ASSERT3U(ncells - i, >=, stride);

		switch (ac) {
		case 3:
			cells[i++] = r->regspec_bustype & 0xffffffff;
			/* fallthrough */
		case 2:
			cells[i++] = (r->regspec_addr >> 32) & 0xffffffff;
			/* fallthrough */
		case 1:
			cells[i++] = r->regspec_addr & 0xffffffff;
			break;
		default:
			cmn_err(CE_PANIC,
			    "!acpidev: invalid #address-cells (%d)", ac);
		}

		switch (sc) {
		case 2:
			cells[i++] = (r->regspec_size >> 32) & 0xffffffff;
			/* fallthrough */
		case 1:
			cells[i++] = r->regspec_size & 0xffffffff;
			break;
		default:
			cmn_err(CE_PANIC,
			    "!acpidev: invalid #size-cells (%d)", sc);
		}
	}

	VERIFY3U(i, ==, ncells);

	if (ndi_prop_update_int_array(DDI_DEV_T_NONE, rdip, OBP_REG,
	    (int *)cells, ncells) != DDI_PROP_SUCCESS) {
		st = AE_ERROR;
	}

	if (ndi_prop_update_int_array(DDI_DEV_T_NONE, rdip,
	    "assigned-addresses", (int *)cells, ncells) != DDI_PROP_SUCCESS) {
		st = AE_ERROR;
	}

	kmem_free(cells, nalloc);
	return (st);
}

/*
 * See Documentation/devicetree/bindings/interrupt-controller/arm,gic.txt in
 * the Linux kernel tree, or on https://www.kernel.org/doc/.
 *
 * XXXARM: It's not clear how FDT support for GICv2 with virtualisation
 * extensions works.  Once we figure that out we can incorporate the
 * VGIC maintenance interrupt and the GIC virtual CPU interface and GIC
 * virtual interface control block registers. These are stored in the
 * ACPI_MADT_GENERIC_INTERRUPT structure as VgicInterrupt, GicvBaseAddress and
 * GichBaseAddress.
 */

static int
gic_setup_v2_device(dev_info_t *rdip, struct madt_gic *mg, bool has_children)
{
	int				rv;
	uint64_t			gicc_addr;
	uint64_t			gicd_addr;
	uint64_t			gich_addr;
	uint64_t			gicv_addr;
	uint64_t			gicc_size;
	uint64_t			gicd_size;
	uint64_t			gich_size;
	uint64_t			gicv_size;
	struct gicc_item		*gicc;
	struct regspec			regspecs[4];
	struct acpidev_gic_interrupt	vgic_agi;
	uint32_t			vgic_intid;
	uint32_t			vgic_intid_flags;
	bool				vgic_intid_flags_set;
	uint_t				nregs;
	char				*compatible[] = {"arm,gic-400"};

	vgic_intid = 0;
	vgic_intid_flags = 0;
	vgic_intid_flags_set = false;
	gicc_addr = 0;
	gicd_addr = 0;
	gich_addr = 0;
	gicv_addr = 0;
	gicc_size = 0x2000;		/* always 8k */
	gicd_size = 0x1000;		/* always 4k */
	gich_size = 0x2000;		/* always 8k */
	gicv_size = 0x2000;		/* always 8k */

	ASSERT3P(mg, !=, NULL);
	ASSERT3P(rdip, !=, NULL);
	ASSERT3U(mg->mg_ngicc, >, 0);
	ASSERT3U(mg->mg_ngicc, <=, 8);
	ASSERT3U(mg->mg_ngicd, ==, 1);
	ASSERT0(mg->mg_ngicr);
	ASSERT0(mg->mg_ngits);

	gicd_addr = ((struct gicd_item *)list_head(&mg->mg_gicd))->
	    gicd->BaseAddress;
	VERIFY3P(gicd_addr, !=, 0);

	for (gicc = list_head(&mg->mg_gicc); gicc != NULL;
	    gicc = list_next(&mg->mg_gicc, gicc)) {
		if (gicc_addr == 0) {
			gicc_addr = gicc->gicc->BaseAddress;
		}

		if (gicc->gicc->BaseAddress != gicc_addr) {
			panic("Inconsistent GICC base address in MADT. "
			    "Expected 0x%lx, got 0x%lx",
			    gicc_addr, gicc->gicc->BaseAddress);
		}

		if (gich_addr == 0) {
			gich_addr = gicc->gicc->GichBaseAddress;
		}

		if (gicc->gicc->GichBaseAddress != gich_addr) {
			panic("Inconsistent GICH base address in MADT. "
			    "Expected 0x%lx, got 0x%lx",
			    gich_addr, gicc->gicc->GichBaseAddress);
		}

		if (gicv_addr == 0) {
			gicv_addr = gicc->gicc->GicvBaseAddress;
		}

		if (gicc->gicc->GicvBaseAddress != gicv_addr) {
			panic("Inconsistent GICV base address in MADT. "
			    "Expected 0x%lx, got 0x%lx",
			    gicv_addr, gicc->gicc->GicvBaseAddress);
		}

		if (vgic_intid == 0) {
			vgic_intid = gicc->gicc->VgicInterrupt;
		}

		if (gicc->gicc->VgicInterrupt != vgic_intid) {
			panic("Inconsistent VGIC maintenance INTID in MADT. "
			    "Expected %u, got %u",
			    vgic_intid, gicc->gicc->VgicInterrupt);
		}

		if (!vgic_intid_flags_set) {
			vgic_intid_flags =
			    gicc->gicc->Flags & ACPI_MADT_VGIC_IRQ_MODE;
			vgic_intid_flags_set = true;
		}

		if ((gicc->gicc->Flags & ACPI_MADT_VGIC_IRQ_MODE) !=
		    vgic_intid_flags) {
			panic("Inconsistent VGIC maintenance INTID flags in "
			    "MADT. Expected 0x%x, got 0x%x",
			    vgic_intid_flags,
			    gicc->gicc->Flags & ACPI_MADT_VGIC_IRQ_MODE);
		}
	}

	if ((gich_addr == 0 && gicv_addr != 0) ||
	    (gicv_addr == 0 && gich_addr != 0)) {
		panic("Incoherent GIC virtualisation base addresses. Either "
		    "both must be zero, or neither may be zero. "
		    "GICH is 0x%lx, GICV is 0x%lx.", gich_addr, gicv_addr);
	}

	if (gich_addr != 0 && vgic_intid == 0) {
		cmn_err(CE_WARN, "Virtualization base addresses are set while "
		    "the VGIC maintenance interrupt is not. VGIC functionality "
		    "will be disabled.");
	}

	if ((rv = ndi_prop_update_string_array(DDI_DEV_T_NONE, rdip,
	    OBP_COMPATIBLE, compatible, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = e_ddi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    DDI_NO_AUTODETACH, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip, OBP_INTERRUPT_CELLS,
	    ACPI_GIC_INTERRUPT_CELLS)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = ddi_prop_create(DDI_DEV_T_NONE, rdip,
	    DDI_PROP_CANSLEEP|DDI_PROP_HW_DEF, OBP_INTERRUPT_CONTROLLER,
	    NULL, 0)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	/*
	 * For GICv2 the first register is the distributor and the second is
	 * the CPU interface. The distributor has one 4k frame and the CPU
	 * interface has an 8k frame.
	 */
	regspecs[0].regspec_bustype = 0;
	regspecs[0].regspec_addr = gicd_addr;
	regspecs[0].regspec_size = gicd_size;
	regspecs[1].regspec_bustype = 0;
	regspecs[1].regspec_addr = gicc_addr;
	regspecs[1].regspec_size = gicc_size;
	nregs = 2;

	/*
	 * When we have virtualisation extensions we add two registers to
	 * the configuration (virtual interface control and virtual CPU
	 * interface) and add an interrupt to service the virtual GIC.
	 *
	 * Our only other reason to add an interrupts entry would be when
	 * the GIC is chained to another interrupt controller, which will
	 * never be the case for a system GIC described by the MADT.
	 */
	if (gich_addr != 0 && gicv_addr != 0 && vgic_intid != 0) {
		vgic_agi.gi_gsiv = vgic_intid;
		vgic_agi.gi_flags =
		    (vgic_intid_flags & ACPI_MADT_VGIC_IRQ_MODE) ?
		    AGIF_EDGE : AGIF_LEVEL;

		if (ACPI_FAILURE(acpidev_set_gsiv_interrupts(
		    rdip, &vgic_agi, 1))) {
			rv = DDI_FAILURE;
			goto out;
		}

		regspecs[2].regspec_bustype = 0;
		regspecs[2].regspec_addr = gich_addr;
		regspecs[2].regspec_size = gich_size;

		regspecs[3].regspec_bustype = 0;
		regspecs[3].regspec_addr = gicv_addr;
		regspecs[3].regspec_size = gicv_size;

		nregs += 2;
	}

	if (ACPI_FAILURE(acpidev_set_mmio_regs(rdip, regspecs, nregs))) {
		rv = DDI_FAILURE;
		goto out;
	}

	if (has_children) {
		if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
		    OBP_ADDRESS_CELLS, ACPIDEV_ROOT_ADDRESS_CELLS)) !=
		    DDI_PROP_SUCCESS) {
			goto out;
		}

		if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
		    OBP_SIZE_CELLS, ACPIDEV_ROOT_SIZE_CELLS)) !=
		    DDI_PROP_SUCCESS) {
			goto out;
		}

		/* an empty ranges property indicates identity mapping */
		if ((rv = ddi_prop_create(DDI_DEV_T_NONE, rdip,
		    DDI_PROP_CANSLEEP|DDI_PROP_HW_DEF, OBP_RANGES,
		    NULL, 0)) != DDI_PROP_SUCCESS) {
			goto out;
		}
	}

out:
	return (rv);
}

static int
gic_setup_v2m_child(dev_info_t *rdip, ACPI_MADT_GENERIC_MSI_FRAME *v2m)
{
	int		rv;
	uint64_t	gv2m_size;
	struct regspec	rs;
	char		*compatible[] = {"arm,gic-v2m-frame"};

	ASSERT3P(rdip, !=, NULL);
	ASSERT3P(v2m, !=, NULL);
	VERIFY3U(v2m->BaseAddress, !=, 0);

	gv2m_size = 0x1000;	/* always 4k */

	if ((rv = ndi_prop_update_string_array(DDI_DEV_T_NONE, rdip,
	    OBP_COMPATIBLE, compatible, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = e_ddi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    DDI_NO_AUTODETACH, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = ddi_prop_create(DDI_DEV_T_NONE, rdip,
	    DDI_PROP_CANSLEEP|DDI_PROP_HW_DEF,
	    OBP_MSI_CONTROLLER, NULL, 0)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	rs.regspec_bustype = 0;
	rs.regspec_addr = v2m->BaseAddress;
	rs.regspec_size = gv2m_size;

	if (ACPI_FAILURE(acpidev_set_mmio_regs(rdip, &rs, 1))) {
		rv = DDI_FAILURE;
		goto out;
	}

	if (v2m->Flags & ACPI_MADT_OVERRIDE_SPI_VALUES) {
		if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
		    "arm,msi-base-spi", v2m->SpiBase)) != DDI_PROP_SUCCESS) {
			goto out;
		}

		if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
		    "arm,msi-num-spis", v2m->SpiCount)) != DDI_PROP_SUCCESS) {
			goto out;
		}
	}

out:
	return (rv);
}

static int
gic_setup_v2_node(dev_info_t *dip, struct madt_gic *mg, dev_info_t **xdipp)
{
	int			rv;
	struct gv2m_item	*gv2m;
	struct v2m_entry	*v2me;
	struct v2m_entry	*v2me_next;
	dev_info_t		*rdip;
	dev_info_t		*gdip;
	dev_info_t		*tdip;

	VERIFY3P(dip, !=, NULL);
	VERIFY3P(mg, !=, NULL);
	VERIFY3P(xdipp, !=, NULL);
	VERIFY3P(*xdipp, ==, NULL);

	if (mg->mg_ngicc == 0 || mg->mg_ngicc > 8) {
		panic("GICv2 only supports up to eight CPU interfaces, and "
		    "must have at least one CPU interface. "
		    "Found %u CPU interface(s)",
		    mg->mg_ngicc);
	}

	if (mg->mg_ngicd != 1) {
		panic("One, and only one, GIC Distributor Interface must exist "
		    "in the MADT. Found %u GIC Distributor Interfaces",
		    mg->mg_ngicd);
	}

	VERIFY0(mg->mg_ngicr);
	VERIFY0(mg->mg_ngits);

	ndi_devi_alloc_sleep(dip, "intc", (pnode_t)DEVI_SID_NODEID, &rdip);

	if ((rv = gic_setup_v2_device(rdip, mg,
	    !list_is_empty(&mg->mg_gv2m))) != DDI_SUCCESS) {
		goto out;
	}

	if ((rv = ndi_devi_bind_driver(rdip, 0)) != NDI_SUCCESS) {
		goto out;
	}

	/*
	 * Add any MSI frames that are exposed in the MADT.
	 */
	for (gv2m = list_head(&mg->mg_gv2m); gv2m != NULL;
	    gv2m = list_next(&mg->mg_gv2m, gv2m)) {
		gdip = NULL;
		VERIFY3P(gv2m->gv2m, !=, NULL);

		ndi_devi_alloc_sleep(rdip, "v2m",
		    (pnode_t)DEVI_SID_NODEID, &gdip);

		if ((rv = gic_setup_v2m_child(gdip, gv2m->gv2m)) !=
		    DDI_SUCCESS) {
			goto out;
		}

		if ((rv = ndi_devi_bind_driver(gdip, 0)) != NDI_SUCCESS) {
			cmn_err(CE_NOTE, "acpidev: failed to bind the GICv2 "
			    "MSI Frame driver");
			goto out;
		}

		/* Record in v2m registry for msi-parent assignment */
		v2me = kmem_zalloc(sizeof (*v2me), KM_SLEEP);
		v2me->v2me_dip = gdip;
		list_insert_tail(&v2m_list, v2me);
		v2m_count++;
	}

out:
	if (rv == DDI_SUCCESS) {
		*xdipp = rdip;
	} else {
		/*
		 * Remove v2m registry entries whose dev_info_t nodes
		 * are about to be offlined.
		 */
		for (v2me = list_head(&v2m_list); v2me != NULL;
		    v2me = v2me_next) {
			v2me_next = list_next(&v2m_list, v2me);
			if (ddi_get_parent(v2me->v2me_dip) == rdip) {
				list_remove(&v2m_list, v2me);
				v2m_count--;
				kmem_free(v2me, sizeof (*v2me));
			}
		}

		if ((gdip = ddi_get_child(rdip)) != NULL) {
			while (gdip != NULL) {
				tdip = ddi_get_next_sibling(gdip);
				(void) ndi_devi_offline(gdip, NDI_DEVI_REMOVE);
				gdip = tdip;
			}
		}

		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
	}

	return (rv);
}

/*
 * See Documentation/devicetree/bindings/interrupt-controller/arm,gic-v3.txt in
 * the Linux kernel tree, or on https://www.kernel.org/doc/.
 */

static int
gic_setup_v3_device(dev_info_t *rdip, struct madt_gic *mg, bool has_children)
{
	int			rv;
	uint64_t		gicd_addr;
	uint64_t		gicd_size;
	uint64_t		gicr_size;
	struct gicc_item	*gicc;
	struct gicr_item	*gicr;
	struct regspec		*regspecs;
	char			*compatible[] = {"arm,gic-v3"};
	uint_t			nreg;
	uint32_t		nredist_regions;
	uint32_t		i;
	uint32_t		vgic_maintenance_interrupt;
	uint64_t		probe_addr;

	VERIFY3P(rdip, !=, NULL);
	VERIFY3P(mg, !=, NULL);

	vgic_maintenance_interrupt = 0;
	nreg = 1;		/* we always have a distributor */
	regspecs = NULL;
	gicd_addr = 0;
	gicd_size = 0x10000;	/* always 64k */
	nredist_regions = 0;

	/*
	 * Determine the per-CPU redistributor region size by reading
	 * GICR_TYPER from the hardware.  Each redistributor has two 64K
	 * frames (RD_base + SGI_base = 128K).  When GICR_TYPER.VLPIS is
	 * set the redistributor implements GICv4 virtual LPIs, adding a
	 * VLPI_base frame and a reserved frame for a total of four 64K
	 * frames (256K).
	 *
	 * This size is only used when redistributors are described via
	 * per-CPU GICC entries.  Separate GICR entries carry their own
	 * Length field.
	 */
	gicr_size = 0x20000;	/* default: 2 frames, 128K */
	probe_addr = 0;

	if (mg->mg_ngicr > 0) {
		gicr = list_head(&mg->mg_gicr);
		probe_addr = gicr->gicr->BaseAddress;
	} else if (mg->mg_ngicc > 0) {
		gicc = list_head(&mg->mg_gicc);
		probe_addr = gicc->gicc->GicrBaseAddress;
	}

	if (probe_addr != 0) {
		caddr_t va;

		va = psm_map_phys((paddr_t)probe_addr, 0x10000, PROT_READ);
		if (va != NULL) {
			uint64_t typer = *(volatile uint64_t *)
			    (va + GICR_TYPER);
			if (typer & GICR_TYPER_VLPIS) {
				gicr_size = 0x40000;	/* 4 frames */
			}
			psm_unmap_phys(va, 0x10000);
		}
	}

	if (mg->mg_ngicc == 0) {
		panic("Must have at least one CPU interface.");
	}

	for (gicc = list_head(&mg->mg_gicc); gicc != NULL;
	    gicc = list_next(&mg->mg_gicc, gicc)) {
		if (gicc->gicc->BaseAddress != 0) {
			panic("GICv3 and GICv4 must use the system "
			    "register interface.");
		}

		if (vgic_maintenance_interrupt == 0) {
			vgic_maintenance_interrupt =
			    gicc->gicc->VgicInterrupt;
		} else if (gicc->gicc->VgicInterrupt !=
		    vgic_maintenance_interrupt) {
			panic("Inconsistent VGIC maintenance interrupt in "
			    "MADT. Expected %u, got %u.",
			    vgic_maintenance_interrupt,
			    gicc->gicc->VgicInterrupt);
		}

		if (mg->mg_ngicr == 0 &&
		    gicc->gicc->GicrBaseAddress == 0) {
			panic("GICC entry has no GicrBaseAddress "
			    "and no separate GICR structures");
		}
	}

	if (mg->mg_ngicd != 1) {
		panic("One, and only one, GIC Distributor Interface must exist "
		    "in the MADT. Found %u GIC Distributor Interfaces",
		    mg->mg_ngicd);
	}

	gicd_addr = ((struct gicd_item *)list_head(&mg->mg_gicd))->
	    gicd->BaseAddress;
	VERIFY3U(gicd_addr, !=, 0);

	/*
	 * When all redistributors are in the always-on power domain we will
	 * have GICR structures. When any redistributor is not in the always-on
	 * power domain we will have redistributors exposed via the GICC
	 * structures.
	 *
	 * We will never have a combination of GICR structures and redistributor
	 * information in GICC structures.
	 */
	if ((nredist_regions = mg->mg_ngicr) == 0) {
		nredist_regions = mg->mg_ngicc;
	}

	nreg += nredist_regions;

	if ((rv = ndi_prop_update_string_array(DDI_DEV_T_NONE, rdip,
	    OBP_COMPATIBLE, compatible, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = e_ddi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    DDI_NO_AUTODETACH, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip, OBP_INTERRUPT_CELLS,
	    ACPI_GIC_INTERRUPT_CELLS)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = ddi_prop_create(DDI_DEV_T_NONE, rdip,
	    DDI_PROP_CANSLEEP|DDI_PROP_HW_DEF, OBP_INTERRUPT_CONTROLLER,
	    NULL, 0)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    "#redistributor-regions", nredist_regions)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	/*
	 * redistributor-stride - Unnecessary in ACPI-based implementations, as
	 * there are no padding pages.
	 */

	/*
	 * msi-controller - iff Message Based Interrupt AND mbi-ranges present
	 * mbi-ranges - [start, size] of MBIs
	 * mbi-alias - not 100% sure, but used for hardware isolation
	 *
	 * ... but we do not support MBIs, so ignore for now.
	 */

	regspecs = kmem_zalloc(sizeof (struct regspec) * nreg, KM_SLEEP);
	i = 0;

	regspecs[i].regspec_bustype = 0;
	regspecs[i].regspec_addr = gicd_addr;
	regspecs[i++].regspec_size = gicd_size;

	/*
	 * When there are no separate redistributor structures we add
	 * redistributors from the GICC structures. These must have a
	 * populated redistributor address in this case.
	 *
	 * However, when separate redistributor structures exist we must
	 * ignore the GicrBaseAddress on the GICC and just use the separate
	 * structures.
	 */
	VERIFY3U(i, ==, 1);
	if (mg->mg_ngicr == 0) {
		for (gicc = list_head(&mg->mg_gicc); gicc != NULL;
		    gicc = list_next(&mg->mg_gicc, gicc)) {
			VERIFY3U(i, <, nreg);

			regspecs[i].regspec_bustype = 0;
			regspecs[i].regspec_addr = gicc->gicc->GicrBaseAddress;
			regspecs[i++].regspec_size = gicr_size;
		}
	} else {
		for (gicr = list_head(&mg->mg_gicr); gicr != NULL;
		    gicr = list_next(&mg->mg_gicr, gicr)) {
			VERIFY3U(i, <, nreg);

			regspecs[i].regspec_bustype = 0;
			regspecs[i].regspec_addr = gicr->gicr->BaseAddress;
			regspecs[i++].regspec_size = gicr->gicr->Length;
		}
	}

	/* any additional registers must be populated before this check */
	ASSERT3U(i, ==, nreg);

	if (ACPI_FAILURE(acpidev_set_mmio_regs(rdip, regspecs, nreg))) {
		rv = DDI_FAILURE;
		goto out;
	}

	if (has_children) {
		if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
		    OBP_ADDRESS_CELLS, ACPIDEV_ROOT_ADDRESS_CELLS)) !=
		    DDI_PROP_SUCCESS) {
			goto out;
		}

		if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
		    OBP_SIZE_CELLS, ACPIDEV_ROOT_SIZE_CELLS)) !=
		    DDI_PROP_SUCCESS) {
			goto out;
		}

		/* an empty ranges property indicates identity mapping */
		if ((rv = ddi_prop_create(DDI_DEV_T_NONE, rdip,
		    DDI_PROP_CANSLEEP|DDI_PROP_HW_DEF, OBP_RANGES,
		    NULL, 0)) != DDI_PROP_SUCCESS) {
			goto out;
		}
	}

out:
	if (regspecs != NULL && nreg != 0) {
		kmem_free(regspecs, sizeof (struct regspec) * nreg);
	}

	return (rv);
}

static int
gic_setup_v3_child(dev_info_t *rdip, ACPI_MADT_GENERIC_TRANSLATOR *gits)
{
	int		rv;
	uint64_t	gits_size;
	struct regspec	rs;
	caddr_t		va;
	char		*compatible[] = {"arm,gic-v3-its"};

	/*
	 * Determine the ITS MMIO region size by reading GITS_TYPER from
	 * the hardware.  The ITS always has two 64K frames (control +
	 * translation = 128K).  When GITS_TYPER.VSGI is set the ITS
	 * implements GICv4.1 virtual SGI support, adding a third 64K
	 * frame for 192K total (IHI0069H, 12.18).
	 */
	gits_size = 0x20000;	/* default: 2 frames, 128K */

	va = psm_map_phys((paddr_t)gits->BaseAddress, 0x10000, PROT_READ);
	if (va != NULL) {
		uint64_t typer = *(volatile uint64_t *)(va + GITS_TYPER);
		if (typer & GITS_TYPER_VSGI) {
			gits_size = 0x30000;	/* 3 frames */
		}
		psm_unmap_phys(va, 0x10000);
	}

	if ((rv = ndi_prop_update_string_array(DDI_DEV_T_NONE, rdip,
	    OBP_COMPATIBLE, compatible, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = e_ddi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    DDI_NO_AUTODETACH, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = ddi_prop_create(DDI_DEV_T_NONE, rdip,
	    DDI_PROP_CANSLEEP|DDI_PROP_HW_DEF, "msi-controller",
	    NULL, 0)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    OBP_MSI_CELLS, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = ndi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    "illumos,translation-id", gits->TranslationId)) !=
	    DDI_PROP_SUCCESS) {
		goto out;
	}

	rs.regspec_bustype = 0;
	rs.regspec_addr = gits->BaseAddress;
	rs.regspec_size = gits_size;

	if (ACPI_FAILURE(acpidev_set_mmio_regs(rdip, &rs, 1))) {
		rv = DDI_FAILURE;
		goto out;
	}

out:
	return (rv);
}

static int
gic_setup_v3_node(dev_info_t *dip, struct madt_gic *mg, dev_info_t **xdipp)
{
	int			rv;
	boolean_t		has_children;
	dev_info_t		*rdip;
	dev_info_t		*gdip;
	dev_info_t		*tdip;
	struct gv2m_item	*gv2m;
	struct v2m_entry	*v2me;
	struct v2m_entry	*v2me_next;
	struct gicc_item	*gicc;
	struct gits_item	*gits;
	struct its_entry	*ite;
	struct its_entry	*ite_next;
	uint32_t		vgic_maintenance_interrupt;

	VERIFY3P(mg, !=, NULL);
	VERIFY3P(xdipp, !=, NULL);
	VERIFY3P(*xdipp, ==, NULL);

	vgic_maintenance_interrupt = 0;

	if (mg->mg_ngicc == 0) {
		panic("Must have at least one CPU interface.");
	}

	for (gicc = list_head(&mg->mg_gicc); gicc != NULL;
	    gicc = list_next(&mg->mg_gicc, gicc)) {
		if (gicc->gicc->BaseAddress != 0) {
			panic("GICv3 and GICv4 must use the system "
			    "register interface.");
		}

		if (vgic_maintenance_interrupt == 0) {
			vgic_maintenance_interrupt =
			    gicc->gicc->VgicInterrupt;
		} else if (gicc->gicc->VgicInterrupt !=
		    vgic_maintenance_interrupt) {
			panic("Inconsistent VGIC maintenance interrupt in "
			    "MADT. Expected %u, got %u.",
			    vgic_maintenance_interrupt,
			    gicc->gicc->VgicInterrupt);
		}
	}

	if (mg->mg_ngicd != 1) {
		panic("One, and only one, GIC Distributor Interface must exist "
		    "in the MADT. Found %u GIC Distributor Interfaces",
		    mg->mg_ngicd);
	}

	/*
	 * A GICv3 may have ITS children, v2m MSI frame children, or both (this
	 * is technically possible, though it would have to happen in some
	 * bizarre alternate reality).  The has_children flag gates creation of
	 * #address-cells, #size-cells, and ranges on the parent.
	 */
	has_children = (!list_is_empty(&mg->mg_gits) ||
	    !list_is_empty(&mg->mg_gv2m));

	ndi_devi_alloc_sleep(dip, "intc", (pnode_t)DEVI_SID_NODEID, &rdip);

	if ((rv = gic_setup_v3_device(rdip, mg, has_children)) !=
	    DDI_SUCCESS) {
		goto out;
	}

	if ((rv = ndi_devi_bind_driver(rdip, 0)) != NDI_SUCCESS) {
		goto out;
	}

	for (gits = list_head(&mg->mg_gits); gits != NULL;
	    gits = list_next(&mg->mg_gits, gits)) {
		gdip = NULL;

		ndi_devi_alloc_sleep(rdip, "its",
		    (pnode_t)DEVI_SID_NODEID, &gdip);

		if ((rv = gic_setup_v3_child(gdip, gits->gits)) !=
		    DDI_SUCCESS) {
			goto out;
		}

		if ((rv = ndi_devi_bind_driver(gdip, 0)) != NDI_SUCCESS) {
			cmn_err(CE_NOTE, "acpidev: failed to bind the GICv3 "
			    "Interrupt Translation Service driver");
			goto out;
		}

		/* Record in ITS registry for IORT lookup */
		ite = kmem_zalloc(sizeof (*ite), KM_SLEEP);
		ite->ite_trid = gits->gits->TranslationId;
		ite->ite_dip = gdip;
		list_insert_tail(&its_list, ite);
	}

	/*
	 * A GICv3 without ITS can use GICv2m MSI frames for
	 * MSI support.  Create v2m children and register them
	 * in the v2m registry for msi-parent assignment via IORT.
	 */
	for (gv2m = list_head(&mg->mg_gv2m); gv2m != NULL;
	    gv2m = list_next(&mg->mg_gv2m, gv2m)) {
		gdip = NULL;
		VERIFY3P(gv2m->gv2m, !=, NULL);

		ndi_devi_alloc_sleep(rdip, "v2m",
		    (pnode_t)DEVI_SID_NODEID, &gdip);

		if ((rv = gic_setup_v2m_child(gdip, gv2m->gv2m)) !=
		    DDI_SUCCESS) {
			goto out;
		}

		if ((rv = ndi_devi_bind_driver(gdip, 0)) != NDI_SUCCESS) {
			cmn_err(CE_NOTE, "acpidev: failed to bind the "
			    "GICv2 MSI Frame driver (GICv3 parent)");
			goto out;
		}

		/* Record in v2m registry for msi-parent assignment */
		v2me = kmem_zalloc(sizeof (*v2me), KM_SLEEP);
		v2me->v2me_dip = gdip;
		list_insert_tail(&v2m_list, v2me);
		v2m_count++;
	}

out:
	if (rv == DDI_SUCCESS) {
		*xdipp = rdip;
	} else {
		/*
		 * Remove ITS and v2m registry entries whose dev_info_t
		 * nodes are about to be offlined.
		 */
		for (ite = list_head(&its_list); ite != NULL;
		    ite = ite_next) {
			ite_next = list_next(&its_list, ite);
			if (ddi_get_parent(ite->ite_dip) == rdip) {
				list_remove(&its_list, ite);
				kmem_free(ite, sizeof (*ite));
			}
		}

		for (v2me = list_head(&v2m_list); v2me != NULL;
		    v2me = v2me_next) {
			v2me_next = list_next(&v2m_list, v2me);
			if (ddi_get_parent(v2me->v2me_dip) == rdip) {
				list_remove(&v2m_list, v2me);
				v2m_count--;
				kmem_free(v2me, sizeof (*v2me));
			}
		}

		if ((gdip = ddi_get_child(rdip)) != NULL) {
			while (gdip != NULL) {
				tdip = ddi_get_next_sibling(gdip);
				(void) ndi_devi_offline(gdip, NDI_DEVI_REMOVE);
				gdip = tdip;
			}
		}

		(void) ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
	}

	return (rv);
}

static int
acpidev_gic_setup_device_node(dev_info_t **xdipp)
{
	int				rv;
	ACPI_STATUS			st;
	ACPI_SUBTABLE_HEADER		*item;
	ACPI_SUBTABLE_HEADER		*end;
	struct madt_gic			mg;
	struct gicc_item		*gicc;
	struct gicd_item		*gicd;
	struct gv2m_item		*gv2m;
	struct gicr_item		*gicr;
	struct gits_item		*gits;
	size_t			min_len;

	VERIFY3P(xdipp, !=, NULL);
	VERIFY3P(*xdipp, ==, NULL);

	memset(&mg, 0, sizeof (mg));

	list_create(&mg.mg_gicc, sizeof (struct gicc_item),
	    offsetof(struct gicc_item, node));
	list_create(&mg.mg_gicd, sizeof (struct gicd_item),
	    offsetof(struct gicd_item, node));
	list_create(&mg.mg_gv2m, sizeof (struct gv2m_item),
	    offsetof(struct gv2m_item, node));
	list_create(&mg.mg_gicr, sizeof (struct gicr_item),
	    offsetof(struct gicr_item, node));
	list_create(&mg.mg_gits, sizeof (struct gits_item),
	    offsetof(struct gits_item, node));

	list_create(&its_list, sizeof (struct its_entry),
	    offsetof(struct its_entry, ite_node));
	list_create(&v2m_list, sizeof (struct v2m_entry),
	    offsetof(struct v2m_entry, v2me_node));
	gic_registries_initialized = true;

	if ((st = AcpiGetTable(ACPI_SIG_MADT, 1,
	    (ACPI_TABLE_HEADER **)&mg.mg_madt)) != AE_OK) {
		panic("Unable to get the MADT: 0x%x", st);
	}

	end = (ACPI_SUBTABLE_HEADER *)
	    (mg.mg_madt->Header.Length + (uintptr_t)mg.mg_madt);
	item = (ACPI_SUBTABLE_HEADER *)
	    ((uintptr_t)mg.mg_madt + sizeof (*mg.mg_madt));

	while (item < end) {
		/*
		 * The subtable header must fit in the table before
		 * Type and Length can be read.
		 */
		if ((uintptr_t)item + sizeof (*item) > (uintptr_t)end) {
			cmn_err(CE_WARN, "acpidev: truncated MADT subtable "
			    "at offset 0x%lx",
			    (uintptr_t)item - (uintptr_t)mg.mg_madt);
			break;
		}

		/*
		 * Each known subtable type has a fixed struct size.
		 *
		 * A Length smaller than that, or one that runs past
		 * the end of the table, means the entry is truncated.
		 */
		switch (item->Type) {
		case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
			min_len = sizeof (ACPI_MADT_GENERIC_INTERRUPT);
			break;
		case ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR:
			min_len = sizeof (ACPI_MADT_GENERIC_DISTRIBUTOR);
			break;
		case ACPI_MADT_TYPE_GENERIC_MSI_FRAME:
			min_len = sizeof (ACPI_MADT_GENERIC_MSI_FRAME);
			break;
		case ACPI_MADT_TYPE_GENERIC_REDISTRIBUTOR:
			min_len = sizeof (ACPI_MADT_GENERIC_REDISTRIBUTOR);
			break;
		case ACPI_MADT_TYPE_GENERIC_TRANSLATOR:
			min_len = sizeof (ACPI_MADT_GENERIC_TRANSLATOR);
			break;
		default:
			min_len = sizeof (*item);
			break;
		}

		if (item->Length < min_len ||
		    (uintptr_t)item + item->Length > (uintptr_t)end) {
			cmn_err(CE_WARN, "acpidev: MADT subtable type %u "
			    "with invalid length %u at offset 0x%lx",
			    item->Type, item->Length,
			    (uintptr_t)item - (uintptr_t)mg.mg_madt);
			break;
		}

		switch (item->Type) {
		case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
			gicc = kmem_zalloc(sizeof (*gicc), KM_SLEEP);
			gicc->gicc = (ACPI_MADT_GENERIC_INTERRUPT *)item;
			list_insert_tail(&mg.mg_gicc, gicc);
			mg.mg_ngicc++;
			break;
		case ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR:
			gicd = kmem_zalloc(sizeof (*gicd), KM_SLEEP);
			gicd->gicd = (ACPI_MADT_GENERIC_DISTRIBUTOR *)item;
			list_insert_tail(&mg.mg_gicd, gicd);
			mg.mg_ngicd++;
			break;
		case ACPI_MADT_TYPE_GENERIC_MSI_FRAME:
			gv2m = kmem_zalloc(sizeof (*gv2m), KM_SLEEP);
			gv2m->gv2m = (ACPI_MADT_GENERIC_MSI_FRAME *)item;
			list_insert_tail(&mg.mg_gv2m, gv2m);
			mg.mg_ngv2m++;
			break;
		case ACPI_MADT_TYPE_GENERIC_REDISTRIBUTOR:
			gicr = kmem_zalloc(sizeof (*gicr), KM_SLEEP);
			gicr->gicr = (ACPI_MADT_GENERIC_REDISTRIBUTOR *)item;
			list_insert_tail(&mg.mg_gicr, gicr);
			mg.mg_ngicr++;
			break;
		case ACPI_MADT_TYPE_GENERIC_TRANSLATOR:
			gits = kmem_zalloc(sizeof (*gits), KM_SLEEP);
			gits->gits = (ACPI_MADT_GENERIC_TRANSLATOR *)item;
			list_insert_tail(&mg.mg_gits, gits);
			mg.mg_ngits++;
			break;
		default:
			cmn_err(CE_WARN, "acpidev: unhandled interrupt "
			    "controller structure type: %u", item->Type);
			break;
		}

		item = (ACPI_SUBTABLE_HEADER *)((uintptr_t)item + item->Length);
	}

	if (mg.mg_ngicd != 1) {
		panic("One, and only one, GIC Distributor Interface must exist "
		    "in the MADT. Found %u GIC Distributor Interfaces",
		    mg.mg_ngicd);
	}
	gicd = list_head(&mg.mg_gicd);
	VERIFY3P(gicd->gicd, !=, NULL);

	if (gicd->gicd->Version == 0) {
		panic("Retrieving GIC version from hardware is unsupported in "
		    "illumos, as there is no portable interface to divine "
		    "this information. Please fix your firmware to declare "
		    "the GIC version.");
	}

	/* GICv2, GICv3 and GICv4 are supported */
	VERIFY3U(gicd->gicd->Version, >=, 2);
	VERIFY3U(gicd->gicd->Version, <=, 4);

	switch (gicd->gicd->Version) {
	case 2:
		gic_version = 2;
		rv = gic_setup_v2_node(ddi_root_node(), &mg, xdipp);
		break;
	case 3:	/* fallthrough */
	case 4:
		gic_version = 3;
		rv = gic_setup_v3_node(ddi_root_node(), &mg, xdipp);
		break;
	default:
		panic("Unexpected GIC version %u", gicd->gicd->Version);
	}

	while ((gicc = list_remove_head(&mg.mg_gicc)) != NULL) {
		kmem_free(gicc, sizeof (*gicc));
	}
	list_destroy(&mg.mg_gicc);

	while ((gicd = list_remove_head(&mg.mg_gicd)) != NULL) {
		kmem_free(gicd, sizeof (*gicd));
	}
	list_destroy(&mg.mg_gicd);

	while ((gv2m = list_remove_head(&mg.mg_gv2m)) != NULL) {
		kmem_free(gv2m, sizeof (*gv2m));
	}
	list_destroy(&mg.mg_gv2m);

	while ((gicr = list_remove_head(&mg.mg_gicr)) != NULL) {
		kmem_free(gicr, sizeof (*gicr));
	}
	list_destroy(&mg.mg_gicr);

	while ((gits = list_remove_head(&mg.mg_gits)) != NULL) {
		kmem_free(gits, sizeof (*gits));
	}
	list_destroy(&mg.mg_gits);

	AcpiPutTable((ACPI_TABLE_HEADER *)mg.mg_madt);
	return (rv);
}

dev_info_t *
acpidev_gic_find_its_by_trid(uint32_t trid)
{
	struct its_entry *ite;

	for (ite = list_head(&its_list); ite != NULL;
	    ite = list_next(&its_list, ite)) {
		if (ite->ite_trid == trid) {
			return (ite->ite_dip);
		}
	}

	return (NULL);
}

boolean_t
acpidev_gic_get_sole_its_trid(uint32_t *tridp)
{
	struct its_entry *ite;

	if (list_is_empty(&its_list)) {
		return (B_FALSE);
	}

	ite = list_head(&its_list);
	if (list_next(&its_list, ite) != NULL) {
		return (B_FALSE);
	}

	*tridp = ite->ite_trid;
	return (B_TRUE);
}

boolean_t
acpidev_gic_has_v2m_frames(void)
{
	return (!list_is_empty(&v2m_list));
}

/*
 * Return the next GICv2m MSI frame dev_info_t in round-robin order.
 * Called once per root complex during MCFG enumeration to distribute
 * MSI traffic across all available v2m frames.
 */
dev_info_t *
acpidev_gic_next_v2m_frame(void)
{
	struct v2m_entry *v2me;
	uint32_t idx;

	if (list_is_empty(&v2m_list)) {
		return (NULL);
	}

	idx = v2m_rr_idx % v2m_count;
	v2m_rr_idx++;

	for (v2me = list_head(&v2m_list); idx > 0;
	    v2me = list_next(&v2m_list, v2me)) {
		idx--;
	}

	return (v2me->v2me_dip);
}

/*
 * Free the ITS and GICv2m registries.  Safe to call even if
 * acpidev_create_gic_node() was never called or failed partway
 * through.
 */
void
acpidev_gic_fini(void)
{
	struct its_entry *ite;
	struct v2m_entry *v2me;

	if (!gic_registries_initialized) {
		return;
	}

	while ((ite = list_remove_head(&its_list)) != NULL) {
		kmem_free(ite, sizeof (*ite));
	}
	list_destroy(&its_list);

	while ((v2me = list_remove_head(&v2m_list)) != NULL) {
		kmem_free(v2me, sizeof (*v2me));
	}
	list_destroy(&v2m_list);

	v2m_count = 0;
	v2m_rr_idx = 0;
	gic_registries_initialized = false;
}


ACPI_STATUS
acpidev_create_gic_node(dev_info_t **xdipp)
{
	dev_info_t *xdip;

	ASSERT3P(xdipp, !=, NULL);
	xdip = NULL;

	if (acpidev_gic_setup_device_node(&xdip) == DDI_SUCCESS) {
		ASSERT3P(xdip, !=, NULL);
		*xdipp = xdip;
		return (AE_OK);
	}

	return (AE_ERROR);
}
