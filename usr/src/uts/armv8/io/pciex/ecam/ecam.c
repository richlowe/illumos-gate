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

#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/sysmacros.h>

#include <sys/pci_cfgspace.h>
#include <sys/pcie.h>
#include <sys/pcie_impl.h>
#include <sys/pci_cfgacc.h>

typedef struct {
	dev_info_t *ec_dip;
	ddi_acc_handle_t ec_handle;
	caddr_t ec_base;
} ecam_softc_t;

static void *ecam_soft_state;

static void
ecam_cfgspace_acc(pci_cfgacc_req_t *req)
{
	ecam_softc_t *softc;
	int bus, dev, func, reg;

	bus = PCI_CFGACC_BUS(req);
	dev = PCI_CFGACC_DEV(req);
	func = PCI_CFGACC_FUNC(req);
	reg = req->offset;

	softc = ddi_get_soft_state(ecam_soft_state,
	    ddi_get_instance(req->rcdip));

	VERIFY3P(softc, !=, NULL);

	if (!pcie_cfgspace_access_check(bus, dev, func, reg, req->size)) {
		if (!req->write)
			VAL64(req) = PCI_EINVAL64;
		return;
	}

	caddr_t addr = softc->ec_base + PCIE_CADDR_ECAM(bus, dev, func, reg);

	switch (req->size) {
	case PCI_CFG_SIZE_BYTE:
		if (req->write) {
			ddi_put8(softc->ec_handle, (uint8_t *)addr, VAL8(req));
		} else {
			VAL16(req) = ddi_get8(softc->ec_handle,
			    (uint8_t *)addr);
		}
		break;
	case PCI_CFG_SIZE_WORD:
		if (req->write) {
			ddi_put16(softc->ec_handle, (uint16_t *)addr,
			    VAL16(req));
		} else {
			VAL16(req) = ddi_get16(softc->ec_handle,
			    (uint16_t *)addr);
		}
		break;
	case PCI_CFG_SIZE_DWORD:
		if (req->write) {
			ddi_put32(softc->ec_handle, (uint32_t *)addr,
			    VAL32(req));
		} else {
			VAL32(req) = ddi_get32(softc->ec_handle,
			    (uint32_t *)addr);
		}
		break;
	case PCI_CFG_SIZE_QWORD:
		if (req->write) {
			ddi_put64(softc->ec_handle, (uint64_t *)addr,
			    VAL64(req));
		} else {
			VAL64(req) = ddi_get64(softc->ec_handle,
			    (uint64_t *)addr);
		}
		break;
	default:
		dev_err(softc->ec_dip, CE_PANIC,
		    "weird %d bit config space access", req->size * NBBY);
	}
}

/*
 * XXXPCI: This could be in the misc module, and the same for all nexi.
 * This is cut/pated between the two, don't mess that up
 */
static int
ecam_bus_config(dev_info_t *pdip, uint_t flags, ddi_bus_config_op_t op,
    void *arg, dev_info_t **rdip)
{
	int rval = DDI_SUCCESS;
	ndi_devi_enter(pdip);

	/*
	 * XXXPCI: This is the wrong way to be doing this.  We need to do
	 * stuff to rdip no doubt?
	 */
	if (op == BUS_CONFIG_ALL) {
		extern void pci_enumerate(dev_info_t *);
		pci_enumerate(pdip);
	}

	rval = ndi_busop_bus_config(pdip, flags, op, arg, rdip, 0);

	ndi_devi_exit(pdip);

	return (rval);
}

static int
ecam_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	ecam_softc_t *softc = NULL;
	int ret;

	if (cmd == DDI_RESUME)
		return (DDI_SUCCESS);

	int instance = ddi_get_instance(dip);

	if ((ret = ddi_soft_state_zalloc(ecam_soft_state, instance)) !=
	    DDI_SUCCESS) {
		return (ret);
	}

	softc = ddi_get_soft_state(ecam_soft_state, instance);
	VERIFY3P(softc, !=, NULL);

	const ddi_device_acc_attr_t attr = {
		.devacc_attr_version = DDI_DEVICE_ATTR_V1,
		.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC,
		.devacc_attr_dataorder = DDI_STRICTORDER_ACC,
		.devacc_attr_access = DDI_DEFAULT_ACC,
	};

	if ((ret = ddi_regs_map_setup(dip, 0, &softc->ec_base,
	    0, 0, &attr, &softc->ec_handle)) != DDI_SUCCESS) {
		dev_err(dip, CE_WARN, "failed to map configuration space: %d",
		    ret);
		return (ret);
	}

	VERIFY3U(softc->ec_base, !=, 0);

	softc->ec_dip = dip;

	/*
	 * XXXPCI: This is not the mechanism I would prefer, but I cannot find
	 * one I prefer.
	 */
	if ((ret = ddi_prop_update_int64(DDI_DEV_T_NONE, dip,
	    OBP_CFGSPACE_HOOK, (intptr_t)&ecam_cfgspace_acc)) !=
	    DDI_PROP_SUCCESS) {
		dev_err(dip, CE_WARN, "failed to set cfgspace access "
		    "hook: %d\n", ret);
		return (DDI_FAILURE);
	}

	ddi_report_dev(dip);

	return (DDI_SUCCESS);
}

static int
ecam_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_SUCCESS);

	ecam_softc_t *softc = ddi_get_soft_state(ecam_soft_state,
	    ddi_get_instance(dip));

	VERIFY3P(softc, !=, NULL);

	ddi_prop_remove(DDI_DEV_T_NONE, dip, OBP_CFGSPACE_HOOK);

	ddi_regs_map_free(&softc->ec_handle);
	ddi_soft_state_free(ecam_soft_state,
	    ddi_get_instance(dip));

	return (DDI_SUCCESS);
}

static struct bus_ops ecam_bus_ops = {
	.busops_rev = BUSO_REV,
	.bus_config = ecam_bus_config,
};

static struct dev_ops ecam_ops = {
	.devo_rev = DEVO_REV,
	.devo_refcnt = 0,
	.devo_getinfo = ddi_no_info,
	.devo_identify = nulldev,
	.devo_probe = nulldev,
	.devo_attach = ecam_attach,
	.devo_detach = ecam_detach,
	.devo_reset = nodev,
	.devo_bus_ops = &ecam_bus_ops,
	.devo_quiesce = ddi_quiesce_not_needed,
};

static struct modldrv ecam_modldrv = {
	.drv_modops = &mod_driverops,
	.drv_linkinfo = "Generic ECAM PCIe",
	.drv_dev_ops = &ecam_ops,
};

static struct modlinkage modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &ecam_modldrv, NULL }
};

int
_init(void)
{
	int err;

	if ((err = ddi_soft_state_init(&ecam_soft_state,
	    sizeof (ecam_softc_t), 1)) != DDI_SUCCESS) {
		return (err);
	}

	if ((err = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&ecam_soft_state);
		return (err);
	}

	return (DDI_SUCCESS);
}

int
_fini(void)
{
	int err;

	if ((err = mod_remove(&modlinkage)) != DDI_SUCCESS) {
		return (err);
	}

	ddi_soft_state_fini(&ecam_soft_state);
	return (DDI_SUCCESS);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
