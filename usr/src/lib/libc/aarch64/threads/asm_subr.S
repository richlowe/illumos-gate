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
 *
 * Copyright 2017 Hayashi Naoyuki
 */

	.file	"asm_subr.s"

#include <SYS.h>
#include "assym.h"

	/*
	 * This is where execution resumes when a thread created with
	 * thr_create() or pthread_create() returns (see setup_context()).
	 * We pass the (void *) return value to _thrp_terminate().
	 */
	ENTRY(_lwp_start)
	bl	_thrp_terminate
	RET		/* actually, never returns */
	SET_SIZE(_lwp_start)

	/* All we need to do now is (carefully) call lwp_exit(). */
	ENTRY(_lwp_terminate)
	SYSTRAP_RVAL1(lwp_exit)
	RET		/* if we return, it is very bad */
	SET_SIZE(_lwp_terminate)

	/*
	 * On AArch64 the thread register points to a tcb_t, but we still
	 * take the ulwp_t as our param, and pull the correct tcb out of it
	 */
	ENTRY(set_curthread)
	add	x0, x0, #UL_TCB
	msr	tpidr_el0, x0
	ret
	SET_SIZE(set_curthread)

	ENTRY(__lwp_park)
	mov	x2, x1
	mov	x1, x0
	mov	x0, #0
	SYSTRAP_RVAL1(lwp_park)
	SYSLWPERR
	RET
	SET_SIZE(__lwp_park)

	ENTRY(__lwp_unpark)
	mov	x1, x0
	mov	x0, #1
	SYSTRAP_RVAL1(lwp_park)
	SYSLWPERR
	RET
	SET_SIZE(__lwp_unpark)

	ENTRY(__lwp_unpark_all)
	mov	x2, x1
	mov	x1, x0
	mov	x0, #2
	SYSTRAP_RVAL1(lwp_park)
	SYSLWPERR
	RET
	SET_SIZE(__lwp_unpark_all)

/*
 * __sighndlr(int sig, siginfo_t *si, ucontext_t *uc, void (*hndlr)())
 *
 * This is called from sigacthandler() for the entire purpose of
 * communicating the ucontext to java's stack tracing functions.
 */
	ENTRY(__sighndlr)
	.globl	__sighndlrend
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	blr	x3
	ldp	x29, x30, [sp], #16
	ret
__sighndlrend:
	SET_SIZE(__sighndlr)

/*
 * int _sigsetjmp(sigjmp_buf env, int savemask)
 *
 * This version is faster than the old non-threaded version because we
 * don't normally have to call __getcontext() to get the signal mask.
 * (We have a copy of it in the ulwp_t structure.)
 */

#undef	sigsetjmp

	ENTRY2(sigsetjmp,_sigsetjmp)
	// env		x0
	// savemask	x1
	mov	x3, sp

	sub	sp, sp, #(7 * 16)
	stp	x19, x20, [sp, #(0 * 16)]
	stp	x21, x22, [sp, #(1 * 16)]
	stp	x23, x24, [sp, #(2 * 16)]
	stp	x25, x26, [sp, #(3 * 16)]
	stp	x27, x28, [sp, #(4 * 16)]
	stp	x29, x30, [sp, #(5 * 16)]
	str	x3,       [sp, #(6 * 16)]
	mov	x2, sp
	bl	__csigsetjmp
	ldr	x30,      [sp, #(5 * 16 + 8)]
	add	sp, sp, #(7 * 16)
	ret
	SET_SIZE(sigsetjmp)
	SET_SIZE(_sigsetjmp)
