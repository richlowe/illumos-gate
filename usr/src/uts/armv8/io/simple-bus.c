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
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/autoconf.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/ddidmareq.h>
#include <sys/ddi_impldefs.h>
#include <sys/dma_engine.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/mach_intr.h>
#include <sys/note.h>
#include <sys/avintr.h>
#include <sys/gic.h>
#include <sys/promif.h>
#include <sys/sysmacros.h>
#include <sys/obpdefs.h>

static int smpl_bus_map(dev_info_t *, dev_info_t *, ddi_map_req_t *, off_t,
    off_t, caddr_t *);
static int smpl_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
    void *, void *);
static int smpl_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);

static struct bus_ops smpl_bus_ops = {
	.busops_rev		= BUSO_REV,
	.bus_map		= smpl_bus_map,
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

/*ARGSUSED*/
static int
smpl_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int rval;
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
get_address_cells(pnode_t node)
{
	int address_cells = 0;

	while (node > 0) {
		int len = prom_getproplen(node, OBP_ADDRESS_CELLS);
		if (len > 0) {
			ASSERT(len == sizeof (int));
			int prop;
			prom_getprop(node, OBP_ADDRESS_CELLS, (caddr_t)&prop);
			address_cells = ntohl(prop);
			break;
		}
		node = prom_parentnode(node);
	}
	return (address_cells);
}

static int
get_size_cells(pnode_t node)
{
	int size_cells = 0;

	while (node > 0) {
		int len = prom_getproplen(node, "#size-cells");
		if (len > 0) {
			ASSERT(len == sizeof (int));
			int prop;
			prom_getprop(node, "#size-cells", (caddr_t)&prop);
			size_cells = ntohl(prop);
			break;
		}
		node = prom_parentnode(node);
	}
	return (size_cells);
}

static int
smpl_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp, off_t offset,
    off_t len, caddr_t *vaddrp)
{
	ddi_map_req_t mr;
	dev_info_t *pdip = ddi_get_parent(dip);
	int error;

	int addr_cells = get_address_cells(ddi_get_nodeid(dip));
	int size_cells = get_size_cells(ddi_get_nodeid(dip));

	int parent_addr_cells = get_address_cells(ddi_get_nodeid(pdip));
	int parent_size_cells = get_size_cells(ddi_get_nodeid(pdip));

	ASSERT(addr_cells == 1 || addr_cells == 2);
	ASSERT(size_cells == 1 || size_cells == 2);

	ASSERT(parent_addr_cells == 1 || parent_addr_cells == 2);
	ASSERT(parent_size_cells == 1 || parent_size_cells == 2);

	int *regs;
	struct regspec reg = {0};
	struct rangespec range = {0};

	uint32_t *rangep;
	uint_t rangelen;

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, OBP_RANGES, (int **)&rangep, &rangelen) !=
	    DDI_SUCCESS || rangelen == 0) {
		rangelen = 0;
		rangep = NULL;
	}

	if (mp->map_type == DDI_MT_RNUMBER) {
		uint_t reglen;
		int rnumber = mp->map_obj.rnumber;
		uint32_t *rp;

		if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
		    DDI_PROP_DONTPASS, OBP_REG, (int **)&rp, &reglen) !=
		    DDI_SUCCESS || reglen == 0) {
			if (rangep != NULL) {
				ddi_prop_free(rangep);
			}
			return (DDI_ME_RNUMBER_RANGE);
		}

		int n = reglen / addr_cells + size_cells;
		ASSERT(reglen % (addr_cells + size_cells) == 0);

		if (rnumber < 0 || rnumber >= n) {
			if (rangep != NULL) {
				ddi_prop_free(rangep);
			}
			ddi_prop_free(rp);
			return (DDI_ME_RNUMBER_RANGE);
		}

		uint64_t addr = 0;
		uint64_t size = 0;

		for (int i = 0; i < addr_cells; i++) {
			addr <<= 32;
			addr |= rp[(addr_cells + size_cells) * rnumber + i];
		}
		for (int i = 0; i < size_cells; i++) {
			size <<= 32;
			size |= rp[(addr_cells + size_cells) *
			    rnumber + addr_cells + i];
		}

		ddi_prop_free(rp);

		ASSERT((addr & 0xffff000000000000ul) == 0);
		ASSERT((size & 0xffff000000000000ul) == 0);
		reg.regspec_bustype = ((addr >> 32) & 0xffff);
		reg.regspec_bustype |= (((size >> 32)) << 16);
		reg.regspec_addr    = (addr & 0xffffffff);
		reg.regspec_size    = (size & 0xffffffff);
	} else if (mp->map_type == DDI_MT_REGSPEC) {
		reg = *mp->map_obj.rp;
		uint64_t rel_addr = (reg.regspec_bustype & 0xffff);
		rel_addr <<= 32;
		rel_addr |= (reg.regspec_addr & 0xffffffff);
	} else {
		return (DDI_ME_INVAL);
	}

	if (rangep != NULL) {
		int i;
		int ranges_cells = (addr_cells + parent_addr_cells + size_cells);
		int n = rangelen / ranges_cells;

		for (i = 0; i < n; i++) {
			uint64_t base = 0;
			uint64_t target = 0;
			uint64_t rsize = 0;
			for (int j = 0; j < addr_cells; j++) {
				base <<= 32;
				base += rangep[ranges_cells * i + j];
			}
			for (int j = 0; j < parent_addr_cells; j++) {
				target <<= 32;
				target += rangep[ranges_cells * i + addr_cells + j];
			}
			for (int j = 0; j < size_cells; j++) {
				rsize <<= 32;
				rsize += rangep[ranges_cells * i + addr_cells + parent_addr_cells + j];
			}

			uint64_t rel_addr = (reg.regspec_bustype & 0xffff);
			rel_addr <<= 32;
			rel_addr |= (reg.regspec_addr & 0xffffffff);

			if (base <= rel_addr && rel_addr <= base + rsize - 1) {
				rel_addr = (rel_addr - base) + target;

				reg.regspec_bustype &= ~0xffff;
				reg.regspec_bustype |= ((rel_addr >> 32) &
				    0xffff);
				reg.regspec_addr    = (rel_addr & 0xffffffff);

				break;
			}
		}

		ddi_prop_free(rangep);

		if (i == n) {
			return (DDI_FAILURE);
		}
	}

	mr = *mp;
	mr.map_type = DDI_MT_REGSPEC;
	mr.map_obj.rp = &reg;
	mp = &mr;
	return (ddi_map(dip, mp, offset, 0, vaddrp));
}

static int
smpl_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	struct regspec *child_rp;
	uint_t reglen;
	int nreg;
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
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?%s%d at %s%d",
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
