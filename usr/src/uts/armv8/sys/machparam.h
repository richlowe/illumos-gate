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
 * Copyright (c) 1992, 2010, Oracle and/or its affiliates. All rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

#ifndef _SYS_MACHPARAM_H
#define	_SYS_MACHPARAM_H

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_ASM)
#define	ADDRESS_C(c)	(c)
#else
#include <sys/types.h>
#include <sys/int_const.h>
#include <sys/pte.h>

#define	ADDRESS_C(c)	UINT64_C(c)
#endif

#define	NCPU		128
#define	NCPU_LOG2	7
#define	NCPU_P2		(1 << NCPU_LOG2)

/*
 * The largest number of page sizes/table levels supported on this platform
 * (52bit VA)
 *
 * The actual value supported at runtime may be less than this.
 */
#define	MMU_PAGE_SIZES	3	/* levels 0,1,2: 4k, 2M, 1G (in 4k granule) */
#define	MMU_PAGE_LEVELS	6	/* levels [-2,-1,0,1,2,3] (in ARM terms) */

/* The default levels and sizes (48bit VA) */
#define	DEFAULT_MMU_PAGE_SIZES	MMU_PAGE_SIZES
#define	DEFAULT_MMU_PAGE_LEVELS	4 /* levels [0,1,2,3] */

#define	MMU_PAGESHIFT		12
#define	MMU_PAGESIZE		(ADDRESS_C(1) << MMU_PAGESHIFT)
#define	MMU_PAGEOFFSET		(MMU_PAGESIZE - 1)
#define	MMU_PAGEMASK		(~MMU_PAGEOFFSET)

#define	PAGESHIFT		MMU_PAGESHIFT
#define	PAGESIZE		MMU_PAGESIZE
#define	PAGEOFFSET		MMU_PAGEOFFSET
#define	PAGEMASK		MMU_PAGEMASK

/*
 * DATA_ALIGN is used to define the alignment of the Unix data segment.
 */
#define	DATA_ALIGN	PAGESIZE

/*
 * DEFAULT KERNEL THREAD stack size.
 */
#define	DEFAULTSTKSZ	(5 * PAGESIZE)

/*
 * DEFAULT initial thread stack size.
 */
#define	T0STKSZ		(2 * DEFAULTSTKSZ)

/*
 * Effective virtual address size
 */
#define	VA_BITS		48

/*
 * KERNELBASE is the virtual address at which the kernel segments start in
 * all contexts.
 *
 * The default is unused and set to a poisonous value.
 */
#define	DEFAULT_KERNELBASE	-1ull

#define	BOOT_VEC_SIZE	(8L * 1024L * 1024L)
#define	BOOT_VEC_BASE	(- BOOT_VEC_SIZE)		// 0xffffffff_ff800000

#define	SEGDEBUGBASE	(BOOT_VEC_BASE - SEGDEBUGSIZE)	// 0xffffffff_ff000000
#define	SEGDEBUGSIZE	(8L * 1024L * 1024L)

// 0xffffffff_fe000000 (top - 31MB)
#define	KERNEL_TEXT	ADDRESS_C(0xfffffffffe000000)

/* 0xffffffff_fc000000 (top - 64MB) */
#define	COREHEAP_BASE	ADDRESS_C(0xfffffffffc000000)

/*
 * The heap has a region allocated from it of HEAPTEXT_SIZE bytes specifically
 * for module text (the core heap)
 */
#define	HEAPTEXT_SIZE	(KERNEL_TEXT - COREHEAP_BASE)

/*
 * The so called "Misc VA" is the address space carved out for early boot to
 * allocate from before any of the virtual memory system is available.
 *
 * We put it immediately below the core heap used for module text, so that its
 * address is fixed regardless of kernelbase.
 */
#define	MISC_VA_SIZE	(1L * 1024L * 1024L * 1024L) /* 1G */
#define	MISC_VA_BASE	(COREHEAP_BASE - MISC_VA_SIZE)

/* The end of the 48bit VA hole */
#define	SEGKPM_BASE	(~((ADDRESS_C(1) << VA_BITS) - 1))

/*
 * default and boundary sizes for segkp
 */
#define	SEGKPDEFSIZE	(2L * 1024L * 1024L * 1024L)		/*   2G */
#define	SEGKPMAXSIZE	(8L * 1024L * 1024L * 1024L)		/*   8G */
#define	SEGKPMINSIZE	(200L * 1024 * 1024L)			/* 200M */

/*
 * minimum size for segzio
 */
#define	SEGZIOMINSIZE	(400L * 1024 * 1024L)			/* 400M */

/*
 * Define upper limit on user address space.
 * The default is unused and set to a poisonous value.
 */
#define	DEFAULT_USERLIMIT	0

/*
 * This limit, traditionally for ILP32 processes, may also be applied to LP64
 * processes with the SF1_SUNW_ADDR32 flag set.
 */
#define	USERLIMIT32	((ADDRESS_C(1) << 32) - 0x1000)

/*
 * Use a slightly larger thread stack size for interrupt threads rather than
 * the default. This is useful for cases where the networking stack may do an
 * rx and a tx in the context of a single interrupt and when combined with
 * various promisc hooks that need memory, can cause us to get dangerously
 * close to the edge of the traditional stack sizes. This is only a few pages
 * more than a traditional stack and given that we don't have that many
 * interrupt threads, the memory costs end up being more than worthwhile.
 */
#define	LL_INTR_STKSZ_NPGS	8
#define	LL_INTR_STKSZ		(LL_INTR_STKSZ_NPGS * PAGESIZE)

#if !defined(_ASM) && !defined(_KMDB)
extern uintptr_t kernelbase, segmap_start, segmapsize;
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHPARAM_H */
