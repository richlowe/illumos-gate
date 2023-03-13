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
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _LIBC_AARCH64_INC_SYS_H
#define	_LIBC_AARCH64_INC_SYS_H

/*
 * This file defines common code sequences for system calls.
 */
#include <sys/asm_linkage.h>
#include <sys/syscall.h>
#include <sys/errno.h>
#include <sys/controlregs.h>

#include "assym.h"

	.globl	__cerror

#define	__SYSCALL(name)		\
	svc #(SYS_##name)

#define	SYSTRAP_RVAL1(name)	__SYSCALL(name)
#define	SYSTRAP_RVAL2(name)	__SYSCALL(name)
#define	SYSTRAP_2RVALS(name)	__SYSCALL(name)
#define	SYSTRAP_64RVAL(name)	__SYSCALL(name)

	/* XXXARM: Make this magic number symbolic */
#define	SYSFASTTRAP(name)		\
	svc #(T_##name + 0x8000)

#define	SYSCERROR			\
	b.cc	1f;			\
	mov	x0, x9;			\
	b	__cerror;		\
1:

#define	SYSLWPERR			\
	b.cc	1f;			\
	mov	x0, x9;			\
	cmp	x0, #ERESTART;		\
	b.ne	2f;			\
	mov	x0, #EINTR;		\
	b	2f;			\
1:	mov	x0, #0;			\
2:

/*
 * SYSREENTRY provides the entry sequence for restartable system calls.
 */
#define	SYSREENTRY(name)	\
	ENTRY(name);		\
.restart_##name:

#define	SYSRESTART(name)		\
	b.cc	1f;			\
	cmp	x9, #ERESTART;		\
	b.eq	name;			\
	mov	x0, x9;			\
	b	__cerror;		\
1:

/*
 * SYSINTR_RESTART provides the error handling sequence for restartable
 * system calls in case of EINTR or ERESTART.
 */
#define	SYSINTR_RESTART(name)		\
	b.cc	1f;			\
	cmp	x9, #ERESTART;		\
	b.eq	name;			\
	cmp	x9, #EINTR;		\
	b.eq	name;			\
	mov	x0, x9;			\
	b	2f;			\
1:	mov	x0, #0;			\
2:

/*
 * SYSCALL provides the standard (i.e.: most common) system call sequence.
 */
#define	SYSCALL(name)			\
	ENTRY(name);			\
	SYSTRAP_2RVALS(name);		\
	SYSCERROR

#define	SYSCALL_RVAL1(name)		\
	ENTRY(name);			\
	SYSTRAP_RVAL1(name);		\
	SYSCERROR

/*
 * SYSCALL64 provides the standard (i.e.: most common) system call sequence
 * for system calls that return 64-bit values.
 */
#define	SYSCALL64(name)			\
	SYSCALL(name)

/*
 * SYSCALL_RESTART provides the most common restartable system call sequence.
 */
#define	SYSCALL_RESTART(name)		\
	SYSREENTRY(name);		\
	SYSTRAP_2RVALS(name);		\
	SYSRESTART(.restart_##name)

#define	SYSCALL_RESTART_RVAL1(name)	\
	SYSREENTRY(name);		\
	SYSTRAP_RVAL1(name);		\
	SYSRESTART(.restart_##name)

/*
 * SYSCALL2 provides a common system call sequence when the entry name
 * is different than the trap name.
 */
#define	SYSCALL2(entryname, trapname)	\
	ENTRY(entryname);		\
	SYSTRAP_2RVALS(trapname);	\
	SYSCERROR

#define	SYSCALL2_RVAL1(entryname, trapname)	\
	ENTRY(entryname);			\
	SYSTRAP_RVAL1(trapname);		\
	SYSCERROR

/*
 * SYSCALL2_RESTART provides a common restartable system call sequence when the
 * entry name is different than the trap name.
 */
#define	SYSCALL2_RESTART(entryname, trapname)	\
	SYSREENTRY(entryname);			\
	SYSTRAP_2RVALS(trapname);		\
	SYSRESTART(.restart_##entryname)

#define	SYSCALL2_RESTART_RVAL1(entryname, trapname)	\
	SYSREENTRY(entryname);				\
	SYSTRAP_RVAL1(trapname);			\
	SYSRESTART(.restart_##entryname)

/*
 * SYSCALL_NOERROR provides the most common system call sequence for those
 * system calls which don't check the error reture (carry bit).
 */
#define	SYSCALL_NOERROR(name)		\
	ENTRY(name);			\
	SYSTRAP_2RVALS(name)

#define	SYSCALL_NOERROR_RVAL1(name)	\
	ENTRY(name);			\
	SYSTRAP_RVAL1(name)

/*
 * Standard syscall return sequence, return code equal to rval1.
 */
#define	RET			\
	ret

/*
 * Syscall return sequence, return code equal to rval2.
 */
#define	RET2			\
	mov	x0, x1;		\
	ret

/*
 * Syscall return sequence with return code forced to zero.
 */
#define	RETC			\
	mov	x0, #0;		\
	ret

#endif	/* _LIBC_AARCH64_INC_SYS_H */
