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
 * Platform-specific PCI topology labeling for armv8 (aarch64).
 *
 * On aarch64, slot labels come from SMBIOS Type 9 records and don't
 * need platform-specific rewrites or fixups.
 */

#include <fm/topo_mod.h>

#include "pcibus.h"
#include "pcibus_labels.h"

/*
 * No platform-specific slot label rewrites needed.
 */
slotnm_rewrite_t *Slot_Rewrites = NULL;
physlot_names_t *Physlot_Names = NULL;
missing_names_t *Missing_Names = NULL;

int
platform_pci_label(topo_mod_t *mod, tnode_t *node, nvlist_t *in,
    nvlist_t **out)
{
	return (pci_label_cmn(mod, node, in, out));
}

int
platform_pci_fru(topo_mod_t *mod, tnode_t *node, nvlist_t *in,
    nvlist_t **out)
{
	return (pci_fru_cmn(mod, node, in, out));
}
