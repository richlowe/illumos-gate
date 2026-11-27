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
 * PCC Types 1 and 2 (HW-Reduced Communications Subspaces).
 *
 * PCC types 1 and 2 (ACPI 6.6 §14.1.4 and §14.1.5) both use the generic
 * shared memory header described in §14.2.  Both notify the platform of
 * the presence of a message by ringing a doorbell register.
 *
 * Type 2 adds a Platform Interrupt Ack register so that level-triggered
 * interrupts can be cleared, while type 1 only supports edge-triggered
 * interrupts (§14.1.4).
 */

#include "pcc_impl.h"

/*
 * Interlocked read of the Type 1/2 Status field.
 *
 * ACPI 6.6 §14.5 requires interlocked access for all Status field accesses.
 */
static uint16_t
pcc_type12_read_status(pcc_chan_t *pc)
{
	volatile uint16_t *status;

	status = (volatile uint16_t *)(void *)(pc->pc_shmem_va +
	    PCC_SHMEM_STATUS);
	return (atomic_add_16_nv(status, 0));
}

/*
 * Extract Type 1/2 subtable fields into a channel.
 *
 * Returns DDI_SUCCESS or DDI_FAILURE (firmware validation failure).
 */
static int
pcc_type12_parse(void *subtable, pcc_chan_t *pc)
{
	ACPI_PCCT_HW_REDUCED *sub1;
	ACPI_PCCT_HW_REDUCED_TYPE2 *sub2;
	ACPI_GENERIC_ADDRESS *dbg;
	ACPI_GENERIC_ADDRESS *ackg;
	uint64_t base;
	uint64_t len;
	uint64_t preserve;
	uint64_t set;
	uint32_t latency;
	uint32_t max_rate;
	uint16_t turnaround;

	pc->pc_id = pc->pc_idx;

	if (pc->pc_type == ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2) {
		sub2 = (ACPI_PCCT_HW_REDUCED_TYPE2 *)subtable;
		base = sub2->BaseAddress;
		len = sub2->Length;
		dbg = &sub2->DoorbellRegister;
		preserve = sub2->PreserveMask;
		set = sub2->WriteMask;
		latency = sub2->Latency;
		max_rate = sub2->MaxAccessRate;
		turnaround = sub2->MinTurnaroundTime;
		ackg = &sub2->PlatformAckRegister;
		pc->pc_gsiv = sub2->PlatformInterrupt;
		pc->pc_irq_flags = sub2->Flags;
	} else {
		sub1 = (ACPI_PCCT_HW_REDUCED *)subtable;
		base = sub1->BaseAddress;
		len = sub1->Length;
		dbg = &sub1->DoorbellRegister;
		preserve = sub1->PreserveMask;
		set = sub1->WriteMask;
		latency = sub1->Latency;
		max_rate = sub1->MaxAccessRate;
		turnaround = sub1->MinTurnaroundTime;
		ackg = NULL;
		pc->pc_gsiv = sub1->PlatformInterrupt;
		pc->pc_irq_flags = sub1->Flags;
	}

	/*
	 * ACPI 6.6 Tables 14.4/14.5/14.6: Memory Length "Must be > 8".
	 */
	if (len <= PCC_SHMEM_HDR_LEN) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: shared memory "
		    "length %llu too short (must be > %u)",
		    pc->pc_id, (unsigned long long)len, PCC_SHMEM_HDR_LEN);
		return (DDI_FAILURE);
	}

	/*
	 * ACPI 6.6 Table 14.7: Doorbell is required for initiator subspaces.
	 */
	if (dbg->Address == 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: doorbell "
		    "register absent", pc->pc_id);
		return (DDI_FAILURE);
	}

	if (pcc_validate_gas(pc, "doorbell", dbg) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	if (ackg != NULL &&
	    pcc_validate_gas(pc, "ack", ackg) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	pc->pc_db_gas = *dbg;

	/*
	 * The ack register is present only in type 2, and optional
	 * there.  Store it (and its masks) only when present.  Type 1
	 * has no ack register, so its ack state stays zeroed.
	 */
	if (ackg != NULL && ackg->Address != 0) {
		pc->pc_ack_gas = *ackg;
		pc->pc_ack_preserve = sub2->AckPreserveMask;
		pc->pc_ack_set = sub2->AckWriteMask;
	}

	/*
	 * Disallow register aliasing for types 1 and 2.
	 */
	if (PCC_GAS_ALIASES(pc->pc_ack_gas, pc->pc_db_gas)) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: ack register aliases "
		    "doorbell register", pc->pc_id);
		return (DDI_FAILURE);
	}

	/*
	 * Disallow partial overlap between the ack and doorbell registers.
	 */
	if (PCC_GAS_OVERLAPS(pc->pc_ack_gas, pc->pc_db_gas)) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: ack register "
		    "partially overlaps doorbell register", pc->pc_id);
		return (DDI_FAILURE);
	}

	/*
	 * A zero doorbell WriteMask means that the doorbell can never be
	 * rung.
	 */
	if (set == 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: doorbell WriteMask "
		    "is zero", pc->pc_id);
		return (DDI_FAILURE);
	}

	/*
	 * If the set-mask sets bits greater than the register width the
	 * firmware is delinquent.
	 */
	if (pc->pc_db_gas.BitWidth == 32 && set > UINT32_MAX) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: doorbell WriteMask "
		    "0x%llx exceeds 32-bit register width",
		    pc->pc_id, (unsigned long long)set);
		return (DDI_FAILURE);
	}

	if (pc->pc_ack_gas.Address != 0 && pc->pc_ack_gas.BitWidth == 32 &&
	    pc->pc_ack_set > UINT32_MAX) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: ack WriteMask 0x%llx "
		    "exceeds 32-bit register width",
		    pc->pc_id, (unsigned long long)pc->pc_ack_set);
		return (DDI_FAILURE);
	}

	pc->pc_shmem_pa = base;
	pc->pc_shmem_len = len;
	pc->pc_db_preserve = preserve;
	pc->pc_db_set = set;
	pc->pc_nominal_lat = latency;
	pc->pc_max_rate = max_rate;
	pc->pc_turnaround = turnaround;
	pc->pc_hdr_len = PCC_SHMEM_HDR_LEN;
	pc->pc_signature = PCC_SIGNATURE_BASE | pc->pc_id;
	pc->pc_valid = B_TRUE;

	return (DDI_SUCCESS);
}

/*
 * Create register registry entries for the doorbell and (for type 2) ack
 * registers.
 *
 * Clean up any created entries on failure.
 */
static int
pcc_type12_map(pcc_chan_t *pc)
{
	pc->pc_db_reg = pcc_reg_lookup_or_create(&pc->pc_db_gas);
	if (pc->pc_db_reg == NULL) {
		return (DDI_FAILURE);
	}

	if (pc->pc_type == ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2 &&
	    pc->pc_ack_gas.Address != 0) {
		pc->pc_ack_reg = pcc_reg_lookup_or_create(&pc->pc_ack_gas);
		if (pc->pc_ack_reg == NULL) {
			pcc_reg_release(&pc->pc_db_reg);
			return (DDI_FAILURE);
		}
	}

	return (DDI_SUCCESS);
}

/*
 * Release all register registry entries created by pcc_type12_map.
 */
static void
pcc_type12_unmap(pcc_chan_t *pc)
{
	pcc_reg_release(&pc->pc_ack_reg);
	pcc_reg_release(&pc->pc_db_reg);
}

/*
 * Begin a type 1 or 2 transaction.
 *
 * Called with `pc_lock' held.  Establishes that OSPM owns the
 * communication space before the payload is written.
 */
static int
pcc_type12_txn_begin(pcc_chan_t *pc)
{
	uint16_t val;

	/*
	 * ACPI 6.6 §14.5 step 1: OSPM must only proceed when Command Complete
	 * is set, otherwise the platform still owns the channel and has not
	 * released it.
	 */
	val = pcc_type12_read_status(pc);
	if ((val & PCC_STATUS_CMD_COMPLETE) == 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: send with Command "
		    "Complete clear (platform owns channel)", pc->pc_id);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * Commit a type 1 or 2 transaction.
 *
 * Called with `pc_lock' held, after timing enforcement is complete.
 * Channel ownership was established by pcc_type12_txn_begin.
 *
 * The payload has already been written to the shared communication
 * space.
 */
static int
pcc_type12_txn_commit(pcc_chan_t *pc, uint32_t cmd,
    uint32_t __unused payload_len)
{
	volatile uint16_t *status;
	volatile uint16_t *cmdfield;
	boolean_t irq;

	status = (volatile uint16_t *)(void *)(pc->pc_shmem_va +
	    PCC_SHMEM_STATUS);
	cmdfield = (volatile uint16_t *)(void *)(pc->pc_shmem_va +
	    PCC_SHMEM_CMD);

	/*
	 * ACPI 6.6 Table 14.10: the Command field is 16 bits wide.
	 *
	 * Bits 0-7 are the command, bits 8-14 are reserved, bit 15 is
	 * "Notify on Completion".  Our caller passes the command code only.
	 */
	if (cmd > UINT8_MAX) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: command 0x%x exceeds "
		    "8-bit command field", pc->pc_id, cmd);
		return (DDI_FAILURE);
	}

	mutex_enter(&pc->pc_state_lock);
	irq = pc->pc_has_irq;
	mutex_exit(&pc->pc_state_lock);

	/*
	 * Write the 16-bit Command field, setting Notify on Completion
	 * (bit 15) iff we have an interrupt (else we'll do polling mode).
	 *
	 * `pc_has_irq' is false when the PCCT global Platform Interrupt flag is
	 * off, which satisfies Table 14.10's "must be cleared" rule.
	 *
	 * Type 1 and 2 subspaces do not have a Length field, so `payload_len'
	 * is ignored.
	 */
	*cmdfield = (uint16_t)cmd |
	    (irq ? PCC_CMD_NOTIFY_ON_COMPLETE : 0);

	/*
	 * ACPI 6.6 §14.5 step 2: OSPM must ensure all shared memory writes are
	 * observable by the platform before the doorbell (rung by common code
	 * after this function returns).
	 */
	membar_producer();

	/*
	 * Clear the Command Complete flag (bit 0) to transfer ownership of
	 * the communication area to the platform (§14.5 step 3).
	 *
	 * While here, clear any stale Error flag (bit 2).
	 *
	 * Interlocked access is required by ACPI 6.6 §14.5 for all Status
	 * field accesses.
	 */
	atomic_and_16(status,
	    (uint16_t)~(PCC_STATUS_CMD_COMPLETE | PCC_STATUS_ERROR));

	/*
	 * The doorbell is rung after we return.
	 */
	return (DDI_SUCCESS);
}

/*
 * Poll for command completion.
 */
static int
pcc_type12_poll_complete(pcc_chan_t *pc)
{
	hrtime_t deadline;
	hrtime_t timeout;

	timeout = pcc_cmd_timeout(pc);

	deadline = gethrtime() + timeout;

	for (;;) {
		if ((pcc_type12_read_status(pc) & PCC_STATUS_CMD_COMPLETE) !=
		    0) {
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
 * Check platform-reported error status after command completion.
 *
 * Returns DDI_FAILURE when the Error bit is set.
 */
static int
pcc_type12_check_error(pcc_chan_t *pc)
{
	uint16_t val;

	val = pcc_type12_read_status(pc);

	if ((val & PCC_STATUS_ERROR) != 0) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u: platform reported "
		    "command error (status 0x%x)", pc->pc_id, val);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * Read-only ownership peek at the Status field for use by the interrupt
 * handler.
 *
 * Sets PCC_IRQ_CMD_COMPLETE in *result if bit 0 (Command Complete) is set and
 * PCC_IRQ_NOTIFY if bit 3 (Platform Notification) is set.
 *
 * Platform Interrupt (bit 1) is not particularly useful for distinguishing
 * between interrupt-driven completion and notifications, but we report it
 * by setting PCC_IRQ_PLATFORM_IRQ so that the handler can use simplified
 * logic to clear it.
 *
 * Notifications are deprecated (ACPI 6.6 §14.6.1) and unsupported in this
 * driver.
 */
static int
pcc_type12_irq_check(pcc_chan_t *pc, pcc_irq_result_t *result)
{
	uint16_t val;

	val = pcc_type12_read_status(pc);

	*result = PCC_IRQ_NONE;

	if ((val & PCC_STATUS_CMD_COMPLETE) != 0) {
		*result |= PCC_IRQ_CMD_COMPLETE;
	}

	if ((val & PCC_STATUS_PLATFORM_NOTIFY) != 0) {
		*result |= PCC_IRQ_NOTIFY;
	}

	if ((val & PCC_STATUS_PLATFORM_IRQ) != 0) {
		*result |= PCC_IRQ_PLATFORM_IRQ;
	}

	return (DDI_SUCCESS);
}

/*
 * Clear stray platform notification bits (1 and 3) in the Status field.
 */
void
pcc_type12_clear_notify(pcc_chan_t *pc)
{
	volatile uint16_t *status;

	status = (volatile uint16_t *)(void *)(pc->pc_shmem_va +
	    PCC_SHMEM_STATUS);
	atomic_and_16(status,
	    (uint16_t)~(PCC_STATUS_PLATFORM_IRQ | PCC_STATUS_PLATFORM_NOTIFY));
}

/*
 * Atomically clear the Status field's Platform Interrupt bit.
 *
 * See ACPI 6.6 Table 14.11.
 */
static void
pcc_type12_clear_platform_irq(pcc_chan_t *pc)
{
	volatile uint16_t *status;

	status = (volatile uint16_t *)(void *)(pc->pc_shmem_va +
	    PCC_SHMEM_STATUS);
	atomic_and_16(status, (uint16_t)~PCC_STATUS_PLATFORM_IRQ);
}

/*
 * Channel initialisation.
 */
static int
pcc_type12_init(pcc_chan_t *pc)
{
	hrtime_t timeout_ns;
	hrtime_t expire;

	if ((pcc_type12_read_status(pc) & PCC_STATUS_CMD_COMPLETE) != 0) {
		return (DDI_SUCCESS);
	}

	/*
	 * Platform still owns the channel.
	 *
	 * Wait a bounded time for it to be released.
	 */
	timeout_ns = pcc_cmd_timeout(pc);

	expire = gethrtime() + timeout_ns;
	for (;;) {
		if ((pcc_type12_read_status(pc) & PCC_STATUS_CMD_COMPLETE) !=
		    0) {
			break;
		}

		if (gethrtime() >= expire) {
			cmn_err(CE_WARN, "!acpipcc: subspace %u: Command "
			    "Complete still clear after init timeout",
			    pc->pc_id);
			return (PCC_INIT_UNUSABLE);
		}

		drv_usecwait(PCC_POLL_INTERVAL_US);
	}

	return (DDI_SUCCESS);
}

const pcc_type_ops_t pcc_type1_ops = {
	.pto_parse = pcc_type12_parse,
	.pto_map = pcc_type12_map,
	.pto_init = pcc_type12_init,
	.pto_unmap = pcc_type12_unmap,
	.pto_txn_begin = pcc_type12_txn_begin,
	.pto_txn_commit = pcc_type12_txn_commit,
	.pto_poll_complete = pcc_type12_poll_complete,
	.pto_check_error = pcc_type12_check_error,
	.pto_clear_platform_irq = pcc_type12_clear_platform_irq,
	.pto_irq_check = pcc_type12_irq_check,
	.pto_edge_only_irq = B_TRUE,	/* no level IRQ clear */
};

const pcc_type_ops_t pcc_type2_ops = {
	.pto_parse = pcc_type12_parse,
	.pto_map = pcc_type12_map,
	.pto_init = pcc_type12_init,
	.pto_unmap = pcc_type12_unmap,
	.pto_txn_begin = pcc_type12_txn_begin,
	.pto_txn_commit = pcc_type12_txn_commit,
	.pto_poll_complete = pcc_type12_poll_complete,
	.pto_check_error = pcc_type12_check_error,
	.pto_clear_platform_irq = pcc_type12_clear_platform_irq,
	.pto_irq_check = pcc_type12_irq_check,
	.pto_edge_only_irq = B_FALSE,
};
