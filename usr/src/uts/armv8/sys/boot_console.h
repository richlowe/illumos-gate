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
 * Copyright (c) 2012 Gary Mills
 * Copyright 2020 Joyent, Inc.
 *
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * XXXARM: boot_console is not yet implemented for aarch64, and contains
 * only the functions needed to support fakebop.
 */

#ifndef _BOOT_CONSOLE_H
#define	_BOOT_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/bootinfo.h>

/* Read property from command line or environment. */
extern const char *find_boot_prop(const char *);

extern void bcons_init(struct xboot_info *);

extern void bcons_post_bootenvrc(char *, char *, char *);

#ifdef __cplusplus
}
#endif

#endif /* _BOOT_CONSOLE_H */
