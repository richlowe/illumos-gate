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
 * PCC Type 3: Extended PCC Master subspace.
 *
 * Type 3 is an initiator-only type (OSPM drives commands, platform responds).
 *
 * Type 3 uses the 16-byte extended shared memory header and the external
 * Command Complete and Error Status register model.
 *
 * See ACPI 6.6 §14.1.6 and Table 14.7.
 */

#include "pcc_impl.h"

/*
 * Extract type 3 fields from the ACPICA ACPI_PCCT_EXT_PCC_MASTER subtable into
 * the channel.
 */
static int
pcc_type3_parse(void *subtable, pcc_chan_t *pc)
{
	ACPI_PCCT_EXT_PCC_MASTER *sub = subtable;

	/*
	 * ACPI 6.6 §14.1.2: the firmware subspace ID is the subtable's index
	 * in the PCCT list.
	 */
	pc->pc_id = pc->pc_idx;

	if (pcc_validate_gas(
	    pc, "doorbell", &sub->DoorbellRegister) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	if (pcc_validate_gas(
	    pc, "ack", &sub->PlatformAckRegister) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	if (pcc_validate_gas(
	    pc, "cmd_complete", &sub->CmdCompleteRegister) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	if (pcc_validate_gas(
	    pc, "cmd_update", &sub->CmdUpdateRegister) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	if (pcc_validate_gas(
	    pc, "error_status", &sub->ErrorStatusRegister) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	/*
	 * ACPI 6.6 Table 14.7: the shared memory Length "Must be >= 16".
	 */
	if (sub->Length < PCC_EXT_SHMEM_HDR_LEN) {
		cmn_err(CE_WARN, "!acpipcc: Type 3 subspace %u has invalid "
		    "shared memory length %u", pc->pc_id, sub->Length);
		return (DDI_FAILURE);
	}

	/*
	 * The doorbell is required for the initiator (ACPI 6.6 Table 14.7).
	 */
	if (sub->DoorbellRegister.Address == 0) {
		cmn_err(CE_WARN, "!acpipcc: Type 3 subspace %u has no doorbell "
		    "register", pc->pc_id);
		return (DDI_FAILURE);
	}

	/*
	 * A zero doorbell WriteMask means that the doorbell can never be
	 * rung.
	 */
	if (sub->WriteMask == 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: doorbell WriteMask "
		    "is zero", pc->pc_id);
		return (DDI_FAILURE);
	}

	/*
	 * The Command Complete check and update registers are both required.
	 */
	if (sub->CmdCompleteRegister.Address == 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u has no Command "
		    "Complete check register", pc->pc_id);
		return (DDI_FAILURE);
	}

	if (sub->CmdUpdateRegister.Address == 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u has no Command "
		    "Complete update register", pc->pc_id);
		return (DDI_FAILURE);
	}

	pc->pc_shmem_pa = sub->BaseAddress;
	pc->pc_shmem_len = sub->Length;
	pc->pc_db_gas = sub->DoorbellRegister;
	pc->pc_db_preserve = sub->PreserveMask;
	pc->pc_db_set = sub->WriteMask;
	pc->pc_nominal_lat = sub->Latency;
	pc->pc_max_rate = sub->MaxAccessRate;
	pc->pc_turnaround = sub->MinTurnaroundTime;
	if (sub->PlatformAckRegister.Address != 0) {
		pc->pc_ack_gas = sub->PlatformAckRegister;
		pc->pc_ack_preserve = sub->AckPreserveMask;
		pc->pc_ack_set = sub->AckSetMask;
	}
	pc->pc_cc_check_gas = sub->CmdCompleteRegister;
	pc->pc_cc_check_mask = sub->CmdCompleteMask;

	/*
	 * A zero Command Complete check mask makes the ownership protocol
	 * impossible.
	 */
	if (pc->pc_cc_check_mask == 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: zero Command "
		    "Complete check mask", pc->pc_id);
		return (DDI_FAILURE);
	}

	pc->pc_cc_update_gas = sub->CmdUpdateRegister;
	pc->pc_cc_update_preserve = sub->CmdUpdatePreserveMask;
	pc->pc_cc_update_set = sub->CmdUpdateSetMask;
	if (sub->ErrorStatusRegister.Address != 0) {
		pc->pc_err_gas = sub->ErrorStatusRegister;
		pc->pc_err_mask = sub->ErrorStatusMask;
	}

	/*
	 * The set and check masks must not describe bits above the
	 * register width.
	 */
	if (pc->pc_db_gas.BitWidth == 32 && pc->pc_db_set > UINT32_MAX) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: doorbell WriteMask "
		    "0x%llx exceeds 32-bit register width",
		    pc->pc_id, (unsigned long long)pc->pc_db_set);
		return (DDI_FAILURE);
	}

	if (pc->pc_ack_gas.Address != 0 &&
	    pc->pc_ack_gas.BitWidth == 32 &&
	    pc->pc_ack_set > UINT32_MAX) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: ack SetMask "
		    "0x%llx exceeds 32-bit register width",
		    pc->pc_id, (unsigned long long)pc->pc_ack_set);
		return (DDI_FAILURE);
	}

	if (pc->pc_cc_update_gas.BitWidth == 32 &&
	    pc->pc_cc_update_set > UINT32_MAX) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: Command Complete "
		    "update SetMask 0x%llx exceeds 32-bit register width",
		    pc->pc_id, (unsigned long long)pc->pc_cc_update_set);
		return (DDI_FAILURE);
	}

	if (pc->pc_cc_check_gas.BitWidth == 32 &&
	    pc->pc_cc_check_mask > UINT32_MAX) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: Command Complete "
		    "check mask 0x%llx exceeds 32-bit register width",
		    pc->pc_id, (unsigned long long)pc->pc_cc_check_mask);
		return (DDI_FAILURE);
	}

	/*
	 * A width mismatch on the same physical register is a malformed
	 * firmware description.
	 */
	if (PCC_GAS_WIDTH_MISMATCH(pc->pc_cc_check_gas, pc->pc_cc_update_gas) ||
	    PCC_GAS_WIDTH_MISMATCH(pc->pc_err_gas, pc->pc_cc_check_gas) ||
	    PCC_GAS_WIDTH_MISMATCH(pc->pc_err_gas, pc->pc_cc_update_gas) ||
	    PCC_GAS_WIDTH_MISMATCH(pc->pc_db_gas, pc->pc_cc_check_gas) ||
	    PCC_GAS_WIDTH_MISMATCH(pc->pc_db_gas, pc->pc_cc_update_gas) ||
	    PCC_GAS_WIDTH_MISMATCH(pc->pc_db_gas, pc->pc_err_gas) ||
	    PCC_GAS_WIDTH_MISMATCH(pc->pc_db_gas, pc->pc_ack_gas) ||
	    PCC_GAS_WIDTH_MISMATCH(pc->pc_ack_gas, pc->pc_cc_check_gas) ||
	    PCC_GAS_WIDTH_MISMATCH(pc->pc_ack_gas, pc->pc_cc_update_gas) ||
	    PCC_GAS_WIDTH_MISMATCH(pc->pc_ack_gas, pc->pc_err_gas)) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: same physical "
		    "register described at different widths", pc->pc_id);
		return (DDI_FAILURE);
	}

	/*
	 * Partially overlapping registers are not allowed.
	 */
	if (PCC_GAS_OVERLAPS(pc->pc_cc_check_gas, pc->pc_cc_update_gas) ||
	    PCC_GAS_OVERLAPS(pc->pc_err_gas, pc->pc_cc_check_gas) ||
	    PCC_GAS_OVERLAPS(pc->pc_err_gas, pc->pc_cc_update_gas) ||
	    PCC_GAS_OVERLAPS(pc->pc_db_gas, pc->pc_cc_check_gas) ||
	    PCC_GAS_OVERLAPS(pc->pc_db_gas, pc->pc_cc_update_gas) ||
	    PCC_GAS_OVERLAPS(pc->pc_db_gas, pc->pc_err_gas) ||
	    PCC_GAS_OVERLAPS(pc->pc_db_gas, pc->pc_ack_gas) ||
	    PCC_GAS_OVERLAPS(pc->pc_ack_gas, pc->pc_cc_check_gas) ||
	    PCC_GAS_OVERLAPS(pc->pc_ack_gas, pc->pc_cc_update_gas) ||
	    PCC_GAS_OVERLAPS(pc->pc_ack_gas, pc->pc_err_gas)) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: PCC registers "
		    "partially overlap", pc->pc_id);
		return (DDI_FAILURE);
	}

	/*
	 * ACPI 6.6 Table 14.7 allows the Error Status register to be the same
	 * register as the Command Complete check register, but the two masks
	 * must not overlap.
	 */
	if (pc->pc_err_gas.Address != 0 &&
	    pc->pc_err_gas.Address == pc->pc_cc_check_gas.Address &&
	    pc->pc_err_gas.SpaceId == pc->pc_cc_check_gas.SpaceId &&
	    (pc->pc_err_mask & pc->pc_cc_check_mask) != 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: Error Status mask "
		    "overlaps Command Complete check mask on the same register",
		    pc->pc_id);
		return (DDI_FAILURE);
	}

	/*
	 * Table 14.7 sanctions exactly one register aliasing for Error Status,
	 * the Command Complete check register (with non-overlapping masks, as
	 * checked above).
	 */
	if (pc->pc_err_gas.Address != 0 &&
	    pc->pc_err_gas.Address == pc->pc_cc_update_gas.Address &&
	    pc->pc_err_gas.SpaceId == pc->pc_cc_update_gas.SpaceId &&
	    (pc->pc_cc_update_gas.Address != pc->pc_cc_check_gas.Address ||
	    pc->pc_cc_update_gas.SpaceId != pc->pc_cc_check_gas.SpaceId)) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: Error Status register "
		    "aliases the Command Complete update register", pc->pc_id);
		return (DDI_FAILURE);
	}

	/*
	 * Table 14.7 sanctions no aliasing for the doorbell or ack registers.
	 */
	if (PCC_GAS_ALIASES(pc->pc_db_gas, pc->pc_cc_check_gas) ||
	    PCC_GAS_ALIASES(pc->pc_db_gas, pc->pc_cc_update_gas) ||
	    PCC_GAS_ALIASES(pc->pc_db_gas, pc->pc_err_gas) ||
	    PCC_GAS_ALIASES(pc->pc_db_gas, pc->pc_ack_gas) ||
	    PCC_GAS_ALIASES(pc->pc_ack_gas, pc->pc_cc_check_gas) ||
	    PCC_GAS_ALIASES(pc->pc_ack_gas, pc->pc_cc_update_gas) ||
	    PCC_GAS_ALIASES(pc->pc_ack_gas, pc->pc_err_gas)) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: doorbell or ack "
		    "register aliases another PCC register", pc->pc_id);
		return (DDI_FAILURE);
	}

	pc->pc_gsiv = sub->PlatformInterrupt;
	pc->pc_irq_flags = sub->Flags;
	pc->pc_hdr_len = PCC_EXT_SHMEM_HDR_LEN;
	pc->pc_signature = PCC_SIGNATURE_BASE | pc->pc_id;

	return (DDI_SUCCESS);
}

/*
 * Create a registry entry for one register iff it is present.
 */
static int
pcc_type3_map_one(const ACPI_GENERIC_ADDRESS *gas, pcc_reg_entry_t **regp)
{
	if (gas->Address == 0) {
		*regp = NULL;
		return (DDI_SUCCESS);
	}

	*regp = pcc_reg_lookup_or_create(gas);
	if (*regp == NULL) {
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * Create register registry entries for the channel.
 */
static int
pcc_type3_map(pcc_chan_t *pc)
{
	if (pcc_type3_map_one(&pc->pc_db_gas,
	    &pc->pc_db_reg) != DDI_SUCCESS) {
		goto fail;
	}

	if (pcc_type3_map_one(&pc->pc_ack_gas,
	    &pc->pc_ack_reg) != DDI_SUCCESS) {
		goto fail;
	}

	if (pcc_type3_map_one(&pc->pc_cc_check_gas,
	    &pc->pc_cc_check_reg) != DDI_SUCCESS) {
		goto fail;
	}

	if (pcc_type3_map_one(&pc->pc_cc_update_gas,
	    &pc->pc_cc_update_reg) != DDI_SUCCESS) {
		goto fail;
	}

	if (pcc_type3_map_one(&pc->pc_err_gas,
	    &pc->pc_err_reg) != DDI_SUCCESS) {
		goto fail;
	}

	return (DDI_SUCCESS);

fail:
	pcc_reg_release(&pc->pc_err_reg);
	pcc_reg_release(&pc->pc_cc_update_reg);
	pcc_reg_release(&pc->pc_cc_check_reg);
	pcc_reg_release(&pc->pc_ack_reg);
	pcc_reg_release(&pc->pc_db_reg);
	return (DDI_FAILURE);
}

/*
 * Release all register registry entries for the channel.
 */
static void
pcc_type3_unmap(pcc_chan_t *pc)
{
	pcc_reg_release(&pc->pc_err_reg);
	pcc_reg_release(&pc->pc_cc_update_reg);
	pcc_reg_release(&pc->pc_cc_check_reg);
	pcc_reg_release(&pc->pc_ack_reg);
	pcc_reg_release(&pc->pc_db_reg);
}

/*
 * Initialise a type 3 channel.
 */
static int
pcc_type3_init(pcc_chan_t *pc)
{
	uint64_t ccval;
	hrtime_t timeout_ns;
	hrtime_t expire;

	pcc_reg_read(pc->pc_cc_check_reg, &ccval);

	if ((ccval & pc->pc_cc_check_mask) == 0) {
		/*
		 * Platform still owns the channel.  Wait a bounded time for
		 * the platform to release it.
		 */
		timeout_ns = pcc_cmd_timeout(pc);

		expire = gethrtime() + timeout_ns;
		for (;;) {
			pcc_reg_read(pc->pc_cc_check_reg, &ccval);
			if ((ccval & pc->pc_cc_check_mask) != 0) {
				break;
			}

			if (gethrtime() >= expire) {
				cmn_err(CE_WARN, "!acpipcc: chan %u: Command "
				    "Complete still clear after init timeout",
				    pc->pc_id);
				return (PCC_INIT_UNUSABLE);
			}

			drv_usecwait(PCC_POLL_INTERVAL_US);
		}
	}

	return (DDI_SUCCESS);
}

/*
 * Begin a type 3 transaction.
 *
 * Establish that OSPM owns the communication space: Command Complete
 * must be set (the platform has released the channel) before the
 * consumer writes a payload.
 */
static int
pcc_type3_txn_begin(pcc_chan_t *pc)
{
	uint64_t ccval;

	pcc_reg_read(pc->pc_cc_check_reg, &ccval);
	if ((ccval & pc->pc_cc_check_mask) == 0) {
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * Commit a type 3 transaction.
 *
 * Channel ownership was established by pcc_type3_txn_begin.  The
 * consumer payload is already in the communication space (offset 16+).
 * This function writes the 16 byte header and transfers ownership to
 * the platform.
 */
static int
pcc_type3_txn_commit(pcc_chan_t *pc, uint32_t cmd, uint32_t payload_len)
{
	volatile uint32_t *flagsp;
	volatile uint32_t *lenp;
	volatile uint32_t *cmdp;
	boolean_t irq;

	/*
	 * 1. Payload must fit in the Communication Space.
	 */
	if (payload_len > pc->pc_shmem_len - pc->pc_hdr_len) {
		return (DDI_FAILURE);
	}

	/*
	 * 2. Clear stale error bits.
	 */
	if (pc->pc_err_reg != NULL) {
		pcc_reg_rmw(pc->pc_err_reg, ~pc->pc_err_mask, 0);
	}

	/*
	 * 3. Write Flags.  Notify on completion iff we have an interrupt.
	 */
	mutex_enter(&pc->pc_state_lock);
	irq = pc->pc_has_irq;
	mutex_exit(&pc->pc_state_lock);

	flagsp = (volatile uint32_t *)(void *)(pc->pc_shmem_va +
	    PCC_EXT_SHMEM_FLAGS);
	if (irq) {
		*flagsp = PCC_EXT_FLAG_NOTIFY_ON_COMPLETE;
	} else {
		*flagsp = 0;
	}

	/*
	 * 4. Write Length: size of Command field plus payload.
	 *
	 * See ACPI 6.6 Table 14.12.
	 */
	lenp = (volatile uint32_t *)(void *)(pc->pc_shmem_va +
	    PCC_EXT_SHMEM_LENGTH);
	*lenp = (uint32_t)sizeof (uint32_t) + payload_len;

	/*
	 * 5. Write Command.
	 */
	cmdp = (volatile uint32_t *)(void *)(pc->pc_shmem_va +
	    PCC_EXT_SHMEM_CMD);
	*cmdp = cmd;

	/*
	 * 6. Ensure all shared-memory writes are observable by the
	 * platform before ownership is transferred (ACPI 6.6 §14.5).
	 */
	membar_producer();

	/*
	 * 7. Clear Command Complete, transferring ownership to the platform.
	 */
	pcc_reg_rmw(pc->pc_cc_update_reg, pc->pc_cc_update_preserve,
	    pc->pc_cc_update_set);

	/*
	 * Doorbell is rung by common code after we return.
	 */
	return (DDI_SUCCESS);
}

/*
 * Poll for command completion.
 */
static int
pcc_type3_poll_complete(pcc_chan_t *pc)
{
	uint64_t ccval;
	hrtime_t deadline;
	hrtime_t timeout;

	timeout = pcc_cmd_timeout(pc);

	deadline = gethrtime() + timeout;
	for (;;) {
		pcc_reg_read(pc->pc_cc_check_reg, &ccval);
		if ((ccval & pc->pc_cc_check_mask) != 0) {
			return (DDI_SUCCESS);
		}

		if (gethrtime() >= deadline) {
			cmn_err(CE_WARN, "!acpipcc: subspace %u: command "
			    "completion timeout", pc->pc_id);
			return (DDI_FAILURE);
		}

		drv_usecwait(PCC_POLL_INTERVAL_US);
	}
}

/*
 * Check Error Status after command completion.
 *
 * ACPI 6.6 Table 14.7: "Error Status needs to be checked after completion
 * status indicates issued command has been completed."
 */
static int
pcc_type3_check_error(pcc_chan_t *pc)
{
	uint64_t val;

	if (pc->pc_err_mask == 0 || pc->pc_err_reg == NULL) {
		return (DDI_SUCCESS);
	}

	pcc_reg_read(pc->pc_err_reg, &val);
	if ((val & pc->pc_err_mask) != 0) {
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * Peek at the Command Complete check register to determine interrupt ownership.
 *
 * Returns PCC_IRQ_CMD_COMPLETE in `result' if the Command Complete bit is set.
 */
static int
pcc_type3_irq_check(pcc_chan_t *pc, pcc_irq_result_t *result)
{
	uint64_t val;

	pcc_reg_read(pc->pc_cc_check_reg, &val);

	*result = PCC_IRQ_NONE;
	if ((val & pc->pc_cc_check_mask) != 0) {
		*result |= PCC_IRQ_CMD_COMPLETE;
	}

	return (DDI_SUCCESS);
}

static void
pcc_type3_clear_platform_irq(pcc_chan_t *pc __unused)
{
	/*
	 * Type 3 has no Status field bit to clear - an Ack register write
	 * clears the hardware interrupt source.
	 */
}

const pcc_type_ops_t pcc_type3_ops = {
	.pto_parse = pcc_type3_parse,
	.pto_map = pcc_type3_map,
	.pto_init = pcc_type3_init,
	.pto_unmap = pcc_type3_unmap,
	.pto_txn_begin = pcc_type3_txn_begin,
	.pto_txn_commit = pcc_type3_txn_commit,
	.pto_poll_complete = pcc_type3_poll_complete,
	.pto_check_error = pcc_type3_check_error,
	.pto_clear_platform_irq = pcc_type3_clear_platform_irq,
	.pto_irq_check = pcc_type3_irq_check,
	.pto_edge_only_irq = B_FALSE,
};
