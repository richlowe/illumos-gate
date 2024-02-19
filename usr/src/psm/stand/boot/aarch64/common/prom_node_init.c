/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2024 Michael van der Westhuizen
 * Copyright 2017 Hayashi Naoyuki
 */
#include <libfdt.h>

#include <sys/salib.h>
#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/systm.h>
#include <sys/bootvfs.h>
#include "boot_plat.h"
#include <sys/platnames.h>

#define	FDT_FIX_PHANDLE_SUCCESS	0
#define	FDT_FIX_PHANDLE_AGAIN	1
#define	FDT_FIX_PHANDLE_FAILURE	2

static int
i_prom_fdt_fix_phandle(void *fdtp, int node)
{
	int err;
	uint32_t phandle;

	phandle = fdt_get_phandle(fdtp, node);
	if (phandle > 0)
		return (FDT_FIX_PHANDLE_SUCCESS);

	err = fdt_generate_phandle(fdtp, &phandle);
	if (err != 0)
		return (FDT_FIX_PHANDLE_FAILURE);

	err = fdt_setprop_u32(fdtp, node, "phandle", phandle);
	if (err != 0)
		return (FDT_FIX_PHANDLE_FAILURE);

	return (FDT_FIX_PHANDLE_AGAIN);
}

static int
prom_fdt_fix_phandle(void *fdtp, int node)
{
	int err;
	int offset;

	err = i_prom_fdt_fix_phandle(fdtp, node);
	if (err != FDT_FIX_PHANDLE_SUCCESS)
		return (err);

	fdt_for_each_subnode(offset, fdtp, node) {
		err = prom_fdt_fix_phandle(fdtp, offset);
		if (err != FDT_FIX_PHANDLE_SUCCESS)
			return (err);
	}

	return (FDT_FIX_PHANDLE_SUCCESS);
}

static void
prom_fdt_fix_phandles(void *fdtp)
{
	int err;
	int offset;

again:
	fdt_for_each_subnode(offset, fdtp, 0) {
		err = prom_fdt_fix_phandle(fdtp, offset);
		if (err == FDT_FIX_PHANDLE_AGAIN)
			goto again;
		else if (err != FDT_FIX_PHANDLE_SUCCESS)
			return;
	}
}

static void
prom_fdt_ensure_node(void *fdtp, const char *name)
{
	int offset;

	fdt_for_each_subnode(offset, fdtp, 0) {
		const char *n = fdt_get_name(fdtp, offset, NULL);
		if (n && strcmp(n, name) == 0)
			return;
	}

	fdt_add_subnode(fdtp, 0, name);
}

void
prom_node_late_init(void)
{
	void *fdtp = get_fdtp();
	prom_fdt_fix_phandles(fdtp);
	prom_fdt_ensure_node(fdtp, "chosen");
	prom_fdt_ensure_node(fdtp, "options");
	prom_fdt_ensure_node(fdtp, "alias");
}

void
prom_node_init(void)
{
	int err;
	extern char _dtb_start[];

	err = fdt_check_header(_dtb_start);
	if (err) {
		prom_printf("fdt_check_header ng\n");
		return;
	}

	size_t total_size = fdt_totalsize(_dtb_start);
	size_t size = ((total_size + MMU_PAGESIZE - 1) & ~(MMU_PAGESIZE - 1));
	size += MMU_PAGESIZE;
	void *fdtp = (void *)memlist_get(size, MMU_PAGESIZE, &pfreelistp);
	memcpy(fdtp, _dtb_start, total_size);
	fdt_open_into(fdtp, fdtp, size);
	set_fdtp(fdtp);
}
