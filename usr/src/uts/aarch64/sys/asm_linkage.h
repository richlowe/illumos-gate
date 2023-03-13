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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2017 Hayashi Naoyuki
 */

#ifndef	_SYS_ASM_LINKAGE_H
#define	_SYS_ASM_LINKAGE_H

#include <sys/stack.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _ASM

#define	CPTRSHIFT	3
#define	CLONGSHIFT	3
#define	CPTRSIZE	(1<<CPTRSHIFT)
#define	CLONGSIZE	(1<<CLONGSHIFT)
#define	CPTRMASK	(CPTRSIZE - 1)
#define	CLONGMASK	(CLONGSIZE - 1)

#define	RODATA	".rodata"

#define	MCOUNT_SIZE	0
#define	MCOUNT(x)

#define	RTMCOUNT(x)	MCOUNT(x)

#define	ANSI_PRAGMA_WEAK(sym, stype)	\
	.weak	_##sym;			\
	.type	_##sym, @stype;		\
/* CSTYLED */ \
_##sym	= sym

#define	ANSI_PRAGMA_WEAK2(sym1, sym2, stype)	\
	.weak	sym1; \
	.type sym1, @stype; \
sym1	= sym2

#define	ENTRY(x) \
	.text; \
	.align	4; \
	.globl	x; \
	.type	x, @function; \
x:	MCOUNT(x)

#define	ENTRY_SIZE	MCOUNT_SIZE

#define	ENTRY_NP(x) \
	.text; \
	.align	4; \
	.globl	x; \
	.type	x, @function; \
x:

#define	RTENTRY(x) \
	.text; \
	.align	4; \
	.globl	x; \
	.type	x, @function; \
x:	RTMCOUNT(x)

#define	ENTRY2(x, y) \
	.text; \
	.align	4; \
	.globl	x, y; \
	.type	x, @function; \
	.type	y, @function; \
/* CSTYLED */ \
x:	; \
y:	MCOUNT(x)

#define	ENTRY_NP2(x, y) \
	.text; \
	.align	4; \
	.globl	x, y; \
	.type	x, @function; \
	.type	y, @function; \
/* CSTYLED */ \
x:	; \
y:

/*
 * ALTENTRY provides for additional entry points.
 */
#define	ALTENTRY(x) \
	.globl x; \
	.type	x, @function; \
x:

#define	DGDEF2(name, sz) \
	.data; \
	.global name; \
	.type	name, @object; \
	.size	name, sz; \
name:

#define	DGDEF3(name, sz, algn) \
	.data; \
	.balign	algn; \
	.global name; \
	.type	name, @object; \
	.size	name, sz; \
name:

#define	DGDEF(name)	DGDEF3(name, 4, 4)

#define	SET_SIZE(x) \
	.size	x, [.-x]

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASM_LINKAGE_H */
