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

#ifndef _SYS_ACPIDEV_GIC_H
#define	_SYS_ACPIDEV_GIC_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	AGIF_LEVEL		0x00000000
#define	AGIF_EDGE		0x00000001
#define	AGIF_RISING		0x00000000
#define	AGIF_FALLING		0x00000002
#define	AGIF_ACTIVE_HIGH	0x00000000
#define	AGIF_ACTIVE_LOW		0x00000004

struct acpidev_gic_interrupt {
	uint32_t	gi_gsiv;
	uint32_t	gi_flags;
};

extern ACPI_STATUS acpidev_create_gic_node(dev_info_t **);
extern void acpidev_gic_fini(void);

extern uint32_t
acpidev_get_gic_interrupt_cells(void);

extern ACPI_STATUS acpidev_serialize_interrupt(
    const struct acpidev_gic_interrupt *, uint32_t *);

extern ACPI_STATUS acpidev_set_gsiv_interrupts(
    dev_info_t *, const struct acpidev_gic_interrupt *, size_t);

extern ACPI_STATUS acpidev_set_mmio_regs(
    dev_info_t *rdip, struct regspec *rs, size_t nrs);
extern ACPI_STATUS acpidev_set_standard_ranges(
    dev_info_t *rdip, struct rangespec *rs, size_t nrs);

extern ACPI_STATUS acpidev_set_system_pic(dev_info_t *);

extern ACPI_STATUS acpidev_set_up_root_node(void);

extern dev_info_t *acpidev_gic_find_its_by_trid(uint32_t);
extern boolean_t acpidev_gic_get_sole_its_trid(uint32_t *);

extern boolean_t acpidev_gic_has_v2m_frames(void);
extern dev_info_t *acpidev_gic_next_v2m_frame(void);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_ACPIDEV_GIC_H */
