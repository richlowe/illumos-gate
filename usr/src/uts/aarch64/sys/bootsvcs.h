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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_BOOTSVCS_H
#define	_SYS_BOOTSVCS_H

#include <sys/ccompile.h>
#include <sys/stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct boot_syscalls {
	int	(*bsvc_getchar)();
	void	(*bsvc_putchar)(int);
	int	(*bsvc_ischar)();
	void	(*bsvc_reset)(bool) __NORETURN;
} boot_syscalls_t;

#ifdef _KMDB
extern boot_syscalls_t *kmdb_sysp;
#define	SYSP	kmdb_sysp
#else /* !_KMDB */
extern boot_syscalls_t *sysp;
#define	SYSP	sysp
#endif

#define	BSVC_GETCHAR(sysp)		(((sysp)->bsvc_getchar)())
#define	BSVC_PUTCHAR(sysp, c)		(((sysp)->bsvc_putchar)(c))
#define	BSVC_ISCHAR(sysp)		(((sysp)->bsvc_ischar)())
#define	BSVC_RESET(sysp, poff)		(((sysp)->bsvc_reset)(poff))

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_BOOTSVCS_H */
