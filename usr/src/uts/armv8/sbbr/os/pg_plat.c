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
 * Processor Group platform support for ACPI-based aarch64 systems.
 *
 * This file shadows armv8/os/pg_plat.c on ACPI (SBBR) platforms, providing
 * authoritative processor topology derived from the PPTT (Processor Properties
 * Topology Table) defined in ACPI 6.2+.
 *
 * PPTT describes a tree of processor nodes (packages, clusters, cores) and
 * cache nodes with explicit sharing relationships.
 *
 * Design:
 *
 *   pg_plat_set_fw() is called very early, before pg_cpu_bootstrap()
 *   and after cpuinfo_bootstrap(), receiving the physical address of the
 *   PPTT from dboot via xboot_info.  The table is still mapped at this
 *   point (all firmware memory is accessible until bootstrap memory
 *   is released).
 *
 *   Parsing is done in a single pass: Every leaf processor node is walked.
 *     For each, look up the processorid_t, walk the parent chain to find the
 *     physical package and any intermediate cluster nodes.  Walk the private
 *     resource list to find cache references, then follow NextLevelOfCache
 *     chains to the terminal (highest-level) cache node.  Record each CPU's
 *     terminal cache node offset and populate package/cluster/core IDs in the
 *     static pptt_info[NCPU] lookup table.
 *
 *   The LLC for a CPU is its terminal cache node.  If the terminal
 *     cache is private, each CPU gets its own unique LLC ID; this is
 *     the case on the Ampere Altra Max in default SBSA mode, where the
 *     SLC is transparent and no shared cache is visible.
 *
 *   Cache node identity uses the PPTT byte offset of the ACPI_PPTT_CACHE
 *   structure.  Two CPUs whose cache chains converge at the same offset
 *   share that cache.
 *
 *   The pptt_info[] array is static, indexed by cpu_id (0 .. NCPU-1),
 *   populated once at boot, and read-only thereafter.  No locking is needed.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpuvar.h>
#include <sys/cmt.h>
#include <sys/pghw.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/acpi/acpi.h>
#include <sys/cpuinfo.h>

/*
 * Per-CPU topology information derived from PPTT.
 */
typedef struct pptt_cpu_info {
	uint32_t	pci_packageid;		/* PPTT offset of package */
	uint32_t	pci_clusterid;		/* PPTT offset of cluster */
	uint32_t	pci_coreid;		/* PPTT offset of core */
	uint32_t	pci_llc_id;		/* PPTT offset of LLC cache */
	uint32_t	pci_llc_level;		/* cache level (1, 2, 3, ...) */
	uint64_t	pci_llc_size;		/* LLC size in bytes */
	uint32_t	pci_llc_nsets;		/* number of sets */
	uint32_t	pci_llc_assoc;		/* associativity */
	uint32_t	pci_llc_type;		/* cache type (ACPI attrs) */
	boolean_t	pci_valid;		/* entry populated */
} pptt_cpu_info_t;

static pptt_cpu_info_t pptt_info[NCPU];
static boolean_t pptt_parsed = B_FALSE;

/*
 * ACPI 6.6 §5.2.29: bit 2 of the ACPI_PPTT_PROCESSOR Flags field marks a
 * processor node as a thread of its parent core.  This flag is not defined
 * in our copy of ACPICA.
 */
#define	ACPI_PPTT_PROCESSOR_IS_THREAD	(1 << 2)

/*
 * Maximum depth when walking PPTT parent chains.  Guards against
 * malformed tables that would cause infinite loops.
 */
#define	PPTT_MAX_DEPTH	16

/*
 * Maximum number of processor nodes we track for the leaf detection pre-pass.
 *
 * A pre-pass is needed because (unsurprisingly), some firmware gets the
 * semantics of the ACPI_PROCESSOR_ID_VALID bit wrong (either not setting it,
 * or setting in on intermediate nodes).
 *
 * This must be at least as large as the total number of processor hierarchy
 * nodes (packages + clusters + cores).  We allow 8 per CPU, where each CPU
 * has only a handful of ancestors, and ancestors are shared between CPUs.  This
 * means that our sizing is very generous, and should have lots of room to
 * spare.  If the PPTT exceeds this, parsing will fail.
 */
#define	PPTT_MAX_PROC_NODES	(NCPU * 8)

/*
 * Parent offset set for leaf detection.  Any processor node whose offset
 * does not appear in this array is a leaf (no other processor node
 * references it as a parent).
 */
static uint32_t	pptt_parent_offs[PPTT_MAX_PROC_NODES];
static int	pptt_nparents;

/*
 * Return B_TRUE if the given offset appears in the parent set.
 */
static boolean_t
pptt_is_parent(uint32_t offset)
{
	int i;

	for (i = 0; i < pptt_nparents; i++) {
		if (pptt_parent_offs[i] == offset) {
			return (B_TRUE);
		}
	}

	return (B_FALSE);
}

/*
 * Given a byte offset within the PPTT, return a pointer to the subtable header
 * at that offset, or NULL if the offset is zero or out of bounds.
 */
static ACPI_SUBTABLE_HEADER *
pptt_offset_to_sub(ACPI_TABLE_PPTT *pptt, uint32_t offset)
{
	if (offset == 0 || offset >= pptt->Header.Length) {
		return (NULL);
	}

	return ((ACPI_SUBTABLE_HEADER *)((uintptr_t)pptt + offset));
}

/*
 * Walk a processor node's parent chain to find the physical package and the
 * innermost intermediate node (cluster).
 *
 * Sets *pkg_off to the offset of the PHYSICAL_PACKAGE node and *clust_off
 * to the offset of the node immediately above the leaf (if one exists between
 * the leaf and the package).  If there is no intermediate node, *clust_off is
 * set equal to *pkg_off.
 */
static void
pptt_find_package_cluster(ACPI_TABLE_PPTT *pptt, ACPI_PPTT_PROCESSOR *leaf,
    uint32_t leaf_off, uint32_t *pkg_off, uint32_t *clust_off)
{
	ACPI_SUBTABLE_HEADER *sub;
	ACPI_PPTT_PROCESSOR *proc;
	uint32_t cur_off;
	uint32_t prev_off;
	uint32_t inner_off;
	int depth;

	*pkg_off = leaf_off;
	*clust_off = leaf_off;

	prev_off = leaf_off;
	inner_off = leaf_off;
	cur_off = leaf->Parent;
	depth = 0;

	while (cur_off != 0 && depth < PPTT_MAX_DEPTH) {
		sub = pptt_offset_to_sub(pptt, cur_off);
		if (sub == NULL || sub->Type != ACPI_PPTT_TYPE_PROCESSOR) {
			break;
		}

		proc = (ACPI_PPTT_PROCESSOR *)sub;

		if (proc->Flags & ACPI_PPTT_PHYSICAL_PACKAGE) {
			*pkg_off = cur_off;

			/*
			 * The cluster is the innermost intermediate node
			 * (the first node above the leaf).  If there is no
			 * intermediate node, the cluster is the package.
			 */
			if (inner_off != leaf_off) {
				*clust_off = inner_off;
			} else {
				*clust_off = cur_off;
			}

			return;
		}

		/*
		 * The first intermediate node above the leaf is the innermost
		 * cluster.
		 */
		if (inner_off == leaf_off) {
			inner_off = cur_off;
		}

		prev_off = cur_off;
		cur_off = proc->Parent;
		depth++;
	}

	/*
	 * If we didn't find a PHYSICAL_PACKAGE node, use the highest
	 * node we reached as the package.
	 */
	*pkg_off = prev_off;
	if (inner_off != leaf_off) {
		*clust_off = inner_off;
	} else {
		*clust_off = leaf_off;
	}
}

/*
 * Walk a processor node's private resource list, find the highest-level
 * cache (following NextLevelOfCache chains), and return its offset within
 * the PPTT.  Also fills in cache properties for the terminal node.
 *
 * Returns 0 if no cache is found.
 */
static uint32_t
pptt_find_terminal_cache(ACPI_TABLE_PPTT *pptt, ACPI_PPTT_PROCESSOR *proc,
    uint32_t proc_off, uint32_t *level, uint64_t *size, uint32_t *nsets,
    uint32_t *assoc, uint32_t *type)
{
	ACPI_SUBTABLE_HEADER *sub;
	ACPI_PPTT_CACHE *cache;
	uint32_t *res;
	uint32_t best_off = 0;
	uint32_t best_level = 0;
	uint32_t i;
	int depth;

	if (proc->NumberOfPrivResources == 0) {
		return (0);
	}

	/*
	 * The private resource array must fit within this subtable.
	 */
	if (sizeof (ACPI_PPTT_PROCESSOR) +
	    proc->NumberOfPrivResources * sizeof (uint32_t) >
	    proc->Header.Length) {
		return (0);
	}

	/*
	 * Private resource offsets follow immediately after the
	 * ACPI_PPTT_PROCESSOR structure.
	 */
	res = (uint32_t *)((uintptr_t)proc + sizeof (ACPI_PPTT_PROCESSOR));

	for (i = 0; i < proc->NumberOfPrivResources; i++) {
		uint32_t cur_off = res[i];
		uint32_t cur_level = 1;

		sub = pptt_offset_to_sub(pptt, cur_off);
		if (sub == NULL || sub->Type != ACPI_PPTT_TYPE_CACHE) {
			continue;
		}

		cache = (ACPI_PPTT_CACHE *)sub;

		/*
		 * Follow NextLevelOfCache chain to the terminal.
		 */
		depth = 0;
		while (cache->NextLevelOfCache != 0 && depth < PPTT_MAX_DEPTH) {
			sub = pptt_offset_to_sub(pptt, cache->NextLevelOfCache);
			if (sub == NULL || sub->Type != ACPI_PPTT_TYPE_CACHE) {
				break;
			}

			cache = (ACPI_PPTT_CACHE *)sub;
			cur_level++;
			depth++;
		}

		/*
		 * Use the byte offset within the PPTT to track the
		 * terminal node -- this is the last valid cache we
		 * visited.
		 */
		cur_off = (uint32_t)((uintptr_t)cache - (uintptr_t)pptt);

		if (cur_level > best_level) {
			best_level = cur_level;
			best_off = cur_off;
		}
	}

	if (best_off != 0) {
		cache = (ACPI_PPTT_CACHE *)pptt_offset_to_sub(pptt, best_off);

		*level = best_level;
		*size = (cache->Flags & ACPI_PPTT_SIZE_PROPERTY_VALID) ?
		    cache->Size : 0;
		*nsets = (cache->Flags & ACPI_PPTT_NUMBER_OF_SETS_VALID) ?
		    cache->NumberOfSets : 0;
		*assoc = (cache->Flags & ACPI_PPTT_ASSOCIATIVITY_VALID) ?
		    cache->Associativity : 0;
		*type = (cache->Flags & ACPI_PPTT_CACHE_TYPE_VALID) ?
		    (cache->Attributes & ACPI_PPTT_MASK_CACHE_TYPE) : 0;
	}

	return (best_off);
}

/*
 * Parse the PPTT and populate pptt_info[].
 *
 * Called during early boot, before kmem is available.
 *
 * All data is stored in static arrays.
 */
static void
pptt_parse(ACPI_TABLE_PPTT *pptt)
{
	ACPI_SUBTABLE_HEADER *sub;
	ACPI_PPTT_PROCESSOR *proc;
	uint32_t offset;
	uint32_t end;
	int ncpus_found = 0;

	end = pptt->Header.Length;
	offset = sizeof (ACPI_TABLE_PPTT);

	/*
	 * Build the set of offsets that are referenced as Parent by at least
	 * one processor node.  Any processor node whose own offset is NOT in
	 * this set is a leaf (has no children).  This handles a common firmware
	 * bug, where the ACPI_PPTT_ACPI_PROCESSOR_ID_VALID flag is not set on
	 * leaf processors.
	 */
	pptt_nparents = 0;
	while (offset < end) {
		sub = (ACPI_SUBTABLE_HEADER *)((uintptr_t)pptt + offset);

		if (sub->Length < sizeof (ACPI_SUBTABLE_HEADER) ||
		    offset + sub->Length > end) {
			break;
		}

		if (sub->Type == ACPI_PPTT_TYPE_PROCESSOR) {
			proc = (ACPI_PPTT_PROCESSOR *)sub;

			if (proc->Parent != 0 &&
			    !pptt_is_parent(proc->Parent)) {
				if (pptt_nparents < PPTT_MAX_PROC_NODES) {
					pptt_parent_offs[pptt_nparents++] =
					    proc->Parent;
				} else {
					/*
					 * The parent set is full, so leaf
					 * detection would be unreliable.
					 */
					cmn_err(CE_WARN, "!pg_plat: PPTT has "
					    "more than %d parent nodes, "
					    "ignoring PPTT topology",
					    PPTT_MAX_PROC_NODES);
					return;
				}
			}
		}

		offset += sub->Length;
	}

	offset = sizeof (ACPI_TABLE_PPTT);

	/*
	 * Pass 1: Walk all subtables, process leaf processor nodes.
	 */
	while (offset < end) {
		sub = (ACPI_SUBTABLE_HEADER *)((uintptr_t)pptt + offset);

		if (sub->Length < sizeof (ACPI_SUBTABLE_HEADER) ||
		    offset + sub->Length > end) {
			break;
		}

		if (sub->Type == ACPI_PPTT_TYPE_PROCESSOR) {
			proc = (ACPI_PPTT_PROCESSOR *)sub;

			/*
			 * A processor node is a leaf if:
			 *  - ACPI_PPTT_ACPI_PROCESSOR_ID_VALID is set, or
			 *  - No other processor node references it as Parent
			 */
			if ((proc->Flags & ACPI_PPTT_ACPI_PROCESSOR_ID_VALID) ||
			    !pptt_is_parent(offset)) {
				processorid_t cpu_id;
				uint32_t pkg_off, clust_off;
				uint32_t core_off;
				uint32_t llc_level = 0;
				uint64_t llc_size = 0;
				uint32_t llc_nsets = 0, llc_assoc = 0;
				uint32_t llc_type = 0;
				uint32_t term_off;

				cpu_id = cpuinfo_id_for_uid(
				    proc->AcpiProcessorId);
				if (cpu_id == (processorid_t)-1) {
					offset += sub->Length;
					continue;
				}

				if (cpu_id >= NCPU) {
					offset += sub->Length;
					continue;
				}

				if (pptt_info[cpu_id].pci_valid) {
					cmn_err(CE_WARN, "!pg_plat: duplicate "
					    "ACPI Processor ID %u, ignoring",
					    proc->AcpiProcessorId);
					offset += sub->Length;
					continue;
				}

				pptt_find_package_cluster(pptt, proc,
				    offset, &pkg_off, &clust_off);

				term_off = pptt_find_terminal_cache(pptt,
				    proc, offset, &llc_level, &llc_size,
				    &llc_nsets, &llc_assoc, &llc_type);

				/*
				 * If this leaf is a thread, the core is its
				 * parent processor node.  If the parent
				 * does not resolve to a processor node, fall
				 * back to the leaf itself.
				 *
				 * I would hate to see a PPTT where this is
				 * needed, but at this point I am no longer
				 * suprised by anything that comes out of
				 * firmware.
				 */
				if ((proc->Flags &
				    ACPI_PPTT_PROCESSOR_IS_THREAD) != 0) {
					ACPI_SUBTABLE_HEADER *core_sub;

					core_sub = pptt_offset_to_sub(pptt,
					    proc->Parent);
					if (core_sub != NULL &&
					    core_sub->Type ==
					    ACPI_PPTT_TYPE_PROCESSOR) {
						core_off = proc->Parent;
					} else {
						core_off = offset;
					}
				} else {
					core_off = offset;
				}

				pptt_info[cpu_id].pci_packageid = pkg_off;
				pptt_info[cpu_id].pci_clusterid = clust_off;
				pptt_info[cpu_id].pci_coreid = core_off;
				pptt_info[cpu_id].pci_llc_id = term_off;
				pptt_info[cpu_id].pci_llc_level = llc_level;
				pptt_info[cpu_id].pci_llc_size = llc_size;
				pptt_info[cpu_id].pci_llc_nsets = llc_nsets;
				pptt_info[cpu_id].pci_llc_assoc = llc_assoc;
				pptt_info[cpu_id].pci_llc_type = llc_type;
				pptt_info[cpu_id].pci_valid = B_TRUE;

				ncpus_found++;
			}
		}

		offset += sub->Length;
	}

	if (ncpus_found == 0) {
		cmn_err(CE_WARN, "!pg_plat: PPTT contains no leaf processor "
		    "nodes with valid ACPI Processor IDs");
		return;
	}

	pptt_parsed = B_TRUE;
}

/*
 * Set the PPTT physical address and trigger parsing.
 *
 * Must be called before pg_cpu_bootstrap().
 */
void
pg_plat_set_fw(uint64_t pptt_pa)
{
	ACPI_TABLE_PPTT *pptt;

	if (pptt_pa == 0) {
		return;
	}

	pptt = (ACPI_TABLE_PPTT *)(uintptr_t)pptt_pa;

	if (strncmp(pptt->Header.Signature, ACPI_SIG_PPTT, 4) != 0) {
		cmn_err(CE_WARN, "!pg_plat: PPTT table has bad signature");
		return;
	}

	if (pptt->Header.Length < sizeof (ACPI_TABLE_PPTT)) {
		cmn_err(CE_WARN, "!pg_plat: PPTT table too short (%u bytes)",
		    pptt->Header.Length);
		return;
	}

	pptt_parse(pptt);
}

/*
 * Return the instance ID for the given CPU and hardware type, from the
 * parsed PPTT.  Returns -1 for an unsupported hardware type.
 *
 * For CACHE, when no terminal cache was found for this CPU, returns a
 * synthetic ID that cannot collide with a real PPTT offset, so the
 * CPU is not grouped with any other CPU.
 */
static id_t
pptt_instance_id(processorid_t id, pghw_type_t hw)
{
	switch (hw) {
	case PGHW_CHIP:
		return ((id_t)pptt_info[id].pci_packageid);
	case PGHW_CACHE:
		if (pptt_info[id].pci_llc_id == 0) {
			return (-(id + 2));
		}

		return ((id_t)pptt_info[id].pci_llc_id);
	case PGHW_PROCNODE:
		return ((id_t)pptt_info[id].pci_clusterid);
	case PGHW_IPIPE:
		return ((id_t)pptt_info[id].pci_coreid);
	default:
		return ((id_t)-1);
	}
}

/*
 * Return whether the specified CPU shares the given hardware type
 * with any other CPU.
 *
 * This is _not_ called on the hot path (i.e. from the dispatcher).
 */
int
pg_plat_hw_shared(cpu_t *cp, pghw_type_t hw)
{
	id_t my_id;
	int i;
	processorid_t id = cp->cpu_id;

	if (!pptt_parsed || id < 0 || id >= NCPU || !pptt_info[id].pci_valid) {
		return (0);
	}

	if ((my_id = pptt_instance_id(id, hw)) == (id_t)-1) {
		return (0);
	}

	for (i = 0; i < NCPU; i++) {
		if (i == id || !pptt_info[i].pci_valid) {
			continue;
		}

		if (pptt_instance_id(i, hw) == my_id) {
			return (1);
		}
	}

	return (0);
}

/*
 * Compare two CPUs and see if they have a pghw_type_t sharing relationship.
 * If pghw_type_t is an unsupported hardware type, then return -1.
 */
int
pg_plat_cpus_share(cpu_t *cpu_a, cpu_t *cpu_b, pghw_type_t hw)
{
	id_t pgp_a, pgp_b;

	pgp_a = pg_plat_hw_instance_id(cpu_a, hw);
	pgp_b = pg_plat_hw_instance_id(cpu_b, hw);

	if (pgp_a == -1 || pgp_b == -1) {
		return (-1);
	}

	return (pgp_a == pgp_b);
}

/*
 * Return a physical instance identifier for known hardware sharing
 * relationships.  CPUs that return the same ID share the given hardware.
 */
id_t
pg_plat_hw_instance_id(cpu_t *cpu, pghw_type_t hw)
{
	processorid_t id = cpu->cpu_id;

	if (!pptt_parsed || id < 0 || id >= NCPU || !pptt_info[id].pci_valid) {
		/*
		 * Fallback: unique core and cache, single cluster and chip.
		 */
		switch (hw) {
		case PGHW_IPIPE:
			return (id);
		case PGHW_CACHE:
			return (id);
		case PGHW_PROCNODE:
			return (0);
		case PGHW_CHIP:
			return (0);
		default:
			return (-1);
		}
	}

	return (pptt_instance_id(id, hw));
}

/*
 * Override the default CMT dispatcher policy for the specified
 * hardware sharing relationship.
 */
pg_cmt_policy_t
pg_plat_cmt_policy(pghw_type_t hw)
{
	switch (hw) {
	case PGHW_CACHE:
		return (CMT_BALANCE|CMT_AFFINITY);
	default:
		return (CMT_NO_POLICY);
	}
}

id_t
pg_plat_get_core_id(cpu_t *cpu)
{
	processorid_t id = cpu->cpu_id;

	if (pptt_parsed && id >= 0 && id < NCPU &&
	    pptt_info[id].pci_valid) {
		return ((id_t)pptt_info[id].pci_coreid);
	}

	return ((id_t)id);
}

/*
 * Express preference for optimising the sharing relationship of the
 * given hardware types.
 */
pghw_type_t
pg_plat_hw_rank(pghw_type_t hw1, pghw_type_t hw2)
{
	int i, rank1, rank2;

	static pghw_type_t hw_hier[] = {
		PGHW_IPIPE,
		PGHW_CACHE,
		PGHW_PROCNODE,
		PGHW_CHIP,
		PGHW_NUM_COMPONENTS
	};

	rank1 = 0;
	rank2 = 0;

	for (i = 0; hw_hier[i] != PGHW_NUM_COMPONENTS; i++) {
		if (hw_hier[i] == hw1) {
			rank1 = i;
		}

		if (hw_hier[i] == hw2) {
			rank2 = i;
		}
	}

	if (rank1 > rank2) {
		return (hw1);
	} else {
		return (hw2);
	}
}
