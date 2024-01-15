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
 * Copyright 2024 Michael van der Westhuizen
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/byteorder.h>
#include <sys/promif.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

int
prom_get_prop_index(pnode_t node, const char *prop_name, const char *name)
{
	int len;
	len = prom_getproplen(node, prop_name);
	if (len > 0) {
		char *prop = __builtin_alloca(len);
		prom_getprop(node, prop_name, prop);
		int offeset = 0;
		int index = 0;
		while (offeset < len) {
			if (strcmp(name, prop + offeset) == 0)
				return index;
			offeset += strlen(prop + offeset) + 1;
			index++;
		}
	}
	return -1;
}

int
prom_get_prop_int(pnode_t node, const char *name, int def)
{
	int value = def;

	while (node > 0) {
		int len = prom_getproplen(node, name);
		if (len == sizeof(int)) {
			int prop;
			prom_getprop(node, name, (caddr_t)&prop);
			value = ntohl(prop);
			break;
		}
		if (len > 0) {
			break;
		}
		node = prom_parentnode(node);
	}
	return value;
}

uint64_t
prom_get_prop_u64(pnode_t node, const char *name, uint64_t def)
{
	uint64_t prop;
	uint64_t value = def;

	if (node > 0 && prom_getproplen(node, name) == sizeof(uint64_t)) {
		prom_getprop(node, name, (caddr_t)&prop);
		value = ntohll(prop);
	}

	return (value);
}

uint32_t
prom_get_prop_u32(pnode_t node, const char *name, uint32_t def)
{
	uint32_t prop;
	uint32_t value = def;

	if (node > 0 && prom_getproplen(node, name) == sizeof(uint32_t)) {
		prom_getprop(node, name, (caddr_t)&prop);
		value = ntohl(prop);
	}

	return (value);
}

int prom_get_reset(pnode_t node, int index, struct prom_hwreset *reset)
{
	int len = prom_getproplen(node, "resets");
	if (len <= 0)
		return -1;

	uint32_t *resets = __builtin_alloca(len);
	prom_getprop(node, "resets", (caddr_t)resets);

	pnode_t reset_node;
	reset_node = prom_findnode_by_phandle(htonl(resets[0]));
	if (reset_node < 0)
		return -1;

	int reset_cells = prom_get_prop_int(reset_node, "#reset-cells", 1);
	if (reset_cells != 1)
		return -1;

	if ((len % (sizeof(uint32_t) * (reset_cells + 1))) != 0)
		return -1;
	if (len <= index * (sizeof(uint32_t) * (reset_cells + 1)))
		return -1;

	reset_node = prom_findnode_by_phandle(htonl(resets[index * (reset_cells + 1)]));
	if (reset_node < 0)
		return -1;
	reset->node = reset_node;
	reset->id = htonl(resets[index * (reset_cells + 1) + 1]);

	return 0;
}

int prom_get_reset_by_name(pnode_t node, const char *name, struct prom_hwreset *reset)
{
	int index = prom_get_prop_index(node, "reset-names", name);
	if (index >= 0)
		return prom_get_reset(node, index, reset);
	return -1;
}

int prom_get_clock(pnode_t node, int index, struct prom_hwclock *clock)
{
	int len = prom_getproplen(node, "clocks");
	if (len <= 0)
		return -1;

	uint32_t *clocks = __builtin_alloca(len);
	prom_getprop(node, "clocks", (caddr_t)clocks);

	pnode_t clock_node;
	clock_node = prom_findnode_by_phandle(htonl(clocks[0]));
	if (clock_node < 0)
		return -1;

	int clock_cells = prom_get_prop_int(clock_node, "#clock-cells", 1);
	if (clock_cells != 0 && clock_cells != 1)
		return -1;

	if ((len % (sizeof(uint32_t) * (clock_cells + 1))) != 0)
		return -1;
	if (len <= index * (sizeof(uint32_t) * (clock_cells + 1)))
		return -1;

	clock_node = prom_findnode_by_phandle(htonl(clocks[index * (clock_cells + 1)]));
	if (clock_node < 0)
		return -1;
	clock->node = clock_node;
	clock->id = (clock_cells == 0? 0: htonl(clocks[index * (clock_cells + 1) + 1]));

	return 0;
}

int prom_get_clock_by_name(pnode_t node, const char *name, struct prom_hwclock *clock)
{
	int index = prom_get_prop_index(node, "clock-names", name);
	if (index >= 0)
		return prom_get_clock(node, index, clock);
	return -1;
}

int prom_get_address_cells(pnode_t node)
{
	return prom_get_prop_int(prom_parentnode(node), "#address-cells", 2);
}

int prom_get_size_cells(pnode_t node)
{
	return prom_get_prop_int(prom_parentnode(node), "#size-cells", 2);
}

static int
prom_get_reg_bounds(pnode_t node, int index, uint64_t *base, uint64_t *size)
{
	size_t off;
	int len = prom_getproplen(node, "reg");
	if (len <= 0)
		return (-1);

	uint32_t *regs = __builtin_alloca(len);
	prom_getprop(node, "reg", (caddr_t)regs);

	int address_cells = prom_get_address_cells(node);
	int size_cells = prom_get_size_cells(node);

	if (((address_cells + size_cells) * index + address_cells + size_cells)
	    * sizeof(uint32_t) > len)
		return (-1);

	if (address_cells < 1 || address_cells > 2 ||
	    size_cells < 1 || size_cells > 2)
		return (-1);

	off = (address_cells + size_cells) * index;
	switch (address_cells) {
	case 1:
		*base = htonl(regs[off]);
		break;
	case 2:
		*base = htonl(regs[off]);
		*base <<= 32;
		*base |= htonl(regs[off + 1]);
		break;
	default:
		return (-1);
	}

	off += address_cells;
	switch (size_cells) {
	case 1:
		*size = htonl(regs[off]);
		break;
	case 2:
		*size = htonl(regs[off]);
		*size <<= 32;
		*size |= htonl(regs[off + 1]);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
prom_get_reg(pnode_t node, int index, uint64_t *base)
{
	uint64_t size;
	return (prom_get_reg_bounds(node, index, base, &size));
}

int
prom_get_reg_size(pnode_t node, int index, uint64_t *size)
{
	uint64_t base;
	return (prom_get_reg_bounds(node, index, &base, size));
}

int
prom_get_reg_address(pnode_t node, int index, uint64_t *reg)
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

				if ((len % (sizeof(uint32_t) * (address_cells + parent_address_cells + size_cells))) == 0) {
					uint32_t *ranges = __builtin_alloca(len);
					prom_getprop(parent, "ranges", (caddr_t)ranges);
					int ranges_cells = (address_cells + parent_address_cells + size_cells);

					for (int i = 0; i < len / (sizeof(uint32_t) * ranges_cells); i++) {
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

int
prom_get_reg_by_name(pnode_t node, const char *name, uint64_t *base)
{
	int index = prom_get_prop_index(node, "reg-names", name);

	if (index >= 0)
		return prom_get_reg(node, index, base);
	return -1;
}

boolean_t
prom_is_compatible(pnode_t node, const char *name)
{
	int len;
	char *prop_name = "compatible";
	len = prom_getproplen(node, prop_name);
	if (len <= 0)
		return (B_FALSE);

	char *prop = __builtin_alloca(len);
	prom_getprop(node, prop_name, prop);

	int offeset = 0;
	while (offeset < len) {
		if (strcmp(name, prop + offeset) == 0)
			return (B_TRUE);
		offeset += strlen(prop + offeset) + 1;
	}
	return (B_FALSE);
}

pnode_t
prom_find_compatible(pnode_t node, const char *compatible)
{
	pnode_t child;

	if (prom_is_compatible(node, compatible))
		return (node);

	child = prom_childnode(node);

	while (child > 0) {
		node = prom_find_compatible(child, compatible);
		if (node > 0)
			return (node);

		child = prom_nextnode(child);
	}

	return (OBP_NONODE);
}

boolean_t
prom_has_compatible(const char *compatible)
{
	pnode_t node;

	node = prom_find_compatible(prom_rootnode(), compatible);

	if (node == OBP_NONODE)
		return (B_FALSE);

	return (B_TRUE);
}
