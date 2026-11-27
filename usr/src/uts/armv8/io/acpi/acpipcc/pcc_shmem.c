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

#include <sys/types.h>
#include <sys/stddef.h>
#include <sys/memlist.h>
#include <vm/hat.h>
#include <vm/hat_aarch64.h>
#include "pcc_impl.h"

extern struct memlist *phys_avail;

/*
 * Check that [pa, pa+len) lies entirely within installed physical memory.
 */
static boolean_t
pcc_range_installed(uint64_t pa, uint64_t len)
{
	struct memlist *ml;

	for (ml = phys_install; ml != NULL; ml = ml->ml_next) {
		if (pa >= ml->ml_address &&
		    pa + len <= ml->ml_address + ml->ml_size) {
			return (B_TRUE);
		}
	}

	return (B_FALSE);
}

/*
 * Check whether any part of [pa, pa+len) is in usable physical memory.
 *
 * PCC shared memory must be reserved by firmware, not left usable by the OS.
 */
static boolean_t
pcc_range_usable(uint64_t pa, uint64_t len)
{
	struct memlist *ml;
	uint64_t end = pa + len;

	for (ml = phys_avail; ml != NULL; ml = ml->ml_next) {
		if (pa < ml->ml_address + ml->ml_size && end > ml->ml_address) {
			return (B_TRUE);
		}
	}

	return (B_FALSE);
}

/*
 * Map a PCC shared-memory region via KPM.
 *
 * Shared memory is normal RAM, not MMIO.  KPM provides the existing cacheable
 * kernel VA (Normal Inner Shareable on aarch64).
 */
caddr_t
pcc_map_shmem(uint64_t pa, uint64_t len)
{
	pfn_t pfn;
	uint_t pgoffset;

	if (len == 0) {
		return (NULL);
	}

	if (pa + len < pa) {
		cmn_err(CE_WARN, "!acpipcc: shared memory PA 0x%llx+0x%llx "
		    "overflows",
		    (unsigned long long)pa, (unsigned long long)len);
		return (NULL);
	}

	if (!pcc_range_installed(pa, len)) {
		cmn_err(CE_WARN, "!acpipcc: shared memory PA 0x%llx+0x%llx not "
		    "in installed physical memory",
		    (unsigned long long)pa, (unsigned long long)len);
		return (NULL);
	}

	/*
	 * Firmware must reserve PCC shared memory.
	 */
	if (pcc_range_usable(pa, len)) {
		cmn_err(CE_WARN, "!acpipcc: shared memory PA 0x%llx+0x%llx is "
		    "in usable physical memory - firmware should reserve PCC "
		    "shared memory",
		    (unsigned long long)pa, (unsigned long long)len);
		return (NULL);
	}

	pfn = btop(pa);
	pgoffset = (uint_t)(pa & MMU_PAGEOFFSET);

	return (hat_kpm_pfn2va(pfn) + pgoffset);
}
