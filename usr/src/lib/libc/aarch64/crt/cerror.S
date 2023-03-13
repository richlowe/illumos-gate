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

	.file	"cerror.s"

#include <SYS.h>

	ENTRY(__cerror)
	stp	x19, x30, [sp, #-(8*2)]!
	cmp	w0, #ERESTART
	b.ne	1f
	mov	w0, #EINTR
1:	mov	x19, x0
	bl	___errno
	str	w19, [x0, #0]
	mov	x0, #-1
	ldp	x19, x30, [sp], #(8*2)
	ret
	SET_SIZE(__cerror)
