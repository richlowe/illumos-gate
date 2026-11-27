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
 * ACPI Platform Communications Channel Infrastructure Driver
 * ==========================================================
 *
 * See ACPI 6.6 Chapter 14.
 *
 * PCC is a mailbox-and-doorbell communication channel between the OS and
 * platform components.  Standardised interfaces that use PCC include:
 * - CPU power and performance management (Collaborative Processor Performance
 *   Control, or CPPC);
 * - Reliability, availability and serviceability (RASF, RAS2);
 * - Platform debug integration (Platform Debug Trigger Table, or PDTT);
 * - Memory power node management (Memory Power State Table, or MPST);
 * - Firmware-directed generic platform communication (via ACPI AML and a PCC
 *   operation region - described for completeness, but not yet implemented).
 *
 * The above standardised interfaces interact with this module by:
 * - Obtaining a channel reference using the identifier communicated to them
 *   by firmware.  It is expected that a reference is obtained when the
 *   interface driver attaches.  Use the pcc_chan_get function to get a
 *   channel reference.
 * - Transactionally exchanging messages with the firmware by:
 *   - Beginning a transaction on the channel (pcc_chan_txn_begin), which
 *     locks the channel and establishes that OSPM owns the shared memory
 *     message area.
 *     - Preparing the consumer-specific shared memory message area using
 *       the channel write accessors (pcc_chan_write32, pcc_chan_write64).
 *     - Committing the message to the firmware (pcc_chan_txn_commit).
 *     - Reading the consumer-specific firmware response from the shared memory
 *       message area using the channel read accessors (pcc_chan_read32,
 *       pcc_chan_read64).
 *   - Ending the transaction (pcc_chan_txn_end), which unlocks the channel.
 * - Releasing the channel reference when the interface driver detaches.  Use
 *   the pcc_chan_put function to release a channel reference.
 *
 * PCC supports delivery of unsolicited notifications to consumers using
 * interrupts.  This support has not yet been developed, as there is no
 * consumer of these notifications in-tree at present and no standardised
 * consumer of notifications in ACPI.
 *
 * Channel Types
 * -------------
 *
 * ACPI PCCT contains a number of subspace types, reflecting an evolution
 * of PCC design.
 *
 * - Type 0 is the generic communication subspace and shares the same shared
 *   memory layout as types 1 and 2.  FFH registers are not permitted and
 *   completion/notification interrupts are via the ACPI SCI (and therefore
 *   shared across all ACPI SCI consumers).  Type 0 requires the ACPI SCI and
 *   therefore only appears on non-hardware-reduced platforms.  Type 0 is not
 *   implemented by this driver.
 * - Type 1 is the generic hardware reduced communication subspace.  FFH is
 *   a permitted register type and interrupts are delivered via an ACPI
 *   GSIV (interrupt vector).  Type 1 subspaces assume that the provided GSIV
 *   is edge triggered, and can therefore not be shared across subspaces.
 *   Notifications are deprecated on type 1 subspaces due to design shortcomings
 *   that make reliable implementation difficult.  For this reason, this driver
 *   does not implement notifications for type 1 subspaces.
 * - Type 2 is a variant of the type 1 generic hardware reduced communication
 *   subspace.  This type adds an interrupt acknowledge mechanism alongside
 *   an ack preserve mask and an ack write mask.  This allows the use of
 *   level triggered interrupts and the sharing of interrupts across channels.
 *   Like type 1, notifications are deprecated on type 2 subspaces.  This driver
 *   does not implement notifications for type 2 subspaces.
 * - Type 3 is the extended master subspace and is a redesign of the mailbox
 *   interface rather than an extension to previous types.  Mailbox control
 *   is hoisted into registers (doorbell, platform interrupt ack, command
 *   complete check, command complete update and error status) and out of
 *   the shared memory area, which now carries only the message itself
 *   (signature, flags, length, command, payload).  Type 3 is unidirectional,
 *   and no longer attempts to handle notifications.
 * - Type 4 is effectively type 3 with the communication direction reversed.
 *   The type 4 subspace only receives notifications via interrupts and
 *   cannot be used to send messages to the platform.  Type 4 is not implemented
 *   by this driver.
 * - Type 5 is the hardware register-based subspace, and is a dramatic
 *   simplification of a communication channel.  The shared memory area is
 *   optional, and when not present the channel shrinks to only three registers:
 *   doorbell (write carries command), command complete (optional), error
 *   status (also optional).  There is no GSIV - the channel is polling-only,
 *   or can be one-way when no completion register exists.  When the shared
 *   memory area is present it's minimised to only contain the region signature
 *   and raw payload.  Type 5 is not yet supported by this driver as there are
 *   no relevant consumers yet and the subspace descriptor structures are not
 *   yet present in our in-tree ACPICA.
 *
 * A Brief History of PCCT
 * -----------------------
 *
 * - ACPI 5.0 was released in December 2011 and introduced PCCT with table
 *   version 1.  The only subspace supported at the time was type 0, so PCC in
 *   this release did not support hardware-reduced platforms.
 * - ACPI 5.1 was introduced in July 2014, and was the first release to add PCC
 *   support for hardware-reduced platforms.  This release introduced subspace
 *   type 1.  The PCCT version remained at 1.
 * - ACPI 6.0 was introduced in April 2015 and added subspace type 2.  The
 *   introduction of subspace type 2 added a doorbell acknowledgement register,
 *   making level-triggered GSIs usable.  The PCCT version remained at 1.
 * - ACPI 6.2 was introduced in May 2017 and added subspace types 3 and 4,
 *   giving notice of future deprecation of notifications on subspace types 0,
 *   1 and 2.  The table version was bumped to 2 and the name of the "Doorbell
 *   Interrupt" flags bit was changed to "Platform Interrupt".
 * - ACPI 6.4 was introduced in January 2021 and added subspace type 5.  The
 *   PCCT version remained at 2.
 *
 * Module Organisation
 * -------------------
 *
 * The public interface to the module is, as above, channel-centric.  Channels
 * are parsed from the ACPI PCCT table when this module is loaded and placed
 * on a channel list ordered by the subspace identifier firmware uses for
 * the channel.  The type of the subspace identifies the channel-specific
 * implementation operations.
 *
 * This file parses, constructs and tears down channels and the channel
 * registry and also provides channel lookup (pcc_chan_get) and disposal
 * (pcc_chan_put) functions.  Channels are reference counted, so more than
 * one consumer can reference a channel.
 *
 * Consumers then start a transaction on a channel by beginning one
 * (pcc_chan_txn_begin), which locks the channel for exclusive use and
 * establishes that OSPM owns the shared memory area, later ending the
 * transaction (pcc_chan_txn_end) to mark its end and unlock the channel.
 *
 * Inside the transaction, consumers populate the shared memory area using
 * the write helpers (pcc_chan_write[32|64]).  Once the shared memory area is
 * populated the consumer commits the message to the platform
 * (pcc_chan_txn_commit).  When this function returns, the consumer can read
 * the platform response from the shared memory area using the read helpers
 * (pcc_chan_read[32|64]).
 *
 * Internally, both the registers and the interrupts used by channels can be
 * shared.  To facilitate this, the module keeps a registry of registers.  The
 * parse process looks up and creates registers in this registry, which
 * maintains a reference count on the register.  When a register is shared it
 * is locked when used.  Similarly, an interrupt registry is maintained.  When
 * an interrupt is first registered it also registers with the system interrupt
 * controller.  When the last channel unreferences the interrupt it is
 * unregistered from the system interrupt controller.  Interrupt objects
 * maintain a list of channels that reference the interrupt, offering the
 * active interrupt to each channel when an interrupt is handled.  Each
 * participating channel acknowledges based on its own mask information,
 * ensuring isolation between interrupt consumers.
 *
 * Firmware parsing is reasonably strict - any identified firmware deficiency
 * that calls the integrity of the firmware into question results in PCC being
 * disabled.  This is a conservative approach that simply declares, "this
 * firmware is sketchy, I can't trust it at all".  An exception to this rule is
 * channel (subspace) signature validation, which tries to accommodate known
 * firmware bugs.
 */

#include <sys/types.h>
#include <sys/stddef.h>
#include <sys/modctl.h>
#include <sys/sysmacros.h>
#include "pcc_impl.h"

/*
 * Firmware signature tolerance.  See `pcc_check_signature' below.
 *
 * 0: accept any PCC_SIGNATURE_BASE signature (log mismatches)
 * 1: accept off-by-one subspace index (common firmware bug)
 * 2: exact match only
 */
int pcc_strictness = 1;

static list_t pcc_chan_list;		/* channels parsed from the PCCT */
volatile boolean_t pcc_initialised;	/* pcc_init completed */
static kmutex_t pcc_init_lock;		/* serializes pcc_init */

/*
 * Subspace type dispatch table.  Maps PCCT subtable type to the minimum
 * subtable length and the type's ops vector.
 */
static const struct {
	uint8_t			psd_type;	/* PCCT subspace type */
	size_t			psd_min_len;	/* minimum subtable length */
	const pcc_type_ops_t	*psd_ops;
} pcc_subspace_dispatch[] = {
	/* Type 0 deferred: uses x86 SCI */
	{ ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE,
	    sizeof (ACPI_PCCT_HW_REDUCED), &pcc_type1_ops },
	{ ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2,
	    sizeof (ACPI_PCCT_HW_REDUCED_TYPE2), &pcc_type2_ops },
	{ ACPI_PCCT_TYPE_EXT_PCC_MASTER_SUBSPACE,
	    sizeof (ACPI_PCCT_EXT_PCC_MASTER), &pcc_type3_ops },
	/* Type 4 deferred: no notification use case at present */
	/* Type 5 deferred: in-tree ACPICA lacks the subtable struct */
};

/*
 * Get a channel reference by firmware-provided subspace ID.
 *
 * Returns NULL if PCC is not initialised, the ID is unknown, or the channel is
 * otherwise unusable.
 *
 * Callers must use `pcc_chan_put' when done with the channel.
 */
pcc_chan_t *
pcc_chan_get(uint_t chan_id)
{
	pcc_chan_t *pc;

	/*
	 * If PCC failed to initialise (no PCCT, or firmware untrustworthy),
	 * return NULL.
	 */
	if (!pcc_initialised) {
		return (NULL);
	}

	membar_consumer();

	for (pc = list_head(&pcc_chan_list);
	    pc != NULL;
	    pc = list_next(&pcc_chan_list, pc)) {
		if (pc->pc_id == chan_id) {
			break;
		}
	}

	if (pc == NULL || !pc->pc_usable) {
		return (NULL);
	}

	/*
	 * We have the channel, bump the refcount.
	 */
	mutex_enter(&pc->pc_lock);
	pc->pc_refcnt++;
	mutex_exit(&pc->pc_lock);

	return (pc);
}

/*
 * Release a channel reference obtained from `pcc_chan_get'.
 */
void
pcc_chan_put(pcc_chan_t *pc)
{
	mutex_enter(&pc->pc_lock);
	ASSERT3U(pc->pc_refcnt, >, 0);
	pc->pc_refcnt--;
	mutex_exit(&pc->pc_lock);
}

/*
 * Per-channel teardown.
 *
 * Undoes the actions performed by pcc_parse_pcct on a per-channel basis.
 */
static void
pcc_chan_cleanup(pcc_chan_t *pc)
{
	if (pc->pc_init_state & PCC_INIT_IRQ_LINKED) {
		pcc_irq_group_remove(pc);
	}

	/* PCC_INIT_TYPE: no resources to release (state is in shmem) */
	/* PCC_INIT_SIG_OK: no resources to release */

	if (pc->pc_init_state & PCC_INIT_MAPPED) {
		pc->pc_ops->pto_unmap(pc);
	}

	if (pc->pc_init_state & PCC_INIT_SHMEM) {
		/* KPM mapping: no explicit unmap needed */
		pc->pc_shmem_va = NULL;
	}

	/* PCC_INIT_PARSED: no resources to release */

	if (pc->pc_init_state & PCC_INIT_SYNCH) {
		cv_destroy(&pc->pc_cv);
		mutex_destroy(&pc->pc_lock);
		mutex_destroy(&pc->pc_state_lock);
	}

	/*
	 * Clear init_state only.
	 */
	pc->pc_init_state = 0;
}

/*
 * Reverse-order cleanup of all PCC state.
 */
static void
pcc_teardown(void)
{
	pcc_chan_t *pc;

	ASSERT(pcc_initialised);

	/*
	 * Tear down channels in reverse parse order.
	 */
	for (pc = list_tail(&pcc_chan_list);
	    pc != NULL;
	    pc = list_prev(&pcc_chan_list, pc)) {
		pcc_chan_cleanup(pc);
	}

	/*
	 * Final sweep of the irq group list - all should be gone.
	 */
	pcc_irq_groups_destroy();

	/*
	 * The register registry must be empty by this point.
	 */
	VERIFY(pcc_reg_is_empty());

	/*
	 * Free the channels; the list is empty when no PCCT was found.
	 */
	pcc_initialised = B_FALSE;
	membar_producer();

	while ((pc = list_remove_head(&pcc_chan_list)) != NULL) {
		kmem_free(pc, sizeof (*pc));
	}
}

/*
 * Validate the shared-memory signature.
 *
 * The PCC signature is PCC_SIGNATURE_BASE | subspace_id, except RASF which
 * uses the spec-defined ASCII "RASF" (0x52415346).
 *
 * The pcc_strictness tunable controls tolerance for firmware bugs:
 *   0 = accept any PCC_SIGNATURE_BASE signature
 *   1 = accept off-by-one index (Ampere Altra subspaces 12-14)
 *   2 = exact match only
 */
static int
pcc_check_signature(pcc_chan_t *pc)
{
	uint32_t expected_sig;
	uint32_t sig;
	uint32_t sig_base;
	uint32_t sig_idx;
	int32_t idx_delta;

	expected_sig = PCC_SIGNATURE_BASE | pc->pc_id;
	sig = *(volatile uint32_t *)(void *)pc->pc_shmem_va;

	if (sig == expected_sig) {
		return (DDI_SUCCESS);
	}

	/*
	 * RASF uses a spec-defined ASCII signature, not the PCC
	 * formula (ACPI 6.6 Table 5.87).  Accept it at all
	 * strictness levels.
	 */
	if (sig == PCC_SIGNATURE_RASF) {
		return (DDI_SUCCESS);
	}

	sig_base = sig & ~0xffU;
	sig_idx = sig & 0xffU;
	idx_delta = (int32_t)sig_idx - (int32_t)pc->pc_id;

	if (sig_base != PCC_SIGNATURE_BASE) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u signature mismatch "
		    "(0x%x, expected 0x%x)", pc->pc_id, sig, expected_sig);
		return (DDI_FAILURE);
	}

	if (pcc_strictness >= 2) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u signature mismatch "
		    "(0x%x, expected 0x%x)", pc->pc_id, sig, expected_sig);
		return (DDI_FAILURE);
	}

	if (pcc_strictness >= 1 && (idx_delta < -1 || idx_delta > 1)) {
		cmn_err(CE_WARN, "!acpipcc: subspace %u signature index too "
		    "far (0x%x, expected 0x%x)", pc->pc_id, sig, expected_sig);
		return (DDI_FAILURE);
	}

	/*
	 * Warn for strictness 0 and 1.
	 */
	cmn_err(CE_NOTE, "!acpipcc: subspace %u signature index mismatch "
	    "(0x%x, expected 0x%x)", pc->pc_id, sig, expected_sig);
	return (DDI_SUCCESS);
}

/*
 * Walk the PCCT, validate, and bring up channels.
 *
 * The PCCT is a flat array of variable-length subtables following
 * a fixed header.  Walk the subtables with manual pointer arithmetic,
 * validating length at every step.
 *
 * The state of the PCCT represents a trust boundary: firmware-description
 * failures fail all of the PCC module.  OS-side failures mark only that channel
 * as unusable.
 */
static int
pcc_parse_pcct(void)
{
	ACPI_STATUS status;
	ACPI_TABLE_HEADER *hdr;
	ACPI_TABLE_PCCT *pcct;
	ACPI_SUBTABLE_HEADER *sub_hdr;
	uint8_t *tbl, *end, *pos;
	uint_t idx = 0;
	uint_t d;
	boolean_t tbl_irq_capable;
	const pcc_type_ops_t *ops;
	size_t min_len;
	pcc_chan_t *pc;
	int ret;

	status = AcpiGetTable(ACPI_SIG_PCCT, 1, &hdr);
	if (ACPI_FAILURE(status)) {
		/* No PCCT is not an error, just no subspaces */
		return (DDI_SUCCESS);
	}

	if (hdr->Length < sizeof (ACPI_TABLE_PCCT)) {
		cmn_err(CE_WARN, "!acpipcc: PCCT too short (%u)", hdr->Length);
		return (DDI_FAILURE);
	}

	pcct = (ACPI_TABLE_PCCT *)(void *)hdr;

	if (pcct->Header.Revision < 1) {
		cmn_err(CE_WARN, "!acpipcc: PCCT revision %u unsupported "
		    "(expected >= 1)", pcct->Header.Revision);
		return (DDI_FAILURE);
	}

	if (pcct->Header.Revision > 2) {
		cmn_err(CE_NOTE, "!acpipcc: PCCT revision %u unknown, "
		    "assuming compatibility with revision 2",
		    pcct->Header.Revision);
	}

	/*
	 * ACPI 6.6 Table 14.2: Global Flags bits 1-31 are reserved and must
	 * be zero.  Non-zero reserved bits indicate firmware using a newer
	 * spec or corrupt data.
	 */
	if ((pcct->Flags & ~ACPI_PCCT_DOORBELL) != 0) {
		if (pcct->Header.Revision > 2) {
			cmn_err(CE_NOTE, "!acpipcc: PCCT global flags unknown "
			    "bits set (0x%x) in newer PCCT revision %u",
			    pcct->Flags, pcct->Header.Revision);
		} else {
			cmn_err(CE_WARN, "!acpipcc: PCCT global flags reserved "
			    "bits set (0x%x)", pcct->Flags);
			return (DDI_FAILURE);
		}
	}

	tbl = (uint8_t *)(void *)hdr;
	end = tbl + hdr->Length;
	pos = tbl + sizeof (ACPI_TABLE_PCCT);

	/*
	 * Table-level platform interrupt flag - if this is not set then
	 * neither interrupt-driven completion nor notifications can work.
	 */
	tbl_irq_capable = (pcct->Flags & ACPI_PCCT_DOORBELL) != 0;

	while (pos + sizeof (ACPI_SUBTABLE_HEADER) <= end) {
		sub_hdr = (ACPI_SUBTABLE_HEADER *)(void *)pos;
		if (sub_hdr->Length == 0 || pos + sub_hdr->Length > end) {
			cmn_err(CE_WARN, "!acpipcc: corrupt PCCT subtable %u "
			    "(length %u)", idx, sub_hdr->Length);
			goto fail_all;
		}

		/*
		 * Shared memory signatures OR in an 8 bit value, so no more
		 * than 256 channels can be supported.
		 */
		if (idx >= PCC_MAX_CHANNELS) {
			cmn_err(CE_WARN, "!acpipcc: PCCT describes more "
			    "than %u subspaces", PCC_MAX_CHANNELS);
			goto fail_all;
		}

		/*
		 * Allocate the channel basics and link the channel into
		 * the channel list.
		 */
		pc = kmem_zalloc(sizeof (*pc), KM_SLEEP);
		mutex_init(&pc->pc_lock, NULL, MUTEX_DEFAULT, NULL);
		mutex_init(&pc->pc_state_lock, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&pc->pc_cv, NULL, CV_DEFAULT, NULL);
		pc->pc_init_state = PCC_INIT_SYNCH;
		pc->pc_idx = idx;
		list_insert_tail(&pcc_chan_list, pc);
		/* pc_id is set by the pto_parse callback */

		/*
		 * Look up the type ops and minimum length in the dispatch
		 * table.
		 */
		ops = NULL;
		min_len = 0;
		for (d = 0; d < ARRAY_SIZE(pcc_subspace_dispatch); d++) {
			if (pcc_subspace_dispatch[d].psd_type ==
			    sub_hdr->Type) {
				ops = pcc_subspace_dispatch[d].psd_ops;
				min_len = pcc_subspace_dispatch[d].psd_min_len;
				break;
			}
		}

		if (ops == NULL) {
			/*
			 * Types 0, 4 and 5 are not implemented by this driver.
			 * Types above 5 are not defined by ACPI 6.6 and are
			 * therefore unsupported by this driver.
			 */
			cmn_err(CE_NOTE, "!acpipcc: ignoring subspace %u: "
			    "%s type %u", idx,
			    (sub_hdr->Type == 0 || sub_hdr->Type == 4 ||
				sub_hdr->Type == 5) ?
				"unsupported" :
				"unknown",
			    sub_hdr->Type);
			pc->pc_valid = B_FALSE;
			goto next;
		}

		pc->pc_ops = ops;
		pc->pc_type = sub_hdr->Type;

		if (sub_hdr->Length < min_len) {
			cmn_err(CE_WARN, "!acpipcc: subspace %u type %u too "
			    "short (%u < %lu)",
			    idx, sub_hdr->Type, sub_hdr->Length,
			    (ulong_t)min_len);
			pc->pc_valid = B_FALSE;
			goto next;
		}

		/* type-specific parse */
		if (pc->pc_ops->pto_parse(sub_hdr, pc) != DDI_SUCCESS) {
			pc->pc_valid = B_FALSE;
			goto next;
		}
		pc->pc_init_state |= PCC_INIT_PARSED;

		/* firmware description was valid */
		pc->pc_valid = B_TRUE;

		/*
		 * Set pc_has_irq iff the channel has a GSIV and the
		 * table-level platform interrupt flag is set.
		 */
		pc->pc_has_irq = tbl_irq_capable && (pc->pc_gsiv != 0);

		/*
		 * ACPI 6.6 §14.1.4: Type 1 subspaces do not support a level
		 * triggered platform interrupt as no method is provided to
		 * clear the interrupt.
		 *
		 * If the type only supports edge-triggered interrupts and
		 * the platform expressed a level-triggered interrupt we
		 * fall back to polling mode.
		 */
		if (pc->pc_has_irq && pc->pc_ops->pto_edge_only_irq &&
		    (pc->pc_irq_flags & ACPI_PCCT_INTERRUPT_MODE) == 0) {
			cmn_err(CE_NOTE, "!acpipcc: chan %u: Type 1 subspace "
			    "with level-triggered platform interrupt - using "
			    "polling mode", idx);
			pc->pc_has_irq = B_FALSE;
		}

		/*
		 * ACPI 6.6 Table 14.7: for Type 2/3/4 subspaces: if the
		 * subspace does support interrupts, and these are level,
		 * [the Platform Interrupt Ack register] must be supplied.
		 *
		 * Also, when the ack register is present and the AckWriteMask
		 * is zero we can't ack the interrupt.
		 *
		 * When these conditions occur we can't trust the firmware.
		 */
		if (pc->pc_has_irq &&
		    (pc->pc_type == ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2 ||
		    pc->pc_type == ACPI_PCCT_TYPE_EXT_PCC_MASTER_SUBSPACE ||
		    pc->pc_type == ACPI_PCCT_TYPE_EXT_PCC_SLAVE_SUBSPACE) &&
		    (pc->pc_irq_flags & ACPI_PCCT_INTERRUPT_MODE) == 0 &&
		    (pc->pc_ack_gas.Address == 0 || pc->pc_ack_set == 0)) {
			cmn_err(CE_WARN, "!acpipcc: chan %u: level-triggered "
			    "platform interrupt without usable ack register",
			    idx);
			pc->pc_valid = B_FALSE;
			goto fail_chan;
		}

		/*
		 * Type-specific register mapping - if this fails it's an
		 * OS error, so only the channel fails.
		 */
		if (pc->pc_ops->pto_map(pc) != DDI_SUCCESS) {
			goto fail_chan;
		}
		pc->pc_init_state |= PCC_INIT_MAPPED;

		/*
		 * Shared memory communication area mapping - if this fails
		 * it's an OS error, so only the channel fails.
		 */
		pc->pc_shmem_va = pcc_map_shmem(pc->pc_shmem_pa,
		    pc->pc_shmem_len);
		if (pc->pc_shmem_va == NULL) {
			goto fail_chan;
		}
		pc->pc_init_state |= PCC_INIT_SHMEM;

		if (pcc_check_signature(pc) != DDI_SUCCESS) {
			pc->pc_valid = B_FALSE;
			goto fail_chan;
		}
		pc->pc_init_state |= PCC_INIT_SIG_OK;

		if (pc->pc_ops->pto_init != NULL) {
			ret = pc->pc_ops->pto_init(pc);
			if (ret == PCC_INIT_UNUSABLE) {
				goto fail_chan;
			}
			if (ret != DDI_SUCCESS) {
				pc->pc_valid = B_FALSE;
				goto fail_chan;
			}
		}
		pc->pc_init_state |= PCC_INIT_TYPE;

		/*
		 * Set up the channel interrupt as needed.
		 */
		if (pc->pc_has_irq) {
			ret = pcc_irq_group_add(
			    pc->pc_gsiv, pc->pc_irq_flags, pc);
			if (ret == EBUSY) {
				/*
				 * Interrupt is already set up with a different
				 * trigger, or the ack mask conflicts with
				 * another channel.
				 */
				cmn_err(CE_WARN, "!acpipcc: chan %u: "
				    "inconsistent interrupt description for "
				    "GSIV %u", idx, pc->pc_gsiv);
				pc->pc_valid = B_FALSE;
				goto fail_chan;
			}

			if (ret != 0) {
				/*
				 * OS-level interrupt installation failure.
				 */
				cmn_err(CE_WARN, "!acpipcc: chan %u: interrupt "
				    "installation failed; using polling-only "
				    "mode", idx);
				pc->pc_has_irq = B_FALSE;
			} else {
				pc->pc_init_state |= PCC_INIT_IRQ_LINKED;
			}
		}

		pc->pc_usable = B_TRUE;
		goto next;

fail_chan:
		pcc_chan_cleanup(pc);

next:
		pos += sub_hdr->Length;
		idx++;
	}

	/*
	 * The correct way for the above loop to end is when the pos cursor
	 * reaches the end of the data described by the table length.  The loop
	 * will also exit when fewer bytes remain than are needed for a
	 * subtable header structure - in this case the table is malformed
	 * and the firmware cannot be trusted.
	 */
	if (pos != end) {
		cmn_err(CE_WARN, "!acpipcc: PCCT has %lu trailing bytes",
		    (ulong_t)(end - pos));
		goto fail_all;
	}

	/*
	 * If any supported channel failed firmware validation (indicated by
	 * the pc_valid boolean) the firmware description cannot be trusted.
	 */
	for (pc = list_head(&pcc_chan_list);
	    pc != NULL;
	    pc = list_next(&pcc_chan_list, pc)) {
		if (pc->pc_ops != NULL && !pc->pc_valid) {
			cmn_err(CE_WARN, "!acpipcc: supported subspace %u "
			    "(type %u) failed to initialise",
			    pc->pc_idx, pc->pc_type);
			goto fail_all;
		}
	}

	/*
	 * A PCCT with no subspaces (or no supported subspaces) is not
	 * an error - the channel list will simply be empty.
	 */

	return (DDI_SUCCESS);

fail_all:
	/*
	 * Tear down channels in reverse order.
	 *
	 * Each channel will unlink from any interrupt group it may be
	 * referencing, which has the side effect of uninstalling the
	 * interrupt handler when the final interrupt reference is
	 * dropped.
	 */
	for (pc = list_tail(&pcc_chan_list);
	    pc != NULL;
	    pc = list_prev(&pcc_chan_list, pc)) {
		pcc_chan_cleanup(pc);
	}

	/*
	 * Final sweep of the irq group list.  All groups should already
	 * be gone (via pcc_chan_cleanup above), so this will warn for any
	 * lingering groups and finish any remaining cleanup.
	 */
	pcc_irq_groups_destroy();

	/*
	 * Finally, clean up the memory allocated to channel list entries.
	 */
	while ((pc = list_remove_head(&pcc_chan_list)) != NULL) {
		kmem_free(pc, sizeof (*pc));
	}

	return (DDI_FAILURE);
}

/*
 * Initialise PCC by parsing the ACPI PCCT, bringing up supported PCC channels.
 */
static int
pcc_init(void)
{
	int ret;

	if (pcc_initialised) {
		return (DDI_SUCCESS);
	}

	mutex_enter(&pcc_init_lock);

	if (pcc_initialised) {
		mutex_exit(&pcc_init_lock);
		return (DDI_SUCCESS);
	}

	ret = pcc_parse_pcct();
	if (ret == DDI_SUCCESS) {
		membar_producer();
		pcc_initialised = B_TRUE;
	}

	mutex_exit(&pcc_init_lock);
	return (ret);
}

static struct modlmisc modlmisc = {
	.misc_modops	= &mod_miscops,
	.misc_linkinfo	= "ACPI PCC channel driver"
};

static struct modlinkage modlinkage = {
	.ml_rev		= MODREV_1,
	.ml_linkage	= { &modlmisc, NULL }
};

int
_init(void)
{
	int err;

	err = mod_install(&modlinkage);
	if (err != 0) {
		return (err);
	}

	/*
	 * Initialise module-level locks and lists before any channel
	 * bring-up needs them.
	 */
	mutex_init(&pcc_init_lock, NULL, MUTEX_DEFAULT, NULL);
	list_create(&pcc_chan_list, sizeof (pcc_chan_t),
	    offsetof(pcc_chan_t, pc_node));
	pcc_reg_init();
	pcc_irq_init();

	/*
	 * Initialise PCC, parsing the PCCT to bring up PCC channels.
	 */
	if (pcc_init() != DDI_SUCCESS) {
		pcc_irq_fini();
		pcc_reg_fini();
		list_destroy(&pcc_chan_list);
		mutex_destroy(&pcc_init_lock);
		(void) mod_remove(&modlinkage);
		return (-1);
	}

	return (0);
}

int
_fini(void)
{
	int err;

	if ((err = mod_remove(&modlinkage)) != 0) {
		return (err);
	}

	if (pcc_initialised) {
		pcc_teardown();
	}

	pcc_irq_fini();
	pcc_reg_fini();
	list_destroy(&pcc_chan_list);
	mutex_destroy(&pcc_init_lock);

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
