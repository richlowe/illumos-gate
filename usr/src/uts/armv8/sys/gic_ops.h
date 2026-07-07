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

#ifndef _SYS_GIC_OPS_H
#define	_SYS_GIC_OPS_H

/*
 * GIC parent-child operations
 *
 * Function pointer interfaces between the system interrupt controller
 * (a GICv2 or GICv3 driver) and its subordinate MSI controllers
 * (GICv2m frame drivers and GICv3 ITS drivers).
 *
 * The parent GIC driver populates and registers a gic_child_ops_t
 * structure during attach. Subordinate drivers retrieve it via
 * syspic_get_child_ops() and call the parent through the appropriate
 * ops pointer, eliminating direct symbol dependencies between the
 * drivers.
 *
 * Two ops structures are defined, named after the consumers rather
 * than the providers, since the interfaces are shaped by what each
 * consumer needs:
 *
 *   gicv2m_parent_ops_t   SPI-level operations needed by GICv2m
 *                         frame drivers (provided by gictwo or
 *                         gicthree)
 *
 *   gicv3its_parent_ops_t LPI and redistributor operations needed
 *                         by GICv3 ITS drivers (provided by
 *                         gicthree only)
 */

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/ddi_intr_impl.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * Operations a GICv2m frame driver needs from its parent GIC.
 *
 * Both gictwo and gicthree provide these; the context pointer
 * passed to each function is the opaque value from gco_ctx.
 */
typedef struct gicv2m_parent_ops {
	void (*gpo_configure_irq)(void *ctx, uint32_t intid,
	    boolean_t is_edge);
	boolean_t (*gpo_irq_ispending)(void *ctx, uint32_t intid);
	processorid_t (*gpo_get_target_spi)(void *ctx, uint32_t intid);
	void (*gpo_set_target_spi)(void *ctx, uint32_t intid,
	    processorid_t cpu);
	void (*gpo_register_msi_range)(void *ctx, uint32_t base,
	    uint32_t count);
	void (*gpo_unregister_msi_range)(void *ctx, uint32_t base,
	    uint32_t count);
} gicv2m_parent_ops_t;

/*
 * Operations a GICv3 ITS driver needs from its parent GIC.
 *
 * Only gicthree provides these; the context pointer passed to each
 * function is the opaque value from gco_ctx.
 */
typedef struct gicv3its_parent_ops {
	int (*ipo_alloc_lpi)(void *ctx, uint32_t *lpip);
	void (*ipo_free_lpi)(void *ctx, uint32_t lpi);
	void (*ipo_lpi_set_config)(void *ctx, uint32_t lpi, uint8_t prio,
	    boolean_t enable);
	boolean_t (*ipo_lpi_ispending)(void *ctx, uint32_t lpi,
	    processorid_t cpuid);
	size_t (*ipo_lpi_navail)(void *ctx);
	ddi_irm_pool_t *(*ipo_get_lpi_irm_pool)(void *ctx);
	uint64_t (*ipo_redist_pa)(void *ctx, processorid_t cpuid);
	uint32_t (*ipo_redist_procnum)(void *ctx, processorid_t cpuid);
} gicv3its_parent_ops_t;

/*
 * Container registered by the parent GIC with syspic.
 *
 * gco_ctx is the opaque context passed as the first argument to every
 * ops function (typically the parent GIC's dev_info_t pointer).
 *
 * A GICv2 parent fills in gco_v2m_ops only (gco_its_ops == NULL).
 * A GICv3 parent fills in both.
 */
typedef struct gic_child_ops {
	void			*gco_ctx;
	gicv2m_parent_ops_t	*gco_v2m_ops;
	gicv3its_parent_ops_t	*gco_its_ops;
} gic_child_ops_t;

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_GIC_OPS_H */
