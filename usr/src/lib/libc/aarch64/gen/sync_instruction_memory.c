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

#include <sys/types.h>
#include <sys/controlregs.h>

void
sync_instruction_memory(caddr_t addr, size_t len)
{
	uint64_t inst_line_size = CTR_IMINLINE_SIZE(read_ctr_el0());
	uint64_t data_line_size = CTR_DMINLINE_SIZE(read_ctr_el0());

	for (uintptr_t v = (uintptr_t)addr;
	    v < (uintptr_t)addr + len; v += data_line_size) {
		flush_data_cache(v);
	}
	dsb(ish);
	for (uintptr_t v = (uintptr_t)addr;
	    v < (uintptr_t)addr + len; v += inst_line_size) {
		invalidate_instruction_cache(v);
	}
	dsb(ish);
	isb();
}
