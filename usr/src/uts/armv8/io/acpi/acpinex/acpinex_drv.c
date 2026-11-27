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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2009-2010, Intel Corporation.
 * All rights reserved.
 */
/*
 * Copyright 2012 Garrett D'Amore <garrett@damore.org>.  All rights reserved.
 */
/*
 * Copyright 2026 Michael van der Westhuizen
 */
/*
 * This module implements a nexus driver for the ACPI virtual bus.
 * It does not handle any of the DDI functions passed up to it by the child
 * drivers, but instead allows them to bubble up to the root node.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cpuinfo.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddifm.h>
#include <sys/note.h>
#include <sys/ndifm.h>
#include <sys/obpdefs.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/acpidev.h>
#include <sys/acpidev_rsc.h>
#include <sys/acpinex.h>

/* Patchable through /etc/system. */
#ifdef	DEBUG
int acpinex_debug = 1;
#else
int acpinex_debug = 0;
#endif

/*
 * Driver globals
 */
static kmutex_t acpinex_lock;
static void *acpinex_softstates;

static int acpinex_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int acpinex_attach(dev_info_t *, ddi_attach_cmd_t);
static int acpinex_detach(dev_info_t *, ddi_detach_cmd_t);
static int acpinex_open(dev_t *, int, int, cred_t *);
static int acpinex_close(dev_t, int, int, cred_t *);
static int acpinex_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int acpinex_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp);
static int acpinex_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *,
    void *);
static int acpinex_fm_init_child(dev_info_t *, dev_info_t *, int,
    ddi_iblock_cookie_t *);
static void acpinex_fm_init(acpinex_softstate_t *softsp);
static void acpinex_fm_fini(acpinex_softstate_t *softsp);

extern void make_ddi_ppd(dev_info_t *, struct ddi_parent_private_data **);

/*
 * Configuration data structures
 */
static struct bus_ops acpinex_bus_ops = {
	.busops_rev		= BUSO_REV,
	.bus_map		= acpinex_bus_map,
	.bus_get_intrspec	= NULL,
	.bus_add_intrspec	= NULL,
	.bus_remove_intrspec	= NULL,
	.bus_map_fault		= i_ddi_map_fault,
	.bus_dma_map		= NULL,
	.bus_dma_allochdl	= ddi_dma_allochdl,
	.bus_dma_freehdl	= ddi_dma_freehdl,
	.bus_dma_bindhdl	= ddi_dma_bindhdl,
	.bus_dma_unbindhdl	= ddi_dma_unbindhdl,
	.bus_dma_flush		= ddi_dma_flush,
	.bus_dma_win		= ddi_dma_win,
	.bus_dma_ctl		= ddi_dma_mctl,
	.bus_ctl		= acpinex_ctlops,
	.bus_prop_op		= ddi_bus_prop_op,
	.bus_get_eventcookie	= ndi_busop_get_eventcookie,
	.bus_add_eventcall	= ndi_busop_add_eventcall,
	.bus_remove_eventcall	= ndi_busop_remove_eventcall,
	.bus_post_event		= ndi_post_event,
	.bus_intr_ctl		= NULL,
	.bus_config		= NULL,
	.bus_unconfig		= NULL,
	.bus_fm_init		= acpinex_fm_init_child,
	.bus_fm_fini		= NULL,
	.bus_fm_access_enter	= NULL,
	.bus_fm_access_exit	= NULL,
	.bus_power		= NULL,
	.bus_intr_op		= i_ddi_intr_ops,
	.bus_hp_op		= NULL,
};

static struct cb_ops acpinex_cb_ops = {
	.cb_open		= acpinex_open,
	.cb_close		= acpinex_close,
	.cb_strategy		= nodev,
	.cb_print		= nodev,
	.cb_dump		= nodev,
	.cb_read		= nodev,
	.cb_write		= nodev,
	.cb_ioctl		= acpinex_ioctl,
	.cb_devmap		= nodev,
	.cb_mmap		= nodev,
	.cb_segmap		= nodev,
	.cb_chpoll		= nochpoll,
	.cb_prop_op		= ddi_prop_op,
	.cb_str			= NULL,
	.cb_flag		= D_NEW | D_MP | D_HOTPLUG,
	.cb_rev			= CB_REV,
	.cb_aread		= nodev,
	.cb_awrite		= nodev,
};

static struct dev_ops acpinex_ops = {
	.devo_rev		= DEVO_REV,
	.devo_refcnt		= 0,
	.devo_getinfo		= acpinex_info,
	.devo_identify		= nulldev,
	.devo_probe		= nulldev,
	.devo_attach		= acpinex_attach,
	.devo_detach		= acpinex_detach,
	.devo_reset		= nulldev,
	.devo_cb_ops		= &acpinex_cb_ops,
	.devo_bus_ops		= &acpinex_bus_ops,
	.devo_power		= nulldev,
	.devo_quiesce		= ddi_quiesce_not_needed,
};

static struct modldrv modldrv = {
	.drv_modops		= &mod_driverops,
	.drv_linkinfo		= "ACPI virtual bus driver",
	.drv_dev_ops		= &acpinex_ops,
};

static struct modlinkage modlinkage = {
	.ml_rev			= MODREV_1,
	.ml_linkage		= { &modldrv, NULL }
};

/*
 * Module initialization routines.
 */
int
_init(void)
{
	int error;

	/* Initialize soft state pointer. */
	if ((error = ddi_soft_state_init(&acpinex_softstates,
	    sizeof (acpinex_softstate_t), 8)) != 0) {
		cmn_err(CE_WARN,
		    "acpinex: failed to initialize soft state structure.");
		return (error);
	}

	/* Initialize event subsystem. */
	acpinex_event_init();

	/* Install the module. */
	if ((error = mod_install(&modlinkage)) != 0) {
		cmn_err(CE_WARN, "acpinex: failed to install module.");
		ddi_soft_state_fini(&acpinex_softstates);
		return (error);
	}

	mutex_init(&acpinex_lock, NULL, MUTEX_DRIVER, NULL);

	return (0);
}

int
_fini(void)
{
	int error;

	/* Remove the module. */
	if ((error = mod_remove(&modlinkage)) != 0) {
		return (error);
	}

	/* Shut down event subsystem. */
	acpinex_event_fini();

	/* Free the soft state info. */
	ddi_soft_state_fini(&acpinex_softstates);

	mutex_destroy(&acpinex_lock);

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
acpinex_info(dev_info_t *dip __unused,
    ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t	dev;
	int	instance;

	if (infocmd == DDI_INFO_DEVT2INSTANCE) {
		dev = (dev_t)arg;
		instance = ACPINEX_GET_INSTANCE(getminor(dev));
		*result = (void *)(uintptr_t)instance;
		return (DDI_SUCCESS);
	}

	return (DDI_FAILURE);
}

/*
 * Update the ranges in the DDI PPD with the information only we have - the
 * bustype information that must be decoded from an ACPI 3-word address.
 *
 * Unfortunately, this means we know things about the "parent-private" data we
 * should not.  But otherwise the parent knows things that it should not.
 *
 * We do this so that `i_ddi_apply_range()` can map between address spaces,
 * supporting "I/O space" on aarch64.
 *
 * XXXARM: Just like the PCIe counterpart, this sucks.
 */
static int
acpinex_update_ppd_ranges(dev_info_t *dip)
{
	acpidev_ranges_t *ranges;
	uint_t rangesln;

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    OBP_RANGES, (int **)&ranges, &rangesln) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	VERIFY(
	    (CELLS_1275_TO_BYTES(rangesln) % sizeof (acpidev_ranges_t)) == 0);
	VERIFY((CELLS_1275_TO_BYTES(rangesln) / sizeof (acpidev_ranges_t)) >=
	    i_ddi_pd_getnrng(dip));

	for (int i = 0; i < i_ddi_pd_getnrng(dip); i++) {
		switch (ranges[i].child_hi & ACPIDEV_REG_TYPE_M) {
		case ACPIDEV_REG_TYPE_IO:
			i_ddi_pd_getrng(dip, i)->rng_cbustype = 1;
			break;
		case ACPIDEV_REG_TYPE_MEMORY:
			i_ddi_pd_getrng(dip, i)->rng_cbustype = 0;
			break;
		default:
			dev_err(dip, CE_PANIC, "unhandled bus type 0x%x",
			    ranges[i].child_hi & ACPIDEV_REG_TYPE_M);
		}
	}

	ddi_prop_free(ranges);
	return (DDI_SUCCESS);
}

static int
acpinex_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int instance;
	acpinex_softstate_t *softsp;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	/* Get and check instance number. */
	instance = ddi_get_instance(devi);
	if (instance >= ACPINEX_INSTANCE_MAX) {
		cmn_err(CE_WARN, "acpinex: instance number %d is out of range "
		    "in acpinex_attach(), max %d.",
		    instance, ACPINEX_INSTANCE_MAX - 1);
		return (DDI_FAILURE);
	}

	/*
	 * Update the parent-private range data to reflect the range bus types
	 * supported by DDI.
	 */
	(void) acpinex_update_ppd_ranges(devi);

	/* Get soft state structure. */
	if (ddi_soft_state_zalloc(acpinex_softstates, instance)
	    != DDI_SUCCESS) {
		cmn_err(CE_WARN, "!acpinex: failed to allocate soft state "
		    "object in acpinex_attach().");
		return (DDI_FAILURE);
	}
	softsp = ddi_get_soft_state(acpinex_softstates, instance);

	/* Initialize soft state structure */
	softsp->ans_dip = devi;
	(void) ddi_pathname(devi, softsp->ans_path);
	if (ACPI_FAILURE(acpica_get_handle(devi, &softsp->ans_hdl))) {
		ACPINEX_DEBUG(CE_WARN,
		    "!acpinex: failed to get ACPI handle for %s.",
		    softsp->ans_path);
		ddi_soft_state_free(acpinex_softstates, instance);
		return (DDI_FAILURE);
	}
	mutex_init(&softsp->ans_lock, NULL, MUTEX_DRIVER, NULL);

	/* Install event handler for child/descendant objects. */
	if (acpinex_event_scan(softsp, B_TRUE) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "!acpinex: failed to install event handler "
		    "for children of %s.", softsp->ans_path);
	}

	/* nothing to suspend/resume here */
	(void) ddi_prop_update_string(DDI_DEV_T_NONE, devi,
	    "pm-hardware-state", "no-suspend-resume");
	(void) ddi_prop_update_int(DDI_DEV_T_NONE, devi,
	    DDI_NO_AUTODETACH, 1);

	acpinex_fm_init(softsp);
	ddi_report_dev(devi);

	return (DDI_SUCCESS);
}

static int
acpinex_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance;
	acpinex_softstate_t *softsp;

	instance = ddi_get_instance(devi);
	if (instance >= ACPINEX_INSTANCE_MAX) {
		cmn_err(CE_WARN, "acpinex: instance number %d is out of range "
		    "in acpinex_detach(), max %d.",
		    instance, ACPINEX_INSTANCE_MAX - 1);
		return (DDI_FAILURE);
	}

	softsp = ddi_get_soft_state(acpinex_softstates, instance);
	if (softsp == NULL) {
		ACPINEX_DEBUG(CE_WARN, "!acpinex: failed to get soft state "
		    "object for instance %d in acpinex_detach()", instance);
		return (DDI_FAILURE);
	}

	switch (cmd) {
	case DDI_DETACH:
		if (acpinex_event_scan(softsp, B_FALSE) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "!acpinex: failed to uninstall event "
			    "handler for children of %s.", softsp->ans_path);
			return (DDI_FAILURE);
		}
		ddi_remove_minor_node(devi, NULL);
		acpinex_fm_fini(softsp);
		mutex_destroy(&softsp->ans_lock);
		ddi_soft_state_free(acpinex_softstates, instance);
		(void) ddi_prop_update_int(DDI_DEV_T_NONE, devi,
		    DDI_NO_AUTODETACH, 0);
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

static int
name_child(dev_info_t *child, char *name, int namelen)
{
	char *unitaddr;
	char *device_type;

	/* Fill in parent-private data */
	if (ddi_get_parent_data(child) == NULL) {
		struct ddi_parent_private_data *pdptr;
		make_ddi_ppd(child, &pdptr);
		ddi_set_parent_data(child, pdptr);
	}

	name[0] = '\0';

	/*
	 * Name CPUs by cpu_id, the same as FDT systems.
	 */
	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
	    OBP_DEVICETYPE, &device_type) == DDI_PROP_SUCCESS) {
		if (strcmp(device_type, ACPIDEV_TYPE_CPU) == 0) {
			int64_t mpidr = ddi_prop_get_int64(DDI_DEV_T_ANY, child,
			    DDI_PROP_DONTPASS, ACPIDEV_PROP_NAME_MPIDR, -1);

			if (mpidr != -1) {
				int id = cpuinfo_id_for_mpidr((uint64_t)mpidr);

				if (id >= 0) {
					(void) snprintf(name, namelen,
					    "%llx", (unsigned long long)id);
					ddi_prop_free(device_type);
					return (DDI_SUCCESS);
				}
			}
		}

		ddi_prop_free(device_type);
	}

	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
	    ACPIDEV_PROP_NAME_UNIT_ADDR, &unitaddr) == DDI_SUCCESS) {
		(void) strlcpy(name, unitaddr, namelen);
		ddi_prop_free(unitaddr);
	} else {
		ACPINEX_DEBUG(CE_NOTE, "!acpinex: failed to lookup child "
		    "unit-address prop for %p.", (void *)child);

		if (i_ddi_pd_getnreg(child) > 0) {
			(void) snprintf(name, namelen, "%lx",
			    i_ddi_pd_getreg(child, 0)->regspec_addr);
		}
	}

	return (DDI_SUCCESS);
}

static int
init_child(dev_info_t *child)
{
	char name[MAXNAMELEN];

	(void) name_child(child, name, MAXNAMELEN);
	ddi_set_name_addr(child, name);
	if ((ndi_dev_is_persistent_node(child) == 0) &&
	    (ndi_merge_node(child, name_child) == DDI_SUCCESS)) {
		impl_ddi_sunbus_removechild(child);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * Control ops entry point:
 *
 * Requests handled completely:
 *      DDI_CTLOPS_INITCHILD
 *      DDI_CTLOPS_UNINITCHILD
 * All others are passed to the parent.
 */
static int
acpinex_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_ctl_enum_t op, void *arg,
    void *result)
{
	int rval = DDI_SUCCESS;

	switch (op) {
	case DDI_CTLOPS_INITCHILD:
		rval = init_child((dev_info_t *)arg);
		break;

	case DDI_CTLOPS_UNINITCHILD:
		impl_ddi_sunbus_removechild((dev_info_t *)arg);
		break;

	case DDI_CTLOPS_REPORTDEV: {
		if (rdip == (dev_info_t *)0) {
			return (DDI_FAILURE);
		}
		cmn_err(CE_CONT, "?acpinex: %s@%s, %s%d\n",
		    ddi_node_name(rdip), ddi_get_name_addr(rdip),
		    ddi_driver_name(rdip), ddi_get_instance(rdip));
		break;
	}

	default:
		rval = ddi_ctlops(dip, rdip, op, arg, result);
		break;
	}

	return (rval);
}

static int
acpinex_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp)
{
	struct regspec tmp_reg;
	struct regspec *rp;
	ddi_map_req_t mr = *mp;	/* Get private copy of request */
	int error;

	mp = &mr;

	/*
	 * If we are given a register number we need to synthesize a standard
	 * register specification and update the mapping request to reflect
	 * the firmware-specified mapping flags.
	 */
	if (mp->map_type == DDI_MT_RNUMBER) {
		acpidev_regspec_t *ars;
		int *data;
		uint_t nelements;
		int rnumber;
		uint_t bt;
		size_t idx;

		if ((rnumber = mp->map_obj.rnumber) < 0) {
			return (DDI_ME_RNUMBER_RANGE);
		}

		if ((error = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
		    DDI_PROP_DONTPASS, OBP_REG, &data, &nelements)) !=
		    DDI_PROP_SUCCESS) {
			dev_err(rdip, CE_CONT,
			    "?failed to look up %s property: %d\n",
			    OBP_REG, error);
			return (DDI_ME_GENERIC);
		}

		if (nelements == 0 ||
		    (CELLS_1275_TO_BYTES(nelements) %
		    sizeof (acpidev_regspec_t) != 0)) {
			dev_err(rdip, CE_CONT,
			    "?invalid number of cells in %s property\n",
			    OBP_REG);
			ddi_prop_free(data);
			return (DDI_ME_GENERIC);
		}

		idx = rnumber * BYTES_TO_1275_CELLS(sizeof (acpidev_regspec_t));
		if (idx >= nelements) {
			dev_err(rdip, CE_CONT,
			    "?requested index %zu out of range: %u\n",
			    idx, nelements);
			ddi_prop_free(data);
			return (DDI_ME_RNUMBER_RANGE);
		}

		ars = (acpidev_regspec_t *)&data[idx];
		bt = ars->phys_hi;
		tmp_reg.regspec_addr = ars->phys_mid;
		tmp_reg.regspec_addr <<= 32;
		tmp_reg.regspec_addr |= ars->phys_low;
		tmp_reg.regspec_size = ars->size_hi;
		tmp_reg.regspec_size <<= 32;
		tmp_reg.regspec_size |= ars->size_low;
		ars = NULL;

		ddi_prop_free(data);

		if ((bt & ACPIDEV_REG_TYPE_M) == ACPIDEV_REG_TYPE_MEMORY) {
			tmp_reg.regspec_bustype = 0;

			switch (bt & ACPIDEV_REG_MEM_COHERENT_M) {
			case ACPIDEV_REG_MEM_COHERENT_WC:
				mp->map_handlep->ah_acc.devacc_attr_dataorder =
				    DDI_MERGING_OK_ACC;
				break;
			case ACPIDEV_REG_MEM_COHERENT_CA:
				/* fallthrough */
			case ACPIDEV_REG_MEM_COHERENT_PF:
				/* fallthrough */
			case ACPIDEV_REG_MEM_COHERENT_NC:
				/* fallthrough */
			default:
				mp->map_handlep->ah_acc.devacc_attr_dataorder =
				    DDI_STRICTORDER_ACC;
				break;
			}

			mp->map_prot &= ~(PROT_READ|PROT_WRITE);
			mp->map_prot |= PROT_READ;
			if (bt & ACPIDEV_REG_MEM_WRITABLE) {
				mp->map_prot |= PROT_WRITE;
			}
		} else if ((bt & ACPIDEV_REG_TYPE_M) == ACPIDEV_REG_TYPE_IO) {
			tmp_reg.regspec_bustype = 1;
			dev_err(rdip, CE_CONT, "?I/O mapping requested\n");
		} else {
			dev_err(rdip, CE_WARN, "unknown bus type %u",
			    bt & ACPIDEV_REG_TYPE_M);
			return (DDI_ME_GENERIC);
		}

		mp->map_type = DDI_MT_REGSPEC;
		mp->map_obj.rp = &tmp_reg;
	}

	/*
	 * Adjust offset and length corresponding to called values...
	 * XXX: A non-zero length means override the one in the regspec.
	 * XXX: (Regardless of what's in the parent's range)
	 */

	tmp_reg = *(mp->map_obj.rp);	/* Preserve underlying data */
	rp = mp->map_obj.rp = &tmp_reg;	/* Use tmp_reg in request */

	/*
	 * I/O or memory mapping
	 *
	 *	<bustype=0, addr=x, len=x>: memory
	 *	<bustype=1, addr=x, len=x>: i/o
	 */

	/* No compatability I/O on aarch64 */
	ASSERT((rp->regspec_bustype == 0 ||
	    (rp->regspec_bustype == 1 && rp->regspec_addr > 0)));
	/* Normal memory or i/o mapping */
	rp->regspec_addr += offset;

	if (len != 0) {
		rp->regspec_size = (uint64_t)len;
	}

	/*
	 * Apply any parent ranges at this level, if applicable.
	 */
	if ((error = i_ddi_apply_range(dip, rdip, mp->map_obj.rp)) != 0) {
		return (error);
	}

	/*
	 * Call my parents bus_map function with modified values.
	 */
	return (ddi_map(dip, mp, (off_t)0, (off_t)0, vaddrp));
}

static int
acpinex_open(dev_t *devi, int flags __unused,
    int otyp __unused, cred_t *credp __unused)
{
	minor_t minor, instance;
	acpinex_softstate_t *softsp;

	minor = getminor(*devi);
	instance = ACPINEX_GET_INSTANCE(minor);
	if (instance >= ACPINEX_INSTANCE_MAX) {
		ACPINEX_DEBUG(CE_WARN, "!acpinex: instance number %d out of "
		    "range in acpinex_open, max %d.",
		    instance, ACPINEX_INSTANCE_MAX - 1);
		return (EINVAL);
	}

	softsp = ddi_get_soft_state(acpinex_softstates, instance);
	if (softsp == NULL) {
		ACPINEX_DEBUG(CE_WARN, "!acpinex: failed to get soft state "
		    "object for instance %d in acpinex_open().", instance);
		return (EINVAL);
	}

	if (ACPINEX_IS_DEVCTL(minor)) {
		return (0);
	} else {
		ACPINEX_DEBUG(CE_WARN,
		    "!acpinex: invalid minor number %d in acpinex_open().",
		    minor);
		return (EINVAL);
	}
}

static int
acpinex_close(dev_t dev, int flags __unused,
    int otyp __unused, cred_t *credp __unused)
{
	minor_t minor, instance;
	acpinex_softstate_t *softsp;

	minor = getminor(dev);
	instance = ACPINEX_GET_INSTANCE(minor);
	if (instance >= ACPINEX_INSTANCE_MAX) {
		ACPINEX_DEBUG(CE_WARN, "!acpinex: instance number %d out of "
		    "range in acpinex_close(), max %d.",
		    instance, ACPINEX_INSTANCE_MAX - 1);
		return (EINVAL);
	}

	softsp = ddi_get_soft_state(acpinex_softstates, instance);
	if (softsp == NULL) {
		ACPINEX_DEBUG(CE_WARN, "!acpinex: failed to get soft state "
		    "object for instance %d in acpinex_close().", instance);
		return (EINVAL);
	}

	if (ACPINEX_IS_DEVCTL(minor)) {
		return (0);
	} else {
		ACPINEX_DEBUG(CE_WARN,
		    "!acpinex: invalid minor number %d in acpinex_close().",
		    minor);
		return (EINVAL);
	}
}

static int
acpinex_ioctl(dev_t dev, int cmd __unused, intptr_t arg __unused,
    int mode __unused, cred_t *credp __unused, int *rvalp __unused)
{
	int rv;
	minor_t minor, instance;
	acpinex_softstate_t *softsp;

	minor = getminor(dev);
	instance = ACPINEX_GET_INSTANCE(minor);
	if (instance >= ACPINEX_INSTANCE_MAX) {
		ACPINEX_DEBUG(CE_NOTE, "!acpinex: instance number %d out of "
		    "range in acpinex_ioctl(), max %d.",
		    instance, ACPINEX_INSTANCE_MAX - 1);
		return (EINVAL);
	}
	softsp = ddi_get_soft_state(acpinex_softstates, instance);
	if (softsp == NULL) {
		ACPINEX_DEBUG(CE_WARN, "!acpinex: failed to get soft state "
		    "object for instance %d in acpinex_ioctl().", instance);
		return (EINVAL);
	}

	rv = ENOTSUP;
	ACPINEX_DEBUG(CE_WARN,
	    "!acpinex: invalid minor number %d in acpinex_ioctl().", minor);

	return (rv);
}

/*
 * FMA error callback.
 * Register error handling callback with our parent. We will just call
 * our children's error callbacks and return their status.
 */
static int
acpinex_err_callback(dev_info_t *dip, ddi_fm_error_t *derr,
    const void *impl_data __unused)
{
	/* Call our childrens error handlers */
	return (ndi_fm_handler_dispatch(dip, NULL, derr));
}

/*
 * Initialize our FMA resources
 */
static void
acpinex_fm_init(acpinex_softstate_t *softsp)
{
	softsp->ans_fm_cap = DDI_FM_EREPORT_CAPABLE | DDI_FM_ERRCB_CAPABLE |
	    DDI_FM_ACCCHK_CAPABLE | DDI_FM_DMACHK_CAPABLE;

	/*
	 * Request our capability level and get our parent's capability and ibc.
	 */
	ddi_fm_init(softsp->ans_dip, &softsp->ans_fm_cap, &softsp->ans_fm_ibc);
	if (softsp->ans_fm_cap & DDI_FM_ERRCB_CAPABLE) {
		/*
		 * Register error callback with our parent if supported.
		 */
		ddi_fm_handler_register(softsp->ans_dip, acpinex_err_callback,
		    softsp);
	}
}

/*
 * Breakdown our FMA resources
 */
static void
acpinex_fm_fini(acpinex_softstate_t *softsp)
{
	/* Clean up allocated fm structures */
	if (softsp->ans_fm_cap & DDI_FM_ERRCB_CAPABLE) {
		ddi_fm_handler_unregister(softsp->ans_dip);
	}
	ddi_fm_fini(softsp->ans_dip);
}

/*
 * Initialize FMA resources for child devices.
 * Called when child calls ddi_fm_init().
 */
static int
acpinex_fm_init_child(dev_info_t *dip, dev_info_t *tdip __unused,
    int cap __unused, ddi_iblock_cookie_t *ibc)
{
	acpinex_softstate_t *softsp = ddi_get_soft_state(acpinex_softstates,
	    ddi_get_instance(dip));

	*ibc = softsp->ans_fm_ibc;

	return (softsp->ans_fm_cap);
}
