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

#ifndef	_ACPICA_PCC_IMPL_H
#define	_ACPICA_PCC_IMPL_H

/*
 * PCC (Platform Communications Channel) internal header.
 *
 * The public API is in <sys/acpipcc.h>.
 */

#include <sys/types.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/list.h>
#include <sys/time.h>
#include <sys/acpi/acpi.h>
#include <sys/acpipcc.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Shared memory header layouts (ACPI 6.6 Chapter 14).  Offsets are relative to
 * the start of the channel's shared memory region.
 */

/* Type 0-2 generic communications channel header (8 bytes). */
#define	PCC_SHMEM_SIGNATURE	0	/* UINT32 */
#define	PCC_SHMEM_CMD		4	/* UINT16 */
#define	PCC_SHMEM_STATUS	6	/* UINT16 */
#define	PCC_SHMEM_HDR_LEN	8

/* Type 3 extended PCC header (16 bytes). */
#define	PCC_EXT_SHMEM_SIGNATURE	0	/* UINT32 */
#define	PCC_EXT_SHMEM_FLAGS	4	/* UINT32 */
#define	PCC_EXT_SHMEM_LENGTH	8	/* UINT32 */
#define	PCC_EXT_SHMEM_CMD	12	/* UINT32 */
#define	PCC_EXT_SHMEM_HDR_LEN	16

/*
 * Generic shared memory Status field (UINT16 at PCC_SHMEM_STATUS).
 */
#define	PCC_STATUS_CMD_COMPLETE		(1u << 0)
#define	PCC_STATUS_PLATFORM_IRQ		(1u << 1) /* SCI(T0)/doorbell(T1-2) */
#define	PCC_STATUS_ERROR		(1u << 2)
#define	PCC_STATUS_PLATFORM_NOTIFY	(1u << 3) /* deprecated notification */

/*
 * Generic shared memory Command field (UINT16 at PCC_SHMEM_CMD).
 *
 * ACPI 6.6 Table 14.10: bits 0-7 are the command code, bits 8-14 are
 * reserved, bit 15 is Notify on Completion.
 */
#define	PCC_CMD_NOTIFY_ON_COMPLETE	(1u << 15)

/*
 * Extended shared memory Flags field (UINT32 at PCC_EXT_SHMEM_FLAGS).
 */
#define	PCC_EXT_FLAG_NOTIFY_ON_COMPLETE	(1u << 0)

/*
 * Signature base value.  The per-subspace signature is
 * PCC_SIGNATURE_BASE | subspace_id, where subspace_id is the firmware subspace
 * ID (the PCCT subtable index).  Some consumers (e.g. RASF) define their own
 * signatures instead.
 */
#define	PCC_SIGNATURE_BASE		0x50434300u

/* Microseconds between completion poll loops */
#define	PCC_POLL_INTERVAL_US	10
/*
 * Nominal latency multiplier for the completion timeout.  500x nominal matches
 * Linux.  A platform that exceeds its own advertised latency by that margin
 * is treated as failed, and the channel is marked failed so that later sends
 * fail fast.
 */
#define	PCC_TIMEOUT_MULTIPLIER	500
/*
 * Completion timeout upper bound.
 *
 * The largest nominal latency we've seen is 1000us, a 0.5s timeout
 * at the multiplier above.  Five seconds (this value) is 10x that.
 *
 * This is a cap to deal with delinquent firmware that may set hours
 * or days of nominal latency.
 */
#define	PCC_MAX_TIMEOUT		(5 * NANOSEC)
/*
 * Consecutive unclaimed interrupts on one GSIV before the storm fail-stop
 * fires, after which member channels fall back to polling.
 */
#define	PCC_STORM_THRESHOLD	50
/* Maximum number of PCC subspaces in the PCCT */
#define	PCC_MAX_CHANNELS	256

/*
 * Upper bound on the gap enforced between commands.  The gap is derived
 * from firmware-supplied values and is enforced with a busy wait under
 * the channel lock, so is capped to a generous default to avoid issues
 * from delinquent firmware.
 */
#define	PCC_MAX_TIMING_WAIT	(2 * NANOSEC)

/*
 * RASF uses a spec-defined ASCII signature (ACPI 6.6 Table 5.87),
 * not the standard PCC signature derivation.
 */
#define	PCC_SIGNATURE_RASF	0x52415346u	/* "RASF" */

/*
 * Returned from type-specific per-channel initialisation to indicate that the
 * firmware description is valid, but the channel is unusable.
 */
#define	PCC_INIT_UNUSABLE	1

/*
 * Per-channel initialisation state flags (pc_init_state bitmask).
 *
 * Guides teardown of a partially (or fully) initialised channel.
 */
#define	PCC_INIT_PARSED		(1u << 0)	/* pto_parse succeeded */
#define	PCC_INIT_SHMEM		(1u << 1)	/* pc_shmem_va mapped */
#define	PCC_INIT_MAPPED		(1u << 2)	/* pto_map succeeded */
#define	PCC_INIT_SIG_OK		(1u << 3)	/* signature validated */
#define	PCC_INIT_TYPE		(1u << 4)	/* pto_init succeeded */
#define	PCC_INIT_IRQ_LINKED	(1u << 5)	/* linked into irq group */
#define	PCC_INIT_SYNCH		(1u << 6)	/* mutex/cv initialised */

/*
 * Interrupt check result flags.
 */
typedef enum pcc_irq_result {
	PCC_IRQ_NONE		= 0,		/* not for this channel */
	PCC_IRQ_CMD_COMPLETE	= (1 << 0),	/* command response ready */
	PCC_IRQ_NOTIFY		= (1 << 1),	/* platform notification set */
	PCC_IRQ_PLATFORM_IRQ	= (1 << 2)	/* Platform Interrupt bit set */
} pcc_irq_result_t;

/*
 * Register registry entry.
 *
 * Shared doorbell/ack/command-complete/error registers are reference-counted
 * so that multiple channels using the same physical register share a
 * single mapping.
 */
typedef struct pcc_reg_entry {
	ACPI_GENERIC_ADDRESS	pre_gas;	/* register description */
	caddr_t		pre_va;		/* mapped VA (MMIO only) */
	kmutex_t	pre_lock;	/* RMW serialization */
	uint_t		pre_refcnt;	/* number of channels using this reg */
	list_node_t	pre_node;	/* linkage in pcc_reg_list */
} pcc_reg_entry_t;

/*
 * Per-GSIV interrupt group.
 *
 * Channels sharing a platform interrupt GSIV are linked into a group.  The
 * interrupt handler receives the group as its argument and walks only the
 * channels that could have fired.
 *
 * - pig_gsiv: the shared GSIV
 * - pig_edge: true if the GSIV is edge-triggered
 * - pig_ipl: GSIV priority level
 * - pig_refcnt: number of channels using this GSIV
 * - pig_active: number of invocations in flight (not yet returned)
 * - pig_storm: number of unclaimed handler invocations - reset on claim
 * - pig_storm_tripped: true if the storm fail-stop has been scheduled
 * - pig_storm_tid: timeout ID for the pending storm fail-stop timeout
 * - pig_channels: list of pcc_chan_t on this GSIV
 * - pig_node: linkage in pcc_irq_group_list
 */
typedef struct pcc_irq_group {
	uint32_t	pig_gsiv;
	boolean_t	pig_edge;
	uint_t		pig_ipl;
	uint_t		pig_refcnt;
	uint32_t	pig_active;
	uint32_t	pig_storm;
	boolean_t	pig_storm_tripped;
	timeout_id_t	pig_storm_tid;
	list_t		pig_channels;
	list_node_t	pig_node;
} pcc_irq_group_t;

/*
 * Type-specific operations vector.
 *
 * - pto_parse: extract fields from the ACPI subtable into a channel structure
 * - pto_map: map MMIO for channel registers
 * - pto_init: post-map initialisation, can be NULL
 * - pto_unmap: undo what pto_map did
 * - pto_txn_begin: establish ownership of the communication space
 * - pto_txn_commit: populate type-specific shared memory header and initiate
 *   ownership transfer to the platform
 * - pto_poll_complete: poll for command completion
 * - pto_check_error: check for error status in the channel's shared memory
 * - pto_clear_platform_irq: clear the platform interrupt bit in the channel's
 *   shared memory
 * - pto_irq_check: read-only channel ownership/status peek
 * - pto_edge_only_irq: true if the type's platform interrupt must
 *   be edge-triggered
 */
typedef struct pcc_type_ops {
	int	(*pto_parse)(void *subtable, pcc_chan_t *pc);
	int	(*pto_map)(pcc_chan_t *pc);
	int	(*pto_init)(pcc_chan_t *pc);
	void	(*pto_unmap)(pcc_chan_t *pc);
	int	(*pto_txn_begin)(pcc_chan_t *pc);
	int	(*pto_txn_commit)(pcc_chan_t *pc, uint32_t cmd,
		    uint32_t payload_len);
	int	(*pto_poll_complete)(pcc_chan_t *pc);
	int	(*pto_check_error)(pcc_chan_t *pc);
	void	(*pto_clear_platform_irq)(pcc_chan_t *pc);
	int	(*pto_irq_check)(pcc_chan_t *pc,
		    pcc_irq_result_t *result);
	boolean_t pto_edge_only_irq;
} pcc_type_ops_t;

/*
 * PCC channel structure
 *
 * One per subspace in the PCCT.  Consumers get an opaque handle to this
 * structure.
 *
 * - pc_id: the firmware subspace ID (the PCCT subtable index)
 * - pc_idx: index in the PCCT subspace list
 * - pc_type: the ACPI subspace type (1-3)
 * - pc_valid: true if the firmware description is valid
 * - pc_usable: true if the OS bring-up completed
 * - pc_refcnt: number of consumers of this channel via pcc_chan_get
 * - pc_ops: type-specific operations vector
 * - pc_init_state: bitmask of completed init phases (PCC_INIT_*)
 * - pc_shmem_pa: physical address of the shared memory region
 * - pc_shmem_len: length of the shared memory region from PCCT
 * - pc_shmem_va: mapped virtual address of the shared memory region
 * - pc_hdr_len: 8 for Types 0-2, 16 for Types 3-4
 * - pc_signature: expected PCC signature
 * - pc_db_gas: GAS description stored by pto_parse, consumed by pto_map
 * - pc_ack_gas: GAS description stored by pto_parse, consumed by pto_map
 * - pc_cc_check_gas: GAS description stored by pto_parse, consumed by pto_map
 * - pc_cc_update_gas: GAS description stored by pto_parse, consumed by pto_map
 * - pc_err_gas: GAS description stored by pto_parse, consumed by pto_map
 * - pc_db_reg: doorbell register
 * - pc_db_preserve: doorbell preserve mask
 * - pc_db_set: doorbell set mask
 * - pc_ack_reg: ack register (Types 2, 3)
 * - pc_ack_preserve: ack preserve mask
 * - pc_ack_set: ack set mask
 * - pc_cc_check_reg: command complete check register (Types 3, 4)
 * - pc_cc_check_mask: command complete check mask
 * - pc_cc_update_reg: command complete update register (Types 3, 4)
 * - pc_cc_update_preserve: command complete update preserve mask
 * - pc_cc_update_set: command complete update set mask
 * - pc_err_reg: error status register (Types 3, 4)
 * - pc_err_mask: error status mask
 * - pc_gsiv: platform interrupt GSIV
 * - pc_irq_flags: edge/level, polarity
 * - pc_has_irq: true if GSIV *and* table IRQ flag set
 * - pc_irq_node: linkage in pcc_irq_group_t
 * - pc_node: linkage in pcc_chan_list
 * - pc_nominal_lat: nominal command latency (usec)
 * - pc_max_rate: max periodic access rate (cmds/min, 0=unlimited)
 * - pc_turnaround: min turnaround (usec)
 * - pc_last_complete: hrtime of last observed command completion
 * - pc_broken: true if a command timed out and the platform is not responding
 * - pc_lock: channel mutex - held if a transaction is in progress
 * - pc_cv: completion condition variable
 * - pc_cmd_complete: true if the command is complete - this is the predicate
 *   for pcc_wait_irq
 * - pc_cmd_outstanding: true if a command is issued (shared memory ownership
 *   transferred to the platform) - remains true until the command completion
 *   is observed
 * - pc_state_lock: guards interrupt handoff state
 */
typedef struct pcc_chan {
	/* Lifecycle and identity */
	uint32_t		pc_id;
	uint32_t		pc_idx;
	uint8_t			pc_type;
	boolean_t		pc_valid;
	boolean_t		pc_usable;
	uint_t			pc_refcnt;
	const pcc_type_ops_t	*pc_ops;
	uint_t			pc_init_state;

	/* Shared memory */
	uint64_t		pc_shmem_pa;
	uint64_t		pc_shmem_len;
	caddr_t			pc_shmem_va;
	uint32_t		pc_hdr_len;
	uint32_t		pc_signature;

	/* GAS register descriptions */
	ACPI_GENERIC_ADDRESS	pc_db_gas;
	ACPI_GENERIC_ADDRESS	pc_ack_gas;
	ACPI_GENERIC_ADDRESS	pc_cc_check_gas;
	ACPI_GENERIC_ADDRESS	pc_cc_update_gas;
	ACPI_GENERIC_ADDRESS	pc_err_gas;

	/* Doorbell */
	pcc_reg_entry_t		*pc_db_reg;
	uint64_t		pc_db_preserve;
	uint64_t		pc_db_set;

	/* Ack register (Types 2, 3) */
	pcc_reg_entry_t		*pc_ack_reg;
	uint64_t		pc_ack_preserve;
	uint64_t		pc_ack_set;

	/* Command Complete registers (Types 3, 4) */
	pcc_reg_entry_t		*pc_cc_check_reg;
	uint64_t		pc_cc_check_mask;
	pcc_reg_entry_t		*pc_cc_update_reg;
	uint64_t		pc_cc_update_preserve;
	uint64_t		pc_cc_update_set;

	/* Error Status register (Types 3, 4) */
	pcc_reg_entry_t		*pc_err_reg;
	uint64_t		pc_err_mask;

	/* Interrupt */
	uint32_t		pc_gsiv;
	uint8_t			pc_irq_flags;
	boolean_t		pc_has_irq;

	/* Linkage */
	list_node_t		pc_irq_node;
	list_node_t		pc_node;

	/* Timing */
	uint32_t		pc_nominal_lat;
	uint32_t		pc_max_rate;
	uint32_t		pc_turnaround;
	hrtime_t		pc_last_complete;

	/* Channel health */
	boolean_t		pc_broken;

	/* Synchronization */
	kmutex_t		pc_lock;
	kcondvar_t		pc_cv;
	boolean_t		pc_cmd_complete;
	boolean_t		pc_cmd_outstanding;
	kmutex_t		pc_state_lock;
} pcc_chan_t;

/*
 * Physical register identity is (Address, SpaceId, BitWidth).
 */
#define	PCC_GAS_WIDTH_MISMATCH(a, b) \
	((a).Address != 0 && (b).Address != 0 && \
	(a).Address == (b).Address && (a).SpaceId == (b).SpaceId && \
	(a).BitWidth != (b).BitWidth)

/*
* Register aliasing check.
 */
#define	PCC_GAS_ALIASES(a, b) \
	((a).Address != 0 && (b).Address != 0 && \
	(a).Address == (b).Address && (a).SpaceId == (b).SpaceId)

/*
 * Partial register overlap: the two descriptions cover some of the
 * same bytes without describing the same register.
 */
#define	PCC_GAS_OVERLAPS(a, b) \
	((a).Address != 0 && (b).Address != 0 && \
	(a).SpaceId == (b).SpaceId && \
	((a).Address != (b).Address || (a).BitWidth != (b).BitWidth) && \
	(a).Address < (b).Address + (b).BitWidth / 8 && \
	(b).Address < (a).Address + (a).BitWidth / 8)

/*
 * Type ops vectors, defined non-static in their type files.
 */
extern const pcc_type_ops_t pcc_type1_ops;
extern const pcc_type_ops_t pcc_type2_ops;
extern const pcc_type_ops_t pcc_type3_ops;

/*
 * Module-private helpers.
 */

/* GAS validation */
extern int pcc_validate_gas(pcc_chan_t *pc,
    const char *name, ACPI_GENERIC_ADDRESS *gas);

/* Register registry */
extern pcc_reg_entry_t *pcc_reg_lookup_or_create(
    const ACPI_GENERIC_ADDRESS *gas);
extern void pcc_reg_release(pcc_reg_entry_t **regp);
extern void pcc_reg_read(pcc_reg_entry_t *reg, uint64_t *val);
extern void pcc_reg_write(pcc_reg_entry_t *reg, uint64_t val);
extern void pcc_reg_rmw(pcc_reg_entry_t *reg, uint64_t preserve, uint64_t set);
extern void pcc_reg_rmw_locked(pcc_reg_entry_t *reg, uint64_t preserve,
    uint64_t set);

/* Doorbell and interrupt ACK */
extern void pcc_ring_doorbell(pcc_chan_t *pc);
extern void pcc_write_ack(pcc_chan_t *pc);

/* Interrupt group management */
extern int pcc_irq_group_add(uint32_t gsiv, uint8_t flags, pcc_chan_t *pc);
extern void pcc_irq_group_remove(pcc_chan_t *pc);
extern void pcc_irq_groups_destroy(void);

/* Shared memory mapping (KPM) */
extern caddr_t pcc_map_shmem(uint64_t pa, uint64_t len);

/* Type-specific helpers called across files */
extern void pcc_type12_clear_notify(pcc_chan_t *pc);

/* Utilities */
extern hrtime_t pcc_cmd_timeout(pcc_chan_t *pc);

extern void pcc_irq_init(void);
extern void pcc_irq_fini(void);
extern void pcc_reg_init(void);
extern void pcc_reg_fini(void);
extern boolean_t pcc_reg_is_empty(void);

extern int pcc_wait_irq(pcc_chan_t *pc);

#ifdef __cplusplus
}
#endif

#endif	/* _ACPICA_PCC_IMPL_H */
