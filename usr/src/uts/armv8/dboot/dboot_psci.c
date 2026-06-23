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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2026 Michael van der Westhuizen
 */

/*
 * dboot PSCI - a thin client of the SMCCC transport layer.
 *
 * All firmware calls go through dboot_smccc_call which handles
 * conduit selection (SMC/HVC).
 */

#include <sys/types.h>
#include <sys/null.h>
#include <sys/bootinfo.h>

#include "dboot.h"

static uint32_t psci_system_off_id = 0x84000008;
static uint32_t psci_system_reset_id = 0x84000009;
boolean_t psci_initialized = B_FALSE;

extern uint64_t dboot_smccc_call(uint64_t, uint64_t, uint64_t, uint64_t);

/*
 * Initialize PSCI in dboot context.
 *
 * smccc_init must have been called first - it validates the conduit
 * and populates bi_psci_version.  We just verify the version is valid
 * and mark PSCI as usable.
 */
int
psci_init(struct xboot_info *bi)
{
	if (bi == NULL) {
		return (-1);
	}

	if (bi->bi_psci_version & 0x80000000) {
		return (-1);
	}

	psci_initialized = B_TRUE;
	return (0);
}

void
psci_system_off(void)
{
	if (psci_initialized != B_TRUE) {
		for (;;)
			/* spin forever */;
	}

	dboot_smccc_call(psci_system_off_id, 0, 0, 0);
}

void
psci_system_reset(void)
{
	if (psci_initialized != B_TRUE) {
		for (;;)
			/* spin forever */;
	}

	dboot_smccc_call(psci_system_reset_id, 0, 0, 0);
}
