/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/* Copyright 2023 Richard Lowe */

/*
 * Stub file for ldd(1).  Provides for preloading shared libraries.
 */

#include <sys/syscall.h>

	.file	"lddstub.S"

	.text
	.globl	stub
stub:
	mov	x0, #0
	svc	#SYS_exit
