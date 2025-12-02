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
 * Copyright 2025 Richard Lowe
 */

/*
 * Commands dealing with the VMSAv8-64 compatible memory management unit
 */

#include <sys/controlregs.h>
#include <sys/machparam.h>
#include <sys/types.h>

#include <mdb/mdb_err.h>
#include <mdb/mdb_modapi.h>

#include "mmu.h"

struct hat_mmu_info mmu;

void
init_mmu(void)
{
	if (mmu.num_level != 0)
		return;

	if (mdb_readsym(&mmu, sizeof (mmu), "mmu") == -1)
		mdb_warn("Can't use HAT information before mmu_init()\n");
}

static void
decode_ttbr(uint64_t ttbr)
{
	uint16_t asid = (ttbr & TTBR_ASID_MASK) >> TTBR_ASID_SHIFT;
	uint64_t baddr = (ttbr & TTBR_BADDR48_MASK) >> TTBR_BADDR48_SHIFT;
	uint8_t cnp = (ttbr & TTBR_CNP_MASK) >> TTBR_CNP_SHIFT;

	mdb_printf("paddr=%lx pfn=%lx asid=%x cnp=%s\n",
	    baddr << MMU_PAGESHIFT, baddr, asid, cnp ? "true" : "false");
}

int
ttbr_dcmd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (!(flags & DCMD_ADDRSPEC)) {
		mdb_warn("missing ttbr value\n");
		return (DCMD_USAGE);
	}

	decode_ttbr(addr);

	return (DCMD_OK);
}
