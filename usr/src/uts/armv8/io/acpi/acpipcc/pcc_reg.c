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
 * Register registry: deduplicates doorbell/ack/cmd-complete/error
 * registers by GAS identity (Address, SpaceId, BitWidth, BitOffset).
 *
 * Multiple subspaces may share a physical register, each owning different
 * bits via masks.
 */

#include <sys/types.h>
#include <sys/stddef.h>
#include <sys/smp_impldefs.h>
#include "pcc_impl.h"

static list_t pcc_reg_list;		/* global register registry list */
static kmutex_t pcc_reg_list_lock;	/* protects pcc_reg_list */

void
pcc_reg_init(void)
{
	mutex_init(&pcc_reg_list_lock, NULL, MUTEX_DEFAULT, NULL);
	list_create(&pcc_reg_list, sizeof (pcc_reg_entry_t),
	    offsetof(pcc_reg_entry_t, pre_node));
}

boolean_t
pcc_reg_is_empty(void)
{
	boolean_t empty;

	mutex_enter(&pcc_reg_list_lock);
	empty = list_is_empty(&pcc_reg_list);
	mutex_exit(&pcc_reg_list_lock);

	return (empty);
}

void
pcc_reg_fini(void)
{
	list_destroy(&pcc_reg_list);
	mutex_destroy(&pcc_reg_list_lock);
}

/*
 * Find or create a registry entry.
 *
 * Looks up a register by GAS identity (Address, SpaceId, BitWidth,
 * BitOffset).  If found, bumps the refcnt and returns the existing
 * register.  When not found, allocates, maps (MMIO only), and links it
 * before returning it with a held reference.
 *
 * The GAS AccessWidth is an access hint, not a property of the register.  If
 * we were to include it in the identity we would lose cross-channel RMW
 * serialisation.
 *
 * Returns NULL on failure: FFH (no spec-defined invocation convention),
 * System I/O (no port I/O on aarch64), or a mapping failure.
 */
pcc_reg_entry_t *
pcc_reg_lookup_or_create(const ACPI_GENERIC_ADDRESS *gas)
{
	pcc_reg_entry_t *reg;

	if (gas->Address == 0) {
		return (NULL);
	}

	if (gas->SpaceId == ACPI_ADR_SPACE_FIXED_HARDWARE) {
		cmn_err(CE_WARN, "!acpipcc: FFH register at 0x%llx unsupported "
		    "(no spec-defined PCC invocation convention)",
		    (unsigned long long)gas->Address);
		return (NULL);
	}

	if (gas->SpaceId == ACPI_ADR_SPACE_SYSTEM_IO) {
		cmn_err(CE_WARN, "!acpipcc: System I/O register at 0x%llx "
		    "unsupported (no port I/O on aarch64)",
		    (unsigned long long)gas->Address);
		return (NULL);
	}

	if (gas->SpaceId != ACPI_ADR_SPACE_SYSTEM_MEMORY) {
		cmn_err(CE_WARN, "!acpipcc: register at 0x%llx has invalid "
		    "address space %u", (unsigned long long)gas->Address,
		    gas->SpaceId);
		return (NULL);
	}

	if (gas->BitWidth != 32 && gas->BitWidth != 64) {
		cmn_err(CE_WARN, "!acpipcc: register at 0x%llx has invalid "
		    "width %u", (unsigned long long)gas->Address,
		    gas->BitWidth);
		return (NULL);
	}

	mutex_enter(&pcc_reg_list_lock);
	for (reg = list_head(&pcc_reg_list);
	    reg != NULL;
	    reg = list_next(&pcc_reg_list, reg)) {
		if (reg->pre_gas.Address == gas->Address &&
		    reg->pre_gas.SpaceId == gas->SpaceId &&
		    reg->pre_gas.BitWidth == gas->BitWidth &&
		    reg->pre_gas.BitOffset == gas->BitOffset) {
			/*
			 * Register is shared, bump the refcount and return
			 * the pointer.
			 */
			reg->pre_refcnt++;
			mutex_exit(&pcc_reg_list_lock);
			return (reg);
		}
	}

	/*
	 * Register is not (yet) shared - create with one reference and map it
	 * before returning it.
	 */
	reg = kmem_zalloc(sizeof (*reg), KM_SLEEP);
	reg->pre_gas = *gas;
	reg->pre_refcnt = 1;
	mutex_init(&reg->pre_lock, NULL, MUTEX_DEFAULT, NULL);

	reg->pre_va = psm_map_phys(
	    (paddr_t)gas->Address, gas->BitWidth / 8, PROT_READ | PROT_WRITE);
	if (reg->pre_va == NULL) {
		cmn_err(CE_WARN, "!acpipcc: failed to map register at 0x%llx",
		    (unsigned long long)gas->Address);
		mutex_destroy(&reg->pre_lock);
		kmem_free(reg, sizeof (*reg));
		mutex_exit(&pcc_reg_list_lock);
		return (NULL);
	}

	list_insert_tail(&pcc_reg_list, reg);
	mutex_exit(&pcc_reg_list_lock);
	return (reg);
}

/*
 * Drop a reference on a register registry entry.
 *
 * Safe to call with a NULL pointer (no-op), so partial mapping cleanup in
 * callers works without special cases.  Takes a pointer-to-pointer and NULLs
 * the caller's pointer.
 */
void
pcc_reg_release(pcc_reg_entry_t **regp)
{
	pcc_reg_entry_t *reg;

	if (regp == NULL || *regp == NULL) {
		return;
	}

	reg = *regp;
	*regp = NULL;

	mutex_enter(&pcc_reg_list_lock);
	ASSERT3U(reg->pre_refcnt, >, 0);
	reg->pre_refcnt--;
	if (reg->pre_refcnt == 0) {
		list_remove(&pcc_reg_list, reg);
		mutex_exit(&pcc_reg_list_lock);
		if (reg->pre_gas.SpaceId == ACPI_ADR_SPACE_SYSTEM_MEMORY) {
			psm_unmap_phys(reg->pre_va, reg->pre_gas.BitWidth / 8);
		}
		mutex_destroy(&reg->pre_lock);
		kmem_free(reg, sizeof (*reg));
		return;
	}
	mutex_exit(&pcc_reg_list_lock);
}

/*
 * Read an MMIO register via a register registry entry.
 */
void
pcc_reg_read(pcc_reg_entry_t *reg, uint64_t *val)
{
	if (reg->pre_gas.BitWidth == 32) {
		volatile uint32_t *r = (volatile uint32_t *)(void *)reg->pre_va;
		*val = (uint64_t)*r;
	} else {
		volatile uint64_t *r = (volatile uint64_t *)(void *)reg->pre_va;
		*val = *r;
	}
}

/*
 * Write an MMIO register via a register registry entry.
 */
void
pcc_reg_write(pcc_reg_entry_t *reg, uint64_t val)
{
	if (reg->pre_gas.BitWidth == 32) {
		volatile uint32_t *r = (volatile uint32_t *)(void *)reg->pre_va;
		*r = (uint32_t)val;
	} else {
		volatile uint64_t *r = (volatile uint64_t *)(void *)reg->pre_va;
		*r = val;
	}
}

/*
 * Read-modify-write a register with preserve/set masks.
 *
 * The caller must hold the register's lock.
 */
void
pcc_reg_rmw_locked(pcc_reg_entry_t *reg, uint64_t preserve, uint64_t set)
{
	uint64_t val;

	ASSERT(MUTEX_HELD(&reg->pre_lock));
	pcc_reg_read(reg, &val);
	val = (val & preserve) | set;
	pcc_reg_write(reg, val);
}

/*
 * Read-modify-write a register with preserve/set masks.
 */
void
pcc_reg_rmw(pcc_reg_entry_t *reg, uint64_t preserve, uint64_t set)
{
	mutex_enter(&reg->pre_lock);
	pcc_reg_rmw_locked(reg, preserve, set);
	mutex_exit(&reg->pre_lock);
}

/*
 * Ring the channel's doorbell register.
 */
void
pcc_ring_doorbell(pcc_chan_t *pc)
{
	/*
	 * Initiators (Types 1-3) always have a doorbell.
	 */
	ASSERT(pc->pc_db_reg != NULL);
	pcc_reg_rmw(pc->pc_db_reg, pc->pc_db_preserve, pc->pc_db_set);
}

/*
 * Write the channel's platform-interrupt ack register.
 *
 * No-op if the channel has none (type 1, edge-triggered).
 */
void
pcc_write_ack(pcc_chan_t *pc)
{
	if (pc->pc_ack_reg == NULL) {
		return;
	}

	pcc_reg_rmw(pc->pc_ack_reg, pc->pc_ack_preserve, pc->pc_ack_set);
}
