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
 * Copyright 2022 Michael van der Westhuizen
 */

#ifndef _ASM_THREAD_H
#define	_ASM_THREAD_H

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(__lint) && defined(__GNUC__)

struct _kthread;

extern __GNU_INLINE struct _kthread *
threadp(void)
{
	uint64_t tpidr_el1;
	__asm__ __volatile__("mrs %0, TPIDR_EL1"
	    :"=r"(tpidr_el1)
	    :
	    :"memory");
	return ((struct _kthread *)tpidr_el1);
}

/* XXXARM: These should be done properly, and not be here */
extern __GNU_INLINE caddr_t
caller(void)
{
	return (__builtin_return_address(0));
}

extern __GNU_INLINE caddr_t
callee(void)
{
	return (__builtin_return_address(0));
}

#endif	/* !__lint && __GNUC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _ASM_THREAD_H */
