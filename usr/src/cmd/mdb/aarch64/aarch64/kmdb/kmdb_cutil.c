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
 * Utilities that would be ASM on other platforms, but are easier like this
 * here.
 */

#include <sys/types.h>

#include <mdb/mdb_debug.h>

uint64_t
cas(volatile uint64_t *target, uint64_t cmp, uint64_t newval)
{
	return (__sync_val_compare_and_swap(target, cmp, newval));
}
