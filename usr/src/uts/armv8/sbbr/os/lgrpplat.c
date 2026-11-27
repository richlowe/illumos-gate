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
 * Locality Group (lgroup) platform support for aarch64/ACPI (SBBR) platforms
 *
 * SBSA/SBBR-compliant ARM systems may have Non Uniform Memory Access (NUMA).
 * This module determines the NUMA topology by reading the ACPI SRAT, SLIT,
 * and (optional) MSCT firmware tables.
 *
 * Unlike the i86pc version, there is no hardware probing fallback and no
 * Opteron PCI config space path.  If SLIT is absent, all nodes are treated
 * as equidistant.
 *
 * CPU identification uses the ACPI Processor UID from MADT GICC entries,
 * matched against SRAT GICC Affinity structures (type 3).  The cpuinfo
 * facility provides the ACPI UID to processorid_t mapping.
 */

#include <sys/archsystm.h>
#include <sys/bootconf.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/cpupart.h>
#include <sys/cpuvar.h>
#include <sys/lgrp.h>
#include <sys/machsystm.h>
#include <sys/memlist.h>
#include <sys/memnode.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/promif.h>
#include <sys/systm.h>
#include <sys/thread.h>
#include <sys/types.h>
#include <sys/cpuinfo.h>
#include <sys/sysmacros.h>
#include <sys/acpi/acpi.h>
#include <sys/acpi/actbl2.h>
#include <sys/acpi/actbl3.h>
#include <vm/vm_dep.h>

#define	MAX_NODES		MAX_MEM_NODES
#define	NLGRP			(MAX_NODES * (MAX_NODES - 1) + 1)

/*
 * Default SLIT distances when no SLIT is available.
 */
#define	SLIT_LOCAL_DISTANCE	10
#define	SLIT_REMOTE_DISTANCE	20

/*
 * Proximity domain to node ID mapping.
 */
typedef struct node_domain_map {
	int		exists;
	uint32_t	prox_domain;
} node_domain_map_t;

/*
 * CPU to node ID mapping, keyed by ACPI Processor UID.
 */
typedef struct cpu_node_map {
	uint32_t	uid;
	uint32_t	prox_domain;
	int		node;
	boolean_t	exists;
} cpu_node_map_t;

/*
 * Physical address range for a memory node.
 */
typedef struct memnode_phys_addr_map {
	pfn_t		start;		/* start PFN (inclusive) */
	pfn_t		end;		/* end PFN (exclusive) */
	int		exists;
	uint32_t	prox_domain;
	int		lgrphand;
	boolean_t	orphan;		/* EFI memory not described by SRAT */
} memnode_phys_addr_map_t;

/*
 * Latency statistics from SLIT.
 */
typedef struct lgrp_plat_latency_stats {
	int	latencies[MAX_NODES][MAX_NODES];
	int	latency_min;
	int	latency_max;
} lgrp_plat_latency_stats_t;

/*
 * Static lgrp allocation.
 */
static lgrp_t		lgrp_space[NLGRP];
static int		nlgrps_alloc;
struct lgrp_stats	lgrp_stats[NLGRP];

/*
 * CPU to node mapping table, indexed by processorid_t.
 *
 * Allocated via BOP_ALLOC in STAGE1, replaced with kmem in STAGE4.
 */
static cpu_node_map_t		*lgrp_plat_cpu_node = NULL;
static uint_t			lgrp_plat_cpu_node_nentries = 0;

/*
 * Latency statistics from SLIT.
 */
static lgrp_plat_latency_stats_t	lgrp_plat_lat_stats;

/*
 * Node to proximity domain mapping.
 */
static node_domain_map_t	lgrp_plat_node_domain[MAX_NODES];

/*
 * Physical address ranges per memory node.
 */
static memnode_phys_addr_map_t	lgrp_plat_memnode_info[MAX_MEM_NODES];

/*
 * Error codes from processing SRAT and SLIT.
 */
static int			lgrp_plat_srat_error = 0;
static int			lgrp_plat_slit_error = 0;

/*
 * Maximum memnode index in use.
 */
static uint_t			lgrp_plat_max_mem_node;

/*
 * Tunables and counters.
 */
uint_t		lgrp_plat_node_cnt = 1;
int		lgrp_plat_node_sort_enable = 1;

/*
 * ACPI table pointers, set by lgrp_plat_set_fw_tables.
 */
static ACPI_TABLE_SRAT		*lgrp_plat_srat_ptr = NULL;
static ACPI_TABLE_SLIT		*lgrp_plat_slit_ptr = NULL;
static ACPI_TABLE_MSCT		*lgrp_plat_msct_ptr = NULL;

/*
 * Forward declarations.
 */
static int	lgrp_plat_domain_to_node(node_domain_map_t *node_domain,
		    int node_cnt, uint32_t domain);
static int	lgrp_plat_node_domain_update(node_domain_map_t *node_domain,
		    int node_cnt, uint32_t domain);
static int	lgrp_plat_cpu_node_update(node_domain_map_t *node_domain,
		    int node_cnt, cpu_node_map_t *cpu_node, int nentries,
		    uint32_t uid, uint32_t domain);
static int	lgrp_plat_memnode_info_update(node_domain_map_t *node_domain,
		    int node_cnt, memnode_phys_addr_map_t *memnode_info,
		    int memnode_cnt, uint64_t base, uint64_t length,
		    uint32_t domain, uint32_t flags);
static int	lgrp_plat_srat_domains(ACPI_TABLE_SRAT *srat);
static int	lgrp_plat_msct_domains(ACPI_TABLE_MSCT *msct);
static int	lgrp_plat_process_srat(ACPI_TABLE_SRAT *srat,
		    ACPI_TABLE_MSCT *msct,
		    node_domain_map_t *node_domain, cpu_node_map_t *cpu_node,
		    int cpu_count, memnode_phys_addr_map_t *memnode_info);
static int	lgrp_plat_process_slit(ACPI_TABLE_SLIT *slit,
		    node_domain_map_t *node_domain, uint_t node_cnt,
		    lgrp_plat_latency_stats_t *lat_stats);
static void	lgrp_plat_node_sort(node_domain_map_t *node_domain,
		    int node_cnt, cpu_node_map_t *cpu_node, int cpu_count,
		    memnode_phys_addr_map_t *memnode_info);
static void	lgrp_plat_get_numa_config(void);
static boolean_t lgrp_plat_memnode_validate(node_domain_map_t *node_domain,
		    int node_cnt, memnode_phys_addr_map_t *memnode_info,
		    uint_t max_mem_node);
static void	lgrp_plat_main_init(void);
static void	lgrp_plat_orphan_memnode(pfn_t start, pfn_t end);

/*
 * Look up the node ID for a given proximity domain.
 * Returns -1 if not found.
 */
static int
lgrp_plat_domain_to_node(node_domain_map_t *node_domain, int node_cnt,
    uint32_t domain)
{
	int i;

	for (i = 0; i < node_cnt; i++) {
		if (node_domain[i].exists &&
		    node_domain[i].prox_domain == domain) {
			return (i);
		}
	}

	return (-1);
}

/*
 * Add or update a node-to-proximity-domain mapping.
 * Returns the node ID for the domain.
 */
static int
lgrp_plat_node_domain_update(node_domain_map_t *node_domain, int node_cnt,
    uint32_t domain)
{
	int i, node;

	/*
	 * See if this domain is already known.
	 */
	node = lgrp_plat_domain_to_node(node_domain, node_cnt, domain);
	if (node >= 0) {
		return (node);
	}

	/*
	 * Find the first unused slot and assign this domain.
	 */
	for (i = 0; i < node_cnt; i++) {
		if (!node_domain[i].exists) {
			node_domain[i].prox_domain = domain;
			node_domain[i].exists = 1;
			return (i);
		}
	}

	return (-1);
}

/*
 * Update the CPU-to-node mapping for a CPU identified by its ACPI UID.
 * Returns 0 on success, -1 if the CPU couldn't be found or mapped.
 */
static int
lgrp_plat_cpu_node_update(node_domain_map_t *node_domain, int node_cnt,
    cpu_node_map_t *cpu_node, int nentries, uint32_t uid, uint32_t domain)
{
	processorid_t	id;
	int		node;

	id = cpuinfo_id_for_uid(uid);
	if (id < 0 || id >= nentries) {
		return (-1);
	}

	/*
	 * A duplicate UID means firmware described the same CPU twice.
	 * Keep the first entry; the second is ignored.
	 */
	if (cpu_node[id].exists) {
		return (0);
	}

	node = lgrp_plat_node_domain_update(node_domain, node_cnt, domain);
	if (node < 0) {
		return (-1);
	}

	cpu_node[id].uid = uid;
	cpu_node[id].prox_domain = domain;
	cpu_node[id].node = node;
	cpu_node[id].exists = B_TRUE;

	return (0);
}

/*
 * Update the memnode info for a memory affinity range.
 * Each SRAT memory entry maps to a memnode.  Multiple ranges in the
 * same proximity domain may share a memnode if they are contiguous,
 * or they may use separate memnodes.
 *
 * Returns the memnode ID on success, -1 on failure.
 */
static int
lgrp_plat_memnode_info_update(node_domain_map_t *node_domain, int node_cnt,
    memnode_phys_addr_map_t *memnode_info, int memnode_cnt,
    uint64_t base, uint64_t length, uint32_t domain, uint32_t flags)
{
	int	node;
	int	mnode;
	pfn_t	start_pfn;
	pfn_t	end_pfn;

	if (!(flags & ACPI_SRAT_MEM_ENABLED)) {
		return (-1);
	}

	if (length == 0) {
		return (-1);
	}

	node = lgrp_plat_node_domain_update(node_domain, node_cnt, domain);
	if (node < 0) {
		return (-1);
	}

	start_pfn = btop(base);
	end_pfn = btop(base + length);

	/*
	 * See if this extends an existing memnode for the same domain.
	 */
	for (mnode = 0; mnode < memnode_cnt; mnode++) {
		if (!memnode_info[mnode].exists) {
			continue;
		}

		if (memnode_info[mnode].prox_domain != domain) {
			continue;
		}

		if (memnode_info[mnode].end == start_pfn) {
			memnode_info[mnode].end = end_pfn;
			return (mnode);
		}

		if (end_pfn == memnode_info[mnode].start) {
			memnode_info[mnode].start = start_pfn;
			return (mnode);
		}
	}

	/*
	 * Allocate a new memnode.
	 */
	for (mnode = 0; mnode < memnode_cnt; mnode++) {
		if (!memnode_info[mnode].exists) {
			memnode_info[mnode].start = start_pfn;
			memnode_info[mnode].end = end_pfn;
			memnode_info[mnode].exists = 1;
			memnode_info[mnode].prox_domain = domain;
			memnode_info[mnode].lgrphand = (lgrp_handle_t)node;

			if ((uint_t)mnode >= lgrp_plat_max_mem_node) {
				lgrp_plat_max_mem_node = mnode + 1;
			}

			return (mnode);
		}
	}

	return (-1);
}

/*
 * Validate that an SRAT subtable header and its full Length fit within the
 * remaining table bounds.
 *
 * Returns B_TRUE if the subtable can be safely read and advanced past.
 */
static boolean_t
lgrp_plat_srat_item_valid(ACPI_SUBTABLE_HEADER *item,
    ACPI_SUBTABLE_HEADER *end)
{
	if ((uintptr_t)item + sizeof (ACPI_SUBTABLE_HEADER) >
	    (uintptr_t)end) {
		return (B_FALSE);
	}

	if (item->Length < sizeof (ACPI_SUBTABLE_HEADER)) {
		return (B_FALSE);
	}

	if ((uintptr_t)item + item->Length > (uintptr_t)end) {
		return (B_FALSE);
	}

	return (B_TRUE);
}

/*
 * Count the number of distinct proximity domains in the SRAT.
 *
 * Returns the count, or -1 on error.
 */
static int
lgrp_plat_srat_domains(ACPI_TABLE_SRAT *srat)
{
	ACPI_SUBTABLE_HEADER	*item, *end;
	uint32_t		domains[MAX_NODES];
	int			ndomains = 0;
	uint32_t		domain;
	int			i;
	boolean_t		found;

	if (srat == NULL) {
		return (-1);
	}

	end = (ACPI_SUBTABLE_HEADER *)
	    ((uintptr_t)srat + srat->Header.Length);
	item = (ACPI_SUBTABLE_HEADER *)
	    ((uintptr_t)srat + sizeof (*srat));

	while (item < end) {
		domain = UINT32_MAX;

		if (!lgrp_plat_srat_item_valid(item, end)) {
			break;
		}

		switch (item->Type) {
		case ACPI_SRAT_TYPE_GICC_AFFINITY: {
			ACPI_SRAT_GICC_AFFINITY *ga =
			    (ACPI_SRAT_GICC_AFFINITY *)item;
			if (item->Length < sizeof (ACPI_SRAT_GICC_AFFINITY)) {
				break;
			}

			if (ga->Flags & ACPI_SRAT_GICC_ENABLED) {
				domain = ga->ProximityDomain;
			}

			break;
		}
		case ACPI_SRAT_TYPE_MEMORY_AFFINITY: {
			ACPI_SRAT_MEM_AFFINITY *ma =
			    (ACPI_SRAT_MEM_AFFINITY *)item;
			if (item->Length < sizeof (ACPI_SRAT_MEM_AFFINITY)) {
				break;
			}

			if (ma->Flags & ACPI_SRAT_MEM_ENABLED) {
				domain = ma->ProximityDomain;
			}

			break;
		}
		default:
			break;
		}

		if (domain != UINT32_MAX) {
			found = B_FALSE;

			for (i = 0; i < ndomains; i++) {
				if (domains[i] == domain) {
					found = B_TRUE;
					break;
				}
			}

			if (!found && ndomains < MAX_NODES) {
				domains[ndomains++] = domain;
			}
		}

		item = (ACPI_SUBTABLE_HEADER *)
		    ((uintptr_t)item + item->Length);
	}

	return (ndomains > 0 ? ndomains : -1);
}

/*
 * If MSCT is available we use it to get an upper bound on the number of
 * proximity domains.
 */
static int
lgrp_plat_msct_domains(ACPI_TABLE_MSCT *msct)
{
	if (msct == NULL) {
		return (-1);
	}

	return (msct->MaxProximityDomains + 1);
}

/*
 * Build CPU-to-node and memory-to-node mappings.
 *
 * Returns the number of NUMA nodes on success, or a negative value on error.
 */
static int
lgrp_plat_process_srat(ACPI_TABLE_SRAT *srat, ACPI_TABLE_MSCT *msct,
    node_domain_map_t *node_domain,
    cpu_node_map_t *cpu_node, int cpu_count,
    memnode_phys_addr_map_t *memnode_info)
{
	ACPI_SUBTABLE_HEADER	*item, *end;
	int			node_cnt;
	int			msct_cnt;

	if (srat == NULL) {
		return (-1);
	}

	/*
	 * First pass: count proximity domains.
	 */
	node_cnt = lgrp_plat_srat_domains(srat);
	if (node_cnt <= 0) {
		return (-1);
	}

	/*
	 * Use MSCT as a cross-check if available.
	 */
	msct_cnt = lgrp_plat_msct_domains(msct);
	if (msct_cnt > 0 && node_cnt > msct_cnt) {
		cmn_err(CE_WARN, "!lgrp: SRAT describes %d proximity "
		    "domains, exceeding MSCT maximum of %d",
		    node_cnt, msct_cnt);
		return (-1);
	}

	/*
	 * If the firmware describes more proximity domains than we support,
	 * CPUs and memory in excess domains will lose their locality
	 * information.
	 */
	if (node_cnt > MAX_NODES) {
		cmn_err(CE_WARN, "!lgrp: %d proximity domains exceeds "
		    "maximum of %lu, clamping", node_cnt, MAX_NODES);
		node_cnt = MAX_NODES;
	}

	/*
	 * Second pass: populate the mappings.
	 */
	end = (ACPI_SUBTABLE_HEADER *)((uintptr_t)srat + srat->Header.Length);
	item = (ACPI_SUBTABLE_HEADER *)((uintptr_t)srat + sizeof (*srat));

	while (item < end) {
		if (!lgrp_plat_srat_item_valid(item, end)) {
			break;
		}

		switch (item->Type) {
		case ACPI_SRAT_TYPE_GICC_AFFINITY: {
			ACPI_SRAT_GICC_AFFINITY *ga =
			    (ACPI_SRAT_GICC_AFFINITY *)item;
			if (item->Length < sizeof (ACPI_SRAT_GICC_AFFINITY)) {
				break;
			}

			if (ga->Flags & ACPI_SRAT_GICC_ENABLED) {
				if (lgrp_plat_cpu_node_update(
				    node_domain, node_cnt, cpu_node,
				    cpu_count, ga->AcpiProcessorUid,
				    ga->ProximityDomain) < 0) {
					cmn_err(CE_WARN, "!lgrp: CPU UID %u "
					    "in domain %u could not be "
					    "mapped to a NUMA node",
					    ga->AcpiProcessorUid,
					    ga->ProximityDomain);
				}
			}

			break;
		}
		case ACPI_SRAT_TYPE_MEMORY_AFFINITY: {
			ACPI_SRAT_MEM_AFFINITY *ma =
			    (ACPI_SRAT_MEM_AFFINITY *)item;
			if (item->Length < sizeof (ACPI_SRAT_MEM_AFFINITY)) {
				break;
			}

			if (lgrp_plat_memnode_info_update(
			    node_domain, node_cnt, memnode_info,
			    MAX_MEM_NODES, ma->BaseAddress, ma->Length,
			    ma->ProximityDomain, ma->Flags) < 0 &&
			    (ma->Flags & ACPI_SRAT_MEM_ENABLED) &&
			    ma->Length > 0) {
				cmn_err(CE_WARN, "!lgrp: memory range "
				    "[%llx, %llx) in domain %u could "
				    "not be mapped to a NUMA node",
				    (unsigned long long)ma->BaseAddress,
				    (unsigned long long)(ma->BaseAddress +
				    ma->Length),
				    ma->ProximityDomain);
			}

			break;
		}
		default:
			break;
		}

		item = (ACPI_SUBTABLE_HEADER *)((uintptr_t)item + item->Length);
	}

	/*
	 * Find the extent of populated node slots.
	 */
	node_cnt = 0;
	for (int i = 0; i < MAX_NODES; i++) {
		if (node_domain[i].exists) {
			node_cnt = i + 1;
		}
	}

	return (node_cnt > 0 ? node_cnt : -1);
}

/*
 * Fill in the latency matrix.
 *
 * The SLIT matrix is indexed by proximity domain, not by our internal node IDs.
 * We must map each node to its proximity domain and use that as the SLIT
 * row/column index.  This is called after node_sort() so the node-to-domain
 * mapping is in its final state.
 *
 * Returns 0 on success, -1 on error.
 */
static int
lgrp_plat_process_slit(ACPI_TABLE_SLIT *slit, node_domain_map_t *node_domain,
    uint_t node_cnt, lgrp_plat_latency_stats_t *lat_stats)
{
	uint64_t	localities;
	uint64_t	avail;
	uint_t		from, to;
	uint32_t	from_domain, to_domain;
	int		latency;

	if (slit == NULL) {
		return (-1);
	}

	localities = slit->LocalityCount;
	if (localities == 0) {
		return (-1);
	}

	/*
	 * The SLIT must contain localities^2 one-byte entries following the
	 * header.
	 */
	if (slit->Header.Length < offsetof(ACPI_TABLE_SLIT, Entry)) {
		return (-1);
	}

	avail = slit->Header.Length - offsetof(ACPI_TABLE_SLIT, Entry);
	if (localities > avail || localities * localities > avail) {
		return (-1);
	}

	lat_stats->latency_min = -1;
	lat_stats->latency_max = 0;

	for (from = 0; from < node_cnt; from++) {
		if (!node_domain[from].exists) {
			continue;
		}

		from_domain = node_domain[from].prox_domain;
		if (from_domain >= localities) {
			return (-1);
		}

		for (to = 0; to < node_cnt; to++) {
			if (!node_domain[to].exists) {
				continue;
			}

			to_domain = node_domain[to].prox_domain;
			if (to_domain >= localities) {
				return (-1);
			}

			latency = slit->Entry[from_domain * localities +
			    to_domain];

			lat_stats->latencies[from][to] = latency;

			if (from == to) {
				continue;
			}

			if (latency < lat_stats->latency_min ||
			    lat_stats->latency_min == -1) {
				lat_stats->latency_min = latency;
			}

			if (latency > lat_stats->latency_max) {
				lat_stats->latency_max = latency;
			}
		}
	}

	return (0);
}

/*
 * Sort nodes by proximity domain ID for deterministic ordering.
 *
 * After sorting, CPU and memnode mappings are rebuilt to reflect the new
 * node numbering.
 */
static void
lgrp_plat_node_sort(node_domain_map_t *node_domain, int node_cnt,
    cpu_node_map_t *cpu_node, int cpu_count,
    memnode_phys_addr_map_t *memnode_info)
{
	int			i, j, n;
	node_domain_map_t	tmp;
	int			new_node;

	if (!lgrp_plat_node_sort_enable || node_cnt <= 1) {
		return;
	}

	/*
	 * Compact, ensuring the populated nodes are contiguous.
	 */
	n = 0;
	for (i = 0; i < node_cnt; i++) {
		if (node_domain[i].exists) {
			if (i != n) {
				node_domain[n] = node_domain[i];
			}

			n++;
		}
	}

	for (i = n; i < node_cnt; i++) {
		node_domain[i].exists = 0;
		node_domain[i].prox_domain = 0;
	}

	/*
	 * Insertion sort by proximity domain ID.
	 */
	for (i = 1; i < n; i++) {
		tmp = node_domain[i];
		j = i - 1;

		while (j >= 0 &&
		    node_domain[j].prox_domain > tmp.prox_domain) {
			node_domain[j + 1] = node_domain[j];
			j--;
		}

		node_domain[j + 1] = tmp;
	}

	/*
	 * Rebuild CPU and memnode mappings to match the new node order.
	 */
	for (i = 0; i < cpu_count; i++) {
		if (!cpu_node[i].exists) {
			continue;
		}
		new_node = lgrp_plat_domain_to_node(node_domain, node_cnt,
		    cpu_node[i].prox_domain);
		if (new_node >= 0) {
			cpu_node[i].node = new_node;
		}
	}

	for (i = 0; i < MAX_MEM_NODES; i++) {
		if (!memnode_info[i].exists) {
			continue;
		}

		new_node = lgrp_plat_domain_to_node(node_domain, node_cnt,
		    memnode_info[i].prox_domain);
		if (new_node >= 0) {
			memnode_info[i].lgrphand = (lgrp_handle_t)new_node;
		}
	}
}

/*
 * Stash ACPI table pointers.
 *
 * Must be called before lgrp_init(LGRP_INIT_STAGE1).
 */
void
lgrp_plat_set_fw_tables(uint64_t srat, uint64_t slit, uint64_t msct,
    uint64_t pptt __unused)
{
	if (srat != 0) {
		lgrp_plat_srat_ptr = (ACPI_TABLE_SRAT *)(uintptr_t)srat;
	}

	if (slit != 0) {
		lgrp_plat_slit_ptr = (ACPI_TABLE_SLIT *)(uintptr_t)slit;
	}

	if (msct != 0) {
		lgrp_plat_msct_ptr = (ACPI_TABLE_MSCT *)(uintptr_t)msct;
	}

	/* PPTT is not consumed by lgrpplat */
}

/*
 * Validate the memnode configuration produced by SRAT processing.
 *
 * Detects two conditions that prevent safe per-range memnode operation:
 *
 * 1. Interleaved domains: a domain's address bounding box overlaps
 *    another domain's.  The per-range memnode model is correct for
 *    interleaved domains, but coalescing (needed for memory DR or
 *    overflow recovery) would cause plat_pfn_to_mem_node to
 *    misattribute pages.  No safe recovery exists, so fall back to UMA.
 *
 * 2. Boot memnode overflow: more SRAT ranges than MAX_MEM_NODES can
 *    hold with DR headroom.  Pages outside any memnode would be
 *    orphaned, causing page counter array overflows.  Fall back to UMA.
 *
 * Returns B_TRUE if the configuration is valid, B_FALSE if UMA fallback
 * is required.  Emits a CE_NOTE describing the reason for fallback.
 */
static boolean_t
lgrp_plat_memnode_validate(node_domain_map_t *node_domain,
    int node_cnt, memnode_phys_addr_map_t *memnode_info,
    uint_t max_mem_node)
{
	pfn_t	domain_min[MAX_NODES];
	pfn_t	domain_max[MAX_NODES];
	int	i, j;

	/*
	 * Check for boot memnode overflow (more per-range memnodes
	 * than MAX_MEM_NODES can hold with DR headroom).
	 */
	if (max_mem_node +
	    (MAX_MEM_NODES_PER_LGROUP * node_cnt) > MAX_MEM_NODES) {
		cmn_err(CE_NOTE, "!lgrp: MPO disabled because SRAT "
		    "memory ranges exceed memnode capacity "
		    "(%u ranges, %lu max)",
		    max_mem_node, MAX_MEM_NODES);
		return (B_FALSE);
	}

	/*
	 * Compute per-domain bounding boxes.
	 */
	for (i = 0; i < MAX_NODES; i++) {
		domain_min[i] = PFN_INVALID;
		domain_max[i] = 0;
	}

	for (i = 0; i < max_mem_node; i++) {
		int	node;

		if (!memnode_info[i].exists) {
			continue;
		}

		node = lgrp_plat_domain_to_node(node_domain,
		    node_cnt, memnode_info[i].prox_domain);
		if (node < 0 || node >= MAX_NODES) {
			continue;
		}

		if (memnode_info[i].start < domain_min[node]) {
			domain_min[node] = memnode_info[i].start;
		}

		if (memnode_info[i].end > domain_max[node]) {
			domain_max[node] = memnode_info[i].end;
		}
	}

	/*
	 * Check for interleaved domains.
	 */
	for (i = 0; i < node_cnt; i++) {
		if (domain_max[i] == 0) {
			continue;
		}

		for (j = i + 1; j < node_cnt; j++) {
			if (domain_max[j] == 0) {
				continue;
			}

			if (domain_min[i] < domain_max[j] &&
			    domain_min[j] < domain_max[i]) {
				cmn_err(CE_NOTE, "!lgrp: MPO disabled "
				    "because memory is interleaved "
				    "across NUMA domains");
				return (B_FALSE);
			}
		}
	}

	return (B_TRUE);
}

/*
 * Determine NUMA configuration from ACPI tables.
 *
 * Called from lgrp_plat_init(LGRP_INIT_STAGE1).
 */
static void
lgrp_plat_get_numa_config(void)
{
	int	retval;
	int	i, j;

	if (lgrp_plat_srat_ptr == NULL) {
		/* No SRAT, we're UMA */
		return;
	}

	/*
	 * We're called before the kernel memory allocator is live, so use
	 * the boot allocator for the CPU-to-node mapping table.  This will
	 * be moved to kmem later in startup.
	 */
	lgrp_plat_cpu_node_nentries = max_ncpus;
	lgrp_plat_cpu_node = (cpu_node_map_t *)BOP_ALLOC(bootops, NULL,
	    lgrp_plat_cpu_node_nentries * sizeof (cpu_node_map_t),
	    sizeof (int));

	if (lgrp_plat_cpu_node == NULL) {
		lgrp_plat_cpu_node_nentries = 0;
		return;
	}

	bzero(lgrp_plat_cpu_node,
	    lgrp_plat_cpu_node_nentries * sizeof (cpu_node_map_t));

	/*
	 * Process SRAT to build CPU and memory node mappings.
	 */
	retval = lgrp_plat_process_srat(lgrp_plat_srat_ptr,
	    lgrp_plat_msct_ptr,
	    lgrp_plat_node_domain, lgrp_plat_cpu_node,
	    lgrp_plat_cpu_node_nentries, lgrp_plat_memnode_info);

	if (retval <= 0) {
		lgrp_plat_srat_error = retval;
		lgrp_plat_node_cnt = 1;
		return;
	}

	lgrp_plat_srat_error = 0;
	lgrp_plat_node_cnt = retval;

	/*
	 * Only one node, so nothing more to do.
	 */
	if (lgrp_plat_node_cnt == 1) {
		max_mem_nodes = 1;
		return;
	}

	/*
	 * Validate the per-range memnode configuration, falling back to UMA
	 * if necessary.
	 */
	if (!lgrp_plat_memnode_validate(lgrp_plat_node_domain,
	    lgrp_plat_node_cnt, lgrp_plat_memnode_info,
	    lgrp_plat_max_mem_node)) {
		lgrp_plat_node_cnt = max_mem_nodes = 1;
		(void) lgrp_topo_ht_limit_set(1);
		return;
	}

	/*
	 * Tune scheduler for NUMA.
	 */
	lgrp_expand_proc_thresh = LGRP_LOADAVG_THREAD_MAX / 2;
	lgrp_expand_proc_diff = 0;

	/*
	 * Set up memory nodes - boot memnodes plus DR headroom per
	 * proximity domain.
	 */
	max_mem_nodes = lgrp_plat_max_mem_node +
	    (MAX_MEM_NODES_PER_LGROUP * lgrp_plat_node_cnt);
	if (max_mem_nodes > MAX_MEM_NODES) {
		max_mem_nodes = MAX_MEM_NODES;
	}

	/*
	 * Sort nodes by proximity domain for deterministic ordering.
	 */
	lgrp_plat_node_sort(lgrp_plat_node_domain, lgrp_plat_node_cnt,
	    lgrp_plat_cpu_node, lgrp_plat_cpu_node_nentries,
	    lgrp_plat_memnode_info);

	/*
	 * Initialize latency stats.
	 */
	lgrp_plat_lat_stats.latency_min = -1;
	lgrp_plat_lat_stats.latency_max = 0;

	/*
	 * Process SLIT for inter-node latencies.
	 */
	lgrp_plat_slit_error = lgrp_plat_process_slit(lgrp_plat_slit_ptr,
	    lgrp_plat_node_domain, lgrp_plat_node_cnt,
	    &lgrp_plat_lat_stats);

	/*
	 * If no SLIT, use default flat latencies.
	 */
	if (lgrp_plat_slit_error != 0) {
		lgrp_plat_lat_stats.latency_min = SLIT_REMOTE_DISTANCE;
		lgrp_plat_lat_stats.latency_max = SLIT_REMOTE_DISTANCE;
		for (i = 0; i < lgrp_plat_node_cnt; i++) {
			for (j = 0; j < lgrp_plat_node_cnt; j++) {
				lgrp_plat_lat_stats.latencies[i][j] =
				    (i == j) ? SLIT_LOCAL_DISTANCE :
				    SLIT_REMOTE_DISTANCE;
			}
		}
	}
}

/*
 * Post-boot initialization - move boot-allocator allocated data to
 * kmem allocations.
 */
static void
lgrp_plat_main_init(void)
{
	cpu_node_map_t	*new_map;
	size_t		sz;

	if (lgrp_plat_cpu_node == NULL || lgrp_plat_cpu_node_nentries == 0) {
		return;
	}

	sz = lgrp_plat_cpu_node_nentries * sizeof (cpu_node_map_t);
	new_map = kmem_alloc(sz, KM_SLEEP);
	bcopy(lgrp_plat_cpu_node, new_map, sz);
	lgrp_plat_cpu_node = new_map;
}

/*
 * Public lgrp_plat_*() interface
 */

void
lgrp_plat_init(lgrp_init_stages_t stage)
{
	switch (stage) {
	case LGRP_INIT_STAGE1:
		lgrp_plat_get_numa_config();
		break;

	case LGRP_INIT_STAGE3:
		/* No hardware probing on Arm systems */
		break;

	case LGRP_INIT_STAGE4:
		lgrp_plat_main_init();
		break;

	default:
		break;
	}
}

void
lgrp_plat_probe(void)
{
	/* No hardware probing on Arm systems */
}

lgrp_t *
lgrp_plat_alloc(lgrp_id_t lgrpid)
{
	if (lgrpid >= NLGRP || nlgrps_alloc >= NLGRP) {
		return (NULL);
	}

	return (&lgrp_space[nlgrps_alloc++]);
}

void
lgrp_plat_config(lgrp_config_flag_t flag, uintptr_t arg)
{
	/*
	 * CPU and memory DR notifications.  For now, these are no-ops since
	 * we don't support DR on Arm yet.
	 */
}

lgrp_handle_t
lgrp_plat_cpu_to_hand(processorid_t id)
{
	if (lgrp_plat_node_cnt == 1) {
		return (LGRP_DEFAULT_HANDLE);
	}

	if (lgrp_plat_cpu_node == NULL || id < 0 ||
	    id >= lgrp_plat_cpu_node_nentries ||
	    !lgrp_plat_cpu_node[id].exists) {
		return (LGRP_DEFAULT_HANDLE);
	}

	return ((lgrp_handle_t)lgrp_plat_cpu_node[id].node);
}

lgrp_handle_t
lgrp_plat_pfn_to_hand(pfn_t pfn)
{
	int	i;

	if (max_mem_nodes == 1) {
		return (LGRP_DEFAULT_HANDLE);
	}

	for (i = 0; i < lgrp_plat_max_mem_node; i++) {
		if (!lgrp_plat_memnode_info[i].exists) {
			continue;
		}

		if (pfn >= lgrp_plat_memnode_info[i].start &&
		    pfn < lgrp_plat_memnode_info[i].end) {
			return ((lgrp_handle_t)
			    lgrp_plat_memnode_info[i].lgrphand);
		}
	}

	return (LGRP_DEFAULT_HANDLE);
}

int
lgrp_plat_latency(lgrp_handle_t from, lgrp_handle_t to)
{
	/*
	 * Return max latency for root lgroup (LGRP_DEFAULT_HANDLE).
	 *
	 * The topology builder uses this to set the root lgroup's latency,
	 * which must be >= all inter-leaf latencies for intermediate lgroups
	 * to be created correctly.
	 */
	if (from == LGRP_DEFAULT_HANDLE || to == LGRP_DEFAULT_HANDLE) {
		return (lgrp_plat_lat_stats.latency_max);
	}

	if (from >= lgrp_plat_node_cnt || to >= lgrp_plat_node_cnt) {
		return (0);
	}

	return (lgrp_plat_lat_stats.latencies[from][to]);
}

int
lgrp_plat_max_lgrps(void)
{
	int n = lgrp_plat_node_cnt;

	/*
	 * Single-node systems collapse into the root lgroup, so the quadratic
	 * formula's minimum of 1 (the root alone) is correct.
	 */
	return (n * (n - 1) + 1);
}

pgcnt_t
lgrp_plat_mem_size(lgrp_handle_t plathand, lgrp_mem_query_t query)
{
	int		mnode;
	pgcnt_t		npgs = 0;
	extern struct memlist *phys_avail;
	extern struct memlist *phys_install;

	if (plathand == LGRP_NULL_HANDLE) {
		return (0);
	}

	if (plathand == LGRP_DEFAULT_HANDLE) {
		struct memlist *mlist;

		switch (query) {
		case LGRP_MEM_SIZE_FREE:
			if (lgrp_plat_node_cnt == 1) {
				return ((pgcnt_t)freemem);
			}

			/*
			 * Multi-node: sum free pages across all memnodes.
			 */
			for (mnode = 0;
			    mnode < lgrp_plat_max_mem_node; mnode++) {
				if (!lgrp_plat_memnode_info[mnode].exists) {
					continue;
				}

				npgs += MNODE_PGCNT(mnode);
			}

			return (npgs);

		case LGRP_MEM_SIZE_AVAIL:
			memlist_read_lock();
			for (mlist = phys_avail; mlist;
			    mlist = mlist->ml_next) {
				npgs += btop(mlist->ml_size);
			}
			memlist_read_unlock();

			return (npgs);

		case LGRP_MEM_SIZE_INSTALL:
			memlist_read_lock();
			for (mlist = phys_install; mlist;
			    mlist = mlist->ml_next) {
				npgs += btop(mlist->ml_size);
			}
			memlist_read_unlock();

			return (npgs);

		default:
			return (0);
		}
	}

	/*
	 * Walk memnodes that belong to this lgrp handle and sum their
	 * installed/available pages.
	 */
	for (mnode = 0; mnode < lgrp_plat_max_mem_node; mnode++) {
		if (!lgrp_plat_memnode_info[mnode].exists) {
			continue;
		}

		if (lgrp_plat_memnode_info[mnode].lgrphand != plathand) {
			continue;
		}

		switch (query) {
		case LGRP_MEM_SIZE_FREE:
			npgs += MNODE_PGCNT(mnode);
			break;

		case LGRP_MEM_SIZE_AVAIL:
		case LGRP_MEM_SIZE_INSTALL: {
			struct memlist *mlist;
			pfn_t start = lgrp_plat_memnode_info[mnode].start;
			pfn_t end = lgrp_plat_memnode_info[mnode].end;

			memlist_read_lock();
			mlist = (query == LGRP_MEM_SIZE_INSTALL) ?
			    phys_install : phys_avail;
			for (; mlist; mlist = mlist->ml_next) {
				pfn_t ms = btop(mlist->ml_address);
				pfn_t me = btop(mlist->ml_address +
				    mlist->ml_size);
				pfn_t os, oe;

				/* Compute overlap with this memnode */
				os = MAX(ms, start);
				oe = MIN(me, end);
				if (os < oe) {
					npgs += (oe - os);
				}
			}
			memlist_read_unlock();

			break;
		}
		default:
			break;
		}
	}

	return (npgs);
}

lgrp_handle_t
lgrp_plat_root_hand(void)
{
	return (LGRP_DEFAULT_HANDLE);
}

/*
 * VM integration: plat_* functions
 *
 * These are called from the memnode layer via #pragma weak.
 */

/*
 * Allocate an orphan memnode for EFI memory not described by any SRAT Memory
 * Affinity entry.  The range is attributed to memnode 0's proximity domain, or,
 * if the SRAT has no memory entries at all, to the first SRAT proximity domain.
 *
 * start and end are inclusive PFNs.
 */
static void
lgrp_plat_orphan_memnode(pfn_t start, pfn_t end)
{
	int	omn;

	for (omn = 0; omn < MAX_MEM_NODES; omn++) {
		if (!lgrp_plat_memnode_info[omn].exists) {
			break;
		}
	}

	if (omn >= MAX_MEM_NODES) {
		cmn_err(CE_PANIC, "!lgrp: no memnode slot "
		    "for orphan memory [%lx, %lx]",
		    start, end);
	}

	lgrp_plat_memnode_info[omn].start = start;
	lgrp_plat_memnode_info[omn].end = end + 1;
	if (lgrp_plat_memnode_info[0].exists) {
		lgrp_plat_memnode_info[omn].prox_domain =
		    lgrp_plat_memnode_info[0].prox_domain;
		lgrp_plat_memnode_info[omn].lgrphand =
		    lgrp_plat_memnode_info[0].lgrphand;
	} else {
		/*
		 * No SRAT memory entries at all, so no memnode 0 to copy from.
		 *
		 * Attribute the orphan to the first SRAT proximity domain.
		 * Guaranteed to exist because plat_build_mem_nodes() only gets
		 * here when node_cnt > 1.
		 */
		lgrp_plat_memnode_info[omn].prox_domain =
		    lgrp_plat_node_domain[0].prox_domain;
		lgrp_plat_memnode_info[omn].lgrphand =
		    (lgrp_handle_t)0;
	}
	lgrp_plat_memnode_info[omn].exists = 1;
	lgrp_plat_memnode_info[omn].orphan = B_TRUE;

	if ((uint_t)(omn + 1) > lgrp_plat_max_mem_node) {
		lgrp_plat_max_mem_node = omn + 1;
	}

	if (omn + 1 > max_mem_nodes) {
		max_mem_nodes = omn + 1;
	}

	cmn_err(CE_WARN, "!lgrp: memory [%lx, %lx] not described in SRAT, "
	    "assigned to memnode %d (domain %u)",
	    start, end, omn, lgrp_plat_memnode_info[omn].prox_domain);

	mem_node_add_slice(start, end);
}

/*
 * Build memory nodes from the physical memlist.
 *
 * Each memory range is assigned to the appropriate memnode based on SRAT memory
 * affinity data.
 */
void
plat_build_mem_nodes(struct memlist *list)
{
	struct memlist	*ml;
	pfn_t		start, end;

	for (ml = list; ml != NULL; ml = ml->ml_next) {
		start = btop(ml->ml_address);
		if (start > physmax) {
			continue;
		}

		end = btop(ml->ml_address + ml->ml_size) - 1;
		if (end > physmax) {
			end = physmax;
		}

		if (max_mem_nodes == 1 || lgrp_plat_node_cnt <= 1) {
			/*
			 * UMA system or single NUMA node.
			 */
			mem_node_add_range(start, end);
			continue;
		}

		/*
		 * Walk the range memnode-by-memnode, splitting at boundaries.
		 *
		 * Sub-ranges that fall outside all SRAT memnodes are assigned
		 * to orphan memnodes.
		 */
		pfn_t cur = start;
		do {
			pfn_t	cur_end = end;
			int	found = -1;
			int	mnode;

			for (mnode = 0;
			    mnode < lgrp_plat_max_mem_node; mnode++) {
				pfn_t ms, me;

				if (!lgrp_plat_memnode_info[mnode].exists) {
					continue;
				}

				ms = lgrp_plat_memnode_info[mnode].start;
				me = lgrp_plat_memnode_info[mnode].end - 1;

				if (cur >= ms && cur <= me) {
					found = mnode;
					if (cur_end > me) {
						cur_end = me;
					}

					break;
				}

				/*
				 * Tighten orphan upper bound.
				 */
				if (ms > cur && ms - 1 < cur_end) {
					cur_end = ms - 1;
				}
			}

			if (found >= 0) {
				mem_node_add_slice(cur, cur_end);
			} else {
				lgrp_plat_orphan_memnode(cur, cur_end);
			}

			cur = cur_end + 1;
		} while (cur <= end);
	}
}

/*
 * Given a PFN, return the memnode it belongs to.
 */
int
plat_pfn_to_mem_node(pfn_t pfn)
{
	int	i;

	for (i = 0; i < lgrp_plat_max_mem_node; i++) {
		if (!lgrp_plat_memnode_info[i].exists) {
			continue;
		}

		if (pfn >= lgrp_plat_memnode_info[i].start &&
		    pfn < lgrp_plat_memnode_info[i].end)
			return (i);
	}

	/*
	 * Didn't find memnode where this PFN lives.  Should never happen.
	 */
	ASSERT(!"plat_pfn_to_mem_node: PFN not in any memnode");
	return (0);
}

/*
 * Given a memnode, return the lgrp handle it belongs to.
 */
lgrp_handle_t
plat_mem_node_to_lgrphand(int mnode)
{
	if (mnode < 0 || mnode >= MAX_MEM_NODES ||
	    !lgrp_plat_memnode_info[mnode].exists) {
		return (LGRP_DEFAULT_HANDLE);
	}

	return ((lgrp_handle_t)lgrp_plat_memnode_info[mnode].lgrphand);
}

/*
 * Given an lgrp handle, return the first memnode for it.
 */
int
plat_lgrphand_to_mem_node(lgrp_handle_t hand)
{
	int	i;

	for (i = 0; i < lgrp_plat_max_mem_node; i++) {
		if (lgrp_plat_memnode_info[i].exists &&
		    lgrp_plat_memnode_info[i].lgrphand == (int)hand) {
			return (i);
		}
	}

	return (-1);
}

/*
 * Assign an lgrp handle to a memnode.
 */
void
plat_assign_lgrphand_to_mem_node(lgrp_handle_t hand, int mnode)
{
	if (mnode >= 0 && mnode < MAX_MEM_NODES) {
		lgrp_plat_memnode_info[mnode].lgrphand = (int)hand;
	}
}
