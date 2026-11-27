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
 * ACPI _OSC (Operating System Capabilities) Evaluation
 *
 * This module implements two levels of ACPI _OSC negotiation for the
 * aarch64 SBBR platform:
 *
 * 1. Platform-wide \_SB._OSC
 *
 *    Declares OS-level platform capabilities to the firmware.  Called
 *    once during acpidev root initialisation, before any PCIe bridge
 *    enumeration.  Uses the Platform-wide OSPM Capabilities UUID
 *    defined in ACPI 4.0+.
 *
 *    Capabilities should be added here as they're needed, and users
 *    of negotiated capabilities should check that the required capability
 *    was granted.  This can be done using acpica_get_plat_osc, which you
 *    can find in sys/acpica.h.
 *
 * 2. Per-bridge PCIe Host Bridge _OSC
 *
 *    Negotiates control of PCIe hierarchy features (native hotplug, AER,
 *    PCIe capability structure) with the firmware on a per-host-bridge
 *    basis.  Called from pcieb via pcie_osc() dispatch.  Uses the PCI
 *    Host Bridge _OSC UUID from the PCI Firmware Specification 3.0.
 *
 *    The caller (pcieb_armv8.c) decides what to request; this module
 *    is a pure ACPI evaluator.  It walks up the DDI tree to find an
 *    ACPI handle (root port dips are PCI-enumerated and have no ACPI
 *    companion), then walks up the ACPI tree for the _OSC method.
 */

#include <sys/types.h>
#include <sys/stdbool.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/acpidev_osc.h>
#include <sys/pcie_osc.h>
#include <sys/platmod.h>

/*
 * Platform-wide OSPM Capabilities UUID (ACPI 4.0+)
 * 0811b06e-4a27-44f9-8d60-3cbbc22e7b48
 */
static uint8_t acpidev_osc_plat_uuid[16] = {
	0x6e, 0xb0, 0x11, 0x08, 0x27, 0x4a, 0xf9, 0x44,
	0x8d, 0x60, 0x3c, 0xbb, 0xc2, 0x2e, 0x7b, 0x48
};

/*
 * PCI Host Bridge _OSC UUID (PCI Firmware Specification 3.0)
 * 33db4d5b-1ff7-401c-9657-7441c03dd766
 */
static uint8_t acpidev_osc_pcie_uuid[16] = {
	0x5b, 0x4d, 0xdb, 0x33, 0xf7, 0x1f, 0x1c, 0x40,
	0x96, 0x57, 0x74, 0x41, 0xc0, 0x3d, 0xd7, 0x66
};

/* _OSC Status DWORD bit definitions (common to both UUIDs) */
#define	OSC_STS_QUERY		0x01	/* Query Support Flag */
#define	OSC_STS_FAILED		0x02	/* _OSC failure */
#define	OSC_STS_INV_UUID	0x04	/* Unrecognised UUID */
#define	OSC_STS_INV_REV		0x08	/* Unrecognised revision */
#define	OSC_STS_CAPS_MASKED	0x10	/* Capabilities masked */

#define	OSC_STS_ERRORS	\
	(OSC_STS_FAILED | OSC_STS_INV_UUID | OSC_STS_INV_REV)

#define	PLAT_OSC_REVISION_ID	1

int acpidev_osc_max_depth = 25;


/*
 * Evaluate an _OSC method.  Common helper for both platform-wide and
 * per-bridge calls.
 *
 * uuid:     16-byte UUID identifying the _OSC caller
 * osc_hdl:  ACPI handle of the _OSC method to evaluate
 * revision: _OSC revision ID (currently always 1)
 * ndwords:  number of capability DWORDs (excluding status DWORD)
 * caps:     capability DWORD array (ndwords entries); on successful
 *           return, contains the firmware's response
 * query:    if true, set the Query Support Flag (ACPI 6.6 §6.2.12.1)
 *           so the firmware reports what it would grant without
 *           committing
 *
 * Returns AE_OK on success, ACPI error otherwise.
 */
static ACPI_STATUS
acpidev_osc_eval(uint8_t *uuid, ACPI_HANDLE osc_hdl,
    uint32_t revision, uint32_t ndwords, uint32_t *caps, bool query)
{
	ACPI_OBJECT_LIST	arglist;
	ACPI_OBJECT		args[4];
	ACPI_BUFFER		rb;
	ACPI_STATUS		status;
	uint32_t		*rbuf;
	uint32_t		total_dw = ndwords + 1;
	uint32_t		buf_len = total_dw * sizeof (uint32_t);
	uint32_t		*buf;
	uint32_t		i;

	/*
	 * Build the capabilities buffer: DWORD 0 is the status/query
	 * word, followed by ndwords capability DWORDs.
	 */
	buf = kmem_zalloc(buf_len, KM_SLEEP);
	buf[0] = query ? OSC_STS_QUERY : 0;
	for (i = 0; i < ndwords; i++) {
		buf[i + 1] = caps[i];
	}

	/* arg0: UUID */
	args[0].Type = ACPI_TYPE_BUFFER;
	args[0].Buffer.Length = 16;
	args[0].Buffer.Pointer = uuid;

	/* arg1: Revision ID */
	args[1].Type = ACPI_TYPE_INTEGER;
	args[1].Integer.Value = revision;

	/* arg2: Count of DWORDs in capabilities buffer */
	args[2].Type = ACPI_TYPE_INTEGER;
	args[2].Integer.Value = total_dw;

	/* arg3: Capabilities Buffer */
	args[3].Type = ACPI_TYPE_BUFFER;
	args[3].Buffer.Length = buf_len;
	args[3].Buffer.Pointer = (void *)buf;

	arglist.Count = 4;
	arglist.Pointer = args;

	rb.Length = ACPI_ALLOCATE_BUFFER;
	rb.Pointer = NULL;

	status = AcpiEvaluateObjectTyped(osc_hdl, NULL, &arglist,
	    &rb, ACPI_TYPE_BUFFER);

	kmem_free(buf, buf_len);

	if (ACPI_FAILURE(status)) {
		return (status);
	}

	if (((ACPI_OBJECT *)rb.Pointer)->Buffer.Length <
	    total_dw * sizeof (uint32_t)) {
		AcpiOsFree(rb.Pointer);
		return (AE_BAD_VALUE);
	}

	rbuf = (uint32_t *)
	    ((ACPI_OBJECT *)rb.Pointer)->Buffer.Pointer;

	/* Check the status DWORD for hard errors */
	if (rbuf[0] & OSC_STS_ERRORS) {
		if (rbuf[0] & OSC_STS_INV_UUID) {
			cmn_err(CE_NOTE, "!acpidev: _OSC: firmware "
			    "does not recognise UUID");
		}
		if (rbuf[0] & OSC_STS_INV_REV) {
			cmn_err(CE_NOTE, "!acpidev: _OSC: firmware "
			    "does not support revision");
		}
		if (rbuf[0] & OSC_STS_FAILED) {
			cmn_err(CE_NOTE, "!acpidev: _OSC: "
			    "evaluation failed");
		}
		AcpiOsFree(rb.Pointer);
		return (AE_ERROR);
	}

	if (rbuf[0] & OSC_STS_CAPS_MASKED) {
		cmn_err(CE_NOTE, "!acpidev: _OSC: some capabilities "
		    "were masked by firmware");
		for (i = 0; i < ndwords; i++) {
			uint32_t masked = caps[i] & ~rbuf[i + 1];
			if (masked != 0) {
				cmn_err(CE_NOTE, "!acpidev: _OSC: "
				    "DWORD %u: requested 0x%x, "
				    "granted 0x%x (masked 0x%x)",
				    i + 2, caps[i], rbuf[i + 1],
				    masked);
			}
		}
	}

	/* Copy back the response capability DWORDs */
	for (i = 0; i < ndwords; i++) {
		caps[i] = rbuf[i + 1];
	}

	AcpiOsFree(rb.Pointer);
	return (AE_OK);
}

/*
 * Evaluate \_SB._OSC - platform-wide OS capability declaration.
 *
 * Called once during acpidev root initialisation, before any PCIe
 * bridge enumeration begins.  Tells the firmware what platform-level
 * features the OS supports.
 *
 * Platform-level _OSC negotiates support for platform capabilities
 * we support via drivers and frameworks in the aarch64 tree.  As
 * new support-code is added the new capabilities must be added here
 * and the supporting code must gate on successfully negotiated support,
 * which can be checked via `acpica_get_plat_osc'.
 */
void
acpidev_osc_init(void)
{
	ACPI_HANDLE	sb_hdl;
	ACPI_HANDLE	osc_hdl;
	ACPI_STATUS	status;
	uint32_t	caps[1];
	uint32_t	rcaps[1];

	/* Find \_SB */
	status = AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_SB", &sb_hdl);
	if (ACPI_FAILURE(status)) {
		cmn_err(CE_NOTE,
		    "!acpidev: _OSC: \\_SB not found");
		return;
	}

	/*
	 * Build capabilities:
	 * - Basic Collaborative Processor Performance Control (CPPC)
	 * - Low Power Idle
	 * - Generic Event Device
	 * - Arm Functional Fixed Hardware (FFH)
	 *
	 * XXXARM: Missing RAS support
	 *
	 * Somewhat important is that we do not yet negotiate APEI (ACPI
	 * Platform Error Interfaces), which is something we'll need for
	 * RAS, which we do not yet support.  When we introduce RAS support
	 * we'll need to add PLAT_OSC_APEI.
	 */
	caps[0] = PLAT_OSC_CPPC;	/* no autonomous yet */
	caps[0] |= PLAT_OSC_PLAT_LPI;	/* we don't do PLAT_OSC_OS_LPI yet */
	caps[0] |= PLAT_OSC_GED;
	caps[0] |= PLAT_OSC_FFH;

	rcaps[0] = caps[0];

	/* Check if _OSC exists under \_SB */
	status = AcpiGetHandle(sb_hdl, "_OSC", &osc_hdl);
	if (ACPI_FAILURE(status)) {
		/* No \_SB._OSC - nothing to negotiate. */
		cmn_err(CE_CONT, "!acpidev: \\_SB._OSC: platform "
		    "capabilities assumed: 0x%x\n", caps[0]);
		acpica_set_plat_osc(caps[0]);
		return;
	}

	/*
	 * Query first to discover what the firmware supports, then
	 * commit.  Some firmware rejects a commit without a prior
	 * query (ACPI 6.6 §6.2.12.1.1).
	 */
	status = acpidev_osc_eval(acpidev_osc_plat_uuid, osc_hdl,
	    PLAT_OSC_REVISION_ID, 1, caps, true);
	if (ACPI_FAILURE(status)) {
		cmn_err(CE_NOTE, "!acpidev: \\_SB._OSC query "
		    "failed (status 0x%x)", status);
		return;
	}

	caps[0] &= rcaps[0];

	status = acpidev_osc_eval(acpidev_osc_plat_uuid, osc_hdl,
	    PLAT_OSC_REVISION_ID, 1, caps, false);
	if (ACPI_FAILURE(status)) {
		cmn_err(CE_NOTE, "!acpidev: \\_SB._OSC commit "
		    "failed (status 0x%x)", status);
		return;
	}

	cmn_err(CE_CONT, "!acpidev: \\_SB._OSC: platform "
	    "capabilities negotiated (granted 0x%x)\n", caps[0]);

	acpica_set_plat_osc(caps[0]);
}

/*
 * Evaluate _OSC on a PCIe host bridge.
 *
 * Called via plat_pcie_osc_set registration from pcieb (through
 * pcie_osc.c dispatch) for each PCIe root port with AER.  This
 * function is a pure ACPI evaluator: it receives the _OSC parameters
 * from the caller and returns the granted control bits.
 *
 * The caller (pcieb) decides what to request and tracks state in
 * pcie_aarch64_priv_t.
 *
 * Returns DDI_SUCCESS if _OSC was successfully evaluated (even if
 * some capabilities were masked), DDI_FAILURE otherwise.
 */
static int
acpidev_pcie_osc(dev_info_t *dip, uint32_t support,
    uint32_t ctrl_req, uint32_t *ctrl_ret)
{
	ACPI_HANDLE	bridge_hdl;
	ACPI_HANDLE	cur_hdl;
	ACPI_HANDLE	parent_hdl;
	ACPI_HANDLE	osc_hdl;
	ACPI_STATUS	status;
	uint32_t	caps[2];
	dev_info_t	*walk_dip;
	int		i;

	/*
	 * Walk up the DDI tree to find a dip with an ACPI handle.
	 * Root port dips are PCI-enumerated children with no ACPI
	 * companion - only the RC-level (ecam) dips are tagged.
	 */
	bridge_hdl = NULL;
	i = 0;
	for (walk_dip = dip; walk_dip != NULL && i < acpidev_osc_max_depth;
	    walk_dip = ddi_get_parent(walk_dip), ++i) {
		if (acpica_get_handle(walk_dip, &bridge_hdl) == AE_OK) {
			break;
		}
	}
	if (bridge_hdl == NULL) {
		cmn_err(CE_NOTE, "!acpidev: PCIe _OSC: no ACPI handle "
		    "found for %s%d or ancestors",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		return (DDI_FAILURE);
	}

	/*
	 * Walk up the ACPI tree looking for an _OSC method,
	 * following the i86pc pattern.  Some firmware places _OSC
	 * on a parent scope rather than directly on the bridge.
	 */
	i = 0;
	osc_hdl = NULL;
	cur_hdl = bridge_hdl;
	do {
		if (AcpiGetHandle(cur_hdl, "_OSC", &osc_hdl) == AE_OK) {
			break;
		}
		osc_hdl = NULL;
	} while (++i < acpidev_osc_max_depth &&
	    AcpiGetParent(cur_hdl, &parent_hdl) == AE_OK &&
	    (cur_hdl = parent_hdl) != NULL);

	if (osc_hdl == NULL) {
		cmn_err(CE_NOTE, "!acpidev: PCIe _OSC: no _OSC method "
		    "found for %s%d",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		return (DDI_FAILURE);
	}

	/*
	 * DWORD 1 (caps[0]): Support Field
	 * DWORD 2 (caps[1]): Control Field
	 *
	 * Query first, then commit with the (possibly masked)
	 * capabilities the firmware indicated it supports.
	 */
	caps[0] = support;
	caps[1] = ctrl_req;

	status = acpidev_osc_eval(acpidev_osc_pcie_uuid, osc_hdl,
	    PCIE_OSC_REVISION_ID, 2, caps, true);
	if (ACPI_FAILURE(status)) {
		cmn_err(CE_NOTE, "!acpidev: PCIe _OSC query "
		    "failed for %s%d (status 0x%x)",
		    ddi_driver_name(dip), ddi_get_instance(dip),
		    status);
		return (DDI_FAILURE);
	}

	status = acpidev_osc_eval(acpidev_osc_pcie_uuid, osc_hdl,
	    PCIE_OSC_REVISION_ID, 2, caps, false);
	if (ACPI_FAILURE(status)) {
		cmn_err(CE_NOTE, "!acpidev: PCIe _OSC commit "
		    "failed for %s%d (status 0x%x)",
		    ddi_driver_name(dip), ddi_get_instance(dip),
		    status);
		return (DDI_FAILURE);
	}

	cmn_err(CE_CONT, "!acpidev: PCIe _OSC for %s%d: "
	    "granted control 0x%x\n",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    caps[1]);

	*ctrl_ret = caps[1];
	return (DDI_SUCCESS);
}

/*
 * Register the PCIe _OSC evaluator with the platmod interface.
 * Called from acpidev_drv.c during ACPI initialisation.
 */
void
acpidev_pcie_osc_register(void)
{
	if (&plat_pcie_osc_set != NULL) {
		plat_pcie_osc_set(acpidev_pcie_osc);
	}
}
