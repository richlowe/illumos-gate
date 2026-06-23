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
 * Arm PCI Configuration Space Access Firmware Interface (DEN0115).
 *
 * An SMCCC client that provides PCI configuration space read/write and
 * segment enumeration via firmware calls, allowing the OS to work around
 * vendor-specific PCI configuration space access quirks.
 *
 * All DEN0115 functions are SMC32.  The SMCCC transport layer handles
 * conduit selection (SMC vs HVC) automatically.
 *
 * Specification references are against DEN0115A.
 */

#include <sys/types.h>
#include <sys/smccc_pci.h>
#include <sys/smccc.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/debug.h>

static boolean_t smccc_pci_inited = B_FALSE;
static boolean_t smccc_pci_is_avail = B_FALSE;
static uint32_t smccc_pci_ver = 0;

/*
 * Translate a DEN0115 firmware return code to a DDI error.
 */
static int
smccc_pci_to_ddi(int32_t fw_ret)
{
	switch (fw_ret) {
	case SMCCC_PCI_SUCCESS:
		return (DDI_SUCCESS);
	case SMCCC_PCI_NOT_SUPPORTED:	/* fallthrough */
	case SMCCC_PCI_NOT_IMPLEMENTED:
		return (DDI_ENOTSUP);
	case SMCCC_PCI_INVALID_PARAMETER:
		return (DDI_EINVAL);
	default:
		return (DDI_FAILURE);
	}
}

/*
 * Encode segment, bus, device, function into the SBDF format
 * used by PCI_READ and PCI_WRITE (DEN0115 §2.3, §2.4).
 *
 *   [31:16] segment group number
 *   [15:8]  bus number
 *   [7:3]   device number
 *   [2:0]   function number
 */
static uint32_t
smccc_pci_sbdf(uint16_t seg, uint8_t bus, uint8_t dev, uint8_t fn)
{
	return (((uint32_t)seg << 16) |
	    ((uint32_t)bus << 8) |
	    ((uint32_t)(dev & 0x1f) << 3) |
	    ((uint32_t)(fn & 0x7)));
}

/*
 * Probe the firmware for DEN0115 support.
 *
 * Must be called after smccc_init has run.  DEN0115 requires
 * SMCCC 1.1 or later (§1.2).
 */
static void
smccc_pci_init(void)
{
	int rv;
	smccc32_args_t args = { .w = {SMCCC_PCI_VERSION_FID} };

	if (smccc_pci_inited) {
		return;
	}

	/*
	 * DEN0115 is an SMCCC 1.1+ service.
	 */
	if (!smccc_available()) {
		smccc_pci_inited = B_TRUE;
		return;
	}

	/*
	 * Probe PCI_VERSION to discover if the interface is present.
	 */
	rv = smccc32_call(&args);
	if (rv != DDI_SUCCESS) {
		smccc_pci_inited = B_TRUE;
		return;
	}

	/*
	 * A negative return indicates the interface is not supported.
	 * A non-negative return carries the version in the standard
	 * packed format: major[30:16], minor[15:0].
	 */
	if ((int32_t)args.w[0] < 0) {
		smccc_pci_inited = B_TRUE;
		return;
	}

	smccc_pci_ver = args.w[0];

	/*
	 * Reject a zero version - firmware claims support but reports
	 * no actual version.
	 */
	if (smccc_pci_ver == 0) {
		smccc_pci_inited = B_TRUE;
		return;
	}

	smccc_pci_is_avail = B_TRUE;
	smccc_pci_inited = B_TRUE;
}

static void
smccc_pci_fini(void)
{
	smccc_pci_ver = 0;
	smccc_pci_is_avail = B_FALSE;
	smccc_pci_inited = B_FALSE;
}

boolean_t
smccc_pci_available(void)
{
	return (smccc_pci_inited && smccc_pci_is_avail);
}

/*
 * PCI_VERSION (§2.1)
 *
 * Returns the DEN0115 interface version.
 */
int
smccc_pci_version(uint32_t *versionp)
{
	int rv;
	smccc32_args_t args = { .w = { SMCCC_PCI_VERSION_FID } };

	if (!smccc_pci_available()) {
		return (DDI_ENOTSUP);
	}

	rv = smccc32_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)args.w[0] < 0) {
		return (smccc_pci_to_ddi((int32_t)args.w[0]));
	}

	if (versionp != NULL) {
		*versionp = args.w[0];
	}

	return (DDI_SUCCESS);
}

/*
 * PCI_FEATURES (§2.2)
 *
 * Query whether a specific DEN0115 function is implemented and
 * retrieve its feature flags.
 */
int
smccc_pci_features(uint32_t pci_func_id, uint32_t *featuresp)
{
	smccc32_args_t args = { .w = { SMCCC_PCI_FEATURES_FID, pci_func_id } };
	int rv;

	if (!smccc_pci_available()) {
		return (DDI_ENOTSUP);
	}

	rv = smccc32_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)args.w[0] < 0) {
		return (smccc_pci_to_ddi((int32_t)args.w[0]));
	}

	if (featuresp != NULL) {
		*featuresp = args.w[0];
	}

	return (DDI_SUCCESS);
}

/*
 * PCI_READ (§2.3)
 *
 * Read from PCI configuration space via firmware.
 */
int
smccc_pci_read(uint16_t seg, uint8_t bus, uint8_t dev, uint8_t fn,
    uint32_t reg, uint32_t access_size, uint32_t *datap)
{
	int rv;
	smccc32_args_t args = {
		.w = {
			SMCCC_PCI_READ_FID,
			smccc_pci_sbdf(seg, bus, dev, fn),
			reg,
			access_size
		},
	};

	if (!smccc_pci_available()) {
		return (DDI_ENOTSUP);
	}

	VERIFY(datap != NULL);

	switch (access_size) {
	case 1:	/* fallthrough */
	case 2:	/* fallthrough */
	case 4:
		break;
	default:
		return (DDI_EINVAL);
	}

	rv = smccc32_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)args.w[0] != SMCCC_PCI_SUCCESS) {
		return (smccc_pci_to_ddi((int32_t)args.w[0]));
	}

	*datap = args.w[1];
	return (DDI_SUCCESS);
}

/*
 * PCI_WRITE (§2.4)
 *
 * Write to PCI configuration space via firmware.
 */
int
smccc_pci_write(uint16_t seg, uint8_t bus, uint8_t dev, uint8_t fn,
    uint32_t reg, uint32_t access_size, uint32_t data)
{
	smccc32_args_t args = {
		.w = {
			SMCCC_PCI_WRITE_FID,
			smccc_pci_sbdf(seg, bus, dev, fn),
			reg,
			access_size,
			data
		}
	};
	int rv;

	if (!smccc_pci_available()) {
		return (DDI_ENOTSUP);
	}

	switch (access_size) {
	case 1:	/* fallthrough */
	case 2:	/* fallthrough */
	case 4:
		break;
	default:
		return (DDI_EINVAL);
	}

	rv = smccc32_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)args.w[0] != SMCCC_PCI_SUCCESS) {
		return (smccc_pci_to_ddi((int32_t)args.w[0]));
	}

	return (DDI_SUCCESS);
}

/*
 * PCI_GET_SEG_INFO (§2.5)
 *
 * Query bus range and next segment for a given PCI segment group.
 *
 * §2.5.3:
 *   The caller may use this function to iterate through all the supported
 *   PCI segments in the platform. This can be done by calling this function
 *   with the value of zero in the pci_seg parameter, then using the value
 *   returned in pci_next_seg as the pci_seg input for subsequent calls to
 *   this function, until a value of zero is returned in pci_next_seg.
 *
 * §2.5.4 requires the implementation to implement segment group zero.
 *
 * So, use something like this:
 *   uint8_t sbus;
 *   uint8_t ebus;
 *   uint16_t nseg;
 *   uint16_t seg = 0;
 *   while (smccc_pci_get_seg_info(seg, &sbus, &ebus, &nseg) == DDI_SUCCESS) {
 *     // process segment from sbus to ebus
 *     if (nseg == 0)
 *       break;
 *     seg = nseg;
 *   }
 */
int
smccc_pci_get_seg_info(uint16_t seg, uint8_t *start_busp,
    uint8_t *end_busp, uint16_t *next_segp)
{
	smccc32_args_t args = {
		.w = { SMCCC_PCI_GET_SEG_INFO_FID, seg },
	};
	int rv;

	if (!smccc_pci_available()) {
		return (DDI_ENOTSUP);
	}

	VERIFY(start_busp != NULL);
	VERIFY(end_busp != NULL);
	VERIFY(next_segp != NULL);

	rv = smccc32_call(&args);
	if (rv != DDI_SUCCESS) {
		return (rv);
	}

	if ((int32_t)args.w[0] != SMCCC_PCI_SUCCESS) {
		return (smccc_pci_to_ddi((int32_t)args.w[0]));
	}

	*start_busp = (uint8_t)(args.w[1] & 0xff);
	*end_busp = (uint8_t)((args.w[1] >> 8) & 0xff);
	*next_segp = (uint16_t)(args.w[2] & 0xffff);
	return (DDI_SUCCESS);
}

static struct modlmisc modlmisc = {
	&mod_miscops,
	"SMCCC PCI (DEN0115A)",
};

static struct modlinkage modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &modlmisc, NULL }
};

int
_init(void)
{
	int err;

	if ((err = mod_install(&modlinkage)) != 0) {
		return (err);
	}

	smccc_pci_init();
	return (err);
}

int
_fini(void)
{
	int err;

	if ((err = mod_remove(&modlinkage))) {
		return (err);
	}

	smccc_pci_fini();
	return (err);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
