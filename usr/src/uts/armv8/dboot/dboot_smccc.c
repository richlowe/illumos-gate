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

/*
 * Copyright 2026 Michael van der Westhuizen
 */

/*
 * SMCCC transport layer for dboot.
 *
 * Provides SMC/HVC trampolines and SMCCC version discovery.
 *
 * This runs very early during dboot when no kernel services are available.
 * We use inline asm directly rather than the separate assembly trampolines
 * used by the kernel proper, as this implementation is really all about
 * discovery, not features.
 *
 * Discovery sequence:
 * 1. Issue PSCI_VERSION via the conduit from bootinfo.
 *    If this fails, the conduit is broken: no firmware calls are possible.
 * 2. Issue PSCI_FEATURES(SMCCC_VERSION) to check for SMCCC 1.1+ support.
 *    SMCCC 1.0 means "just the basic PSCI conduit", and does not include
 *    SMCCC_VERSION.
 * 3. If PSCI_FEATURES succeeds, issue SMCCC_VERSION to get
 *    the actual SMCCC version, which will be 1.1+.
 *
 * Populates bi_psci_version and bi_smccc_version in xboot_info.
 */

#include <sys/types.h>
#include <sys/null.h>
#include <sys/bootinfo.h>
#include <sys/smccc.h>

#include "dboot.h"
#include "dboot_printf.h"

static boolean_t dboot_smccc_is_hvc = B_FALSE;

static inline uint64_t
dboot_smccc_smc(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
	register uint64_t x0 __asm__("x0") = a0;
	register uint64_t x1 __asm__("x1") = a1;
	register uint64_t x2 __asm__("x2") = a2;
	register uint64_t x3 __asm__("x3") = a3;

	__asm__ volatile("smc #0"
	    : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
	    :
	    :
	    "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11",
	    "x12", "x13", "x14", "x15", "x16", "x17", "x18", "memory", "cc");

	return (x0);
}

static inline uint64_t
dboot_smccc_hvc(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
	register uint64_t x0 __asm__("x0") = a0;
	register uint64_t x1 __asm__("x1") = a1;
	register uint64_t x2 __asm__("x2") = a2;
	register uint64_t x3 __asm__("x3") = a3;

	__asm__ volatile("hvc #0"
	    : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
	    :
	    :
	    "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11",
	    "x12", "x13", "x14", "x15", "x16", "x17", "x18", "memory", "cc");

	return (x0);
}

/*
 * Issue an SMCCC call in dboot context.  Exported so that dboot_psci.c
 * can use it for PSCI firmware calls.
 */
uint64_t
dboot_smccc_call(uint64_t fid, uint64_t a1, uint64_t a2, uint64_t a3)
{
	if (dboot_smccc_is_hvc) {
		return (dboot_smccc_hvc(fid, a1, a2, a3));
	} else {
		return (dboot_smccc_smc(fid, a1, a2, a3));
	}
}

/*
 * Discover the SMCCC version.
 *
 * Returns 0 on success, -1 if the conduit doesn't work at all.
 *
 * On success, bi_smccc_version is set (1.0 if only PSCI-level, i.e., no
 * SMCCC 1.1+ discovery).
 */
int
smccc_init(struct xboot_info *bi)
{
	uint32_t psci_ver;
	uint32_t feat_ret;
	uint32_t smccc_ver;

	if (bi == NULL) {
		return (-1);
	}

	dboot_smccc_is_hvc =
	    (bi->bi_smccc_conduit == BI_SMCCC_CONDUIT_HVC) ? B_TRUE : B_FALSE;

	/*
	 * Step 1: PSCI_VERSION: validate that the conduit works.
	 */
	psci_ver = (uint32_t)dboot_smccc_call(SMCCC_PSCI_VERSION_FID,
	    0, 0, 0);
	if (psci_ver & 0x80000000) {
		dprintf("smccc_init: PSCI_VERSION failed (0x%x)\n", psci_ver);
		bi->bi_smccc_version = 0;
		return (-1);
	}

	dprintf("smccc_init: PSCI version %d.%d\n",
	    (psci_ver >> 16) & 0x7FFF, psci_ver & 0xFFFF);
	bi->bi_psci_version = psci_ver;

	/*
	 * Step 2: PSCI_FEATURES(SMCCC_VERSION): check if firmware
	 * supports the SMCCC_VERSION call.  If not, then this is
	 * just a basic PSCI implementation.
	 */
	feat_ret = (uint32_t)dboot_smccc_call(SMCCC_PSCI_FEATURES_FID,
	    SMCCC_VERSION_FID, 0, 0);
	if (feat_ret & 0x80000000) {
		/*
		 * No SMCCC 1.1+ support.  The conduit works (PSCI is
		 * reachable) but there's no standalone SMCCC version.
		 * This is fine, PSCI can still use the conduit directly.
		 */
		dprintf("smccc_init: PSCI_FEATURES(SMCCC_VERSION) not "
		    "supported (0x%x), PSCI-only mode\n", feat_ret);
		bi->bi_smccc_version = 0x00010000;
		return (0);
	}

	/*
	 * Step 3: SMCCC_VERSION: get the actual version.
	 */
	smccc_ver = (uint32_t)dboot_smccc_call(SMCCC_VERSION_FID, 0, 0, 0);
	if (smccc_ver & 0x80000000) {
		dprintf("smccc_init: SMCCC_VERSION call failed (0x%x)\n",
		    smccc_ver);
		bi->bi_smccc_version = 0x00010000;
		return (0);
	}

	dprintf("smccc_init: SMCCC version %d.%d\n",
	    (smccc_ver >> 16) & 0x7FFF, smccc_ver & 0xFFFF);
	bi->bi_smccc_version = smccc_ver;

	return (0);
}
