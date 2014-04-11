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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2012 by Delphix. All rights reserved.
 */

/*
 * This is the basis for ldrti.o which ld(1) links into dynamic objects
 * created with DTrace provider definitions.
 */

#include <dlfcn.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <dlink.h>


/*
 * XXX: The way gen is used in dlink_init.c smells of a bug?  We don't
 * initialize it from the ioctl, but I think we may be being clever with it
 * being static and linked as we are.
 */
static int *gen;			/* current DOF helper generation */
extern dof_hdr_t *__SUNW_dof_array[];

#pragma init(dtrace_ld_init)
static void
dtrace_ld_init(void)
{
	Link_map *lmp;
	Lmid_t lmid;
	int ngen = 4;
	int i, gi = 0;

	dtrace_link_init();

	if (dlinfo(RTLD_SELF, RTLD_DI_LINKMAP, &lmp) == -1 || lmp == NULL) {
		dprintf(1, "couldn't discover module name or address\n");
		return;
	}

	if (dlinfo(RTLD_SELF, RTLD_DI_LMID, &lmid) == -1) {
		dprintf(1, "couldn't discover link map ID\n");
		return;
	}

	if ((gen = calloc(sizeof (int), ngen)) == NULL) {
		dprintf(1, "couldn't allocate\n");
		return;
	}

	for (i = 0; __SUNW_dof_array[i] != 0; i++) {
		int g;
		dof_hdr_t *dof = __SUNW_dof_array[i];

		/*
		 * If the DOF has its own DRTI (which is unlikely) or is
		 * to be lazy-loaded, don't load it.
		 */
		if (((dof->dofh_rtflags & DOF_RTFL_NODRTI) == 0) ||
		    ((dof->dofh_rtflags & DOF_RTFL_LAZY) != 0))
			continue;


		if (i >= ngen) {
			ngen *= 2;
			if ((gen = realloc(gen, sizeof (int) * ngen)) == NULL) {
				dprintf(1, "couldn't allocate\n");
				return;
			}
		}

		if ((g = dtrace_link_dof(dof, lmid, lmp->l_name,
		    lmp->l_addr)) >= 0)
			gen[gi++] = g;
	}

	gen[gi++] = -1;	/* terminate */
}

#pragma fini(dtrace_ld_fini)
static void
dtrace_ld_fini(void)
{
	int fd;
	int i;

	if ((fd = open64(devname, O_RDWR)) < 0) {
		dprintf(1, "failed to open helper device %s", devname);
		return;
	}

	for (i = 0; gen[i] != -1; i++) {
		int rval;
		if ((rval = ioctl(fd, DTRACEHIOC_REMOVE, gen[i])) == -1)
			dprintf(1, "DTrace ioctl failed to remove DOF (%d)\n",
			    gen[i]);
		else
			dprintf(1, "DTrace ioctl removed DOF (%d)\n", gen[i]);
	}

	(void) close(fd);
}
