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
 * Copyright 2017 Hayashi Naoyuki
 */

#include <libfdt.h>
#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/systm.h>
#include <sys/sunddi.h>

/*
 * XXXARM:
 *
 * The rest of the system expects the "PROM" to provide device name and
 * unit-address as properties, however the devicetree specification gives
 * nodes true names, formed <name>@<unit-address>.
 *
 * We synthesize these properties at runtime, rather unfortunately.  An
 * alternative is to stuff them as actual properties into the devicetree, but
 * depending on platform this runs us out of string space in the FDT.
 *
 * It's possible in future it would be better to:
 * 1) Just resize the FDT to be big enough in prom_init (though this
 *    would move it, and leave the old one dangling).
 * 2) Specify a larger FDT in u-boot or other boot firmware
 * 3) Copy the whole FDT into a data structure of our own,
 *    rather than manipulating the actual FDT.
 */

static struct fdt_header *fdtp;

static phandle_t
get_phandle(int offset)
{
	int len;
	const void *prop = fdt_getprop(fdtp, offset, "phandle", &len);

	/*
	 * XXXARM: It is not obvious to me, based on the specification, how we
	 * could ever not have a phandle
	 */
	if (prop == NULL || len != sizeof (uint32_t)) {
		uint32_t phandle = fdt_get_max_phandle(fdtp) + 1;
		uint32_t v = htonl(phandle);
		int r = fdt_setprop(fdtp, offset, "phandle", &v,
		    sizeof (uint32_t));
		if (r != 0)
			return (-1);
		return (phandle);
	}

	uint32_t v;
	memcpy(&v, prop, sizeof (uint32_t));
	return (ntohl(v));
}

pnode_t
prom_findnode_by_phandle(phandle_t phandle)
{
	int offset = fdt_node_offset_by_phandle(fdtp, phandle);
	if (offset < 0)
		return (-1);
	return ((pnode_t)phandle);
}

int
prom_getprop(pnode_t nodeid, const char *name, caddr_t value)
{
	int offset = fdt_node_offset_by_phandle(fdtp, nodeid);

	if (offset < 0)
		return (-1);

	int len;
	const void *prop = fdt_getprop(fdtp, offset, name, &len);

	if (prop == NULL) {
		if (strcmp(name, "name") == 0) {
			const char *name_ptr = fdt_get_name(fdtp, offset, &len);
			const char *p = strchr(name_ptr, '@');

			if (!name_ptr)
				return (-1);

			if (p) {
				len = p - name_ptr;
			} else {
				len = strlen(name_ptr);
			}
			memcpy(value, name_ptr, len);
			value[len] = '\0';

			return (len + 1);
		}
		if (strcmp(name, "unit-address") == 0) {
			const char *name_ptr = fdt_get_name(fdtp, offset, &len);
			const char *p = strchr(name_ptr, '@');
			if (p) {
				p++;
				len = strlen(p);
			} else {
				return (-1);
			}
			if (len == 0)
				return (-1);

			memcpy(value, p, len);
			value[len] = '\0';
			return (len + 1);
		}

		return (-1);
	}

	memcpy(value, prop, len);
	return (len);
}

int
prom_setprop(pnode_t nodeid, const char *name, const caddr_t value, int len)
{
	int offset = fdt_node_offset_by_phandle(fdtp, (pnode_t)nodeid);
	if (offset < 0)
		return (-1);

	/*
	 * The name and unit-address properties are special,
	 * and should never be altered.
	 */
	ASSERT3U(strcmp(name, "name"), !=, 0);
	ASSERT3U(strcmp(name, "unit-address"), !=, 0);

	int r = fdt_setprop(fdtp, offset, name, value, len);

	return (r == 0 ? len : -1);
}

int
prom_getproplen(pnode_t nodeid, const char *name)
{
	int offset = fdt_node_offset_by_phandle(fdtp, (pnode_t)nodeid);

	if (offset < 0)
		return (-1);

	int len;
	const struct fdt_property *prop = fdt_get_property(fdtp, offset, name,
	    &len);

	if (prop == NULL) {
		if (strcmp(name, "name") == 0) {
			const char *name_ptr = fdt_get_name(fdtp, offset, &len);
			if (!name_ptr)
				return (-1);
			const char *p = strchr(name_ptr, '@');
			if (p) {
				len = p - name_ptr;
			} else {
				len = strlen(name_ptr);
			}

			return (len + 1);
		}
		if (strcmp(name, "unit-address") == 0) {
			const char *name_ptr = fdt_get_name(fdtp, offset, &len);
			if (!name_ptr)
				return (-1);
			const char *p = strchr(name_ptr, '@');
			if (p) {
				p++;
				len = strlen(p);
			} else {
				return (-1);
			}
			if (len == 0)
				return (-1);
			return (len + 1);
		}

		return (-1);
	}

	return (len);
}

pnode_t
prom_finddevice(const char *device)
{
	int offset = fdt_path_offset(fdtp, device);
	if (offset < 0)
		return (OBP_BADNODE);

	phandle_t phandle = get_phandle(offset);
	if (phandle < 0)
		return (OBP_BADNODE);

	return ((pnode_t)phandle);
}

pnode_t
prom_rootnode(void)
{
	pnode_t root = prom_finddevice("/");
	if (root < 0) {
		return (OBP_NONODE);
	}
	return (root);
}

pnode_t
prom_chosennode(void)
{
	pnode_t node = prom_finddevice("/chosen");
	if (node != OBP_BADNODE)
		return (node);
	return (OBP_NONODE);
}

pnode_t
prom_optionsnode(void)
{
	pnode_t node = prom_finddevice("/options");
	if (node != OBP_BADNODE)
		return (node);
	return (OBP_NONODE);
}

/*
 * Returning NULL means something went wrong, returning '\0' means no more
 * properties.
 */
char *
prom_nextprop(pnode_t nodeid, const char *name, char *next)
{
	int offset = fdt_node_offset_by_phandle(fdtp, (pnode_t)nodeid);
	if (offset < 0)
		return (NULL);

	/*
	 * The first time we're called, present the "name" pseudo-property
	 */
	if (name[0] == '\0') {
		strcpy(next, "name");
		return (next);
	}

	/*
	 * The second time we're called, present the "unit-address"
	 * pseudo-property, if appropriate
	 */
	if (strcmp(name, "name") == 0) {
		int len;
		const char *fullname = fdt_get_name(fdtp, offset, &len);

		if (strchr(fullname, '@') != NULL) {
			strcpy(next, "unit-address");
			return (next);
		}

		/* Fall through to get real properties */
	}

	*next = '\0';
	offset = fdt_first_property_offset(fdtp, offset);
	if (offset < 0) {
		return (next);
	}

	const struct fdt_property *data;
	for (;;) {
		data = fdt_get_property_by_offset(fdtp, offset, NULL);
		const char *name0 = fdt_string(fdtp,
		    fdt32_to_cpu(data->nameoff));
		if (name0) {
			/*
			 * If we reach here with "name" as on of our
			 * pseudo-properties, give the first real property.
			 */
			if ((strcmp(name, "name") == 0) ||
			    (strcmp(name, "unit-address") == 0)) {
				strcpy(next, name0);
				return (next);
			}
			if (strcmp(name, name0) == 0)
				break;
		}
		offset = fdt_next_property_offset(fdtp, offset);
		if (offset < 0) {
			return (next);
		}
	}
	offset = fdt_next_property_offset(fdtp, offset);
	if (offset < 0) {
		return (next);
	}
	data = fdt_get_property_by_offset(fdtp, offset, NULL);
	strcpy(next, (char *)fdt_string(fdtp, fdt32_to_cpu(data->nameoff)));
	return (next);
}

pnode_t
prom_nextnode(pnode_t nodeid)
{
	if (nodeid == OBP_NONODE)
		return (prom_rootnode());

	int offset = fdt_node_offset_by_phandle(fdtp, (phandle_t)nodeid);
	if (offset < 0)
		return (OBP_BADNODE);

	int depth = 1;
	for (;;) {
		offset = fdt_next_node(fdtp, offset, &depth);
		if (offset < 0)
			return (OBP_NONODE);
		if (depth == 1)
			break;
	}

	phandle_t phandle = get_phandle(offset);
	if (phandle < 0)
		return (OBP_NONODE);
	return ((pnode_t)phandle);
}

pnode_t
prom_childnode(pnode_t nodeid)
{
	if (nodeid == OBP_NONODE)
		return (prom_rootnode());

	int offset = fdt_node_offset_by_phandle(fdtp, (phandle_t)nodeid);
	if (offset < 0)
		return (OBP_NONODE);

	int depth = 0;
	for (;;) {
		offset = fdt_next_node(fdtp, offset, &depth);
		if (offset < 0)
			return (OBP_NONODE);
		if (depth == 0)
			return (OBP_NONODE);
		if (depth == 1)
			break;
	}
	phandle_t phandle = get_phandle(offset);
	if (phandle < 0)
		return (OBP_NONODE);
	return ((pnode_t)phandle);
}

pnode_t
prom_parentnode(pnode_t nodeid)
{
	int offset = fdt_node_offset_by_phandle(fdtp, (pnode_t)nodeid);
	if (offset < 0)
		return (OBP_NONODE);

	int parent_offset = fdt_parent_offset(fdtp, offset);
	if (parent_offset < 0)
		return (OBP_NONODE);
	phandle_t phandle = get_phandle(parent_offset);
	if (phandle < 0)
		return (OBP_NONODE);
	return ((pnode_t)phandle);
}

char *
prom_decode_composite_string(void *buf, size_t buflen, char *prev)
{
	if ((buf == 0) || (buflen == 0) || ((int)buflen == -1))
		return (NULL);

	if (prev == 0)
		return (buf);

	prev += strlen(prev) + 1;
	if (prev >= ((char *)buf + buflen))
		return (NULL);
	return (prev);
}

int
prom_bounded_getprop(pnode_t nodeid, char *name, caddr_t value, int len)
{
	int prop_len = prom_getproplen(nodeid, name);
	if (prop_len < 0 || len < prop_len) {
		return (-1);
	}

	return (prom_getprop(nodeid, name, value));
}

pnode_t
prom_alias_node(void)
{
	return (OBP_BADNODE);
}

/*ARGSUSED*/
void
prom_pathname(char *buf)
{
	/* nothing, just to get consconfig_dacf to compile */
}

void
prom_init(char *pgmname, void *cookie)
{
	int err;
	fdtp = cookie;

	err = fdt_check_header(fdtp);
	if (err == 0) {
		phandle_t chosen = prom_chosennode();
		if (chosen == OBP_NONODE) {
			fdt_add_subnode(fdtp, fdt_node_offset_by_phandle(fdtp,
			    prom_rootnode()), "chosen");
		}
	} else {
		fdtp = NULL;
	}
}

static void
prom_dump_node(dev_info_t *dip)
{
	ddi_prop_t *hwprop;

	prom_printf("name=%s\n", ddi_node_name(dip));
	hwprop = DEVI(dip)->devi_hw_prop_ptr;
	while (hwprop != NULL) {
		prom_printf("    prop=%s\n", hwprop->prop_name);
		hwprop = hwprop->prop_next;
	}
}

static void
prom_dump_peers(dev_info_t *dip)
{
	while (dip) {
		prom_dump_node(dip);
		dip = ddi_get_next_sibling(dip);
	}
}

void
prom_setup(void)
{
}

static void
prom_walk_dev(pnode_t nodeid, void(*func)(pnode_t, void*), void *arg)
{
	func(nodeid, arg);

	pnode_t child = prom_childnode(nodeid);
	while (child > 0) {
		prom_walk_dev(child, func, arg);
		child = prom_nextnode(child);
	}
}

void
prom_walk(void(*func)(pnode_t, void*), void *arg)
{
	prom_walk_dev(prom_rootnode(), func, arg);
}
