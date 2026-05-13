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
 * Copyright 2026 Michael van der Westhuizen
 */

/*
 * PCI Resource Discovery on armv8.  We just ask the device tree.
 */

#include <sys/esunddi.h>
#include <sys/memlist.h>
#include <sys/promif.h>
#include <sys/types.h>
#include <sys/pci.h>
#include <sys/pci_impl.h>
#include <sys/plat/pci_prd.h>
#include <sys/sunndi.h>
#include <sys/obpdefs.h>
#include <sys/smbios.h>
#include <sys/pcie_impl.h>
#include <sys/cmn_err.h>

/*
 * We always just tell the system to scan all PCI buses.
 */
uint32_t
pci_prd_max_bus(void)
{
	return (PCI_MAX_BUS_NUM - 1);
}

boolean_t
pci_prd_multi_root_ok(void)
{
	return (B_TRUE);
}

pci_prd_compat_flags_t
pci_prd_compat_flags(void)
{
	/*
	 * On systems using devicetree, we need IEEE 1275 compatible naming.
	 */
	return (PCI_PRD_COMPAT_1275);
}

/*
 * Walk up the devinfo tree from dip to find the PCIe root complex
 * ancestor and read its PCI segment number from the "linux,pci-domain"
 * property.  The root complex is identified by its DEVI_PORT_TYPE_PCIRC
 * registration (set by the RC driver at attach time).  Returns 0
 * (single-segment default) if the property is absent on the RC node.
 */
static uint16_t
pci_prd_get_segment(dev_info_t *dip)
{
	dev_info_t *pdip;

	for (pdip = dip; pdip != NULL; pdip = ddi_get_parent(pdip)) {
		if (ndi_port_type(pdip, B_TRUE, DEVI_PORT_TYPE_PCIRC)) {
			return ((uint16_t)ddi_prop_get_int(DDI_DEV_T_ANY,
			    pdip, DDI_PROP_DONTPASS,
			    "linux,pci-domain", 0));
		}
	}

	return (0);
}

/*
 * SMBIOS Type 9 (System Slot) iteration state for pci_prd_slot_name.
 *
 * We collect per-device-number slot names, then emit the IEEE 1275
 * "slot-names" property with strings ordered by device number.
 */
#define	PCI_PRD_MAX_DEV	32

typedef struct pci_prd_slot_cb {
	uint16_t	psc_seg;			/* matcher segment */
	uint8_t		psc_bus;			/* matcher bus */
	uint32_t	psc_mask;			/* device bitmask */
	const char	*psc_name[PCI_PRD_MAX_DEV];	/* per-device name */
} pci_prd_slot_cb_t;

static int
pci_prd_slot_iter_cb(smbios_hdl_t *shp, const smbios_struct_t *strp,
    void *data)
{
	pci_prd_slot_cb_t *cbp = data;
	smbios_slot_t slot;
	uint8_t dev;

	if (strp->smbstr_type != SMB_TYPE_SLOT) {
		return (0);
	}

	if (smbios_info_slot(shp, strp->smbstr_id, &slot) != 0) {
		return (0);
	}

	if (slot.smbl_sg != cbp->psc_seg || slot.smbl_bus != cbp->psc_bus) {
		return (0);
	}

	dev = slot.smbl_df >> 3;
	if (dev >= PCI_PRD_MAX_DEV) {	/* exposition only */
		return (0);
	}

	cbp->psc_mask |= (1U << dev);
	cbp->psc_name[dev] = slot.smbl_name;

	return (0);
}

/*
 * Use SMBIOS Type 9 (System Slot) records to set the "slot-names"
 * property on bus devinfo nodes.  The property uses the IEEE 1275
 * format: a uint32_t device bitmask followed by packed NUL-terminated
 * name strings in ascending device-number order, padded to a 4-byte
 * boundary.
 *
 * On multi-segment platforms (e.g. Ampere Altra Max with 8 root
 * complexes), the SMBIOS segment group (smbl_sg) is matched against
 * the "linux,pci-domain" property on the root complex ancestor of dip.
 *
 * If multiple SMBIOS records share the same device number (different
 * functions on the same device), the last record wins.  The IEEE 1275
 * slot-names format supports only one name per device number and this
 * choice matches i86pc behaviour.
 */
void
pci_prd_slot_name(uint32_t bus, dev_info_t *dip)
{
	pci_prd_slot_cb_t cb;
	char *buf;
	size_t totlen;
	size_t len;

	if (dip == NULL || ksmbios == NULL) {
		return;
	}

	bzero(&cb, sizeof (cb));
	cb.psc_seg = pci_prd_get_segment(dip);
	cb.psc_bus = (uint8_t)bus;

	(void) smbios_iter(ksmbios, pci_prd_slot_iter_cb, &cb);

	if (cb.psc_mask == 0) {
		return;
	}

	/* Build the slot-names property: bitmask and packed strings. */
	totlen = sizeof (uint32_t);

	for (uint_t dev = 0; dev < PCI_PRD_MAX_DEV; dev++) {
		if (cb.psc_name[dev] == NULL) {
			continue;
		}

		totlen += strlen(cb.psc_name[dev]) + 1;
	}

	while (totlen % sizeof (uint32_t) != 0) {
		totlen++;
	}

	ASSERT((totlen % sizeof (int)) == 0);
	buf = kmem_zalloc(totlen, KM_SLEEP);
	ASSERT3P(buf, !=, NULL);

	*(uint32_t *)buf = cb.psc_mask;
	len = sizeof (uint32_t);

	for (uint_t dev = 0; dev < PCI_PRD_MAX_DEV; dev++) {
		size_t nlen;

		if (cb.psc_name[dev] == NULL) {
			continue;
		}

		nlen = strlen(cb.psc_name[dev]) + 1;
		if (len + nlen > totlen) {
			break;
		}

		bcopy(cb.psc_name[dev], buf + len, nlen);
		len += nlen;
	}

	/* NUL padding is taken care of by kmem_zalloc */

	(void) ndi_prop_update_int_array(DDI_DEV_T_NONE, dip, "slot-names",
	    (int *)buf, totlen / sizeof (int));
	kmem_free(buf, totlen);
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
