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

#ifndef _SYS_PCIE_OSC_H
#define	_SYS_PCIE_OSC_H

/*
 * PCI Host Bridge _OSC (Operating System Capabilities) definitions
 * from PCI Firmware Specification 3.3, Section 4.5.1.
 *
 * These constants define the bit fields in the _OSC Capabilities
 * Buffer for PCI/PCI-X/PCI Express hierarchies.  They are independent
 * of the ACPI transport used to evaluate _OSC.
 */

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Revision */
#define	PCIE_OSC_REVISION_ID	1

/* DWORD 1: Status (returned by firmware) */
#define	PCIE_OSC_STS_QUERY	0x01	/* Query Support Flag */
#define	PCIE_OSC_STS_FAILED	0x02	/* _OSC failure */
#define	PCIE_OSC_STS_INV_UUID	0x04	/* Unrecognised UUID */
#define	PCIE_OSC_STS_INV_REV	0x08	/* Unrecognised revision */
#define	PCIE_OSC_STS_MASKED	0x10	/* Capabilities masked */

#define	PCIE_OSC_STS_ERRORS \
	(PCIE_OSC_STS_FAILED | PCIE_OSC_STS_INV_UUID | PCIE_OSC_STS_INV_REV)

/* DWORD 2: Support Field (OS -> firmware) */
#define	PCIE_OSC_SUP_EXT_CFG	0x01	/* Extended PCI Config Ops */
#define	PCIE_OSC_SUP_ACT_PM	0x02	/* Active State PM */
#define	PCIE_OSC_SUP_CLK_PM	0x04	/* Clock PM Capability */
#define	PCIE_OSC_SUP_SEGS	0x08	/* PCI Segment Groups */
#define	PCIE_OSC_SUP_MSI	0x10	/* MSI */
#define	PCIE_OSC_SUP_OBFF	0x20	/* OBFF (PCI FW 3.2+) */
#define	PCIE_OSC_SUP_ASPM_OPT	0x40	/* ASPM Optionality */
#define	PCIE_OSC_SUP_EDR	0x80	/* Error Disconnect Recover */
#define	PCIE_OSC_SUP_HPX_T3	0x100	/* _HPX Type 3 */

/* DWORD 3: Control Field (OS <-> firmware) */
#define	PCIE_OSC_CTL_NAT_HP	0x01	/* Native Hot Plug */
#define	PCIE_OSC_CTL_SHPC_HP	0x02	/* SHPC Native Hot Plug */
#define	PCIE_OSC_CTL_NAT_PM	0x04	/* Native Power Mgmt */
#define	PCIE_OSC_CTL_AER	0x08	/* Adv Error Reporting */
#define	PCIE_OSC_CTL_CAPS	0x10	/* PCIe Cap Structure */
#define	PCIE_OSC_CTL_LTR	0x20	/* Latency Tolerance Reporting */
#define	PCIE_OSC_CTL_SHR	0x40	/* Invalid data / Surprise Hot Remove */
#define	PCIE_OSC_CTL_DPC	0x80	/* Downstream Port Containment */
#define	PCIE_OSC_CTL_CTO	0x100	/* Completion Timeout Control */
#define	PCIE_OSC_CTL_SFI	0x200	/* SFI Configuration Control */

/*
 * What we declare as supported.
 */
#define	PCIE_OSC_SUPPORT_INIT \
	(PCIE_OSC_SUP_EXT_CFG | PCIE_OSC_SUP_ACT_PM | \
	PCIE_OSC_SUP_CLK_PM | PCIE_OSC_SUP_MSI | PCIE_OSC_SUP_SEGS)

/*
 * Base control request (Caps only).
 * AER and Hotplug bits added conditionally by the caller.
 */
#define	PCIE_OSC_CONTROL_INIT	(PCIE_OSC_CTL_CAPS)

extern int pcie_osc(dev_info_t *dip, uint32_t support,
    uint32_t ctrl_req, uint32_t *ctrl_ret);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PCIE_OSC_H */
