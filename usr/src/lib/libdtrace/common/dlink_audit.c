/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2012 by Delphix. All rights reserved.
 */

/*
 * This file implements an audit library that can be used to force the loading
 * of helper providers. The default disposition for a helper provider -- USDT
 * and ustack helpers -- is to load itself from it's containing object's .init
 * section. In cases where startup time is deemed critical, USDT authors can
 * use the -xlazyload option to dtrace(1M) to disable automatic loading (it's
 * difficult to make the case for the utility of this feature for anything
 * other than libc which, indeed, was the sole motivation). If a binary has
 * been compiled with automatic loading disabled, this audit library may be
 * used to force automatic loading:
 *
 *	LD_AUDIT_32=/usr/lib/dtrace/libdtrace_forceload.so
 *	LD_AUDIT_64=/usr/lib/dtrace/64/libdtrace_forceload.so
 */

#include <link.h>
#include <stdio.h>
#include <libproc.h>
#include <strings.h>

#include <dlink.h>

typedef struct obj_list {
	struct obj_list *ol_next;
	char *ol_name;
	uintptr_t ol_addr;
	Lmid_t ol_lmid;
} obj_list_t;

static obj_list_t *list;

#pragma init(dtrace_forceload_init)
static void
dtrace_forceload_init(void)
{
	dtrace_link_init();
}

/*ARGSUSED*/
uint_t
la_version(uint_t version)
{
	return (LAV_CURRENT);
}

/*
 * Record objects into our linked list as they're loaded.
 */
/*ARGSUSED*/
uint_t
la_objopen(Link_map *lmp, Lmid_t lmid, uintptr_t *cookie)
{
	obj_list_t *node;

	/*
	 * If we can't allocate the next node in our list, we'll try to emit a
	 * message, but it's possible that might fail as well.
	 */
	if ((node = malloc(sizeof (obj_list_t))) == NULL) {
		dprintf(0, "forceload: failed to allocate");
		return (0);
	}
	node->ol_next = list;
	node->ol_name = strdup(lmp->l_name);
	node->ol_addr = lmp->l_addr;
	node->ol_lmid = lmid;
	list = node;

	return (0);
}

/*
 * Once the link maps have reached a consistent state, process the list of
 * objects that were loaded. We need to use libproc to search for symbols
 * rather than dlsym(3C) since they may not be in the dynamic (run-time)
 * symbol table (though they are, of course, in the symtab).  Once we find
 * them, we ioctl(2) it to the kernel just as we would have from the .init
 * section if automatic loading were enabled.
 */
/*ARGSUSED*/
void
la_activity(uintptr_t *cookie, uint_t flags)
{
	struct ps_prochandle *P;
	int err;
	GElf_Sym sym;

	if (flags != LA_ACT_CONSISTENT)
		return;

	while (list != NULL) {
		obj_list_t *node = list;
		char *name = node->ol_name;
		intptr_t *dofs;	/* terminated by a 0 entry */
		int ndofs = 4;	/* allocated size of dofs */
		int i = 0;

		/*
		 * Two providers per object would be the commonly worst case
		 * (provider + helper).
		 *
		 * More is especially uncommon.
		 */
		if ((dofs = calloc(ndofs, sizeof (uintptr_t))) == NULL) {
			dprintf(0, "forceload: failed to allocate");
			return;
		}

		list = node->ol_next;

		P = Pgrab(getpid(), PGRAB_RDONLY, &err);
		if (Plookup_by_name(P, name, "___SUNW_dof", &sym) == 0)
			dofs[i++] = sym.st_value;

		/*
		 * XXX: This loads all DOF, not just lazy-loaded DOF.
		 * The same is true of dt_pid, however, so maybe that's safe?
		 */
		if (Plookup_by_name(P, name, "__SUNW_dof_array", &sym) == 0) {
			uintptr_t dofaddr, p;

			for (p = sym.st_value; ; p += sizeof (p)) {
				if (Pread(P, &dofaddr, sizeof (dofaddr), p) !=
				    sizeof (dofaddr)) {
					dprintf(0, "libaudit: __SUNW_dof_array"
					    " corrupt");
					free(dofs);
					return;
				}

				if (i == ndofs) {
					ndofs *= 2;
					if ((dofs = realloc(dofs,
					    sizeof (uintptr_t) * ndofs)) ==
					    NULL) {
						dprintf(0, "libaudit: couldn't"
						    " allocate");
						return;
					}
				}

				if (dofaddr == 0)
					break;

				dofs[i++] = dofaddr;
			}
		}
		Prelease(P, 0);
		dofs[i++] = -1;

		for (i = 0; dofs[i] != -1; i++) {
			(void) dtrace_link_dof((void *)dofs[i],
			    node->ol_lmid, node->ol_name, node->ol_addr);
		}
		free(dofs);
		free(node->ol_name);
		free(node);
	}
}
