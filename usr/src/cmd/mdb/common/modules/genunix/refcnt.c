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
 * Copyright 2016, Richard Lowe.
 */

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ctf.h>
#include <mdb/mdb_ks.h>

#include <sys/refcnt_impl.h>

int
reftoken_walk_init(mdb_walk_state_t *wsp)
{
	if (wsp->walk_addr == NULL) {
		return (WALK_ERR);
	} else {
		wsp->walk_addr = wsp->walk_addr + OFFSETOF(refcnt_t, rc_holders);
	}

	/* XXX: read the refcnt and warn if the debug flag isn't set */

	if (mdb_layered_walk("avl", wsp) == -1)
		return (WALK_ERR);

	return (WALK_NEXT);
}

int
reftoken(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	reftoken_t tok;
	reftoken_audit_t ra;
	uint_t verbose = FALSE;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_getopts(argc, argv, 'v', MDB_OPT_SETBITS,
	    TRUE, &verbose, NULL) != argc) {
		return (DCMD_USAGE);
	}

	if (mdb_vread(&tok, sizeof (tok), addr) == -1) {
		mdb_warn("failed to read reftoken_t at %p", addr);
		return (DCMD_ERR);
	}

	/* XXX: Read back the refcnt_t and check the flags too? */

	if (mdb_vread(&ra, sizeof (ra), (uintptr_t)tok.rt_audit) == -1) {
		mdb_warn("failed to read reftoken_audit_t at %p", tok.rt_audit);
		return (DCMD_ERR);
	}

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%<u>%16s %16s %s\n%</u>",
		    "ADDR", "THREAD", "TAKEN");
	}

	mdb_printf("%p %p T-%lld.%09lld\n",
	    addr, ra.ra_thread, mdb_gethrtime() - ra.ra_hrtime);

	if (verbose == TRUE) {
		for (int i = 0; i < ra.ra_depth; i++)
			mdb_printf("  %a\n", ra.ra_stack[i]);
	}

	return (DCMD_OK);
}

/* ARGSUSED */
int
refcount(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	refcnt_t cnt;

	/* XXX: if -v print flags and lock */
	if (mdb_vread(&cnt, sizeof (cnt), addr) == -1) {
		mdb_warn("failed to read refcnt_t at %p", addr);
		return (DCMD_ERR);
	}

	mdb_printf("%d\n", cnt.rc_count);

	return (DCMD_OK);
}

/* ARGSUSED */
int
reflog(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	refcnt_t cnt;
	refcnt_histent_t ents[REFCNT_HISTORY_SIZE];
	uint_t verbose = FALSE;
	int i;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_getopts(argc, argv, 'v', MDB_OPT_SETBITS,
	    TRUE, &verbose, NULL) != argc) {
		return (DCMD_USAGE);
	}

	if (mdb_vread(&cnt, sizeof (cnt), addr) == -1) {
		mdb_warn("failed to read refcnt_t at %p", addr);
		return (DCMD_ERR);
	}

	if ((cnt.rc_flags & REFCNT_LOGGING) == 0) {
		mdb_warn("logging is disabled for this refcnt_t");
		return (DCMD_ERR);
	}


	if (mdb_vread(ents, sizeof (ents[0]) * REFCNT_HISTORY_SIZE,
	    (uintptr_t)cnt.rc_history) == -1) {
		mdb_warn("failed to read refcnt_histent_t at %p", cnt.rc_history);
		return (DCMD_ERR);
	}

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%<u>%4s %16s %s\n%</u>",
		    "EVENT", "TOKEN", "TIMESTAMP");
	}

	/*
	 * XXX: Isn't going to handle the = 0 case, is in general just crap.
	 */
	i = (cnt.rc_hist_next - 1);
	do {
		reftoken_audit_t ra;

		if (ents[i].rh_token == NULL)
			break;

		if (mdb_vread(&ra, sizeof (ra),
		    (uintptr_t)ents[i].rh_audit) == -1) {
			mdb_warn("failed to read reftoken_audit_t at %p",
			    ents[i].rh_audit);
			return (DCMD_ERR);
		}

		mdb_printf("%4s %p T-%lld.%09lld\n",
		    ents[i].rh_event == RCE_HOLD ? "HOLD" : "RELE",
		    ents[i].rh_token,
		    mdb_gethrtime() - ra.ra_hrtime);

		if (verbose == TRUE) {
			for (int j = 0; j < ra.ra_depth; j++)
				mdb_printf("    %a\n", ra.ra_stack[j]);
		}

		i = i - 1;
		if (i < 0)
			i = (REFCNT_HISTORY_SIZE - 1);
	} while (i != (cnt.rc_hist_next - 1));

	return (DCMD_OK);
}
