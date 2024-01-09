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
 * Copyright 2024 Michael van der Westhuizen
 */

#include <sys/types.h>
#include <sys/obpdefs.h>
#include <sys/cpuinfo.h>
#include <asm/controlregs.h>
#include <sys/promif.h>
#include <sys/cpuvar.h>
#include <sys/byteorder.h>
#include <sys/systm.h>
#include <sys/stddef.h>

typedef enum {
	CPUNODE_STATUS_OKAY	= 0,
	CPUNODE_STATUS_DISABLED	= 1,
	CPUNODE_STATUS_FAILED	= 2,
	CPUNODE_STATUS_UNKNOWN	= 3,
	CPUNODE_STATUS_OTHER	= 4,
	CPUNODE_STATUS_ERROR	= 5,
} cpunode_status_t;

#define	CPUNODE_STATUS_IS_ENABLED(v)	((v) == CPUNODE_STATUS_OKAY || \
					(v) == CPUNODE_STATUS_UNKNOWN)

#define	CPUNODE_BAD_AFFINITY		0xffffffffffffffffull

#define	CPUNODE_BAD_ENABLE_METHOD	-1

#define	CPUNODE_BAD_PARKED_ADDRESS	0xffffffffffffffffull

static list_t		ci_lst;
static struct cpuinfo	ci0;

/*
 * Extract the MPIDR of the target CPU object, or CPUNODE_BAD_AFFINITY on error.
 */
static uint64_t
get_cpu_mpidr(pnode_t node)
{
	int		cv;
	int		ac;
	uint64_t	affinity = CPUNODE_BAD_AFFINITY;
	uint32_t	parts[2] = {0, 0};

	cv = prom_get_size_cells(node);
	if (cv != 0x0)
		return (affinity);

	ac = prom_get_address_cells(node);
	if (ac != 1 && ac != 2)
		return (affinity);

	cv = prom_getprop(node, "reg", (caddr_t)parts);
	if ((ac == 1 && cv != 4) || (ac == 2 && cv != 8))
		return (affinity);

	switch (ac) {
	case 1:
		affinity = (uint64_t)ntohl(parts[0]);
		break;
	case 2:
		affinity = (uint64_t)ntohl(parts[0]);
		affinity <<= 32;
		affinity |= (uint64_t)ntohl(parts[1]);
		break;
	default:
		break;
	}

	return (affinity);
}

static int
get_enable_method(pnode_t node)
{
	/* enable-method: psci|spin-table */
	int		cv;
	boolean_t	exists;
	char		prop[OBP_STANDARD_MAXPROPNAME];

	exists = prom_node_has_property(node, "enable-method");
	if (exists != B_TRUE)
		return (CPUNODE_BAD_ENABLE_METHOD);

	cv = prom_bounded_getprop(
	    node, "enable-method", prop, OBP_STANDARD_MAXPROPNAME - 1);
	if (cv < 0)
		return (CPUNODE_BAD_ENABLE_METHOD);

	if (strcmp(prop, "psci") == 0)
		return (CPUINFO_ENABLE_METHOD_PSCI);
	else if (strcmp(prop, "spin-table") == 0)
		return (CPUINFO_ENABLE_METHOD_SPINTABLE_SIMPLE);

	return (CPUNODE_BAD_ENABLE_METHOD);
}

static uint64_t
get_parked_address(pnode_t node)
{
	int		cv;
	int		ac;
	uint64_t	parked_addr = CPUNODE_BAD_PARKED_ADDRESS;
	uint32_t	parts[2] = {0, 0};

	/*
	 * XXXARM: This is a bit sketchy, but it should hold true due to the
	 * #size-cells needed for the MPIDR.
	 */
	cv = prom_get_size_cells(node);
	if (cv != 0x0)
		return (parked_addr);

	ac = prom_get_address_cells(node);
	if (ac != 1 && ac != 2)
		return (parked_addr);

	cv = prom_getprop(node, "cpu-release-addr", (caddr_t)parts);
	if ((ac == 1 && cv != 4) || (ac == 2 && cv != 8))
		return (parked_addr);

	switch (ac) {
	case 1:
		parked_addr = (uint64_t)ntohl(parts[0]);
		break;
	case 2:
		parked_addr = (uint64_t)ntohl(parts[0]);
		parked_addr <<= 32;
		parked_addr |= (uint64_t)ntohl(parts[1]);
		break;
	default:
		break;
	}

	return (parked_addr);
}

static cpunode_status_t
get_cpu_status(pnode_t node)
{
	int		cv;
	boolean_t	exists;
	char		prop[OBP_STANDARD_MAXPROPNAME];

	exists = prom_node_has_property(node, "status");
	if (exists != B_TRUE)
		return (CPUNODE_STATUS_UNKNOWN);

	cv = prom_bounded_getprop(
	    node, "status", prop, OBP_STANDARD_MAXPROPNAME - 1);
	if (cv < 0)
		return (CPUNODE_STATUS_ERROR);

	if (strcmp(prop, "okay") == 0 || strcmp(prop, "ok") == 0)
		return (CPUNODE_STATUS_OKAY);
	else if (strcmp(prop, "disabled") == 0)
		return (CPUNODE_STATUS_DISABLED);
	else if (strncmp(prop, "fail", 4) == 0)
		return (CPUNODE_STATUS_FAILED);

	return (CPUNODE_STATUS_OTHER);
}

static boolean_t
is_cpu_node(pnode_t node)
{
	int ret;
	char prop[OBP_STANDARD_MAXPROPNAME];

	ret = prom_getprop(node, "device_type", prop);
	if (ret < 0)
		return (B_FALSE);

	return (strcmp(prop, "cpu") == 0 ? B_TRUE : B_FALSE);
}

static pnode_t
next_cpu_node(pnode_t node)
{
	for (node = prom_nextnode(node); node > 0; node = prom_nextnode(node)) {
		if (is_cpu_node(node) == B_TRUE) {
			return (node);
		}
	}

	return (node);
}

static pnode_t
first_cpu_node(void)
{
	pnode_t	cpus;
	pnode_t	node;

	cpus = prom_finddevice("/cpus");
	if (cpus <= 0)
		return (cpus);

	for (node = prom_childnode(cpus);
	    node > 0; node = prom_nextnode(node)) {
		if (is_cpu_node(node) == B_TRUE) {
			return (node);
		}
	}

	return (node);
}

struct cpuinfo *
cpuinfo_first(void)
{
	return (list_head(&ci_lst));
}

struct cpuinfo *
cpuinfo_next(struct cpuinfo *ci)
{
	return (list_next(&ci_lst, ci));
}

struct cpuinfo *
cpuinfo_first_enabled(void)
{
	struct cpuinfo *ci;

	ci = cpuinfo_first();
	if (ci == cpuinfo_end())
		return (ci);

	do {
		if (ci->ci_flags & CPUINFO_ENABLED)
			break;
	} while ((ci = cpuinfo_next(ci)) != cpuinfo_end());

	return (ci);
}

struct cpuinfo *
cpuinfo_next_enabled(struct cpuinfo *ci)
{
	if (ci == cpuinfo_end())
		return (ci);

	while ((ci = cpuinfo_next(ci)) != cpuinfo_end()) {
		if (ci->ci_flags & CPUINFO_ENABLED)
			break;
	}

	return (ci);
}

struct cpuinfo *
cpuinfo_end(void)
{
	return (NULL);
}

/*
 * Returns the cpuinfo matching the requested affinity (in MPIDR format), or
 * NULL when none was found.
 */
struct cpuinfo *
cpuinfo_for_affinity(uint64_t affinity)
{
	struct cpuinfo *ci;

	for (ci = cpuinfo_first(); ci != cpuinfo_end(); ci = cpuinfo_next(ci))
		if (ci->ci_mpidr == affinity)
			return (ci);

	return (NULL);
}

static boolean_t
is_gicv3(void)
{
	pnode_t	node;
	node = prom_find_compatible(prom_rootnode(), "arm,gic-v3");
	return (node == OBP_NONODE ? B_FALSE : B_TRUE);
}

/*
 * In the FDT case we have to infer the CPU Interface Number.
 *
 * This seems a bit sketchy, but looking at edk2 sources (as
 * DynamicTablesPkg/Library/FdtHwInfoParserLib/Gic/ArmGicCParser.c) and at
 * u-boot sources (arch/arm/cpu/armv8/spin_table.c, arch/arm/cpu/armv8/start.S
 * and arch/arm/cpu/armv8/spin_table_v8.S).
 *
 * Specifically, this comment from edk2 sums up the situation nicely:
 *
 * To fit the Affinity [0-3] a 32bits value, place the Aff3 on bits
 * [31:24] instead of their original place ([39:32]).
 *
 * Furthermore, the CPU Interface Number is only poopulated for GICv2, as GICv3
 * in legacy mode is unsupported by edk2.
 *
 * ARM Trusted Firmware updates the board device tree to use PSCI.
 */
static int
fill_cpuinfo(pnode_t node, struct cpuinfo *ci)
{
	int st = get_cpu_status(node);
	if (st == CPUNODE_STATUS_ERROR)
		return (-1);

	/*
	 * Translate FDT CPU node status to ACPI-like "Enabled" and
	 * "Online Capable" flags.
	 */
	ci->ci_flags = 0;
	if (CPUNODE_STATUS_IS_ENABLED(st))
		ci->ci_flags |= CPUINFO_ENABLED;
	/*
	 * There isn't really a notion of "Online Capable" in the FDT CPU node,
	 * so no hot-pluggable CPUs for embedded devices (which makes sense).
	 *
	 * If there were such a notion we'd set the CPUINFO_ONLINE_CAPABLE bit
	 * here if the CPU were not enabled but capable of coming online.
	 *
	 * Other flags we're not yet setting are the interrupt mode bits for
	 * the performance and VGIC maintenance interrupts.
	 */

	ci->ci_mpidr = get_cpu_mpidr(node);
	if (ci->ci_mpidr == CPUNODE_BAD_AFFINITY)
		return (-1);

	ci->ci_ppver = get_enable_method(node);
	if (ci->ci_ppver == CPUNODE_BAD_ENABLE_METHOD)
		return (-1);

	if (ci->ci_ppver == CPUINFO_ENABLE_METHOD_SPINTABLE_SIMPLE) {
		ci->ci_parked_addr = get_parked_address(node);
		if (ci->ci_parked_addr == CPUNODE_BAD_PARKED_ADDRESS)
			return (-1);
	} else {
		ci->ci_parked_addr = 0;
	}

	if (is_gicv3() == B_TRUE) {
		ci->ci_cpuif = 0;
	} else {
		/* Assume GICv2, since we don't support anything else */
		ci->ci_cpuif = (ci->ci_mpidr & 0x00ffffff) |
		    ((ci->ci_mpidr >> 8) & 0xff000000);
	}

	return (0);
}

static struct cpuinfo *
create_cpuinfo(pnode_t node)
{
	struct cpuinfo		*ci;
	int			st;

	ci = kmem_zalloc(sizeof (*ci), KM_SLEEP);

	st = fill_cpuinfo(node, ci);
	if (st != 0) {
		kmem_free(ci, sizeof (*ci));
		ci = NULL;
	}

	return (ci);
}

/*
 * Initialize the CPU information list, incorporating the boot CPU already set
 * up by a prior call to cpuinfo_bootstrap.
 *
 * After calling this function you can use the iterator functions
 * cpuinfo_first/cpuinfo_first_enabled, cpuinfo_next/cpuinfo_next_enabled and
 * cpuinfo_end.
 */
int
cpuinfo_init(void)
{
	pnode_t		node;
	int		n;
	int		idx;
	int		rv;
	struct cpuinfo	*ci;

	list_create(&ci_lst, sizeof (struct cpuinfo),
	    offsetof(struct cpuinfo, ci_list_node));
	list_insert_head(&ci_lst, &ci0);

	for (node = first_cpu_node(), idx = 1;
	    node > 0; node = next_cpu_node(node)) {
		if (get_cpu_mpidr(node) == ci0.ci_mpidr)
			continue;

		ci = create_cpuinfo(node);
		if (ci == NULL) {
			while ((ci = list_remove_tail(&ci_lst)) != NULL) {
				if (ci->ci_id != 0)
					kmem_free(ci, sizeof (*ci));
			}

			list_destroy(&ci_lst);
			return (-1);
		}

		ci->ci_id = idx++;
		list_insert_tail(&ci_lst, ci);
	}

	return (0);
}

/*
 * Bootstrap the boot processor CPU information, attach it to the passed boot
 * processor structure (which must have cpu_id zero) and set the boot_ncpus,
 * boot_max_ncpus and max_ncpus values.
 */
void
cpuinfo_bootstrap(cpu_t *cp)
{
	pnode_t		node;
	int		st;
	int		cpu_count;
	int		cpu_possible_count;

	ASSERT(cp->cpu_id == 0);
	cpu_count = cpu_possible_count = 0;

	for (node = first_cpu_node(); node > 0; node = next_cpu_node(node)) {
		st = get_cpu_status(node);

		if (CPUNODE_STATUS_IS_ENABLED(st)) {
			cpu_count++;
			cpu_possible_count++;
		} else {
			/*
			 * If we supported pluggable CPUs in FDT we'd add an
			 * "else if" here and bump the cpu_possible_count iff
			 * the CPU was online-capable.
			 *
			 * In the FDT case we simply bump cpu_possible_count
			 * for all CPUs that have a status that is neither
			 * explicitly nor implicitly "okay".
			 */
			cpu_possible_count++;
		}

		if (cp->cpu_m.mcpu_ci == NULL &&
		    cp->cpu_m.affinity == get_cpu_mpidr(node)) {
			st = fill_cpuinfo(node, &ci0);
			if (st != 0) {
				prom_panic("CPUINFO: failed to fill cpuinfo "
				    "for the boot processor");
			}
			ci0.ci_id = 0;
			cp->cpu_m.mcpu_ci = &ci0;
		}
	}

	if (cp->cpu_m.mcpu_ci == NULL)
		prom_panic("CPUINFO: did not find the boot processor");

	/*
	 * cpu_count is guaranteed to be greater than zero due to the check
	 * for the boot processor cpuinfo having been set.
	 */
	boot_ncpus = cpu_count;
	boot_max_ncpus = max_ncpus = cpu_possible_count;
}
