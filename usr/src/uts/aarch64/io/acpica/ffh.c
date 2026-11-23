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
 * Arm Functional Fixed Hardware (FFH) - DEN0048D v1.3.
 *
 * Provides the call dispatch layer for FFH OpRegion fields and AMU counter
 * access for CPPC.  Sits on top of the SMCCC and FF-A transports, and is
 * consumed by an ACPICA address space handler.
 *
 * OpRegion BufferAcc writes trigger firmware calls:
 *   Offset 0 - SMC32: buffer is uint32_t[N], N in [1,8].
 *   Offset 1 - SMC64: buffer is uint64_t[N], N in [1,18].
 *   Offset 2 - FF-A DIRECT_REQ2: buffer is uint64_t[M], M in [5,18].
 *
 * For offsets 0 and 1, the FID in the first register must be in the
 * SMCCC SiP or OEM service call range (DEN0048D §2.3.1.1).
 *
 * For offset 2, the handler converts the UUID from bytes in the ACPI UUID
 * buffer format to the format expected by FF-A, resolves the receiver
 * endpoint if needed, delegates the call to ffa_direct_req2_raw and maps
 * completion status per DEN0048D Table 3.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sunddi.h>
#include <sys/cpuid.h>
#include <sys/cmn_err.h>
#include <sys/uuid.h>
#include <sys/smccc.h>
#include <sys/ffa.h>
#include "ffh.h"

static boolean_t ffh_amu_present = B_FALSE;

void
ffh_init(void)
{
	uint64_t pfr0 = read_id_aa64pfr0();

	if (PFR0_AMU(pfr0) >= 0x1) {
		ffh_amu_present = B_TRUE;
	}
}

/*
 * FID validation for SMC32/SMC64 OpRegion calls (DEN0048D §2.3.1.1).
 *
 * Only SiP and OEM service call ranges are permitted.  FF-A FIDs
 * (Standard Secure Service, owner 4) are also allowed per the spec's
 * note about "FF-A specific Function Identifiers".
 */
static boolean_t
ffh_fid_allowed(uint32_t fid)
{
	uint32_t owner = SMCCC_FID_OWNER(fid);

	return (owner == SMCCC_FID_OWNER_SIP ||
	    owner == SMCCC_FID_OWNER_OEM ||
	    owner == SMCCC_FID_OWNER_STD_SEC);
}

/*
 * SMC32 OpRegion call (offset 0).
 *
 * buf points to N consecutive uint32_t values representing W0-W(N-1).
 * len must be 4*N where 1 <= N <= 8.
 */
static int
ffh_call_smc32(void *buf, size_t len)
{
	smccc32_args_t args = {
		.w = { 0 },
	};
	uint32_t *regs = (uint32_t *)buf;
	uint_t nregs;
	uint_t i;

	ASSERT3P(buf, !=, NULL);

	if (len == 0 || len > FFH_SMC32_MAX_BYTES || (len % 4) != 0) {
		return (EINVAL);
	}

	nregs = len / 4;

	if (!ffh_fid_allowed(regs[0])) {
		/*
		 * Per DEN0048D §2.3.1.1: return a negative SMCCC error
		 * in W0 when the FID is outside the allowed ranges.
		 */
		regs[0] = (uint32_t)SMCCC_NOT_SUPPORTED;
		return (0);
	}

	for (i = 0; i < nregs; i++) {
		args.w[i] = regs[i];
	}

	if (smccc32_call(&args) != DDI_SUCCESS) {
		return (EIO);
	}

	for (i = 0; i < nregs; i++) {
		regs[i] = args.w[i];
	}

	return (0);
}

/*
 * SMC64 OpRegion call (offset 1).
 *
 * buf points to N consecutive uint64_t values representing X0-X(N-1).
 * len must be 8*N where 1 <= N <= 18.
 */
static int
ffh_call_smc64(void *buf, size_t len)
{
	smccc64_args_t args = {
		.x = { 0 },
	};
	uint64_t *regs = (uint64_t *)buf;
	uint_t nregs;
	uint_t i;

	ASSERT3P(buf, !=, NULL);

	if (len == 0 || len > FFH_SMC64_MAX_BYTES || (len % 8) != 0) {
		return (EINVAL);
	}

	nregs = len / 8;

	if (!ffh_fid_allowed((uint32_t)regs[0])) {
		regs[0] = (uint64_t)(int64_t)SMCCC_NOT_SUPPORTED;
		return (0);
	}

	for (i = 0; i < nregs; i++) {
		args.x[i] = regs[i];
	}

	if (smccc64_call(&args) != DDI_SUCCESS) {
		return (EIO);
	}

	for (i = 0; i < nregs; i++) {
		regs[i] = args.x[i];
	}

	return (0);
}

/*
 * Convert a UUID from a buffer in ACPI ToUUID byte order to native uuid_t.
 *
 * ACPI's ToUUID macro produces a 16-byte buffer in RFC 4122 mixed-endian
 * wire format:
 *   bytes 0-3 :   time_low         (little-endian)
 *   bytes 4-5 :   time_mid         (little-endian)
 *   bytes 6-7 :   time_hi_version  (little-endian)
 *   bytes 8-15:   clock_seq + node (network order)
 *
 * uuid_t is a flat 16-byte array in RFC 4122 canonical (big-endian)
 * order: the first three fields are byte-swapped relative to the
 * ACPI representation.
 */
static void
ffh_acpi_uuid_to_native(const uint8_t *acpi, uuid_t *uuid)
{
	uint8_t *u = (uint8_t *)uuid;

	ASSERT3P(acpi, !=, NULL);
	ASSERT3P(uuid, !=, NULL);

	/* Reverse LE time_low (bytes 0-3) to BE */
	u[0] = acpi[3];
	u[1] = acpi[2];
	u[2] = acpi[1];
	u[3] = acpi[0];

	/* Reverse LE time_mid (bytes 4-5) to BE */
	u[4] = acpi[5];
	u[5] = acpi[4];

	/* Reverse LE time_hi_and_version (bytes 6-7) to BE */
	u[6] = acpi[7];
	u[7] = acpi[6];

	/* clock_seq and node (bytes 8-15) are already in network order */
	(void) memcpy(&u[8], &acpi[8], 8);
}

/*
 * Map an ffa_partition_lookup() errno to an FFH pre-call status code.
 */
static int64_t
ffh_ffa_resolve_status(int err)
{
	switch (err) {
	case ENOTSUP:	/* fallthrough */
	case ENXIO:
		return (FFH_FFA_NOT_SUPPORTED);
	case EINVAL:	/* fallthrough */
	case ENOENT:
		return (FFH_FFA_INVALID_PARAMETERS);
	case ENOMEM:
		return (FFH_FFA_OUT_OF_MEMORY);
	default:
		return (FFH_FFA_UNSPECIFIED_ERROR);
	}
}

/*
 * Copy FF-A result registers back to the OpRegion buffer.
 */
static void
ffh_ffa_copy_results(uint64_t *regs, uint_t nregs,
    const uint64_t *raw, int64_t status)
{
	uint_t i;

	ASSERT3P(regs, !=, NULL);
	ASSERT3P(raw, !=, NULL);

	regs[0] = (uint64_t)status;
	for (i = 1; i < nregs; i++) {
		regs[i] = raw[i];
	}
}

/*
 * FF-A DIRECT_REQ2 OpRegion call (offset 2).
 *
 * The buffer layout is uint64_t fields representing X0-X17.
 * Minimum length is 5 registers (X0-X4), 32 + 8 = 40 bytes.
 * Maximum is 18 registers (X0-X17), 32 + 14*8 = 144 bytes.
 *
 * On entry, the buffer contains:
 *   [0] X0: unused (handler sets FID)
 *   [1] X1: bits[15:0] = receiver endpoint ID (0 = resolve from UUID)
 *   [2] X2: UUID bytes 0-7 in ToUUID format
 *   [3] X3: UUID bytes 8-15 in ToUUID format
 *   [4..N] X4-X17: payload registers
 *
 * On return, the buffer contains:
 *   [0] X0: FFH status code per DEN0048D Table 3
 *   [1..N]: register contents from the firmware response
 */
static int
ffh_call_ffa_direct_req2(void *buf, size_t len)
{
	uint64_t raw[FFA_DIRECT_REQ2_NREGS];
	uint64_t *regs = (uint64_t *)buf;
	uint_t nregs;
	uint16_t receiver_id;
	uuid_t uuid;
	uint32_t fid;
	int ret;

	ASSERT3P(buf, !=, NULL);

	/* Minimum 5 registers (X0-X4), must be 8-byte aligned */
	if (len < (FFH_FFA_HDR_BYTES + 8) || len > FFH_FFA_MAX_BYTES ||
	    (len % 8) != 0) {
		return (EINVAL);
	}

	if (!ffa_available()) {
		return (ENXIO);
	}

	nregs = len / 8;

	/*
	 * Convert the UUID from ACPI ToUUID format to native uuid_t.
	 * The UUID occupies regs[2] and regs[3] (X2-X3).
	 */
	ffh_acpi_uuid_to_native((const uint8_t *)&regs[2], &uuid);

	receiver_id = (uint16_t)(regs[1] & 0xFFFF);

	/*
	 * If the receiver endpoint ID is zero, resolve it from the
	 * service UUID via the FF-A partition info interface.
	 */
	if (receiver_id == 0) {
		ret = ffa_partition_lookup(&uuid, &receiver_id);
		if (ret != 0) {
			regs[0] = (uint64_t)ffh_ffa_resolve_status(ret);
			return (0);
		}
	}

	ret = ffa_direct_req2_raw(receiver_id, &uuid, &regs[4], raw,
	    nregs - 4);
	if (ret != 0) {
		return (ret);
	}

	/*
	 * Map the FF-A return to DEN0048D Table 3 status codes.
	 */
	fid = (uint32_t)raw[0];

	if (fid == FFA_ERROR) {
		ffh_ffa_copy_results(regs, nregs, raw,
		    FFH_FFA_CALL_FAILED);
	} else if (fid == FFA_SUCCESS32 || fid == FFA_SUCCESS64 ||
	    fid == FFA_MSG_SEND_DIRECT_RESP2) {
		ffh_ffa_copy_results(regs, nregs, raw, FFH_FFA_SUCCESS);
	} else {
		ffh_ffa_copy_results(regs, nregs, raw,
		    FFH_FFA_UNSPECIFIED_ERROR);
	}

	return (0);
}

/*
 * FFH OpRegion write dispatch.
 *
 * Called from the ACPICA address space handler when AML writes to an
 * FFH BufferAcc field.  offset is the OperationRegion Offset (0, 1,
 * or 2).  buf/len describe the register buffer.
 */
int
ffh_opregion_write(uint_t offset, void *buf, size_t len)
{
	ASSERT3P(buf, !=, NULL);

	switch (offset) {
	case FFH_OFFSET_SMC32:
		return (ffh_call_smc32(buf, len));
	case FFH_OFFSET_SMC64:
		return (ffh_call_smc64(buf, len));
	case FFH_OFFSET_FFA_DIRECT_REQ2:
		return (ffh_call_ffa_direct_req2(buf, len));
	default:
		return (ENOTSUP);
	}
}

/*
 * AMU event counter read (DEN0048D §2.2.1).
 *
 * Reads the specified AMU event counter via MRS.
 *   addr 0x0: AMEVCNTR00_EL0 - delivered performance counter.
 *   addr 0x1: AMEVCNTR01_EL0 - reference performance counter.
 */
int
ffh_amu_read(uint64_t addr, uint64_t *val)
{
	ASSERT3P(val, !=, NULL);

	if (!ffh_amu_present) {
		return (ENXIO);
	}

	switch (addr) {
	case FFH_AMU_DELIVERED_PERF:
		*val = read_amevcntr00();
		return (0);
	case FFH_AMU_REFERENCE_PERF:
		*val = read_amevcntr01();
		return (0);
	default:
		return (EINVAL);
	}
}
