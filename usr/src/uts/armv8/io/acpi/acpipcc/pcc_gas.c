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
 * PCC Generic Address Structure (GAS) validation, shared by the subspace
 * type parsers.
 */

#include "pcc_impl.h"

/*
 * Validate a PCC register Generic Address Structure (GAS) field.
 */
int
pcc_validate_gas(pcc_chan_t *pc, const char *name,
    ACPI_GENERIC_ADDRESS *gas)
{
	/*
	 * A zero address means the register is absent, which is not an error
	 * at this stage of processing.
	 */
	if (gas->Address == 0) {
		return (DDI_SUCCESS);
	}

	/*
	 * ACPI 6.6 §14.1.4/§14.1.5 and Table 14.7: only System I/O, System
	 * Memory and Functional Fixed Hardware spaces are valid for PCC
	 * register GAS Address_Space_ID values.
	 */
	switch (gas->SpaceId) {
	case ACPI_ADR_SPACE_SYSTEM_IO:		/* fallthrough */
	case ACPI_ADR_SPACE_FIXED_HARDWARE:	/* fallthrough */
	case ACPI_ADR_SPACE_SYSTEM_MEMORY:
		break;
	default:
		cmn_err(CE_WARN, "!acpipcc: subspace %u %s: invalid "
		    "Address_Space_ID 0x%x", pc->pc_id, name, gas->SpaceId);
		return (DDI_FAILURE);
	}

	switch (gas->BitWidth) {
	case 32:	/* fallthrough */
	case 64:
		break;
	default:
		cmn_err(CE_WARN, "!acpipcc: subspace %u %s: invalid "
		    "BitWidth %u", pc->pc_id, name, gas->BitWidth);
		return (DDI_FAILURE);
	}

	/*
	 * This _driver_ cannot address sub-register bit fields, as those
	 * are not representable in the register registry identity.
	 */
	if (gas->BitOffset != 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u %s: non-zero BitOffset "
		    "%u unsupported", pc->pc_id, name, gas->BitOffset);
		return (DDI_FAILURE);
	}

	/*
	 * A System Memory register must be naturally aligned to its
	 * width - a misaligned device access faults.
	 */
	if (gas->SpaceId == ACPI_ADR_SPACE_SYSTEM_MEMORY &&
	    gas->Address % (gas->BitWidth / 8) != 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u %s: address 0x%llx "
		    "not aligned to register width %u", pc->pc_id, name,
		    (unsigned long long)gas->Address, gas->BitWidth);
		return (DDI_FAILURE);
	}

	/*
	 * AccessSize (AccessWidth) must be consistent with BitWidth:
	 * 0 means undefined (any access), otherwise the access size
	 * must not exceed the register width.
	 */
	if (gas->BitWidth == 32) {
		if (gas->AccessWidth > 3) {
			cmn_err(CE_WARN, "!acpipcc: subspace %u %s: "
			    "AccessWidth %u inconsistent with 32-bit BitWidth",
			    pc->pc_id, name, gas->AccessWidth);
			return (DDI_FAILURE);
		}
	} else {
		ASSERT3U(gas->BitWidth, ==, 64);

		if (gas->AccessWidth > 4) {
			cmn_err(CE_WARN, "!acpipcc: subspace %u %s: "
			    "AccessWidth %u inconsistent with 64-bit BitWidth",
			    pc->pc_id, name, gas->AccessWidth);
			return (DDI_FAILURE);
		}
	}

	return (DDI_SUCCESS);
}
