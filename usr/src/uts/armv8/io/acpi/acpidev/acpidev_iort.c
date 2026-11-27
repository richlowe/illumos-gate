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
 * IORT (I/O Remapping Table) parser.
 *
 * The IORT describes the topology connecting PCIe root complexes to
 * MSI controllers (ITS groups) and, optionally, SMMUs.  We parse it
 * to synthesise msi-parent / msi-map and dma-coherent properties on
 * PCIe root complex device tree nodes created by acpidev_mcfg.c.
 *
 * SMMU nodes are treated as transparent for now - we follow through
 * them to reach the ITS group on the other side.  Full SMMU support
 * is future work.
 *
 * When no IORT is present but exactly one ITS exists (registered via
 * acpidev_gic.c), we create a wildcard entry so that all root
 * complexes get an msi-parent pointing to the sole ITS.
 *
 * When no ITS exists, but GICv2m frames exist, we allocate MSI frames
 * to devices in a round-robin fashion (distributing the constrained
 * MSI/MSI-X space across consumers).
 */

#include <sys/types.h>
#include <sys/list.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/stdbool.h>
#include <sys/stddef.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/acpidev_gic.h>
#include <sys/acpidev_iort.h>
#include <sys/acpidev_impl.h>

/* Linked list of parsed root complex entries */
static acpidev_iort_rc_t *iort_rc_list = NULL;

/* Saved table header for AcpiPutTable() in fini */
static ACPI_TABLE_HEADER *iort_table_hdr = NULL;

/*
 * Temporary structure used during parsing to index ITS groups
 * by their byte offset within the IORT table (OutputReference
 * in ID mappings is a byte offset from the start of the table).
 */
typedef struct iort_its_group_entry {
	uint32_t		ige_offset;	/* byte offset of IORT node */
	uint32_t		ige_nids;
	uint32_t		*ige_ids;	/* points into ACPI table */
	list_node_t		ige_node;
} iort_its_group_entry_t;

/*
 * Find an ITS group in the temporary list by byte offset.
 */
static iort_its_group_entry_t *
iort_find_its_group(list_t *list, uint32_t offset)
{
	iort_its_group_entry_t *e;

	for (e = list_head(list); e != NULL; e = list_next(list, e)) {
		if (e->ige_offset == offset) {
			return (e);
		}
	}

	return (NULL);
}

/*
 * Given an IORT node at byte offset 'node_offset' in the table,
 * return a pointer to the ACPI_IORT_NODE, or NULL if the offset
 * does not point at a whole node header within the table.
 */
static ACPI_IORT_NODE *
iort_node_at(ACPI_TABLE_IORT *iort, uint32_t node_offset)
{
	uint32_t table_len = iort->Header.Length;

	if (node_offset < sizeof (ACPI_TABLE_IORT) ||
	    node_offset > table_len ||
	    table_len - node_offset < sizeof (ACPI_IORT_NODE)) {
		return (NULL);
	}

	return ((ACPI_IORT_NODE *)((uintptr_t)iort + node_offset));
}

/*
 * Check that a node's Length fits within the table and covers at
 * least 'min_len' bytes.  The node must have been obtained from
 * iort_node_at(), so its header already lies within the table.
 */
static boolean_t
iort_node_len_ok(ACPI_TABLE_IORT *iort, ACPI_IORT_NODE *node,
    uint32_t min_len)
{
	uint32_t table_len = iort->Header.Length;
	uint32_t node_off = (uint32_t)((uintptr_t)node - (uintptr_t)iort);

	if (node_off > table_len) {
		return (B_FALSE);
	}

	if (node->Length < min_len ||
	    node->Length > table_len - node_off) {
		return (B_FALSE);
	}

	return (B_TRUE);
}

/*
 * Check that a node's ID mapping array fits within the node.
 * The node's Length is validated to fit within the table first,
 * so the array bound here is relative to the node only.
 */
static boolean_t
iort_mappings_ok(ACPI_TABLE_IORT *iort, ACPI_IORT_NODE *node)
{
	uint32_t avail;

	if (!iort_node_len_ok(iort, node, sizeof (ACPI_IORT_NODE))) {
		return (B_FALSE);
	}

	if (node->MappingCount == 0) {
		return (B_TRUE);
	}

	if (node->MappingOffset > node->Length) {
		return (B_FALSE);
	}

	avail = node->Length - node->MappingOffset;
	if (node->MappingCount > avail / sizeof (ACPI_IORT_ID_MAPPING)) {
		return (B_FALSE);
	}

	return (B_TRUE);
}

/*
 * Resolve an ID mapping's OutputReference to an ITS group, following
 * through SMMU nodes transparently.  Returns the ITS group entry or
 * NULL if the chain doesn't lead to an ITS group.
 *
 * input_id is the ID entering the target node (e.g. the StreamID
 * from the RC's OutputBase).  At each SMMU hop the function matches
 * the mapping whose input range covers this ID, translates through
 * it, and recurses with the translated output ID.
 *
 * On success, *devid_out is set to the fully translated DeviceID
 * at the ITS group.  The caller uses this directly as the DeviceID
 * base for the mapping entry.
 *
 * depth_limit prevents infinite loops in malformed tables.
 */
static iort_its_group_entry_t *
iort_resolve_to_its(ACPI_TABLE_IORT *iort, list_t *its_list,
    uint32_t target_offset, uint32_t input_id, int depth_limit,
    uint32_t *devid_out)
{
	ACPI_IORT_NODE		*target;
	iort_its_group_entry_t	*ige;

	if (depth_limit <= 0) {
		return (NULL);
	}

	target = iort_node_at(iort, target_offset);
	if (target == NULL) {
		return (NULL);
	}

	switch (target->Type) {
	case ACPI_IORT_NODE_ITS_GROUP:
		/* End of the chain - the input ID is the DeviceID. */
		if (devid_out != NULL) {
			*devid_out = input_id;
		}
		ige = iort_find_its_group(its_list, target_offset);
		return (ige);

	case ACPI_IORT_NODE_SMMU:
	case ACPI_IORT_NODE_SMMU_V3: {
		/*
		 * Follow through SMMU to the ITS group, translating
		 * input_id through the matching data-path mapping.
		 *
		 * For SMMUv3 nodes, the IdMappingIndex entry is the
		 * "special" mapping (typically SINGLE_MAPPING) used
		 * for the SMMU's own MSI domain - skip it.  The
		 * remaining mappings describe data-path ID translation.
		 *
		 * For SMMUv1/v2 nodes, there is no special index.
		 */
		ACPI_IORT_ID_MAPPING	*maps;
		ACPI_IORT_ID_MAPPING	*follow;
		uint32_t		k;
		uint32_t		output_id;
		int			skip_index = -1;

		if (target->MappingCount == 0 ||
		    !iort_mappings_ok(iort, target)) {
			return (NULL);
		}

		maps = (ACPI_IORT_ID_MAPPING *)
		    ((uintptr_t)target + target->MappingOffset);

		/*
		 * For SMMUv3, get the special ID mapping index
		 * and skip it during ID translation (matches
		 * Linux iort_get_id_mapping_index).
		 */
		if (target->Type == ACPI_IORT_NODE_SMMU_V3 &&
		    target->Revision >= 1) {
			ACPI_IORT_SMMU_V3 *smmu;

			if (!iort_node_len_ok(iort, target,
			    offsetof (ACPI_IORT_NODE, NodeData) +
			    sizeof (ACPI_IORT_SMMU_V3))) {
				return (NULL);
			}

			smmu = (ACPI_IORT_SMMU_V3 *)target->NodeData;
			/*
			 * The IdMappingIndex is valid when not all
			 * interrupts are wired (GSIV-based).  This
			 * matches Linux's heuristic for rev < 5.
			 */
			if (!(smmu->EventGsiv && smmu->PriGsiv &&
			    smmu->GerrGsiv && smmu->SyncGsiv)) {
				if (smmu->IdMappingIndex <
				    target->MappingCount) {
					skip_index =
					    (int)smmu->IdMappingIndex;
				}
			}
		}

		/*
		 * Find the data-path mapping that covers input_id.
		 * SINGLE_MAPPING covers the entire ID space.
		 * Otherwise match by InputBase/IdCount range.
		 */
		follow = NULL;
		for (k = 0; k < target->MappingCount; k++) {
			if ((int)k == skip_index) {
				continue;
			}
			if (maps[k].Flags & ACPI_IORT_ID_SINGLE_MAPPING) {
				follow = &maps[k];
				break;
			}
			/*
			 * IdCount carries the number of IDs minus 1, so
			 * the range is [InputBase, InputBase + IdCount]
			 * inclusive.
			 */
			if (input_id >= maps[k].InputBase &&
			    input_id - maps[k].InputBase <=
			    maps[k].IdCount) {
				follow = &maps[k];
				break;
			}
		}

		if (follow == NULL) {
			return (NULL);
		}

		/*
		 * Translate input_id through this mapping.
		 */
		output_id = follow->OutputBase + (input_id - follow->InputBase);

		return (iort_resolve_to_its(iort, its_list,
		    follow->OutputReference, output_id, depth_limit - 1,
		    devid_out));
	}

	default:
		return (NULL);
	}
}

/*
 * Check whether all mappings for a root complex resolve to a single
 * ITS with an identity RID-to-DeviceID translation covering the full
 * 16-bit RID space.  If so, msi-parent alone suffices (no msi-map).
 *
 * Returns B_TRUE and sets *tridp to the ITS Translation ID if simple.
 */
static boolean_t
iort_is_simple(acpidev_iort_map_t *maps, uint32_t nmaps, uint32_t *tridp)
{
	uint32_t	trid;
	uint32_t	i;
	uint64_t	coverage;

	if (nmaps == 0) {
		return (B_FALSE);
	}

	/* All mappings must target the same ITS */
	trid = maps[0].aim_its_trid;
	for (i = 1; i < nmaps; i++) {
		if (maps[i].aim_its_trid != trid) {
			return (B_FALSE);
		}
	}

	/*
	 * Verify the mappings cover [0, 0x10000) contiguously with
	 * identity translation (rid_base == devid_base for each range).
	 *
	 * A simple sum of rid_count values would be fooled by
	 * overlapping ranges from malformed firmware.  Sort by
	 * rid_base first, then walk the sorted ranges checking
	 * that each abuts the previous with no gaps or overlaps.
	 *
	 * In practice, well-formed IORT tables will not have
	 * overlapping RC ID mappings, this guards against
	 * pathologically broken firmware only.
	 */
	for (i = 0; i < nmaps; i++) {
		if (maps[i].aim_rid_base != maps[i].aim_devid_base) {
			return (B_FALSE);
		}
	}

	/* Insertion sort by rid_base (typically 1-4 entries) */
	for (i = 1; i < nmaps; i++) {
		acpidev_iort_map_t tmp = maps[i];
		uint32_t j = i;
		while (j > 0 &&
		    maps[j - 1].aim_rid_base > tmp.aim_rid_base) {
			maps[j] = maps[j - 1];
			j--;
		}
		maps[j] = tmp;
	}

	/* Must start at RID 0 */
	if (maps[0].aim_rid_base != 0) {
		return (B_FALSE);
	}

	/* Each range must abut the previous */
	coverage = maps[0].aim_rid_count;
	for (i = 1; i < nmaps; i++) {
		if (maps[i].aim_rid_base != coverage) {
			return (B_FALSE);
		}
		coverage += maps[i].aim_rid_count;
	}

	if (coverage >= 0x10000) {
		*tridp = trid;
		return (B_TRUE);
	}

	return (B_FALSE);
}

/*
 * Condense adjacent msi-map entries.  Two entries can be merged when
 * they target the same ITS and their RID and DeviceID ranges are
 * contiguous.  Operates in-place; returns the new count.
 *
 * Simple insertion sort by rid_base first, then merge pass.
 */
static uint32_t
iort_condense_maps(acpidev_iort_map_t *maps, uint32_t nmaps)
{
	uint32_t	i, j, out;
	acpidev_iort_map_t tmp;

	if (nmaps <= 1) {
		return (nmaps);
	}

	/* Insertion sort by rid_base */
	for (i = 1; i < nmaps; i++) {
		tmp = maps[i];
		j = i;
		while (j > 0 && maps[j - 1].aim_rid_base > tmp.aim_rid_base) {
			maps[j] = maps[j - 1];
			j--;
		}
		maps[j] = tmp;
	}

	/* Merge pass */
	out = 0;
	for (i = 1; i < nmaps; i++) {
		if (maps[out].aim_its_trid == maps[i].aim_its_trid &&
		    maps[out].aim_rid_base + maps[out].aim_rid_count ==
		    maps[i].aim_rid_base &&
		    maps[out].aim_devid_base + maps[out].aim_rid_count ==
		    maps[i].aim_devid_base) {
			maps[out].aim_rid_count += maps[i].aim_rid_count;
		} else {
			out++;
			maps[out] = maps[i];
		}
	}

	return (out + 1);
}

/*
 * Parse the IORT table, building a list of per-root-complex entries
 * that acpidev_mcfg.c will query when creating PCIe device tree nodes.
 */
ACPI_STATUS
acpidev_iort_init(void)
{
	ACPI_TABLE_IORT		*iort;
	ACPI_STATUS		st;
	ACPI_IORT_NODE		*node;
	uint32_t		offset;
	uint32_t		i;
	list_t			its_groups;
	iort_its_group_entry_t	*ige;

	st = AcpiGetTable(ACPI_SIG_IORT, 1,
	    (ACPI_TABLE_HEADER **)&iort);
	if (st != AE_OK) {
		if (st == AE_NOT_FOUND || st == AE_NOT_EXIST) {
			uint32_t sole_trid;

			/*
			 * No IORT.  If exactly one ITS exists, create a
			 * wildcard entry so all root complexes get an
			 * msi-parent pointing to it.
			 */
			if (acpidev_gic_get_sole_its_trid(&sole_trid)) {
				acpidev_iort_rc_t *rc;

				rc = kmem_zalloc(sizeof (*rc), KM_SLEEP);
				rc->air_segment = UINT32_MAX;
				rc->air_coherent = B_TRUE;
				rc->air_simple = B_TRUE;
				rc->air_its_trid = sole_trid;
				rc->air_nmaps = 0;
				rc->air_maps = NULL;
				rc->air_next = NULL;
				iort_rc_list = rc;

				cmn_err(CE_CONT, "?acpidev: no IORT, "
				    "using sole ITS (translation-id %u) "
				    "for all root complexes\n", sole_trid);
				return (AE_OK);
			}

			if (acpidev_gic_has_v2m_frames()) {
				acpidev_iort_rc_t *rc;

				rc = kmem_zalloc(sizeof (*rc), KM_SLEEP);
				rc->air_segment = UINT32_MAX;
				rc->air_coherent = B_TRUE;
				rc->air_simple = B_TRUE;
				rc->air_v2m = B_TRUE;
				rc->air_its_trid = 0;
				rc->air_nmaps = 0;
				rc->air_maps = NULL;
				rc->air_next = NULL;
				iort_rc_list = rc;

				cmn_err(CE_CONT, "?acpidev: no IORT, "
				    "using GICv2m MSI frames for all "
				    "root complexes (round-robin)\n");
				return (AE_OK);
			}

			cmn_err(CE_WARN, "acpidev: no IORT and no "
			    "ITS or GICv2m frames; MSI unconfigured");
			return (AE_OK);
		}
		return (st);
	}

	/* Validate the table header and node directory before walking it. */
	if (iort->Header.Length < sizeof (ACPI_TABLE_IORT) ||
	    iort->NodeOffset < sizeof (ACPI_TABLE_IORT) ||
	    iort->NodeOffset > iort->Header.Length ||
	    iort->NodeCount > (iort->Header.Length - iort->NodeOffset) /
	    sizeof (ACPI_IORT_NODE)) {
		cmn_err(CE_WARN, "acpidev: IORT has invalid length %u, "
		    "node offset 0x%x or node count %u",
		    iort->Header.Length, iort->NodeOffset,
		    iort->NodeCount);
		AcpiPutTable(&iort->Header);
		return (AE_BAD_DATA);
	}

	/* Save for AcpiPutTable() in fini */
	iort_table_hdr = &iort->Header;

	/*
	 * First pass: collect ITS groups, indexed by byte offset
	 * within the IORT table.
	 */
	list_create(&its_groups, sizeof (iort_its_group_entry_t),
	    offsetof(iort_its_group_entry_t, ige_node));
	offset = iort->NodeOffset;
	for (i = 0; i < iort->NodeCount; i++) {
		node = iort_node_at(iort, offset);
		if (node == NULL) {
			cmn_err(CE_WARN, "acpidev: IORT node at offset "
			    "0x%x out of bounds", offset);
			break;
		}

		if (!iort_node_len_ok(iort, node, sizeof (ACPI_IORT_NODE))) {
			cmn_err(CE_WARN, "acpidev: IORT node with invalid "
			    "length %u at offset 0x%x", node->Length, offset);
			break;
		}

		if (node->Type == ACPI_IORT_NODE_ITS_GROUP) {
			ACPI_IORT_ITS_GROUP *grp;

			if (!iort_node_len_ok(iort, node,
			    offsetof (ACPI_IORT_NODE, NodeData) +
			    sizeof (ACPI_IORT_ITS_GROUP))) {
				cmn_err(CE_WARN, "acpidev: IORT ITS group "
				    "node too short (length %u) at offset "
				    "0x%x", node->Length, offset);
				break;
			}

			grp = (ACPI_IORT_ITS_GROUP *)node->NodeData;

			ige = kmem_zalloc(sizeof (*ige), KM_SLEEP);
			ige->ige_offset = offset;
			ige->ige_nids = grp->ItsCount;
			ige->ige_ids = grp->Identifiers;
			list_insert_tail(&its_groups, ige);

			ACPIDEV_DEBUG(CE_NOTE, "!acpidev: IORT ITS group at "
			    "offset 0x%x, %u ITS(es)", offset, grp->ItsCount);
		}

		offset += node->Length;
	}

	/*
	 * Second pass: process root complex nodes.
	 */
	offset = iort->NodeOffset;
	for (i = 0; i < iort->NodeCount; i++) {
		ACPI_IORT_ROOT_COMPLEX	*rc_data;
		ACPI_IORT_MEMORY_ACCESS	*ma;
		ACPI_IORT_ID_MAPPING	*idmap;
		acpidev_iort_map_t	*maps;
		uint32_t		nmaps;
		uint32_t		j;
		boolean_t		coherent;
		acpidev_iort_rc_t	*rc;

		node = iort_node_at(iort, offset);
		if (node == NULL) {
			cmn_err(CE_WARN, "acpidev: IORT node at offset "
			    "0x%x out of bounds", offset);
			break;
		}

		if (!iort_node_len_ok(iort, node, sizeof (ACPI_IORT_NODE))) {
			cmn_err(CE_WARN, "acpidev: IORT node with invalid "
			    "length %u at offset 0x%x", node->Length, offset);
			break;
		}

		if (node->Type != ACPI_IORT_NODE_PCI_ROOT_COMPLEX) {
			offset += node->Length;
			continue;
		}

		if (!iort_node_len_ok(iort, node,
		    offsetof (ACPI_IORT_NODE, NodeData) +
		    sizeof (ACPI_IORT_ROOT_COMPLEX))) {
			cmn_err(CE_WARN, "acpidev: IORT root complex node "
			    "too short (length %u) at offset 0x%x",
			    node->Length, offset);
			break;
		}

		rc_data = (ACPI_IORT_ROOT_COMPLEX *)node->NodeData;

		/* Extract coherency from MemoryProperties */
		ma = (ACPI_IORT_MEMORY_ACCESS *)&rc_data->MemoryProperties;
		coherent = (ma->CacheCoherency == ACPI_IORT_NODE_COHERENT);

		ACPIDEV_DEBUG(CE_NOTE, "!acpidev: IORT root complex "
		    "segment %u, %u mapping(s), %scoherent\n",
		    rc_data->PciSegmentNumber, node->MappingCount,
		    coherent ? "" : "not ");

		/*
		 * Walk ID mappings, resolve each to an ITS group.
		 * For each mapping, compose the DeviceID translation
		 * through any intermediate SMMU nodes.
		 */
		maps = NULL;
		nmaps = 0;

		if (node->MappingCount > 0) {
			if (!iort_mappings_ok(iort, node)) {
				cmn_err(CE_WARN, "acpidev: IORT root "
				    "complex at offset 0x%x has invalid "
				    "ID mappings", offset);
				break;
			}

			maps = kmem_zalloc(
			    node->MappingCount * sizeof (*maps), KM_SLEEP);

			idmap = (ACPI_IORT_ID_MAPPING *)
			    ((uintptr_t)node + node->MappingOffset);

			for (j = 0; j < node->MappingCount; j++) {
				iort_its_group_entry_t *target;
				uint32_t devid_base = 0;

				target = iort_resolve_to_its(iort, &its_groups,
				    idmap[j].OutputReference,
				    idmap[j].OutputBase, 3,
				    &devid_base);
				if (target == NULL || target->ige_nids == 0) {
					cmn_err(CE_CONT, "?acpidev: IORT "
					    "mapping %u for segment %u: "
					    "unresolved\n", j,
					    rc_data->PciSegmentNumber);
					continue;
				}

				if (idmap[j].Flags &
				    ACPI_IORT_ID_SINGLE_MAPPING) {
					/*
					 * SINGLE_MAPPING on the RC itself:
					 * entire RID space maps through.
					 */
					maps[nmaps].aim_rid_base = 0;
					maps[nmaps].aim_rid_count = 0x10000;
					maps[nmaps].aim_devid_base =
					    devid_base;
				} else {
					maps[nmaps].aim_rid_base =
					    idmap[j].InputBase;
					maps[nmaps].aim_rid_count =
					    idmap[j].IdCount + 1;
					maps[nmaps].aim_devid_base =
					    devid_base;
				}

				/* Use first ITS in the group */
				maps[nmaps].aim_its_trid =
				    target->ige_ids[0];
				nmaps++;
			}
		}

		/* Condense adjacent entries */
		if (nmaps > 1) {
			nmaps = iort_condense_maps(maps, nmaps);
		}

		/* Allocate the result entry */
		rc = kmem_zalloc(sizeof (*rc), KM_SLEEP);
		rc->air_segment = rc_data->PciSegmentNumber;
		rc->air_coherent = coherent;

		if (nmaps > 0 &&
		    iort_is_simple(maps, nmaps, &rc->air_its_trid)) {
			rc->air_simple = B_TRUE;
			rc->air_nmaps = 0;
			rc->air_maps = NULL;
			if (maps != NULL) {
				kmem_free(maps,
				    node->MappingCount * sizeof (*maps));
			}
		} else if (nmaps == 0 && acpidev_gic_has_v2m_frames()) {
			/*
			 * No IORT mappings resolved to an ITS group,
			 * but GICv2m MSI frames are available.  Use
			 * round-robin assignment across the frames.
			 */
			rc->air_simple = B_TRUE;
			rc->air_v2m = B_TRUE;
			rc->air_its_trid = 0;
			rc->air_nmaps = 0;
			rc->air_maps = NULL;
			if (maps != NULL) {
				kmem_free(maps,
				    node->MappingCount * sizeof (*maps));
			}
		} else {
			rc->air_simple = B_FALSE;
			rc->air_nmaps = nmaps;
			if (nmaps > 0 && nmaps < node->MappingCount) {
				/* Shrink allocation to actual count */
				acpidev_iort_map_t *newmaps;
				newmaps = kmem_alloc(
				    nmaps * sizeof (*newmaps), KM_SLEEP);
				bcopy(maps, newmaps,
				    nmaps * sizeof (*newmaps));
				kmem_free(maps,
				    node->MappingCount * sizeof (*maps));
				maps = newmaps;
			}
			rc->air_maps = (nmaps > 0) ? maps : NULL;
			if (nmaps == 0 && maps != NULL) {
				kmem_free(maps,
				    node->MappingCount * sizeof (*maps));
			}
		}

		rc->air_next = iort_rc_list;
		iort_rc_list = rc;

		offset += node->Length;
	}

	/* Free temporary ITS group list */
	while ((ige = list_remove_head(&its_groups)) != NULL) {
		kmem_free(ige, sizeof (*ige));
	}
	list_destroy(&its_groups);

	return (AE_OK);
}

/*
 * Look up IORT data for a root complex by PCI segment number.
 * Falls back to the wildcard entry (segment == UINT32_MAX) if
 * no exact match is found.
 */
acpidev_iort_rc_t *
acpidev_iort_lookup_rc(uint32_t segment)
{
	acpidev_iort_rc_t	*rc;
	acpidev_iort_rc_t	*wildcard = NULL;

	for (rc = iort_rc_list; rc != NULL; rc = rc->air_next) {
		if (rc->air_segment == segment) {
			return (rc);
		}
		if (rc->air_segment == UINT32_MAX) {
			wildcard = rc;
		}
	}

	return (wildcard);
}

/*
 * Free all IORT data and release the ACPI table.
 */
void
acpidev_iort_fini(void)
{
	acpidev_iort_rc_t *rc;

	while (iort_rc_list != NULL) {
		rc = iort_rc_list;
		iort_rc_list = rc->air_next;

		if (rc->air_maps != NULL) {
			kmem_free(rc->air_maps,
			    rc->air_nmaps * sizeof (acpidev_iort_map_t));
		}
		kmem_free(rc, sizeof (*rc));
	}

	if (iort_table_hdr != NULL) {
		AcpiPutTable(iort_table_hdr);
		iort_table_hdr = NULL;
	}
}
