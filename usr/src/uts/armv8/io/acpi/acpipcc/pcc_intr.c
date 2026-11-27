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
 * Interrupt support: per-GSIV reference-counted groups.
 *
 * PCC has no dip, so the DDI interrupt path is unavailable.
 *
 * Instead, add_avintr/rem_avintr are used directly with raw INTIDs, alongside
 * syspic interfaces for trigger-mode configuration.
 */

#include <sys/types.h>
#include <sys/stddef.h>
#include <sys/syspic.h>
#include <sys/syspic_impl.h>
#include "pcc_impl.h"

static list_t pcc_irq_group_list;	/* global irq group list */
static kmutex_t pcc_irq_list_lock;	/* protects pcc_irq_group_list */

void
pcc_irq_init(void)
{
	list_create(&pcc_irq_group_list, sizeof (pcc_irq_group_t),
	    offsetof(pcc_irq_group_t, pig_node));
	mutex_init(&pcc_irq_list_lock, NULL, MUTEX_DEFAULT, NULL);
}

void
pcc_irq_fini(void)
{
	list_destroy(&pcc_irq_group_list);
	mutex_destroy(&pcc_irq_list_lock);
}

/*
 * Deferred interrupt-storm fail-stop.
 *
 * Runs in timeout(9F) thread context.
 *
 * Marks every member channel polling-only (pc_has_irq = B_FALSE) so later
 * sends degrade to polling, then unlinks each member via pcc_irq_group_remove.
 *
 * Any thread blocked in pcc_wait_irq is woken so it can fall back to polling
 * rather than sleep out the full timeout waiting for an interrupt that can
 * never arrive.
 */
static void
pcc_irq_storm_stop(void *arg)
{
	uint32_t gsiv = (uint32_t)(uintptr_t)arg;
	pcc_irq_group_t *group;
	pcc_chan_t *pc;

	mutex_enter(&pcc_irq_list_lock);
	for (;;) {
		for (group = list_head(&pcc_irq_group_list);
		    group != NULL;
		    group = list_next(&pcc_irq_group_list, group)) {
			if (group->pig_gsiv == gsiv &&
			    group->pig_storm_tripped) {
				break;
			}
		}

		/*
		 * No group found for this GSIV, we're done.
		 */
		if (group == NULL) {
			break;
		}

		/*
		 * No channels referencing this group, we're done.
		 */
		pc = list_head(&group->pig_channels);
		if (pc == NULL) {
			break;
		}

		/*
		 * We have at least one channel referencing this group, remove
		 * it and keep going until no channels remain (previous check).
		 */
		mutex_enter(&pc->pc_state_lock);
		pc->pc_has_irq = B_FALSE;

		/*
		 * Wake any thread blocked in pcc_wait_irq: the handler is being
		 * unlinked, so no interrupt is coming.  The waiter observes
		 * pc_has_irq == B_FALSE and falls back to polling.
		 */
		cv_broadcast(&pc->pc_cv);
		mutex_exit(&pc->pc_state_lock);

		mutex_exit(&pcc_irq_list_lock);
		pcc_irq_group_remove(pc);
		mutex_enter(&pcc_irq_list_lock);
	}
	mutex_exit(&pcc_irq_list_lock);
}

#define	IS_TYPE1_TYPE2(pc)	\
    ((pc)->pc_type == ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE || \
    (pc)->pc_type == ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2)

/*
 * Per-GSIV interrupt handler.
 *
 * Runs in interrupt context with the pcc_irq_group_t as arg1.
 *
 * Walks only the channels bound to that GSIV.  If it acquires a
 * contended mutex, the illumos interrupt framework transparently
 * converts the handler into an interrupt thread.
 *
 * ACK ordering follows ACPI 6.6 Figures 14-1, 14-2: the platform
 * interrupt is cleared (ACKed) BEFORE the handler acts on a
 * completion or notification.
 *
 * For a level-triggered GSIV shared by multiple channels, the
 * handler peeks at each channel's completion indicator (read-only)
 * to determine ownership before ACKing.  Sole-occupant GSIVs skip
 * the peek.
 */
static uint_t
pcc_irq_handler(caddr_t arg1, caddr_t arg2 __unused)
{
	pcc_irq_group_t *group = (pcc_irq_group_t *)(void *)arg1;
	pcc_chan_t *pc;
	pcc_irq_result_t result;
	boolean_t claimed;
	int ret;

	claimed = B_FALSE;

	atomic_inc_32(&group->pig_active);

	mutex_enter(&pcc_irq_list_lock);
	for (pc = list_head(&group->pig_channels);
	    pc != NULL;
	    pc = list_next(&group->pig_channels, pc)) {
		result = PCC_IRQ_NONE;

		/*
		 * Shared level-triggered GSIV: peek at the completion indicator
		 * to determine ownership before ACKing.
		 */
		if (group->pig_refcnt > 1) {
			ret = pc->pc_ops->pto_irq_check(pc, &result);
			if (ret != DDI_SUCCESS || result == PCC_IRQ_NONE) {
				continue;
			}

			/*
			 * For types 1 and 2 the platform interrupt bit is the
			 * platform's own attribution signal for this interrupt.
			 */
			if (IS_TYPE1_TYPE2(pc) &&
			    (result & PCC_IRQ_PLATFORM_IRQ) == 0) {
				continue;
			}
		}

		/*
		 * ACK the platform interrupt before acting on the result.
		 */
		pcc_write_ack(pc);

		/*
		 * Sole-occupant GSIV check (shared happened above).
		 */
		if (group->pig_refcnt <= 1) {
			ret = pc->pc_ops->pto_irq_check(pc, &result);
			if (ret != DDI_SUCCESS) {
				continue;
			}
		}

		if ((result & PCC_IRQ_CMD_COMPLETE) != 0) {
			mutex_enter(&pc->pc_state_lock);
			/*
			 * Only a channel with a command outstanding can own a
			 * completion interrupt.
			 */
			if (pc->pc_cmd_outstanding) {
				pc->pc_cmd_outstanding = B_FALSE;
				pc->pc_cmd_complete = B_TRUE;
				cv_signal(&pc->pc_cv);
			} else {
				result &= ~PCC_IRQ_CMD_COMPLETE;
			}
			mutex_exit(&pc->pc_state_lock);
		}

		/*
		 * Type 1/2 platform notifications are deprecated
		 * and unsupported - discard the bits without dispatching.
		 */
		if ((result & PCC_IRQ_NOTIFY) != 0) {
			pcc_type12_clear_notify(pc);
		}

		/*
		 * ACPI 6.6 Table 14.11: clear the Platform Interrupt bit
		 * whenever observed.
		 */
		if (result & PCC_IRQ_PLATFORM_IRQ) {
			pc->pc_ops->pto_clear_platform_irq(pc);
		}

		/*
		 * If we got a completion or a stray notification, claim the
		 * interrupt so that it does not count towards the storm
		 * fail-stop logic below.
		 */
		if ((result & (PCC_IRQ_CMD_COMPLETE | PCC_IRQ_NOTIFY)) != 0) {
			claimed = B_TRUE;
		}
	}

	/*
	 * If no channel claimed a shared level-triggered GSIV then the level
	 * line is still asserted, as no ack code has run.
	 *
	 * Ack every member (benign on a non-asserted line) and warn.  Without
	 * doing this the CPU will re-take the interrupt immediately and life
	 * will be miserable.
	 *
	 * Sole-occupant GSIVs were already unconditionally ack'd above.
	 */
	if (!claimed && !group->pig_edge && group->pig_refcnt > 1) {
		cmn_err(CE_WARN, "!acpipcc: spurious interrupt on shared "
		    "level-triggered GSIV %u", group->pig_gsiv);
		for (pc = list_head(&group->pig_channels);
		    pc != NULL;
		    pc = list_next(&group->pig_channels, pc)) {
			pcc_write_ack(pc);
		}
	}

	/*
	 * Our interrupt storm guard counts consecutive handler invocations
	 * that were not claimed by any channel.
	 *
	 * Once our threshold is reached we schedule a deferred fail-stop, which
	 * unlinks all channel members from the GSIV, removes the handler and
	 * marks the channels as polling-only.
	 */
	if (claimed) {
		group->pig_storm = 0;
	} else if (!group->pig_storm_tripped &&
	    ++group->pig_storm >= PCC_STORM_THRESHOLD) {
		group->pig_storm_tripped = B_TRUE;
		cmn_err(CE_WARN, "!acpipcc: interrupt storm on GSIV %u: %u "
		    "consecutive unclaimed interrupts; disabling PCC "
		    "interrupts on this GSIV",
		    group->pig_gsiv, PCC_STORM_THRESHOLD);
		group->pig_storm_tid = timeout(pcc_irq_storm_stop,
		    (void *)(uintptr_t)group->pig_gsiv, 1);
	}
	mutex_exit(&pcc_irq_list_lock);

	atomic_dec_32(&group->pig_active);

	return (claimed ? DDI_INTR_CLAIMED : DDI_INTR_UNCLAIMED);
}

#undef IS_TYPE1_TYPE2

/*
 * Wait for interrupt-driven command completion.
 *
 * Uses pc_cmd_complete (set in the interrupt handler) as a completion
 * predicate for a bounded wait loop via a condition variable (signalled
 * by the handler).  Verification against hardware is performed to allow
 * for delayed interrupt delivery from previous commands.
 *
 * In case of an interrupt storm, which could remove interrupt-driven
 * completion capabilities mid-wait, this function simply returns DDI_FAILURE
 * so that the caller can fall back to polling.
 */
int
pcc_wait_irq(pcc_chan_t *pc)
{
	hrtime_t timeout_ns;
	hrtime_t expire;
	hrtime_t remain;
	pcc_irq_result_t result;
	int ret;

	ASSERT(MUTEX_HELD(&pc->pc_lock));

	timeout_ns = pcc_cmd_timeout(pc);

	expire = gethrtime() + timeout_ns;

	mutex_enter(&pc->pc_state_lock);
	for (;;) {
		while (!pc->pc_cmd_complete) {
			/*
			 * Storm fail-stop took our interrupt.
			 */
			if (!pc->pc_has_irq) {
				mutex_exit(&pc->pc_state_lock);
				return (DDI_FAILURE);
			}

			/*
			 * Recalculate the remaining wait time and either
			 * exit early or await command completion.
			 */
			remain = expire - gethrtime();

			if (remain <= 0) {
				mutex_exit(&pc->pc_state_lock);
				cmn_err(CE_WARN, "!acpipcc: subspace %u: "
				    "command completion timeout", pc->pc_id);
				return (DDI_FAILURE);
			}

			(void) cv_reltimedwait(&pc->pc_cv, &pc->pc_state_lock,
			    remain, TR_NANOSEC);
		}

		ret = pc->pc_ops->pto_irq_check(pc, &result);
		if (ret == DDI_SUCCESS &&
		    (result & PCC_IRQ_CMD_COMPLETE) != 0) {
			break;
		}
		pc->pc_cmd_complete = B_FALSE;
	}
	mutex_exit(&pc->pc_state_lock);

	return (DDI_SUCCESS);
}

/*
 * Link a channel into its GSIV's group.
 *
 * Finds or creates the group for gsiv, links the channel, bumps refcnt.
 *
 * Installs the interrupt handler on first reference.
 */
int
pcc_irq_group_add(uint32_t gsiv, uint8_t flags, pcc_chan_t *pc)
{
	pcc_irq_group_t *group;
	pcc_chan_t *member;
	boolean_t edge;
	uint_t ipl;
	syspic_intr_state_t *state;

	/*
	 * ACPI 6.6 Table 14.7: bit 1 is Interrupt Mode (0=level, 1=edge)
	 */
	edge = (flags & ACPI_PCCT_INTERRUPT_MODE) != 0;
	ipl = DDI_IPL_5;

	if ((flags & ACPI_PCCT_INTERRUPT_POLARITY) != 0) {
		/*
		 * GICv3 SPIs are always active-high.  The GIC will still be
		 * programmed active-high.
		 *
		 * GIC, more generally, cannot have sensitivity programmed.
		 */
		cmn_err(CE_WARN, "!acpipcc: GSIV %u reports active-low "
		    "polarity. GICv3 SPIs are active-high", gsiv);
	}

	mutex_enter(&pcc_irq_list_lock);
	for (group = list_head(&pcc_irq_group_list);
	    group != NULL;
	    group = list_next(&pcc_irq_group_list, group)) {
		if (group->pig_gsiv == gsiv) {
			break;
		}
	}

	/*
	 * If no interrupt group was found we create one now and add it to
	 * the list.
	 */
	if (group == NULL) {
		group = kmem_zalloc(sizeof (*group), KM_SLEEP);
		group->pig_gsiv = gsiv;
		group->pig_edge = edge;
		group->pig_ipl = ipl;
		group->pig_refcnt = 0;
		group->pig_active = 0;
		group->pig_storm = 0;
		group->pig_storm_tripped = B_FALSE;
		group->pig_storm_tid = 0;
		list_create(&group->pig_channels, sizeof (pcc_chan_t),
		    offsetof(pcc_chan_t, pc_irq_node));
		list_insert_tail(&pcc_irq_group_list, group);
	} else {
		/*
		 * ACPI 6.6 §14.1.6: Edge-triggered subspaces must each have a
		 * unique GSIV.  A second channel on this GSIV is a firmware
		 * bug.
		 */
		if (edge || group->pig_edge) {
			mutex_exit(&pcc_irq_list_lock);
			return (EBUSY);
		}

		/*
		 * Level-triggered subspaces may share a GSIV, but ACPI
		 * 6.6 §14.1.6 requires each to have unique Platform Interrupt
		 * Ack preserve and set masks.
		 *
		 * Duplicate masks on the *same* ack register are a firmware
		 * bug: acknowledging one subspace would acknowledge the other's
		 * interrupt.
		 */
		for (member = list_head(&group->pig_channels);
		    member != NULL;
		    member = list_next(&group->pig_channels, member)) {
			if (member->pc_ack_reg != NULL &&
			    pc->pc_ack_reg != NULL &&
			    member->pc_ack_reg->pre_gas.Address ==
			    pc->pc_ack_reg->pre_gas.Address &&
			    member->pc_ack_reg->pre_gas.SpaceId ==
			    pc->pc_ack_reg->pre_gas.SpaceId &&
			    member->pc_ack_preserve == pc->pc_ack_preserve &&
			    member->pc_ack_set == pc->pc_ack_set) {
				mutex_exit(&pcc_irq_list_lock);
				return (EBUSY);
			}
		}
	}

	/*
	 * Register the channel in the interrupt group and bump the group's
	 * reference count.
	 */
	list_insert_tail(&group->pig_channels, pc);
	group->pig_refcnt++;

	/*
	 * If this is not the first reference the underlying interrupt hookup
	 * has already happened, so we're done.
	 */
	if (group->pig_refcnt > 1) {
		mutex_exit(&pcc_irq_list_lock);
		return (0);
	}

	/*
	 * The 0->1 reference transition installs the interrupt handler.
	 */
	state = syspic_get_state((intr_intid_t)gsiv);
	state->si_edge_triggered = edge;
	state->si_prio = ipl;

	/*
	 * Program the trigger mode before `add_avintr' enables the interrupt.
	 */
	if (syspic_config_irq((int)gsiv, edge) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "!acpipcc: GSIV %u already active with a "
		    "different trigger mode; failing channel", gsiv);
		syspic_remove_state((intr_intid_t)gsiv);
		mutex_exit(&syspic_intrs_lock);
		list_remove(&group->pig_channels, pc);
		group->pig_refcnt--;
		list_destroy(&group->pig_channels);
		list_remove(&pcc_irq_group_list, group);
		kmem_free(group, sizeof (*group));
		mutex_exit(&pcc_irq_list_lock);
		return (DDI_FAILURE);
	}

	/*
	 * `add_avintr' returns 0 on failure.
	 */
	if (add_avintr(NULL, ipl, pcc_irq_handler, "pcc", (int)gsiv,
	    (caddr_t)(void *)group, NULL, NULL, NULL) == 0) {
		syspic_remove_state((intr_intid_t)gsiv);
		mutex_exit(&syspic_intrs_lock);
		list_remove(&group->pig_channels, pc);
		group->pig_refcnt--;
		list_destroy(&group->pig_channels);
		list_remove(&pcc_irq_group_list, group);
		kmem_free(group, sizeof (*group));
		mutex_exit(&pcc_irq_list_lock);
		return (DDI_FAILURE);
	}

	mutex_exit(&syspic_intrs_lock);
	mutex_exit(&pcc_irq_list_lock);
	return (0);
}

/*
 * Unlink a channel from its irq group.
 *
 * Cleans up the group when the last channel reference is decremented.
 */
void
pcc_irq_group_remove(pcc_chan_t *pc)
{
	pcc_irq_group_t *group;
	uint32_t gsiv;
	uint_t ipl;

	mutex_enter(&pcc_irq_list_lock);
	for (group = list_head(&pcc_irq_group_list);
	    group != NULL;
	    group = list_next(&pcc_irq_group_list, group)) {
		pcc_chan_t *c;
		for (c = list_head(&group->pig_channels);
		    c != NULL;
		    c = list_next(&group->pig_channels, c)) {
			if (c == pc) {
				break;
			}
		}

		if (c == pc) {
			break;
		}
	}

	/*
	 * If the channel was not linked then there's nothing to do.
	 */
	if (group == NULL) {
		mutex_exit(&pcc_irq_list_lock);
		return;
	}

	list_remove(&group->pig_channels, pc);
	ASSERT(group->pig_refcnt > 0);
	group->pig_refcnt--;
	gsiv = group->pig_gsiv;
	ipl = group->pig_ipl;

	/*
	 * If references remain then there's nothing more to do.
	 */
	if (group->pig_refcnt > 0) {
		mutex_exit(&pcc_irq_list_lock);
		return;
	}

	/*
	 * On the 1->0 reference transition we remove the interrupt handler.
	 */
	mutex_exit(&pcc_irq_list_lock);
	rem_avintr(NULL, ipl, pcc_irq_handler, (int)gsiv);

	/*
	 * Drain in-flight handler invocations.
	 *
	 * `rem_avintr' means no new invocations can start, so this clears
	 * immediately in practice.
	 */
	while (atomic_add_32_nv(&group->pig_active, 0) != 0) {
		drv_usecwait(10);
	}

	/*
	 * Cancel a pending storm fail-stop for this GSIV.
	 *
	 * If the timeout already fired (this may be running from it), untimeout
	 * is a no-op.
	 */
	if (group->pig_storm_tripped) {
		(void) untimeout(group->pig_storm_tid);
	}

	mutex_enter(&pcc_irq_list_lock);
	list_remove(&pcc_irq_group_list, group);
	mutex_exit(&pcc_irq_list_lock);
	list_destroy(&group->pig_channels);
	kmem_free(group, sizeof (*group));
}

/*
 * Final sweep of the irq group list.
 *
 * All groups should already be gone (every linked channel was unlinked during
 * per-channel cleanup).  Any group found here is a bug, so warn about it and
 * clean it up.
 */
void
pcc_irq_groups_destroy(void)
{
	pcc_irq_group_t *group;

	mutex_enter(&pcc_irq_list_lock);
	while ((group = list_head(&pcc_irq_group_list)) != NULL) {
		uint32_t gsiv = group->pig_gsiv;
		uint_t ipl = group->pig_ipl;

		cmn_err(CE_WARN, "!acpipcc: irq group for GSIV %u still "
		    "present at teardown; removing", gsiv);
		list_remove(&pcc_irq_group_list, group);
		mutex_exit(&pcc_irq_list_lock);

		rem_avintr(NULL, ipl, pcc_irq_handler, (int)gsiv);
		while (atomic_add_32_nv(&group->pig_active, 0) != 0) {
			drv_usecwait(10);
		}

		/*
		 * Cancel a pending storm fail-stop for this GSIV, if any.
		 */
		if (group->pig_storm_tripped) {
			(void) untimeout(group->pig_storm_tid);
		}
		list_destroy(&group->pig_channels);
		kmem_free(group, sizeof (*group));

		mutex_enter(&pcc_irq_list_lock);
	}
	mutex_exit(&pcc_irq_list_lock);
}
