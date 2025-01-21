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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_MACH_INTR_H
#define	_SYS_MACH_INTR_H


/*
 * Platform-dependent interrupt data structures
 *
 * This file should not be included by code that purports to be
 * platform-independent.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

/*
 * A 1275/devicetree unit interrupt descriptor.
 *
 * The meaning of elements in the vector is only interpretable based on the
 * device it describes.
 */
typedef struct {
	size_t ui_nelems;	/* Number of elements in ui_v */
	size_t ui_addrcells;	/* # address cells */
	size_t ui_intrcells;	/* # interrupt cells */
	uint32_t ui_v[];	/* unit/interrupt descriptor */
} unit_intr_t;

/*
 * Platform dependent data which hangs off the ih_private field of a
 * ddi_intr_handle_impl_t
 */
typedef struct ihdl_plat {
	kstat_t		*ip_ksp;	/* Kstat pointer */
	uint64_t	ip_ticks;	/* Interrupt ticks for this device */

	/* XXXARM: `void *ihdl_ctlr_private`? */
	unit_intr_t	*ip_unitintr;	/* devicetree unit interrupt spec. */
} ihdl_plat_t;

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MACH_INTR_H */
