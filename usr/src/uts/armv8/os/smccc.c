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
 * Kernel SMCCC transport layer.
 *
 * Reconstructs state from xboot_info at smccc_init time and provides
 * smccc32_call/smccc64_call, which dispatch to the width-appropriate
 * assembly trampolines.
 */

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/systm.h>
#include <sys/bootinfo.h>
#include <sys/smccc.h>
#include <sys/promif.h>

/*
 * Assembly trampoline prototypes (smccc_impl.S).
 */
extern void i_smccc_smc_call32(smccc32_args_t *);
extern void i_smccc_smc_call64(smccc64_args_t *);
extern void i_smccc_hvc_call32(smccc32_args_t *);
extern void i_smccc_hvc_call64(smccc64_args_t *);

static boolean_t smccc_inited = B_FALSE;
static boolean_t smccc_is_hvc = B_FALSE;
static uint32_t smccc_ver = 0;

/*
 * Initialize the kernel SMCCC layer.
 *
 * Called from the unix boot path after xboot_info has been populated by
 * dboot.
 *
 * The conduit type and SMCCC version were already discovered during dboot
 * and stored in xboot_info.
 */
int
smccc_init(struct xboot_info *xbp)
{
	if (xbp == NULL) {
		return (DDI_FAILURE);
	}

	if (smccc_inited) {
		return (DDI_SUCCESS);
	}

	smccc_is_hvc =
	    (xbp->bi_smccc_conduit == BI_SMCCC_CONDUIT_HVC) ? B_TRUE : B_FALSE;
	smccc_ver = xbp->bi_smccc_version;
	smccc_inited = B_TRUE;

	return (DDI_SUCCESS);
}

/*
 * Common transport checks for smccc32_call and smccc64_call.
 *
 * Returns DDI_SUCCESS if the call may proceed, DDI_FAILURE if SMCCC is
 * not initialized, or DDI_ETRANSPORT during panic when not initialized.
 */
static int
smccc_check(void)
{
	/*
	 * Panic-path safety: if we're panicking and SMCCC isn't up,
	 * return gracefully rather than recursively panicking.
	 */
	if (!smccc_inited) {
		if (panicstr != NULL) {
			prom_printf("WARNING: SMCCC call during panic "
			    "before initialization\n");
			return (DDI_ETRANSPORT);
		}

		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * Issue a 32-bit SMCCC call via the appropriate trampoline.
 *
 * The conduit (SMC/HVC) is chosen based on smccc_is_hvc.
 *
 * Clients pass a populated smccc32_args_t with the function ID in
 * args->w[0].  Results from firmware are written back to the passed
 * structure for interpretation by the caller.
 *
 * Returns DDI_SUCCESS on successful dispatch, DDI_FAILURE if SMCCC is
 * not initialized, or DDI_ETRANSPORT during panic when not initialized.
 */
int
smccc32_call(smccc32_args_t *args)
{
	int rv;

	rv = smccc_check();
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if (smccc_is_hvc) {
		i_smccc_hvc_call32(args);
	} else {
		i_smccc_smc_call32(args);
	}

	return (DDI_SUCCESS);
}

/*
 * Issue a 64-bit SMCCC call via the appropriate trampoline.
 *
 * The conduit (SMC/HVC) is chosen based on smccc_is_hvc.
 *
 * Clients pass a populated smccc64_args_t with the function ID in
 * args->x[0].  Results from firmware are written back to the passed
 * structure for interpretation by the caller.
 *
 * Returns DDI_SUCCESS on successful dispatch, DDI_FAILURE if SMCCC is
 * not initialized, or DDI_ETRANSPORT during panic when not initialized.
 */
int
smccc64_call(smccc64_args_t *args)
{
	int rv;

	rv = smccc_check();
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if (smccc_is_hvc) {
		i_smccc_hvc_call64(args);
	} else {
		i_smccc_smc_call64(args);
	}

	return (DDI_SUCCESS);
}

/*
 * Returns true if full SMCCC (1.1+) is available.
 */
boolean_t
smccc_available(void)
{
	return (smccc_inited && smccc_ver > 0x00010000);
}

/*
 * Return the negotiated SMCCC version, or 1.0 if PSCI-only.
 */
uint32_t
smccc_version(void)
{
	return (smccc_ver);
}
