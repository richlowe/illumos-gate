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
 * Copyright 2017 Hayashi Naoyuki
 * Copyright (c) 1992, 2011, Oracle and/or its affiliates. All rights reserved.
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
#include <stdbool.h>


static int
smpl_bus_map(dev_info_t *, dev_info_t *, ddi_map_req_t *, off_t, off_t, caddr_t *);
static int
smpl_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);
static int
smpl_intr_ops(dev_info_t *, dev_info_t *, ddi_intr_op_t, ddi_intr_handle_impl_t *, void *);

struct bus_ops smpl_bus_ops = {
	BUSO_REV,
	smpl_bus_map,
	NULL,
	NULL,
	NULL,
	i_ddi_map_fault,
	NULL,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,
	smpl_ctlops,
	ddi_bus_prop_op,
	NULL,		/* (*bus_get_eventcookie)();	*/
	NULL,		/* (*bus_add_eventcall)();	*/
	NULL,		/* (*bus_remove_eventcall)();	*/
	NULL,		/* (*bus_post_event)();		*/
	NULL,		/* (*bus_intr_ctl)(); */
	NULL,		/* (*bus_config)(); */
	NULL,		/* (*bus_unconfig)(); */
	NULL,		/* (*bus_fm_init)(); */
	NULL,		/* (*bus_fm_fini)(); */
	NULL,		/* (*bus_fm_access_enter)(); */
	NULL,		/* (*bus_fm_access_exit)(); */
	NULL,		/* (*bus_power)(); */
	smpl_intr_ops	/* (*bus_intr_op)(); */
};


static int smpl_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);

/*
 * Internal isa ctlops support routines
 */
struct dev_ops smpl_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	smpl_attach,	/* attach */
	nulldev,		/* detach */
	nodev,			/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&smpl_bus_ops,	/* bus operations */
	NULL,			/* power */
	ddi_quiesce_not_needed,	/* quiesce */
};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This is simple-bus bus driver */
	"simple-bus nexus driver",
	&smpl_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	int	err;

	if ((err = mod_install(&modlinkage)) != 0)
		return (err);

	return (0);
}

int
_fini(void)
{
	int	err;

	if ((err = mod_remove(&modlinkage)) != 0)
		return (err);

	return (0);
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

	return DDI_SUCCESS;
}

static int get_address_cells(pnode_t node)
{
	int address_cells = 0;

	while (node > 0) {
		int len = prom_getproplen(node, "#address-cells");
		if (len > 0) {
			ASSERT(len == sizeof(int));
			int prop;
			prom_getprop(node, "#address-cells", (caddr_t)&prop);
			address_cells = ntohl(prop);
			break;
		}
		node = prom_parentnode(node);
	}
	return address_cells;
}

static int get_size_cells(pnode_t node)
{
	int size_cells = 0;

	while (node > 0) {
		int len = prom_getproplen(node, "#size-cells");
		if (len > 0) {
			ASSERT(len == sizeof(int));
			int prop;
			prom_getprop(node, "#size-cells", (caddr_t)&prop);
			size_cells = ntohl(prop);
			break;
		}
		node = prom_parentnode(node);
	}
	return size_cells;
}

static int get_interrupt_cells(pnode_t node)
{
	int interrupt_cells = 0;

	while (node > 0) {
		int len = prom_getproplen(node, "#interrupt-cells");
		if (len > 0) {
			ASSERT(len == sizeof(int));
			int prop;
			prom_getprop(node, "#interrupt-cells", (caddr_t)&prop);
			interrupt_cells = ntohl(prop);
			break;
		}
		len = prom_getproplen(node, "interrupt-parent");
		if (len > 0) {
			ASSERT(len == sizeof(int));
			int prop;
			prom_getprop(node, "interrupt-parent", (caddr_t)&prop);
			node = prom_findnode_by_phandle(ntohl(prop));
			continue;
		}
		node = prom_parentnode(node);
	}
	return interrupt_cells;
}

static int
smpl_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp, off_t offset, off_t len, caddr_t *vaddrp)
{
	ddi_map_req_t mr;
	int error;

	int addr_cells = get_address_cells(ddi_get_nodeid(rdip));
	int size_cells = get_size_cells(ddi_get_nodeid(rdip));
	int interrupt_cells = get_interrupt_cells(ddi_get_nodeid(rdip));

	int parent_addr_cells = get_address_cells(ddi_get_nodeid(dip));
	int parent_size_cells = get_size_cells(ddi_get_nodeid(dip));

	ASSERT(addr_cells == 1 || addr_cells == 2);
	ASSERT(size_cells == 1 || size_cells == 2);

	ASSERT(parent_addr_cells == 1 || parent_addr_cells == 2);
	ASSERT(parent_size_cells == 1 || parent_size_cells == 2);

	int *regs;
	int range_index = 0;
	struct regspec reg = {0};
	struct rangespec range = {0};

	uint32_t *rangep;
	int rangelen;

	bool parent_has_ranges = false;
	if (ddi_prop_get_int_array(DDI_DEV_T_ANY, ddi_get_parent(dip),
	    DDI_PROP_DONTPASS, "ranges", (caddr_t)&rangep, &rangelen) ==
	    DDI_SUCCESS) {
		parent_has_ranges = true;
		if (rangelen) {
			ddi_prop_free(rangep);
		}
	}

	if (ddi_prop_get_int_array(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "ranges", (caddr_t)&rangep, &rangelen) !=
	    DDI_SUCCESS || rangelen == 0) {
		rangelen = 0;
		rangep = NULL;
	}

	if (mp->map_type == DDI_MT_RNUMBER) {
		int reglen;
		int rnumber = mp->map_obj.rnumber;
		uint32_t *rp;

		if (ddi_prop_get_int_array(DDI_DEV_T_ANY, rdip,
		    DDI_PROP_DONTPASS, "reg", (caddr_t)&rp, &reglen) !=
		    DDI_SUCCESS || reglen == 0) {
			if (rangep != NULL) {
				ddi_prop_free(rangep);
			}
			return (DDI_ME_RNUMBER_RANGE);
		}

		ASSERT((reglen % (addr_cells + size_cells)) == 0);

		if (rnumber < 0 || rnumber >= n) {
			if (rangep != NULL) {
				ddi_prop_free(rangep);
			}
			ddi_prop_free(rp);
			return (DDI_ME_RNUMBER_RANGE);
		}

		uint64_t addr = 0;
		uint64_t size = 0;

		if (rangep) {
			range_index = ntohl(rp[(addr_cells + size_cells) * rnumber + 0]);
			for (int i = 1; i < addr_cells; i++) {
				addr <<= 32;
				addr |= ntohl(rp[(addr_cells + size_cells) * rnumber + i - 1]);
			}
		} else {
			for (int i = 0; i < addr_cells; i++) {
				addr <<= 32;
				addr |= ntohl(rp[(addr_cells + size_cells) * rnumber + i]);
			}
		}
		for (int i = 0; i < size_cells; i++) {
			size <<= 32;
			size |= ntohl(rp[(addr_cells + size_cells) * rnumber + addr_cells + i]);
		}

		ddi_prop_free(rp);

		ASSERT((addr & 0xffff000000000000ul) == 0);
		ASSERT((size & 0xfffff00000000000ul) == 0);
		reg.regspec_bustype = (addr >> 32) ;
		reg.regspec_bustype |= (((size >> 32)) << 16);
		reg.regspec_addr    = (addr & 0xffffffff);
		reg.regspec_size    = (size & 0xffffffff);
	} else if (mp->map_type == DDI_MT_REGSPEC) {
		reg = *mp->map_obj.rp;
		range_index = (reg.regspec_bustype >> 28);
	} else {
		return (DDI_ME_INVAL);
	}

	if (rangep) {
		int i;
		int n = rangelen / (addr_cells + parent_addr_cells + size_cells);
		for (i = 0; i < n; i++) {
			if (rangep[(addr_cells + parent_addr_cells + size_cells) * i + 0] == range_index) {
				uint64_t addr = 0;
				uint64_t regspec_addr = reg.regspec_addr;
				regspec_addr |= (((uint64_t)(reg.regspec_bustype & 0xffff)) << 32);
				for (int j = 1; j < addr_cells; j++) {
					addr <<= 32;
					addr |= ntohl(rangep[(addr_cells + parent_addr_cells + size_cells) * i + j]);
				}
				regspec_addr += addr;

				addr = 0;
				if (parent_has_ranges) {
					reg.regspec_bustype &= ~0xf0000000;
					reg.regspec_bustype |= (ntohl(rangep[(addr_cells + parent_addr_cells + size_cells) * i + addr_cells]) << 28);
					for (int j = 1; j < parent_addr_cells; j++) {
						addr <<= 32;
						addr |= ntohl(rangep[(addr_cells + parent_addr_cells + size_cells) * i + addr_cells + j - 1]);
					}
				} else {
					for (int j = 0; j < parent_addr_cells; j++) {
						addr <<= 32;
						addr |= ntohl(rangep[(addr_cells + parent_addr_cells + size_cells) * i + addr_cells + j]);
					}
				}
				regspec_addr += addr;

				reg.regspec_addr = regspec_addr & 0xffffffff;
				reg.regspec_bustype &= ~0xffff;
				reg.regspec_bustype |= ((regspec_addr >> 32) & 0xffff);
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
	int ret = ddi_map(dip, mp, offset, 0, vaddrp);
	return ret;
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
	return ret;
}

static int
get_pil(dev_info_t *rdip)
{
	static struct {
		const char *name;
		int pil;
	} name_to_pil[] = {
		{"serial",			12},
		{"Ethernet controller", 	6},
		{ NULL}
	};
	const char *type_name[] = {
		"device_type",
		"model",
		NULL
	};

	pnode_t node = ddi_get_nodeid(rdip);
	for (int i = 0; type_name[i]; i++) {
		int len = prom_getproplen(node, type_name[i]);
		if (len <= 0) {
			continue;
		}
		char *name = __builtin_alloca(len);
		prom_getprop(node, type_name[i], name);

		for (int j = 0; name_to_pil[j].name; j++) {
			if (strcmp(name_to_pil[j].name, name) == 0) {
				return (name_to_pil[j].pil);
			}
		}
	}
	return (5);
}

static int
smpl_intr_ops(dev_info_t *pdip, dev_info_t *rdip, ddi_intr_op_t intr_op,
    ddi_intr_handle_impl_t *hdlp, void *result)
{
	switch (intr_op) {
	case DDI_INTROP_GETCAP:
		*(int *)result = DDI_INTR_FLAG_LEVEL;
		break;

	case DDI_INTROP_ALLOC:
		*(int *)result = hdlp->ih_scratch1;
		break;

	case DDI_INTROP_FREE:
		break;

	case DDI_INTROP_GETPRI:
		if (hdlp->ih_pri == 0) {
			hdlp->ih_pri = get_pil(rdip);
		}

		*(int *)result = hdlp->ih_pri;
		break;
	case DDI_INTROP_SETPRI:
		if (*(int *)result > LOCK_LEVEL)
			return (DDI_FAILURE);
		hdlp->ih_pri = *(int *)result;
		break;

	case DDI_INTROP_ADDISR:
		break;
	case DDI_INTROP_REMISR:
		if (hdlp->ih_type != DDI_INTR_TYPE_FIXED)
			return (DDI_FAILURE);
		break;
	case DDI_INTROP_ENABLE:
		{
			int interrupt_cells = get_interrupt_cells(ddi_get_nodeid(rdip));
			switch (interrupt_cells) {
			case 1:
			case 3:
				break;
			default:
				return (DDI_FAILURE);
			}

			int *irupts_prop;
			int irupts_len;
			if (ddi_prop_get_int_array(DDI_DEV_T_ANY, rdip,
			    DDI_PROP_DONTPASS, "interrupts",
			    (caddr_t)&irupts_prop, &irupts_len) !=
			    DDI_SUCCESS || irupts_len == 0) {
				return (DDI_FAILURE);
			}
			if (interrupt_cells * hdlp->ih_inum >= irupts_len) {
				ddi_prop_free(irupts_prop);
				return (DDI_FAILURE);
			}

			int vec;
			int grp;
			int cfg;
			switch (interrupt_cells) {
			case 1:
				grp = 0;
				vec = ntohl((uint32_t)irupts_prop[interrupt_cells * hdlp->ih_inum + 0]);
				cfg = 4;
				break;
			case 3:
				grp = irupts_prop[interrupt_cells *
				    hdlp->ih_inum + 0];
				vec = irupts_prop[interrupt_cells *
				    hdlp->ih_inum + 1];
				cfg = irupts_prop[interrupt_cells *
				    hdlp->ih_inum + 2];
				break;
			default:
				ddi_prop_free(irupts_prop);
				return (DDI_FAILURE);
			}

			ddi_prop_free(irupts_prop);

			switch (grp) {
			case 1:
				hdlp->ih_vector = vec + 16;
				break;
			case 0:
			default:
				hdlp->ih_vector = vec + 32;
				break;
			}

			cfg &= 0xFF;
			switch (cfg) {
			case 1:
				gic_config_irq(hdlp->ih_vector, true);
				break;
			default:
				gic_config_irq(hdlp->ih_vector, false);
				break;
			}

			if (!add_avintr((void *)hdlp, hdlp->ih_pri,
				    hdlp->ih_cb_func, DEVI(rdip)->devi_name, hdlp->ih_vector,
				    hdlp->ih_cb_arg1, hdlp->ih_cb_arg2, NULL, rdip))
				return (DDI_FAILURE);
		}
		break;

	case DDI_INTROP_DISABLE:
		rem_avintr((void *)hdlp, hdlp->ih_pri, hdlp->ih_cb_func, hdlp->ih_vector);
		break;
	case DDI_INTROP_SETMASK:
	case DDI_INTROP_CLRMASK:
	case DDI_INTROP_GETPENDING:
		return (DDI_FAILURE);
	case DDI_INTROP_NAVAIL:
		{
			int interrupt_cells = get_interrupt_cells(ddi_get_nodeid(rdip));
			int irupts_len;
			if (interrupt_cells != 0 &&
			    ddi_getproplen(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS, "interrupts", &irupts_len) == DDI_SUCCESS) {
				*(int *)result = irupts_len / (interrupt_cells * sizeof(int));
			} else {
				return (DDI_FAILURE);
			}
		}
		break;
	case DDI_INTROP_NINTRS:
		{
			int interrupt_cells = get_interrupt_cells(ddi_get_nodeid(rdip));
			int irupts_len;
			if (interrupt_cells != 0 &&
			    ddi_getproplen(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS, "interrupts", &irupts_len) == DDI_SUCCESS) {
				*(int *)result = irupts_len / (interrupt_cells * sizeof(int));
			} else {
				return (DDI_FAILURE);
			}
		}
		break;
	case DDI_INTROP_SUPPORTED_TYPES:
		*(int *)result = DDI_INTR_TYPE_FIXED;	/* Always ... */
		break;
	default:
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}
