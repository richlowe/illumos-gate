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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/asm_linkage.h>

/*
 * This routine is called for the initial trip into the debugger.  We need
 * to switch over to the debugger stack before entering the debugger.
 * Subsequent re-entries will longjmp their way in.
 */

	ENTRY(kmdb_first_start)
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp

	// NB: When kmdb is modloaded, our data and text may end up far enough
	// part we need more displacement than the usual `adrp`/`ldr` pair.
	ldr	x0, =kmdb_main_stack
	ldr	x0, [x0]
	ldr	x1, =kmdb_main_stack_size
	ldr	x1, [x1]

	add	x0, x1, x0
	sub	x0, x0, #1
	and	x0, x0, #~(STACK_ALIGN - 1)

	mov	sp, x0
	bl	kmdb_main
	ldp	fp, lr, [sp], #16
	ret
	SET_SIZE(kmdb_first_start)
