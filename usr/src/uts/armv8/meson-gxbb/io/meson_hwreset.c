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
 * Copyright 2017 Hayashi Naoyuki
 */

#include <sys/types.h>
#include <sys/machclock.h>
#include <sys/platform.h>
#include <sys/modctl.h>
#include <sys/platmod.h>
#include <sys/promif.h>
#include <sys/errno.h>
#include <sys/byteorder.h>

static int
get_reg_addr(pnode_t node, int index, uint64_t *reg)
{
	uint64_t addr;
	if (prom_get_reg(node, index, &addr) != 0)
		return -1;

	pnode_t parent = prom_parentnode(node);
	while (parent > 0) {
		if (prom_is_compatible(parent, "simple-bus")) {
			int len = prom_getproplen(parent, "ranges");
			if (len > 0) {
				int address_cells = prom_get_prop_int(parent, "#address-cells", 2);
				int size_cells = prom_get_prop_int(parent, "#size-cells", 2);
				int parent_address_cells  = prom_get_prop_int(prom_parentnode(parent), "#address-cells", 2);

				if ((len % CELLS_1275_TO_BYTES(address_cells + parent_address_cells + size_cells)) == 0) {
					uint32_t *ranges = __builtin_alloca(len);
					prom_getprop(parent, "ranges", (caddr_t)ranges);
					int ranges_cells = (address_cells + parent_address_cells + size_cells);

					for (int i = 0; i < len / CELLS_1275_TO_BYTES(ranges_cells); i++) {
						uint64_t base = 0;
						uint64_t target = 0;
						uint64_t size = 0;
						for (int j = 0; j < address_cells; j++) {
							base <<= 32;
							base += htonl(ranges[ranges_cells * i + j]);
						}
						for (int j = 0; j < parent_address_cells; j++) {
							target <<= 32;
							target += htonl(ranges[ranges_cells * i + address_cells + j]);
						}
						for (int j = 0; j < size_cells; j++) {
							size <<= 32;
							size += htonl(ranges[ranges_cells * i + address_cells + parent_address_cells + j]);
						}

						if (base <= addr && addr <= base + size - 1) {
							addr = (addr - base) + target;
							break;
						}
					}
				}
			}
		}
		parent = prom_parentnode(parent);
	}
	*reg = addr;
	return 0;
}

int plat_hwreset_assert(struct prom_hwreset *rst)
{
	uint64_t base;
	if (get_reg_addr(rst->node, 0, &base) != 0)
		return -1;
	*(volatile uint32_t *)(SEGKPM_BASE + base + ((0x50 + rst->id / 32) << 2)) &= ~(1u << (rst->id % 32));
	return 0;
}

int plat_hwreset_deassert(struct prom_hwreset *rst)
{
	uint64_t base;
	if (get_reg_addr(rst->node, 0, &base) != 0)
		return -1;
	*(volatile uint32_t *)(SEGKPM_BASE + base + ((0x50 + rst->id / 32) << 2)) |= (1u << (rst->id % 32));
	return 0;
}

int plat_hwreset_is_asserted(struct prom_hwreset *rst, boolean_t *asserted)
{
	uint64_t base;
	if (get_reg_addr(rst->node, 0, &base) == 0)
		return -1;
	*asserted = ((*(volatile uint32_t *)(SEGKPM_BASE + base + ((0x50 + rst->id / 32) << 2)) & (1u << (rst->id % 32))) == 0);
	return 0;
}
