/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2019 Joyent, Inc.
 */

#include <sys/types.h>

#include <vm/hat_aarch64.h>

#include <mdb/mdb_modapi.h>

#include "mmu.h"

static int
do_htables_dcmd(hat_t *hatp)
{
	struct hat hat;
	htable_t *ht;
	htable_t htable;
	int h;

	init_mmu();

	if (mmu.num_level == 0)
		return (DCMD_ERR);

	/*
	 * read the hat and its hash table
	 */
	if (mdb_vread(&hat, sizeof (hat), (uintptr_t)hatp) == -1) {
		mdb_warn("Couldn't read struct hat\n");
		return (DCMD_ERR);
	}

	/*
	 * read the htable hashtable
	 */
	for (h = 0; h < mmu.hash_cnt; ++h) {
		if (mdb_vread(&ht, sizeof (htable_t *),
		    (uintptr_t)(hat.hat_ht_hash + h)) == -1) {
			mdb_warn("Couldn't read htable ptr\\n");
			return (DCMD_ERR);
		}
		for (; ht != NULL; ht = htable.ht_next) {
			mdb_printf("%p\n", ht);
			if (mdb_vread(&htable, sizeof (htable_t),
			    (uintptr_t)ht) == -1) {
				mdb_warn("Couldn't read htable\n");
				return (DCMD_ERR);
			}
		}
	}
	return (DCMD_OK);
}

/*
 * Dump the htables for the given hat
 */
/*ARGSUSED*/
int
htables_dcmd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	hat_t *hat;

	init_mmu();

	if (mmu.num_level == 0)
		return (DCMD_ERR);

	if ((flags & DCMD_ADDRSPEC) == 0)
		return (DCMD_USAGE);

	hat = (hat_t *)addr;

	return (do_htables_dcmd(hat));
}

void
htables_help(void)
{
	mdb_printf(
	    "Given a (hat_t *), generates the list of all (htable_t *)s\n"
	    "that correspond to that address space\n");
}
