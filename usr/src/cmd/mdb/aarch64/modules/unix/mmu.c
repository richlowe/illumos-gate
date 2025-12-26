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
 * Portions Copyright 2022 Oxide Computer Company
 */

/*
 * Commands dealing with the VMSAv8-64 compatible memory management unit
 */

#include <sys/controlregs.h>
#include <sys/debug.h>
#include <sys/machparam.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

#include <vm/as.h>
#include <vm/hat_aarch64.h>

#include <stdbool.h>

#include <mdb/mdb_err.h>
#include <mdb/mdb_modapi.h>

#include "mmu.h"

struct hat_mmu_info mmu;

/*
 * These are in mdb_param.h, but that is an include-file mess v. the kernel
 * headers we need.
 *
 * XXX: For now, cheat
 */
extern uintptr_t _mdb_ks_pagesize;
extern uintptr_t _mdb_ks_pageshift;
extern uintptr_t _mdb_ks_pageoffset;

static inline uint64_t
takebits(uint64_t reg, uint_t high, uint_t low)
{
	uint64_t mask;

	ASSERT3U(high, >=, low);
	ASSERT3U(high, <, 64);
	ASSERT3U(low, <, 64);

	mask = ((1ULL << (high - low + 1)) - 1ULL) << low;
	return (reg & mask);
}

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
	    mmu_ptob(baddr), baddr, asid,
	    cnp ? "true" : "false");
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

int
pte_table(uintptr_t addr, uint_t level)
{
	mdb_printf("pte=%lx table addr=%p level=%d", addr,
	    takebits(addr, 47, _mdb_ks_pageshift), level);

	if (addr & PTE_TABLE_NST)
		mdb_printf(", non-secure");

	if ((addr & PTE_TABLE_APT_RO) && (addr & PTE_TABLE_APT_NOUSER))
		mdb_printf(", ap=ro,nouser");
	else if (addr & PTE_TABLE_APT_RO)
		mdb_printf(", ap=ro");
	else if (addr & PTE_TABLE_APT_NOUSER)
		mdb_printf(", ap=nouser");

	if (addr & PTE_TABLE_UXNT)
		mdb_printf(", uxn");
	if (addr & PTE_TABLE_PXNT)
		mdb_printf(", pxn");
	if (addr & PTE_TABLE_PROTECTED)
		mdb_printf(", protected");
	if (addr & PTE_AF)
		mdb_printf(", af");

	mdb_printf("\n");
	return (DCMD_OK);
}

static int
pte_page_block(uintptr_t addr, uint_t level)
{
	const char *type = NULL;

	if (((addr & PTE_TYPE_MASK) == PTE_BLOCK)) {
		if (level == 0) {
			mdb_warn("block descriptors are invalid at level 0\n");
			/* Decode the rest of it anyway, in case it helps */
		}
		type = "block";
	} else {
		type = "page";
	}

	mdb_printf("pte=%lx %s addr=%p level=%d size=%H", addr, type,
	    takebits(addr, 47, _mdb_ks_pageshift), level, LEVEL_SIZE(level));

	if (addr & PTE_NOSYNC)
		mdb_printf(", nosync");
	if (addr & PTE_NOCONSIST)
		mdb_printf(", noconsist");
	if (addr & PTE_UXN)
		mdb_printf(", uxn");
	if (addr & PTE_PXN)
		mdb_printf(", pxn");
	if (addr & PTE_CONTIG_HINT)
		mdb_printf(", contig");
	if (addr & PTE_DBM)
		mdb_printf(", dbm");
	if (addr & PTE_GP)
		mdb_printf(", guarded");
	if ((addr & PTE_nT) && ((addr & PTE_TYPE_MASK) == PTE_BLOCK))
		mdb_printf(", nT");
	if (addr & PTE_NG)
		mdb_printf(", ng");
	if (addr & PTE_AF)
		mdb_printf(", af");

	if ((addr & PTE_SH_INNER) && (addr & PTE_SH_OUTER))
		mdb_printf(", sh=inner,outer");
	else if (addr & PTE_SH_INNER)
		mdb_printf(", sh=inner");
	else if (addr & PTE_SH_OUTER)
		mdb_printf(", sh=outer");
	else if ((addr & PTE_SH_MASK) == 0)
		mdb_printf(", sh=none");

	if ((addr & PTE_AP_USER) && (addr & PTE_AP_RO))
		mdb_printf(", ap=ro,user");
	else if (addr & PTE_AP_USER)
		mdb_printf(", ap=user");
	else if (addr & PTE_AP_RO)
		mdb_printf(", ap=ro");

	switch (addr & PTE_ATTR_MASK) {
	case PTE_ATTR_STRONG:
		mdb_printf(", attr=strong");
		break;
	case PTE_ATTR_DEVICE:
		mdb_printf(", attr=device");
		break;
	case PTE_ATTR_NORMEM:
		mdb_printf(", attr=normem");
		break;
	case PTE_ATTR_NORMEM_WT:
		mdb_printf(", attr=writethru");
		break;
	case PTE_ATTR_NORMEM_UC:
		mdb_printf(", attr=uncached");
		break;
	case PTE_ATTR_UNORDERED:
		mdb_printf(", attr=unordered");
		break;
	default:
		mdb_warn("unknown pte attribute index\n");
		mdb_printf(", attr=%x",
		    (addr & PTE_ATTR_MASK) >> PTE_ATTR_SHIFT);
	}
	mdb_printf("\n");

	return (DCMD_ERR);
}

/*
 * Note that level here is in the illumos sense, where level 0 is the deepest,
 * smallest, page size.  This is unfortunately the opposite to the order ARM
 * refer to things.
 *
 * Also, for future purposes, this won't work for VMSAv9-128, because mdb
 * lacks 128bit literals.
 */
int
pte_dcmd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint64_t level = 0;

	init_mmu();

	if (mmu.num_level == 0)
		return (DCMD_ERR);

	if (mmu.pa_size >= TCR_IPS_52BIT) {
		mdb_warn("52-bit physical addressing not yet supported\n");
		return (DCMD_ERR);
	}

	if ((flags & DCMD_ADDRSPEC) == 0)
		return (DCMD_USAGE);

	if (mdb_getopts(argc, argv,
	    'l', MDB_OPT_UINT64, &level, NULL) != argc) {
		return (DCMD_USAGE);
	}

	if (level > mmu.max_level) {
		mdb_warn("invalid level %lu, max is %lu\n", level,
		    mmu.max_level);
		return (DCMD_ERR);
	}

	/*
	 * Note the level check is necessary as page and table share the same
	 * encoding.  I'm sure there's a reason it's not page and block that
	 * share, given they share everything else.
	 */
	if ((addr & PTE_VALID) == 0) {
		mdb_printf("invalid %p\n", addr);
		return (DCMD_OK);
	} else if (((addr & PTE_TYPE_MASK) == PTE_BLOCK) ||
	    (((addr & PTE_TYPE_MASK) == PTE_PAGE) && (level == 0))) {
		return (pte_page_block(addr, level));
	} else if (((addr & PTE_TYPE_MASK) == PTE_TABLE) && (level > 0)) {
		return (pte_table(addr, level));
	} else {
		mdb_warn("impossible pte type: %d\n", addr & PTE_TYPE_MASK);
		return (DCMD_ERR);
	}

	/* Unreachable */
	return (DCMD_ERR);
}

int
ptable_dcmd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint64_t level = 0;
	bool opt_v = false;
	bool opt_r = false;
	bool opt_R = false;

	init_mmu();

	if (mmu.num_level == 0)
		return (DCMD_ERR);

	if (!(flags & DCMD_ADDRSPEC)) {
		mdb_warn("missing page table (physical) address");
		return (DCMD_USAGE);
	}

	if (mdb_getopts(argc, argv,
	    'l', MDB_OPT_UINT64, &level,
	    'v', MDB_OPT_SETBITS, 1, &opt_v,
	    'r', MDB_OPT_SETBITS, 1, &opt_r,
	    'R', MDB_OPT_SETBITS, 1, &opt_R,
	    NULL) != argc) {
		return (DCMD_USAGE);
	}

	if (level > mmu.max_level) {
		mdb_warn("invalid level %lu, max is %lu\n", level,
		    mmu.max_level);
		return (DCMD_ERR);
	}

	/* We have as many PTEs as fit on a page in the current granule */
	for (int i = 0; i < _mdb_ks_pagesize / sizeof (pte_t); i++) {
		uint64_t pte = 0;

		if (mdb_pread(&pte, sizeof (pte),
		    addr + (i * sizeof (pte))) != sizeof (pte)) {
			mdb_warn("failed to read page table entry");
			return (DCMD_ERR);
		}

		if ((pte == 0) && (opt_v == 0))
			continue;

		if (opt_r) {
			mdb_printf("[%x]\t%lx\n", i, pte);
		} else {
			mdb_arg_t v[] = {
				{ MDB_TYPE_STRING, { "-l" } },
				{ MDB_TYPE_IMMEDIATE, { .a_val = level } }
			};
			mdb_printf("[%x] ", i);
			mdb_call_dcmd("unix`pte", pte, DCMD_ADDRSPEC,
			    ARRAY_SIZE(v), v);

			if (opt_R &&
			    ((pte & PTE_TYPE_MASK) == PTE_TABLE) &&
			    (level != 0)) {
				mdb_arg_t rv[] = {
					{ MDB_TYPE_STRING, { "-R" } },
					{ MDB_TYPE_STRING, { "-l" } },
					{ MDB_TYPE_IMMEDIATE,
					    { .a_val = level - 1} }
				};

				mdb_inc_indent(4);
				mdb_call_dcmd("unix`ptable",
				    takebits(pte, 47, _mdb_ks_pageshift),
				    DCMD_ADDRSPEC, ARRAY_SIZE(rv), rv);
				mdb_dec_indent(4);
			}
		}

	}

	return (DCMD_OK);
}

/*
 * XXX: this is kernel `LEVEL_INDEX` here for the agnosticism of
 * _mdb_ks_pageshift
 */
static inline uint64_t
pte_index(uintptr_t addr, uint_t level)
{
	return (((addr & LEVEL_MASK(level)) >> LEVEL_SHIFT(level)) &
	    ((1 << (_mdb_ks_pageshift - 3)) - 1));
}

int
vatopfn_dcmd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{

	init_mmu();

	if (mmu.num_level == 0)
		return (DCMD_ERR);

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	struct as as;
	struct hat hat;
	struct htable htable;
	uintptr_t asaddr = 0;

	if (mdb_getopts(argc, argv,
	    'a', MDB_OPT_UINT64, &asaddr, NULL) != argc) {
		return (DCMD_USAGE);
	}

	if (asaddr == 0) {	/* Default is the kernel address space */
		if (mdb_readsym(&as, sizeof (struct as), "kas") == -1) {
			mdb_warn("couldn't read kernel address space (kas)");
			return (DCMD_ERR);
		}
	} else {
		if (mdb_vread(&as, sizeof (as), asaddr) != sizeof (as)) {
			mdb_warn("couldn't read address space from %p", asaddr);
			return (DCMD_ERR);
		}
	}

	if (mdb_vread(&hat, sizeof (hat),
	    (uintptr_t)as.a_hat) != sizeof (hat)) {
		mdb_warn("failed to read hardware address translations");
		return (DCMD_ERR);
	}

	if (mdb_vread(&htable, sizeof (htable),
	    (uintptr_t)hat.hat_htable) != sizeof (htable)) {
		mdb_warn("failed to read hardware address translations");
		return (DCMD_ERR);
	}

	uintptr_t next_table = mmu_ptob(htable.ht_pfn);

	/* Needs dynamism (as indeed the kernel macros do) */
	for (int i = mmu.max_level; i >= 0; i--) {
		uint_t idx = pte_index(addr, i);

		pte_t pte;
		if (mdb_pread(&pte, sizeof (pte),
		    next_table + (idx * sizeof (pte))) != sizeof (pte)) {
			mdb_warn("couldn't read page table entry at %p+%x",
			    next_table, idx);
			return (DCMD_ERR);
		}

		mdb_arg_t v[] = {
			{ MDB_TYPE_STRING, { "-l" } },
			{ MDB_TYPE_IMMEDIATE, { .a_val = i } }
		};

		mdb_printf("[%p+%x] ", next_table, idx);
		mdb_call_dcmd("unix`pte", pte, DCMD_ADDRSPEC,
		    ARRAY_SIZE(v), v);

		if ((pte & PTE_VALID) == 0) {
			mdb_printf("not mapped");
			return (DCMD_OK);
		}

		/* Abstract */
		if (((pte & PTE_TYPE_MASK) == PTE_TABLE) && (i > 0)) {
			next_table = takebits(pte, 47, _mdb_ks_pageshift);
			continue;
		} else if (((pte & PTE_TYPE_MASK) == PTE_PAGE) ||
		    ((pte & PTE_TYPE_MASK) == PTE_BLOCK)) {
			mdb_printf("physaddr=%p\n",
			    takebits(pte, 47, _mdb_ks_pageshift) |
			    (addr & _mdb_ks_pageoffset));
			break;
		}
	}

	return (DCMD_OK);
}
