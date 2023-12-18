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
 * Copyright 2023 Michael van der Westhuizen
 */

#include <sys/types.h>
#include <sys/psciinfo.h>
#include <sys/systm.h>
#include <sys/obpdefs.h>
#include <sys/promif.h>

#define	PSCI_CONDUIT_PROPNAME		"method"
#define	PSCI_CONDUIT_PROPVAL_SMC	"smc"
#define	PSCI_CONDUIT_PROPVAL_HVC	"hvc"

#define	PSCI_CPU_SUSPEND_PROPNAME	"cpu_suspend"
#define	PSCI_CPU_CPU_OFF_PROPNAME	"cpu_off"
#define	PSCI_CPU_CPU_ON_PROPNAME	"cpu_on"
#define	PSCI_CPU_MIGRATE_PROPNAME	"migrate"

static struct psciinfo info = {
	.pi_version			= PSCI_NOT_IMPLEMENTED,
	.pi_conduit			= PSCI_CONDUIT_HVC,
	.pi_cpu_suspend_id		= PSCI_CPU_SUSPEND_ID,
	.pi_cpu_off_id			= PSCI_CPU_OFF_ID,
	.pi_cpu_on_id			= PSCI_CPU_ON_ID,
	.pi_migrate_id			= PSCI_MIGRATE_ID,
};

struct psci_scan {
	const char			*compatible;
	psci_version_t			psci_version;
};

static struct psci_scan scan_data[] = {
	{ .compatible = "arm,psci-1.3", .psci_version = PSCI_VERSION_1_3, },
	{ .compatible = "arm,psci-1.2", .psci_version = PSCI_VERSION_1_2, },
	{ .compatible = "arm,psci-1.1", .psci_version = PSCI_VERSION_1_1, },
	{ .compatible = "arm,psci-1.0", .psci_version = PSCI_VERSION_1_0, },
	{ .compatible = "arm,psci-0.2", .psci_version = PSCI_VERSION_0_2, },
	{ .compatible = "arm,psci", .psci_version = PSCI_VERSION_0_1, },
	{ .compatible = NULL, .psci_version = PSCI_NOT_IMPLEMENTED, },
};

void
psciinfo_init(void)
{
	pnode_t			node;
	static struct psci_scan	*ps;
	boolean_t		exists;
	int			cv;
	char			prop[OBP_STANDARD_MAXPROPNAME];

	if (info.pi_version != PSCI_NOT_IMPLEMENTED)
		return;

	for (ps = &scan_data[0]; ps->compatible != NULL; ++ps) {
		node = prom_find_compatible(prom_rootnode(), ps->compatible);
		if (node > 0)
			break;
	}

	/*
	 * While a machine can theoretically operate without PSCI, illumos
	 * will not do well without it.
	 */
	if (ps->compatible == NULL)
		return;

	exists = prom_node_has_property(node, PSCI_CONDUIT_PROPNAME);
	if (exists != B_TRUE)
		prom_panic("PSCI: no \"method\" property in PSCI node");

	cv = prom_bounded_getprop(node, PSCI_CONDUIT_PROPNAME,
	    prop, OBP_STANDARD_MAXPROPNAME - 1);
	if (cv < 0)
		prom_panic("PSCI: \"method\" property too long");

	if (strcmp(prop, PSCI_CONDUIT_PROPVAL_SMC) == 0)
		info.pi_conduit = PSCI_CONDUIT_SMC;
	else if (strcmp(prop, PSCI_CONDUIT_PROPVAL_HVC) == 0)
		info.pi_conduit = PSCI_CONDUIT_HVC;
	else
		prom_panic("PSCI: \"method\" property has unknown value");

	if (ps->psci_version == PSCI_VERSION_0_1) {
		info.pi_cpu_suspend_id = (uint32_t)prom_get_prop_int(node,
		    PSCI_CPU_SUSPEND_PROPNAME, (int)PSCI_CPU_SUSPEND_ID);
		info.pi_cpu_off_id = (uint32_t)prom_get_prop_int(node,
		    PSCI_CPU_CPU_OFF_PROPNAME, (int)PSCI_CPU_OFF_ID);
		info.pi_cpu_on_id = (uint32_t)prom_get_prop_int(node,
		    PSCI_CPU_CPU_ON_PROPNAME, (int)PSCI_CPU_ON_ID);
		info.pi_migrate_id = (uint32_t)prom_get_prop_int(node,
		    PSCI_CPU_MIGRATE_PROPNAME, (int)PSCI_MIGRATE_ID);
	}

	info.pi_version = ps->psci_version;
}

const struct psciinfo *
psciinfo_get(void)
{
	return (&info);
}
