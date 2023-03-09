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

/* Copyright 2023 Richard Lowe */

#include <mdb/mdb_modapi.h>
#include <sys/arm_features.h>
#include <sys/bitmap.h>

static
int
arm_features_dcmd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	void *fset;
	GElf_Sym sym;
	uintptr_t nptr;
	char name[128];
	int ii;

	size_t sz = sizeof (uchar_t) * BT_SIZEOFMAP(NUM_ARM_FEATURES);

	if (argc != 0)
		return (DCMD_USAGE);

	if (mdb_lookup_by_name("arm_feature_names", &sym) == -1) {
		mdb_warn("couldn't find x86_feature_names");
		return (DCMD_ERR);
	}

	fset = mdb_zalloc(sz, UM_NOSLEEP);
	if (fset == NULL) {
		mdb_warn("failed to allocate memory for arm_features");
		return (DCMD_ERR);
	}

	if (flags & DCMD_ADDRSPEC) {
		if (mdb_vread(fset, sz, addr) != sz) {
			mdb_warn("failed to read arm_features from %p", addr);
			mdb_free(fset, sz);
			return (DCMD_ERR);
		}
	} else {
		if (mdb_readvar(fset, "arm_features") != sz) {
			mdb_warn("failed to read arm_features");
			mdb_free(fset, sz);
			return (DCMD_ERR);
		}
	}

	for (ii = 0; ii < NUM_ARM_FEATURES; ii++) {
		if (!BT_TEST((ulong_t *)fset, ii))
			continue;

		if (mdb_vread(&nptr, sizeof (char *), sym.st_value +
		    sizeof (void *) * ii) != sizeof (char *)) {
			mdb_warn("failed to read feature array %d", ii);
			mdb_free(fset, sz);
			return (DCMD_ERR);
		}

		if (mdb_readstr(name, sizeof (name), nptr) == -1) {
			mdb_printf("unknown feature 0x%x\n", ii);
		} else {
			mdb_printf("%s\n", name);
		}
	}

	mdb_free(fset, sz);
	return (DCMD_OK);
}

static const mdb_dcmd_t dcmds[] = {
	{ "arm_features", ":", "dump the arm_features vector",
	    arm_features_dcmd },
	{ NULL },
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds, NULL };

const mdb_modinfo_t *
_mdb_init(void)
{
	return (&modinfo);
}
