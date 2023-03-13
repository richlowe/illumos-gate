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
 */

	.file	"door.s"

#include "SYS.h"
#include <sys/door.h>

	/*
	 * weak aliases for public interfaces
	 */
	ANSI_PRAGMA_WEAK2(door_bind,__door_bind,function)
	ANSI_PRAGMA_WEAK2(door_getparam,__door_getparam,function)
	ANSI_PRAGMA_WEAK2(door_info,__door_info,function)
	ANSI_PRAGMA_WEAK2(door_revoke,__door_revoke,function)
	ANSI_PRAGMA_WEAK2(door_setparam,__door_setparam,function)

/*
 * Offsets within struct door_results
 */
#define	DOOR_COOKIE	(0 * CLONGSIZE)
#define	DOOR_DATA_PTR	(1 * CLONGSIZE)
#define	DOOR_DATA_SIZE	(2 * CLONGSIZE)
#define	DOOR_DESC_PTR	(3 * CLONGSIZE)
#define	DOOR_DESC_SIZE	(4 * CLONGSIZE)
#define	DOOR_PC		(5 * CLONGSIZE)
#define	DOOR_SERVERS	(6 * CLONGSIZE)
#define	DOOR_INFO_PTR	(7 * CLONGSIZE)

/*
 * All of the syscalls except door_return() follow the same pattern.  The
 * subcode goes in a5, after all of the other arguments.
 */
#define	DOOR_SYSCALL(name, code)					\
	ENTRY(name);							\
	mov	x5, code;			/* subcode */		\
	SYSTRAP_RVAL1(door);						\
	SYSCERROR;							\
	RET;								\
	SET_SIZE(name)

	DOOR_SYSCALL(__door_bind,	DOOR_BIND)
	DOOR_SYSCALL(__door_call,	DOOR_CALL)
	DOOR_SYSCALL(__door_create,	DOOR_CREATE)
	DOOR_SYSCALL(__door_getparam,	DOOR_GETPARAM)
	DOOR_SYSCALL(__door_info,	DOOR_INFO)
	DOOR_SYSCALL(__door_revoke,	DOOR_REVOKE)
	DOOR_SYSCALL(__door_setparam,	DOOR_SETPARAM)
	DOOR_SYSCALL(__door_ucred,	DOOR_UCRED)
	DOOR_SYSCALL(__door_unbind,	DOOR_UNBIND)
	DOOR_SYSCALL(__door_unref,	DOOR_UNREFSYS)

/*
 * int
 * __door_return(
 *	void 			*data_ptr,
 *	size_t			data_size,	(in bytes)
 *	door_return_desc_t	*door_ptr,	(holds returned desc info)
 *	caddr_t			stack_base,
 *	size_t			stack_size)
 */
	ENTRY(__door_return)
door_restart:
	mov	x5, #DOOR_RETURN	/* subcode */
	SYSTRAP_RVAL1(door)
	b.cs	2f			/* errno is set */

	ldr	w2, [sp, #DOOR_SERVERS]
	cbnz	w2, 1f

	/*
	 * this is the last server thread - call creation func for more
	 */
	ldr	x0, [sp, #DOOR_INFO_PTR]
	bl	door_depletion_cb

1:	/* Call the door server function now */
	ldr	x0, [sp, #DOOR_COOKIE]
	ldr	x1, [sp, #DOOR_DATA_PTR]
	ldr	x2, [sp, #DOOR_DATA_SIZE]
	ldr	x3, [sp, #DOOR_DESC_PTR]
	ldr	x4, [sp, #DOOR_DESC_SIZE]
	ldr	x9, [sp, #DOOR_PC]
	blr	x9

	/* Exit the thread if we return here */
	mov	x0, #0
	bl	_thrp_terminate

	/* エラー時 */
2:	cmp	x9, #ERESTART
	b.ne	3f
	mov	x9, #EINTR
3:	cmp	x9, #EINTR
	b.eq	4f

	/* EINTR以外はエラー*/
	mov	x0, x9
	b	__cerror

4:	/* EINTRの場合 */
	stp	x29, x30, [sp, #-(8*2)]!
	bl	getpid
	ldp	x29, x30, [sp], #(8*2)


	adrp	x9, :got:door_create_pid
	ldr	x9, [x9, #:got_lo12:door_create_pid]
	ldr	w9, [x9]
	cmp	w9, w0
	b.eq	5f
	mov	x0, #EINTR
	b	__cerror

5:	/* */
	mov	x0, #0
	mov	x1, #0
	mov	x2, #0
	b	door_restart
	SET_SIZE(__door_return)
