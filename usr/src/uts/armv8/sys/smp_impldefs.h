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
 * Copyright (c) 1993, 2010, Oracle and/or its affiliates. All rights reserved.
 */

#ifndef _SYS_SMP_IMPLDEFS_H
#define	_SYS_SMP_IMPLDEFS_H

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/cpuvar.h>
#include <sys/avintr.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	External Reference Functions
 */
extern int (*slvltovect)(int);	/* ipl interrupt priority level */
extern int setlvl(int);	/* set intr pri represented by vect */
extern void setlvlx(int);	/* set intr pri to specified level */
extern int (*addspl)(int, int, int, int);	/* add intr mask of vector  */
extern int (*delspl)(int, int, int, int);	/* delete intr mask of vector */
extern int (*addintr)(void *, int, avfunc, char *, int, caddr_t, caddr_t,
    uint64_t *, dev_info_t *);	/* replacement of add_avintr */
extern void (*remintr)(void *, int, avfunc, int); /* replace of rem_avintr */

/* trigger a software intr */
extern void (*setsoftint)(int, struct av_softinfo *);

/* kmdb private entry point */
extern void (*kdisetsoftint)(int, struct av_softinfo *);

extern void av_set_softint_pending();	/* set software interrupt pending */
extern void kdi_av_set_softint_pending(); /* kmdb private entry point */

/* map physical address							*/

/*
 * XX64: Changing psm_map_phys() to take a paddr_t rather than a uint32_t
 * will be a flag day.  Other drivers in the WOS use the psm_map()
 * interface, so we need this hack to get them to coexist for
 * pre-integration testing.
 */
extern caddr_t psm_map_phys_new(paddr_t, size_t, int);
#define	psm_map_phys psm_map_phys_new

/* unmap the physical address given in psm_map_phys() from the addr	*/
extern void psm_unmap_phys(caddr_t, size_t);

/*
 *	External Reference Data
 */
extern struct av_head autovect[]; /* array of auto intr vectors		*/
extern uint32_t rm_platter_pa;	/* phy addr realmode startup storage	*/
extern caddr_t rm_platter_va;	/* virt addr realmode startup storage	*/
extern cpuset_t mp_cpus;	/* bit map of possible cpus found	*/

/*
 * virtulization support for psm
 */
extern void *psm_vt_ops;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SMP_IMPLDEFS_H */
