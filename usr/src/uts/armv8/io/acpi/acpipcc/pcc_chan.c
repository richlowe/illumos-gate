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
 * PCC channel operations.
 *
 * Channel operations that are _not_ get/put live here.  This covers:
 * - the transaction bracket (begin/end)
 * - reading/writing the shared memory space
 * - committing a command and waiting for completion
 *
 * Once a user has a channel reference they can use these operations as follows:
 * - Begin a transaction (pcc_chan_txn_begin): takes the channel mutex and
 *   establishes that OSPM owns the communication space
 *   - Write the payload into the shared memory space (pcc_chan_write*)
 *   - Commit the command and wait for completion (pcc_chan_txn_commit)
 *   - Read the response from the shared memory space (pcc_chan_read*)
 * - End the transaction (pcc_chan_txn_end): releases the channel mutex
 */

#include <sys/types.h>
#include "pcc_impl.h"

/*
 * Begin a transaction on a channel.
 *
 * Takes the channel mutex and establishes that OSPM owns the communication
 * space, so the caller may write a payload into it.  A channel marked broken
 * after a completion timeout fails fast here.
 *
 * On failure the channel mutex is not held.
 */
int
pcc_chan_txn_begin(pcc_chan_t *pc)
{
	ASSERT(!MUTEX_HELD(&pc->pc_lock));

	mutex_enter(&pc->pc_lock);

	/*
	 * A previous command timed out: the platform is not responding.
	 */
	if (pc->pc_broken) {
		mutex_exit(&pc->pc_lock);
		return (DDI_FAILURE);
	}

	if (pc->pc_ops->pto_txn_begin(pc) != DDI_SUCCESS) {
		mutex_exit(&pc->pc_lock);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * End a transaction on a channel.
 *
 * Called after the caller has read any response from the communication
 * space.  Releases the channel mutex taken by pcc_chan_txn_begin.
 */
void
pcc_chan_txn_end(pcc_chan_t *pc)
{
	ASSERT(MUTEX_HELD(&pc->pc_lock));

	mutex_exit(&pc->pc_lock);
}

/*
 * Channel shared-memory accessors.
 *
 * The offset argument is relative to the start of the communication space,
 * so _after_ the type-specific header.
 *
 * Callers must hold pc_lock.  Accessors may only be used in a transaction,
 * after pcc_chan_txn_begin has established that OSPM owns the communication
 * space.
 */

int
pcc_chan_read32(pcc_chan_t *pc, uint32_t offset, uint32_t *val)
{
	uint64_t abs_off;
	volatile uint32_t *p;

	ASSERT(MUTEX_HELD(&pc->pc_lock));

	abs_off = (uint64_t)offset + pc->pc_hdr_len;

	if (abs_off + sizeof (uint32_t) > pc->pc_shmem_len) {
		return (DDI_FAILURE);
	}

	p = (volatile uint32_t *)(void *)(pc->pc_shmem_va + abs_off);
	*val = *p;
	return (DDI_SUCCESS);
}

int
pcc_chan_write32(pcc_chan_t *pc, uint32_t offset, uint32_t val)
{
	uint64_t abs_off;
	volatile uint32_t *p;

	ASSERT(MUTEX_HELD(&pc->pc_lock));

	abs_off = (uint64_t)offset + pc->pc_hdr_len;

	if (abs_off + sizeof (uint32_t) > pc->pc_shmem_len) {
		return (DDI_FAILURE);
	}

	p = (volatile uint32_t *)(void *)(pc->pc_shmem_va + abs_off);
	*p = val;
	return (DDI_SUCCESS);
}

int
pcc_chan_read64(pcc_chan_t *pc, uint32_t offset, uint64_t *val)
{
	uint64_t abs_off;
	volatile uint64_t *p;

	ASSERT(MUTEX_HELD(&pc->pc_lock));

	abs_off = (uint64_t)offset + pc->pc_hdr_len;

	if (abs_off + sizeof (uint64_t) > pc->pc_shmem_len) {
		return (DDI_FAILURE);
	}

	p = (volatile uint64_t *)(void *)(pc->pc_shmem_va + abs_off);
	*val = *p;
	return (DDI_SUCCESS);
}

int
pcc_chan_write64(pcc_chan_t *pc, uint32_t offset, uint64_t val)
{
	uint64_t abs_off;
	volatile uint64_t *p;

	ASSERT(MUTEX_HELD(&pc->pc_lock));

	abs_off = (uint64_t)offset + pc->pc_hdr_len;

	if (abs_off + sizeof (uint64_t) > pc->pc_shmem_len) {
		return (DDI_FAILURE);
	}

	p = (volatile uint64_t *)(void *)(pc->pc_shmem_va + abs_off);
	*p = val;
	return (DDI_SUCCESS);
}

/*
 * Enforce Minimum Request Turnaround Time and Maximum Periodic Access Rate.
 *
 * Called at the start of pcc_chan_txn_commit (with pc_lock held).
 * If both are zero, returns immediately.  Otherwise computes:
 *   min_gap = max(turnaround, rate-derived interval)
 * and waits for the remainder if the last observed command completion was too
 * recent.  Turnaround is measured from last completion.
 *
 * Note that the rate limit is applied to *every* command, including infrequent
 * event-driven commands.  This is overly conservative, as ACPI 6.6 §14.1
 * exempts infrequent, event-driven commands from the Maximum Periodic Access
 * Rate.  We do this to ensure that the platform's limit is never exceeded.  If
 * this causes problems we will need to revisit this code.
 */
static void
pcc_enforce_timing(pcc_chan_t *pc)
{
	hrtime_t now, elapsed, min_gap;

	ASSERT(MUTEX_HELD(&pc->pc_lock));

	if (pc->pc_last_complete == 0) {
		return;
	}

	/* min_gap = max(turnaround, rate-derived interval) */
	min_gap = (hrtime_t)pc->pc_turnaround * NANOSEC / MICROSEC;
	if (pc->pc_max_rate > 0) {
		hrtime_t rate_gap = (hrtime_t)60 * NANOSEC / pc->pc_max_rate;
		if (rate_gap > min_gap) {
			min_gap = rate_gap;
		}
	}

	if (min_gap > PCC_MAX_TIMING_WAIT) {
		min_gap = PCC_MAX_TIMING_WAIT;
	}

	if (min_gap == 0) {
		return;
	}

	now = gethrtime();
	elapsed = now - pc->pc_last_complete;

	if (elapsed < min_gap) {
		drv_usecwait(
		    (clock_t)((min_gap - elapsed) / (NANOSEC / MICROSEC)));
	}
}

/*
 * Commit a message to the firmware by sending the command and waiting for
 * completion.
 *
 * The caller must be in a transaction (pcc_chan_txn_begin) and must have
 * written the payload into the communication space via the pcc_chan_write*
 * accessors before calling.
 *
 * The payload_len argument is the payload byte count.  Types 1 and 2 ignore
 * this argument.
 */
int
pcc_chan_txn_commit(pcc_chan_t *pc, uint32_t cmd, uint32_t payload_len)
{
	boolean_t irq;
	pcc_irq_result_t result;
	int ret;

	ASSERT(MUTEX_HELD(&pc->pc_lock));

	pcc_enforce_timing(pc);

	/*
	 * Clear the completion predicate and record the command as outstanding
	 * so the interrupt handler can attribute the completion interrupt to
	 * this channel.
	 */
	mutex_enter(&pc->pc_state_lock);
	pc->pc_cmd_complete = B_FALSE;
	pc->pc_cmd_outstanding = B_TRUE;
	mutex_exit(&pc->pc_state_lock);

	/*
	 * Type-specific actions are:
	 * - populate header
	 * - clear CmdComplete
	 */
	ret = pc->pc_ops->pto_txn_commit(pc, cmd, payload_len);
	if (ret != DDI_SUCCESS) {
		mutex_enter(&pc->pc_state_lock);
		pc->pc_cmd_outstanding = B_FALSE;
		mutex_exit(&pc->pc_state_lock);
		return (ret);
	}

	/*
	 * Ring the doorbell
	 */
	pcc_ring_doorbell(pc);

	mutex_enter(&pc->pc_state_lock);
	irq = pc->pc_has_irq;
	mutex_exit(&pc->pc_state_lock);

	/* Wait for completion (interrupt or poll) */
	if (irq) {
		ret = pcc_wait_irq(pc);
		mutex_enter(&pc->pc_state_lock);
		irq = pc->pc_has_irq;
		mutex_exit(&pc->pc_state_lock);
		if (ret != DDI_SUCCESS && !irq) {
			/*
			 * The interrupt storm fail-stop tore down the
			 * channel's interrupt while we were waiting.  Fall
			 * back to polling for this command's completion.
			 */
			cmn_err(CE_NOTE, "!acpipcc: subspace %u: interrupt "
			    "lost to storm fail-stop; falling back to polling",
			    pc->pc_id);
			ret = pc->pc_ops->pto_poll_complete(pc);
		}
	} else {
		ret = pc->pc_ops->pto_poll_complete(pc);
	}

	/*
	 * The command has completed, record the timestamp for rate enforcement.
	 *
	 * ACPI 6.6 §14.1.4/§14.1.6 measure Minimum Request Turnaround Time from
	 * completion, not from send.
	 *
	 * It's OK to record the completion on failure - the channel is disabled
	 * below.
	 */
	pc->pc_last_complete = gethrtime();

	/*
	 * The command has reached a terminal state (completed, timed out), so
	 * it is no longer outstanding.
	 */
	mutex_enter(&pc->pc_state_lock);
	pc->pc_cmd_outstanding = B_FALSE;
	mutex_exit(&pc->pc_state_lock);

	/*
	 * Ensure that the platform's response data is visible before it's read.
	 */
	membar_consumer();

	/*
	 * Check the error status (this is type specific).
	 */
	mutex_enter(&pc->pc_state_lock);
	irq = pc->pc_has_irq;
	mutex_exit(&pc->pc_state_lock);
	if (ret != DDI_SUCCESS && irq) {
		/*
		 * The interrupt wait timed out, so take a peek at the hardware
		 * to check whether the platform did actually respond.  Use this
		 * knowledge to patch up our next steps.
		 */
		result = PCC_IRQ_NONE;
		if (pc->pc_ops->pto_irq_check(pc, &result) == DDI_SUCCESS &&
		    (result & PCC_IRQ_CMD_COMPLETE) != 0) {
			membar_consumer();

			/*
			 * The handler never ran for this completion, so a
			 * level-triggered interrupt may still be asserted.
			 *
			 * Write the ack as if we were the handler so that we
			 * avoid an interrupt storm.
			 */
			pcc_write_ack(pc);

			/*
			 * Clear the platform IRQ status bit in preparation for
			 * the next command.
			 */
			if (result & PCC_IRQ_PLATFORM_IRQ) {
				pc->pc_ops->pto_clear_platform_irq(pc);
			}

			ret = DDI_SUCCESS;
		}
	}

	if (ret == DDI_SUCCESS) {
		ret = pc->pc_ops->pto_check_error(pc);
	} else {
		/*
		 * Completion timed out and peeking at the hardware found
		 * no lingering completion.  Mark the channel failed.
		 */
		pc->pc_broken = B_TRUE;
		cmn_err(CE_WARN, "!acpipcc: subspace %u: channel marked failed "
		    "after command completion timeout", pc->pc_id);
	}

	return (ret);
}
