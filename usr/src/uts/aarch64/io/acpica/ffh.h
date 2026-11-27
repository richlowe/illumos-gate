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

#ifndef _ACPICA_OSL_FFH_H
#define	_ACPICA_OSL_FFH_H

/*
 * Arm Functional Fixed Hardware (FFH) - DEN0048D v1.3.
 *
 * FFH defines how ACPI address space 0x7F is used on Arm systems for:
 *
 *   1. OpRegion fields that trigger SMCCC or FF-A calls (BufferAcc).
 *   2. GAS registers that read AMU performance counters (CPPC).
 *   3. LPI entry methods encoding PSCI power_state values.
 *
 * Case 3 is parsed directly by the cpuidle driver and does not pass
 * through the address space handler.  This module provides the call
 * dispatch for case 1 and the sysreg access for case 2.
 *
 * FFH sits on top of the SMCCC transport (sys/smccc.h) and the FF-A
 * transport (sys/ffa.h).  It is consumed by the ACPICA OSL address
 * space handler (osl_ffh.c) and, for AMU reads, by the future CPPC
 * driver.
 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * FFH OpRegion offsets (DEN0048D §2.3.1).
 *
 * The OperationRegion Offset field selects the calling convention:
 *   0 - SMC32 (W0-W7, 4*N bytes, 1 <= N <= 8)
 *   1 - SMC64 (X0-X17, 8*N bytes, 1 <= N <= 18)
 *   2 - FFA_MSG_SEND_DIRECT_REQ2 (32 + 8*N bytes, 1 <= N <= 14)
 */
#define	FFH_OFFSET_SMC32		0x0
#define	FFH_OFFSET_SMC64		0x1
#define	FFH_OFFSET_FFA_DIRECT_REQ2	0x2

/*
 * SMCCC Function ID owner field extraction (DEN0028 §5.1, Table 6-1).
 *
 * Bits[29:24] of the FID encode the owning entity.  DEN0048D §2.3.1.1
 * restricts FFH OpRegion calls to SiP and OEM service ranges (plus
 * FF-A FIDs, which use the Standard Secure Service range).
 */
#define	SMCCC_FID_OWNER_SHIFT		24
#define	SMCCC_FID_OWNER_MASK		0x3f
#define	SMCCC_FID_OWNER(fid)		\
	(((fid) >> SMCCC_FID_OWNER_SHIFT) & SMCCC_FID_OWNER_MASK)

#define	SMCCC_FID_OWNER_SIP		2	/* SiP Service Calls */
#define	SMCCC_FID_OWNER_OEM		3	/* OEM Service Calls */
#define	SMCCC_FID_OWNER_STD_SEC		4	/* Standard Secure Service */

/*
 * FFH OpRegion status codes for FF-A calls (DEN0048D Table 3).
 *
 * Returned in the first 64-bit field of the OpRegion buffer (X0
 * position) after an offset-2 operation.  Positive values indicate the
 * call was invoked; negative values indicate a pre-call failure.
 */
#define	FFH_FFA_CALL_FAILED		1
#define	FFH_FFA_SUCCESS			0
#define	FFH_FFA_NOT_SUPPORTED		(-1)
#define	FFH_FFA_INVALID_PARAMETERS	(-2)
#define	FFH_FFA_OUT_OF_MEMORY		(-3)
#define	FFH_FFA_UNSPECIFIED_ERROR	(-4)

/*
 * AMU counter register addresses for CPPC (DEN0048D §2.2.1).
 *
 * Used in GAS registers with AddressSpace 0x7F, BitWidth 64,
 * AccessSize 4 (Qword).  The CPPC driver calls ffh_amu_read
 * with the RegisterAddress value from the _CPC GAS entry.
 */
#define	FFH_AMU_DELIVERED_PERF		0x0	/* AMEVCNTR0_EL0[0] */
#define	FFH_AMU_REFERENCE_PERF		0x1	/* AMEVCNTR0_EL0[1] */

/*
 * FFH OpRegion buffer limits.
 *
 * These are derived from the Length constraints in DEN0048D §2.3.1.
 */
#define	FFH_SMC32_MAX_REGS		8	/* W0-W7 */
#define	FFH_SMC32_MAX_BYTES		(FFH_SMC32_MAX_REGS * 4)
#define	FFH_SMC64_MAX_REGS		18	/* X0-X17 */
#define	FFH_SMC64_MAX_BYTES		(FFH_SMC64_MAX_REGS * 8)
#define	FFH_FFA_HDR_BYTES		32	/* X0-X3 fixed header */
#define	FFH_FFA_MAX_PAYLOAD_REGS	14	/* X4-X17 */
#define	FFH_FFA_MAX_BYTES		\
	(FFH_FFA_HDR_BYTES + FFH_FFA_MAX_PAYLOAD_REGS * 8)

extern void ffh_init(void);

/*
 * FFH OpRegion call dispatch.
 *
 * Validates the buffer contents, issues the firmware call via the
 * SMCCC transport, and writes the results back into buf.  The buffer
 * layout corresponds directly to the register file: consecutive
 * uint32_t values for SMC32, consecutive uint64_t values for SMC64
 * and FF-A.
 *
 * Returns 0 on success (firmware call was issued; the firmware's own
 * return code is in buf[0]).  Returns an errno on pre-call failure
 * (e.g. bad FID, bad length).
 */
extern int ffh_opregion_write(uint_t offset, void *buf, size_t len);

/*
 * AMU counter read for CPPC (DEN0048D §2.2.1).
 *
 * Reads the specified AMU event counter via MRS and stores the 64-bit
 * result in *val.  addr is the GAS RegisterAddress value
 * (FFH_AMU_DELIVERED_PERF or FFH_AMU_REFERENCE_PERF).
 *
 * Returns 0 on success, EINVAL for an unknown address.
 */
extern int ffh_amu_read(uint64_t addr, uint64_t *val);

/*
 * Install the FFH address space handler for ACPI_ADR_SPACE_FIXED_HARDWARE.
 *
 * Called from acpica_install_handlers before AcpiEnableSubsystem.
 *
 * Returns AE_OK on success.
 */
extern int osl_ffh_install(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _ACPICA_OSL_FFH_H */
