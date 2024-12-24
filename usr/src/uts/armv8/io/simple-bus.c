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
 * Copyright (c) 1992, 2011, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2024 Michael van der Westhuizen
 */

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>

static int smpl_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
    void *, void *);
static int smpl_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);

static struct bus_ops smpl_bus_ops = {
	.busops_rev		= BUSO_REV,
	.bus_map		= i_ddi_bus_map,
	.bus_get_intrspec	= NULL,	/* obsolete */
	.bus_add_intrspec	= NULL,	/* obsolete */
	.bus_remove_intrspec	= NULL,	/* obsolete */
	.bus_map_fault		= i_ddi_map_fault,
	.bus_dma_map		= NULL,
	.bus_dma_allochdl	= ddi_dma_allochdl,
	.bus_dma_freehdl	= ddi_dma_freehdl,
	.bus_dma_bindhdl	= ddi_dma_bindhdl,
	.bus_dma_unbindhdl	= ddi_dma_unbindhdl,
	.bus_dma_flush		= ddi_dma_flush,
	.bus_dma_win		= ddi_dma_win,
	.bus_dma_ctl		= ddi_dma_mctl,
	.bus_ctl		= smpl_ctlops,
	.bus_prop_op		= ddi_bus_prop_op,
	.bus_get_eventcookie	= NULL,
	.bus_add_eventcall	= NULL,
	.bus_remove_eventcall	= NULL,
	.bus_post_event		= NULL,
	.bus_intr_ctl		= NULL,	/* obsolete */
	.bus_config		= NULL,
	.bus_unconfig		= NULL,
	.bus_fm_init		= NULL,
	.bus_fm_fini		= NULL,
	.bus_fm_access_enter	= NULL,
	.bus_fm_access_exit	= NULL,
	.bus_power		= NULL,
	.bus_intr_op		= i_ddi_intr_ops,
	.bus_hp_op		= NULL
};

static struct dev_ops smpl_ops = {
	.devo_rev		= DEVO_REV,
	.devo_refcnt		= 0,
	.devo_getinfo		= ddi_no_info,
	.devo_identify		= nulldev,
	.devo_probe		= nulldev,
	.devo_attach		= smpl_attach,
	.devo_detach		= nulldev,
	.devo_reset		= nodev,
	.devo_cb_ops		= NULL,
	.devo_bus_ops		= &smpl_bus_ops,
	.devo_power		= NULL,
	.devo_quiesce		= ddi_quiesce_not_needed,
};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	.drv_modops		= &mod_driverops,
	.drv_linkinfo		= "simple-bus nexus driver",
	.drv_dev_ops		= &smpl_ops,
};

static struct modlinkage modlinkage = {
	.ml_rev			= MODREV_1,
	.ml_linkage		= { &modldrv, NULL }
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Nexus implementation.
 */

static int
smpl_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);
	return (DDI_SUCCESS);
}

static int
smpl_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	int ret;

	switch (ctlop) {
	case DDI_CTLOPS_INITCHILD:
		ret = impl_ddi_sunbus_initchild((dev_info_t *)arg);
		break;

	case DDI_CTLOPS_UNINITCHILD:
		impl_ddi_sunbus_removechild((dev_info_t *)arg);
		ret = DDI_SUCCESS;
		break;

	case DDI_CTLOPS_REPORTDEV:
		if (rdip == NULL)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?%s%d at %s%d\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip),
		    ddi_driver_name(dip), ddi_get_instance(dip));
		ret = DDI_SUCCESS;
		break;

	default:
		ret = ddi_ctlops(dip, rdip, ctlop, arg, result);
		break;
	}

	return (ret);
}
