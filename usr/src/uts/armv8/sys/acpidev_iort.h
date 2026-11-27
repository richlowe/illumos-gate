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

#ifndef _SYS_ACPIDEV_IORT_H
#define	_SYS_ACPIDEV_IORT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Per-root-complex IORT result.  Populated by acpidev_iort_init().
 * Consumed by acpidev_mcfg.c when creating PCIe root complex nodes.
 */
typedef struct acpidev_iort_map {
	uint32_t	aim_rid_base;
	uint32_t	aim_rid_count;
	uint32_t	aim_devid_base;
	uint32_t	aim_its_trid;
} acpidev_iort_map_t;

typedef struct acpidev_iort_rc {
	uint32_t		air_segment;
	boolean_t		air_coherent;
	boolean_t		air_simple;	/* single ITS, identity */
	boolean_t		air_v2m;	/* use v2m round-robin */
	uint32_t		air_its_trid;	/* valid when air_simple */
	uint32_t		air_nmaps;
	acpidev_iort_map_t	*air_maps;
	struct acpidev_iort_rc	*air_next;
} acpidev_iort_rc_t;

extern ACPI_STATUS acpidev_iort_init(void);
extern void acpidev_iort_fini(void);
extern acpidev_iort_rc_t *acpidev_iort_lookup_rc(uint32_t segment);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_ACPIDEV_IORT_H */
