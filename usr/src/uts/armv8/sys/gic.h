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
 * Copyright 2025 Michael van der Westhuizen
 */

#ifndef _SYS_GIC_H
#define	_SYS_GIC_H

#ifdef __cplusplus
extern "C" {
#endif

#define	GIC_FDT_PPI_TO_IRQ(vec)	((vec) + 16)
#define	GIC_FDT_SPI_TO_IRQ(vec)	((vec) + 32)

#define	GIC_FDT_VEC_TO_IRQ(type, vec) \
	(((type) == 0) ? GIC_FDT_SPI_TO_IRQ((vec)) : GIC_FDT_PPI_TO_IRQ((vec)))

#ifdef __cplusplus
}
#endif

#endif /* _SYS_GIC_H */
