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
 * Platform-specific hostbridge enumeration for armv8 (aarch64).
 *
 * On aarch64, PCIe root complexes are driven by "ecam" or a platform-specific
 * root complex driver and appear at the top of the devinfo tree.  Each ecam
 * node's children are "pcieb" root port bridges, structurally equivalent to
 * the NPE -> pcieb relationship on i86pc.
 */

#include <string.h>
#include <libdevinfo.h>
#include <fm/topo_mod.h>
#include <fm/topo_hc.h>

#include <hostbridge.h>
#include <pcibus.h>
#include <did.h>
#include <did_props.h>
#include <util.h>

#define	ECAM	"ecam"
#define	RPI4	"bcm2711_pcie"

static int
rc_process(topo_mod_t *mod, tnode_t *ptn, topo_instance_t hbi, di_node_t bn)
{
	tnode_t *hb;
	tnode_t *rc;
	did_t *hbdid;

	if ((hbdid = did_create(mod, bn, 0, hbi, hbi, TRUST_BDF)) == NULL) {
		return (-1);
	}

	if ((hb = pciexhostbridge_declare(mod, ptn, bn, hbi)) == NULL) {
		return (-1);
	}

	if ((rc = pciexrc_declare(mod, hb, bn, hbi)) == NULL) {
		return (-1);
	}

	if (topo_mod_enumerate(mod,
	    rc, PCI_BUS, PCIEX_BUS, 0, MAX_HB_BUSES, (void *)hbdid) < 0) {
		topo_node_unbind(hb);
		topo_node_unbind(rc);
		return (-1);
	}

	return (0);
}

int
platform_hb_enum(topo_mod_t *mod, tnode_t *parent, const char *name __unused,
    topo_instance_t imin __unused, topo_instance_t imax __unused)
{
	di_node_t devtree;
	di_node_t pnode, cnode;
	size_t i;
	int hbcnt = 0;

	static const char *drv_names[] = {
		ECAM,
		RPI4,
	};
	static const size_t num_drv_names = ARRAY_SIZE(drv_names);

	devtree = topo_mod_devinfo(mod);
	if (devtree == DI_NODE_NIL) {
		topo_mod_dprintf(mod, "devinfo init failed.");
		topo_node_range_destroy(parent, HOSTBRIDGE);
		return (0);
	}

	/*
	 * Scan for driver instances - these are PCIe root complexes on
	 * aarch64.  Each node's pcieb children are root ports.
	 */
	for (i = 0; i < num_drv_names; ++i) {
		if (drv_names[i] == NULL) {
			continue;
		}

		pnode = di_drv_first_node(drv_names[i], devtree);
		while (pnode != DI_NODE_NIL) {
			for (cnode = di_child_node(pnode); cnode != DI_NODE_NIL;
			    cnode = di_sibling_node(cnode)) {
				if (di_driver_name(cnode) == NULL)
					continue;
				if (strcmp(di_driver_name(cnode), PCIEB) != 0)
					continue;
				if (rc_process(mod, parent, hbcnt, cnode) < 0) {
					if (hbcnt == 0)
						topo_node_range_destroy(parent,
						    HOSTBRIDGE);
					return (topo_mod_seterrno(mod,
					    EMOD_PARTIAL_ENUM));
				}
				hbcnt++;
			}
			pnode = di_drv_next_node(pnode);
		}
	}

	return (0);
}

int
platform_hb_label(topo_mod_t *mod, tnode_t *node, nvlist_t *in, nvlist_t **out)
{
	return (labelmethod_inherit(mod, node, in, out));
}
