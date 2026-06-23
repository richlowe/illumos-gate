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

#ifndef _SYS_SMCCC_PCI_H
#define	_SYS_SMCCC_PCI_H

/*
 * Arm PCI Configuration Space Access Firmware Interface (DEN0115).
 *
 * An SMCCC-based interface for PCI configuration space access that
 * papers over quirks in vendor PCI configuration space implementations.
 *
 * Can also be used as a replacement for ECAM when needed.
 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * DEN0115 function IDs (all SMC32).
 */
#define	SMCCC_PCI_VERSION_FID		0x84000130
#define	SMCCC_PCI_FEATURES_FID		0x84000131
#define	SMCCC_PCI_READ_FID		0x84000132
#define	SMCCC_PCI_WRITE_FID		0x84000133
#define	SMCCC_PCI_GET_SEG_INFO_FID	0x84000134

/*
 * DEN0115 return codes (§2.6).
 *
 * These are firmware return values; callers work with DDI error codes.
 */
#define	SMCCC_PCI_SUCCESS		0
#define	SMCCC_PCI_NOT_SUPPORTED		(-1)
#define	SMCCC_PCI_INVALID_PARAMETER	(-2)
#define	SMCCC_PCI_NOT_IMPLEMENTED	(-3)

extern boolean_t smccc_pci_available(void);

extern int smccc_pci_version(uint32_t *versionp);
extern int smccc_pci_features(uint32_t pci_func_id, uint32_t *featuresp);
extern int smccc_pci_read(uint16_t seg, uint8_t bus, uint8_t dev,
    uint8_t fn, uint32_t reg, uint32_t access_size, uint32_t *datap);
extern int smccc_pci_write(uint16_t seg, uint8_t bus, uint8_t dev,
    uint8_t fn, uint32_t reg, uint32_t access_size, uint32_t data);
extern int smccc_pci_get_seg_info(uint16_t seg, uint8_t *start_busp,
    uint8_t *end_busp, uint16_t *next_segp);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SMCCC_PCI_H */
