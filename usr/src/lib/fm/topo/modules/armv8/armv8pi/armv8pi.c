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
 * Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2023 Richard Lowe
 */

#include <sys/fm/protocol.h>
#include <sys/systeminfo.h>

#include <strings.h>

#include <fm/topo_hc.h>
#include <fm/topo_mod.h>

/*
 * A generic, and very interim, legacy armv8 topology.
 */

/*
 * Entry point called by libtopo when enumeration is required
 */
static topo_enum_f armv8pi_enum;	/* libtopo enumeration entry point */

/*
 * Declare the operations vector and information structure used during
 * module registration
 */
static topo_modops_t armv8pi_ops = {
	.tmo_enum = armv8pi_enum,
	.tmo_release = NULL,
};

static topo_modinfo_t	armv8_modinfo = {
	.tmi_desc = "armv8 Topology Enumerator",
	.tmi_scheme = FM_FMRI_SCHEME_HC,
	.tmi_version = TOPO_VERSION,
	.tmi_ops = &armv8pi_ops,
};

/*
 * Called by libtopo when the topo module is loaded.
 */
int
_topo_init(topo_mod_t *mod, topo_version_t version)
{
	int	result;
	char	isa[MAXNAMELEN];

	if (getenv("TOPOARMV8PIDBG") != NULL) {
		/* Debugging is requested for this module */
		topo_mod_setdebug(mod);
	}
	topo_mod_dprintf(mod, "module initializing.\n");

	if (version != TOPO_VERSION) {
		(void) topo_mod_seterrno(mod, EMOD_VER_NEW);
		topo_mod_dprintf(mod, "incompatible topo version %d\n",
		    version);
		return (-1);
	}

	/* Verify that this is an armv8 architecture machine */
	(void) sysinfo(SI_MACHINE, isa, MAXNAMELEN);
	if (strncmp(isa, "armv8", MAXNAMELEN) != 0) {
		topo_mod_dprintf(mod, "not armv8 architecture: %s\n", isa);
		return (-1);
	}

	result = topo_mod_register(mod, &armv8_modinfo, TOPO_VERSION);
	if (result < 0) {
		topo_mod_dprintf(mod, "registration failed: %s\n",
		    topo_mod_errmsg(mod));
		/* module errno already set */
		return (-1);
	}
	topo_mod_dprintf(mod, "module ready.\n");
	return (0);
}

/*
 * Clean up any data used by the module before it is unloaded.
 */
void
_topo_fini(topo_mod_t *mod)
{
	topo_mod_dprintf(mod, "module finishing.\n");

	/* Unregister from libtopo */
	topo_mod_unregister(mod);
}

/*
 * Enumeration entry point for the armv8 topology enumerator.
 *
 * We fall directly back onto a legacy XML system, like legacy i86pc.
 */
static int
armv8pi_enum(topo_mod_t *mod, tnode_t *t_parent, const char *name,
    topo_instance_t min, topo_instance_t max, void *pi_private, void *data)
{
	int result;

	result = topo_mod_enummap(mod, t_parent, "armv8-legacy",
	    FM_FMRI_SCHEME_HC);

	if (result != 0) {
		topo_mod_dprintf(mod, "Enumeration failed.\n");
		return (-1);
	}

	return (result);
}
