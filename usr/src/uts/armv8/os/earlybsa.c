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
 * Copyright 2020 Oxide Computer Company
 * Copyright 2024 Michael van der Westhuizen
 */

/*
 * Routines used by pre-rootnex Base System Architecture hardware support.
 */

#include <sys/types.h>
#include <sys/null.h>
#include <sys/machparam.h>
#include <sys/param.h>
#include <sys/vmem.h>
#include <vm/hat.h>
#include <vm/seg_kmem.h>
#include <sys/smp_impldefs.h>

/*
 * from startup.c - kernel VA range allocator for device mappings
 */
extern void *device_arena_alloc(size_t size, int vm_flag);
extern void device_arena_free(void * vaddr, size_t size);

caddr_t
psm_map_phys(paddr_t addr, size_t len, int prot)
{
	uint_t pgoffset;
	paddr_t base;
	pgcnt_t npages;
	caddr_t cvaddr;

	if (len == 0)
		return (NULL);

	pgoffset = addr & MMU_PAGEOFFSET;
	base = addr;

	npages = mmu_btopr(len + pgoffset);
	cvaddr = device_arena_alloc(ptob(npages), VM_NOSLEEP);
	if (cvaddr == NULL)
		return (NULL);
	hat_devload(kas.a_hat, cvaddr, mmu_ptob(npages), mmu_btop(base),
	    prot, HAT_LOAD_LOCK);
	return (cvaddr + pgoffset);
}

void
psm_unmap_phys(caddr_t addr, size_t len)
{
	uint_t pgoffset;
	caddr_t base;
	pgcnt_t npages;

	if (len == 0)
		return;

	pgoffset = (uintptr_t)addr & MMU_PAGEOFFSET;
	base = addr - pgoffset;
	npages = mmu_btopr(len + pgoffset);
	hat_unload(kas.a_hat, base, ptob(npages), HAT_UNLOAD_UNLOCK);
	device_arena_free(base, ptob(npages));
}
