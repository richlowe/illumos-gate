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
#include <sys/sysmacros.h>

static int
smpl_bus_map(dev_info_t *, dev_info_t *, ddi_map_req_t *, off_t, off_t,
    caddr_t *);
static int
smpl_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);
static int
smpl_intr_ops(dev_info_t *, dev_info_t *, ddi_intr_op_t,
    ddi_intr_handle_impl_t *, void *);

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

	return (DDI_SUCCESS);
}

static int
get_interrupt_cells(pnode_t node)
{
	int interrupt_cells = 0;

	while (node > 0) {
		int len = prom_getproplen(node, "#interrupt-cells");
		if (len > 0) {
			ASSERT(len == sizeof (int));
			int prop;
			prom_getprop(node, "#interrupt-cells", (caddr_t)&prop);
			interrupt_cells = ntohl(prop);
			break;
		}
		len = prom_getproplen(node, "interrupt-parent");
		if (len > 0) {
			ASSERT(len == sizeof (int));
			int prop;
			prom_getprop(node, "interrupt-parent", (caddr_t)&prop);
			node = prom_findnode_by_phandle(ntohl(prop));
			continue;
		}
		node = prom_parentnode(node);
	}
	return (interrupt_cells);
}

typedef struct {
	enum {
		SBR_REGS_1x1,
		SBR_REGS_1x2,
		SBR_REGS_2x1,
		SBR_REGS_2x2,
	} sbr_type;
	union {
		uint32_t sbr_raw[4];
		struct {
			uint32_t addr;
			uint32_t size;
		} sbr_1x1;
		struct {
			uint32_t addr;
			uint32_t size_hi;
			uint32_t size_lo;
		} sbr_1x2;
		struct {
			uint32_t addr_hi;
			uint32_t addr_lo;
			uint32_t size;
		} sbr_2x1;
		struct {
			uint32_t addr_hi;
			uint32_t addr_lo;
			uint32_t size_hi;
			uint32_t size_lo;
		} sbr_2x2;
	};
} smpl_bus_regs_t;

void
smpl_bus_cook_regs(uint32_t *regs, smpl_bus_regs_t *out, int addr_cells,
    int size_cells)
{
	ASSERT(addr_cells == 1 || addr_cells == 2);
	ASSERT(size_cells == 1 || size_cells == 2);

	bcopy(regs, out->sbr_raw,
	    CELLS_1275_TO_BYTES(addr_cells + size_cells));

	if (addr_cells == 1 && size_cells == 1)
		out->sbr_type = SBR_REGS_1x1;
	else if (addr_cells == 1 && size_cells == 2)
		out->sbr_type = SBR_REGS_1x2;
	else if (addr_cells == 2 && size_cells == 1)
		out->sbr_type = SBR_REGS_2x1;
	else if (addr_cells == 2 && size_cells == 2)
		out->sbr_type = SBR_REGS_2x2;
}

static inline int
smpl_bus_regno_to_offset(int regno, int addr_cells, int size_cells)
{
	return (regno * (addr_cells + size_cells));
}

/*
 * Apply our ranges property to the child's reg property and return addresses
 * in the parent bus's space
 *
 * XXXROOTNEX: We can't use `i_ddi_apply_range` since there are no ranges in
 * the parent data, because we can't guarantee the address and size formats
 *
 * XXXROOTNEX: We should probably look like `i_ddi_apply_range` API wise, but
 * let's not right now.
 */
static int
smpl_bus_apply_range(dev_info_t *dip, struct regspec64 *out)
{
	dev_info_t *parent;
	uint32_t *rangep;
	uint_t rangelen;
	int parent_addr_cells, parent_size_cells;
	int child_addr_cells, child_size_cells;

	ASSERT3P(dip, !=, NULL);

	parent = ddi_get_parent(dip);
	ASSERT3P(parent, !=, NULL);

	child_addr_cells = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "#address-cells", 0);
	child_size_cells = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "#size-cells", 0);
	parent_addr_cells = ddi_prop_get_int(DDI_DEV_T_ANY, parent,
	    DDI_PROP_DONTPASS, "#address-cells", 0);
	parent_size_cells = ddi_prop_get_int(DDI_DEV_T_ANY, parent,
	    DDI_PROP_DONTPASS, "#size-cells", 0);

	VERIFY3S(parent_addr_cells, !=, 0);
	VERIFY3S(parent_size_cells, !=, 0);
	VERIFY3S(child_addr_cells, !=, 0);
	VERIFY3S(child_size_cells, !=, 0);

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "ranges", (int **)&rangep, &rangelen) != DDI_SUCCESS) {
		dev_err(dip, CE_WARN, "error reading ranges property");
		return (DDI_SUCCESS);
	} else if (rangelen == 0) {
		ddi_prop_free(rangep);
		dev_err(dip, CE_WARN, "0-length ranges property");
		return (DDI_SUCCESS);
	}

	int i;
	int ranges_cells = (child_addr_cells + parent_addr_cells +
	    child_size_cells);
	int n = rangelen / ranges_cells;

	for (i = 0; i < n; i++) {
		uint64_t base = 0;
		uint64_t target = 0;
		uint64_t rsize = 0;
		for (int j = 0; j < child_addr_cells; j++) {
			base <<= 32;
			base += rangep[ranges_cells * i + j];
		}
		for (int j = 0; j < parent_addr_cells; j++) {
			target <<= 32;
			target += rangep[ranges_cells * i +
			    child_addr_cells + j];
		}
		for (int j = 0; j < child_size_cells; j++) {
			rsize <<= 32;
			rsize += rangep[ranges_cells * i + child_addr_cells +
			    parent_addr_cells + j];
		}

		uint64_t rel_addr = out->regspec_addr;
		uint64_t rel_offset = out->regspec_addr - base;

		if (base <= rel_addr && rel_addr <= base + rsize - 1) {
			out->regspec_addr = rel_offset + target;
			out->regspec_size = MIN(out->regspec_size, (rsize -
			    rel_offset));
			break;
		}

		ddi_prop_free(rangep);

		/* Not found */
		if (i == n) {
			dev_err(dip, CE_WARN, "specified register bounds "
			    "are outside range");
			return (DDI_FAILURE);
		}
	}

	return (DDI_SUCCESS);
}

static int
smpl_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp, off_t offset,
    off_t len, caddr_t *vaddrp)
{
	smpl_bus_regs_t child_rp;
	ddi_map_req_t mr;
	struct regspec64 reg = {0};
	int error, addr_cells, size_cells;
	uint32_t *cregs = NULL;

	if ((addr_cells = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "#address-cells", 0)) == 0) {
		dev_err(rdip, CE_WARN, "couldn't read #address-cells");
		return (DDI_ME_INVAL); /* XXXROOTNEX */
	}

	if ((size_cells = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "#size-cells", 0)) == 0) {
		dev_err(rdip, CE_WARN, "couldn't read #size-cells");
		return (DDI_ME_INVAL); /* XXXROOTNEX */
	}

	switch (mp->map_type) {
	case DDI_MT_REGSPEC:
		smpl_bus_cook_regs((uint32_t *)mp->map_obj.rp, &child_rp,
		    addr_cells, size_cells);
		break;
	case DDI_MT_RNUMBER: {
		uint_t n;
		int rnumber = mp->map_obj.rnumber;

		if ((ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
		    DDI_PROP_DONTPASS, "reg", (int **)&cregs, &n) !=
		    DDI_SUCCESS) || (n == 0)) {
			dev_err(rdip, CE_WARN,
			    "couldn't read reg property\n");
			return (DDI_ME_RNUMBER_RANGE);
		}

		ASSERT(n % (addr_cells + size_cells) == 0);

		if (rnumber < 0 || rnumber >= n) {
			ddi_prop_free(cregs);
			return (DDI_ME_RNUMBER_RANGE);
		}

		int off = smpl_bus_regno_to_offset(rnumber, addr_cells,
		    size_cells);
		smpl_bus_cook_regs(&cregs[off], &child_rp,
		    addr_cells, size_cells);

		break;
	}
	default:
		return (DDI_ME_INVAL);
	}

	/* Convert our child regspec into our parents */
	switch (child_rp.sbr_type) {
	case SBR_REGS_1x1:
		reg.regspec_bustype = 0;
		reg.regspec_addr = child_rp.sbr_1x1.addr;
		reg.regspec_size = child_rp.sbr_1x1.size;
		break;
	case SBR_REGS_1x2:
		reg.regspec_bustype = 0;
		reg.regspec_addr = child_rp.sbr_1x2.addr;
		reg.regspec_size = ((uint64_t)child_rp.sbr_1x2.size_hi << 32) |
		    child_rp.sbr_1x2.size_lo;
		break;
	case SBR_REGS_2x1:
		reg.regspec_bustype = 0;
		reg.regspec_addr = ((uint64_t)child_rp.sbr_2x1.addr_hi << 32) |
		    child_rp.sbr_2x1.addr_lo;
		reg.regspec_size = child_rp.sbr_2x1.size;
		break;
	case SBR_REGS_2x2:
		reg.regspec_bustype = 0;
		reg.regspec_addr = ((uint64_t)child_rp.sbr_2x2.addr_hi << 32) |
		    child_rp.sbr_2x2.addr_lo;
		reg.regspec_size = ((uint64_t)child_rp.sbr_2x2.size_hi << 32) |
		    child_rp.sbr_2x2.size_lo;
		break;
	}

	if (cregs != NULL)
		ddi_prop_free(cregs);

	/* Adjust our reg property with offset and length */
	if (reg.regspec_addr + offset < MAX(reg.regspec_addr, offset))
		return (DDI_FAILURE);

	reg.regspec_addr += offset;
	if (len)
		reg.regspec_size = len;


#ifdef	DDI_MAP_DEBUG
	dev_err(dip, CE_CONT, "<%s,%s> <0x%lx, 0x%lx, %ld> "
	    "offset %ld len %ld handle 0x%p\n", ddi_get_name(dip),
	    ddi_get_name(rdip), reg.regspec_bustype, reg.regspec_addr,
	    reg.regspec_size, offset, len, mp->map_handlep);
#endif	/* DDI_MAP_DEBUG */

	if ((error = smpl_bus_apply_range(dip, &reg)) != DDI_SUCCESS)
		return (DDI_SUCCESS);

	mr = *mp;
	mr.map_type = DDI_MT_REGSPEC;
	mr.map_obj.rp = (struct regspec *)&reg;
	mr.map_flags |= DDI_MF_EXT_REGSPEC;
	mp = &mr;
#ifdef	DDI_MAP_DEBUG
	cmn_err(CE_CONT, "             <%s,%s> <0x%" PRIx64 ", 0x%" PRIx64
	    ", %" PRId64 "> offset %ld len %ld handle 0x%p\n",
	    ddi_get_name(dip), ddi_get_name(rdip), reg.regspec_bustype,
	    reg.regspec_addr, reg.regspec_size, offset, len, mp->map_handlep);
#endif	/* DDI_MAP_DEBUG */

	return (ddi_map(dip, mp, 0, 0, vaddrp));
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

static int
get_pil(dev_info_t *rdip)
{
	static struct {
		const char *name;
		int pil;
	} name_to_pil[] = {
		{"serial",			12},
		{"Ethernet controller",		6},
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
			pnode_t node = ddi_get_nodeid(rdip);
			int interrupt_cells = get_interrupt_cells(node);

			/*
			 * XXXROOTNEX:
			 * 1 == illumos normal, <vec>
			 * 3 == arm,gic <type, vec, flags>
			 */
			VERIFY(interrupt_cells == 1 || interrupt_cells == 3);

			int *irupts_prop;
			uint_t irupts_len;
			if ((ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
			    DDI_PROP_DONTPASS, "interrupts",
			    (int **)&irupts_prop, &irupts_len) !=
			    DDI_SUCCESS) || (irupts_len == 0)) {
				return (DDI_FAILURE);
			}

			if ((interrupt_cells * hdlp->ih_inum) >=
			    CELLS_1275_TO_BYTES(irupts_len)) {
				kmem_free(irupts_prop, irupts_len);
				return (DDI_FAILURE);
			}

			int vec;
			int grp;
			int cfg;
			switch (interrupt_cells) {
			case 1:
				grp = 0;
				vec = (uint32_t)irupts_prop[interrupt_cells *
				    hdlp->ih_inum + 0];
				cfg = 4;
				break;
			case 3:
				grp = (uint32_t)irupts_prop[interrupt_cells *
				    hdlp->ih_inum + 0];
				vec = (uint32_t)irupts_prop[interrupt_cells *
				    hdlp->ih_inum + 1];
				cfg = (uint32_t)irupts_prop[interrupt_cells *
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
				gic_config_irq(hdlp->ih_vector, B_TRUE);
				break;
			default:
				gic_config_irq(hdlp->ih_vector, B_FALSE);
				break;
			}

			if (!add_avintr((void *)hdlp, hdlp->ih_pri,
			    hdlp->ih_cb_func, DEVI(rdip)->devi_name,
			    hdlp->ih_vector, hdlp->ih_cb_arg1, hdlp->ih_cb_arg2,
			    NULL, rdip))
				return (DDI_FAILURE);
		}
		break;

	case DDI_INTROP_DISABLE:
		rem_avintr((void *)hdlp, hdlp->ih_pri, hdlp->ih_cb_func,
		    hdlp->ih_vector);
		break;
	case DDI_INTROP_SETMASK:
	case DDI_INTROP_CLRMASK:
	case DDI_INTROP_GETPENDING:
		return (DDI_FAILURE);
	case DDI_INTROP_NAVAIL:
		{
			pnode_t node = ddi_get_nodeid(rdip);
			int interrupt_cells = get_interrupt_cells(node);
			int irupts_len;
			if (interrupt_cells != 0 &&
			    ddi_getproplen(DDI_DEV_T_ANY, rdip,
			    DDI_PROP_DONTPASS, "interrupts",
			    &irupts_len) == DDI_SUCCESS) {
				*(int *)result = irupts_len /
				    CELLS_1275_TO_BYTES(interrupt_cells);
			} else {
				return (DDI_FAILURE);
			}
		}
		break;
	case DDI_INTROP_NINTRS:
		{
			pnode_t node = ddi_get_nodeid(rdip);
			int interrupt_cells = get_interrupt_cells(node);
			int irupts_len;
			if (interrupt_cells != 0 &&
			    ddi_getproplen(DDI_DEV_T_ANY, rdip,
			    DDI_PROP_DONTPASS, "interrupts",
			    &irupts_len) == DDI_SUCCESS) {
				*(int *)result = irupts_len /
				    CELLS_1275_TO_BYTES(interrupt_cells);
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
