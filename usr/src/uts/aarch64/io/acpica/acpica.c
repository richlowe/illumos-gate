/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 */
/*
 * Copyright (c) 2009, Intel Corporation.
 * All rights reserved.
 */
/*
 * Copyright 2011 Nexenta Systems, Inc.  All rights reserved.
 * Copyright 2016, Joyent, Inc.
 * Copyright 2026 Michael van der Westhuizen
 */
/*
 * illumos aarch64 ACPI CA services
 */

#include <sys/file.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/stat.h>
#include <sys/spl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/kstat.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/archsystm.h>

#include "ffh.h"

/*
 * Module glue
 */
static	struct modlmisc modlmisc = {
	.misc_modops	= &mod_miscops,
	.misc_linkinfo	= "ACPI interpreter",
};

static	struct modlinkage modlinkage = {
	.ml_rev		= MODREV_1,
	.ml_linkage	= {&modlmisc, NULL},
};

/*
 * Local prototypes
 */

static void	acpica_init_kstats(void);

/*
 * Local data
 */

static kmutex_t	acpica_module_lock;
static kstat_t	*acpica_ksp;

/*
 * State of acpica subsystem
 * After successful initialization, will be ACPICA_INITIALIZED
 */
int acpica_init_state = ACPICA_NOT_INITIALIZED;

void *AcpiGbl_DbBuffer;
uint32_t AcpiGbl_DbConsoleDebugLevel;

/*
 * Non-zero enables lax behavior with respect to some
 * common ACPI BIOS issues; see ACPI CA documentation
 * Setting this to zero causes ACPI CA to enforce strict
 * compliance with ACPI specification
 */
int acpica_enable_interpreter_slack = 1;

/*
 * For non-DEBUG builds, set the ACPI CA debug level to 0
 * to quiet chatty BIOS output into /var/adm/messages
 * Field-patchable for diagnostic use.
 */
#ifdef  DEBUG
int acpica_muzzle_debug_output = 0;
#else
int acpica_muzzle_debug_output = 1;
#endif

/*
 * ACPI DDI hooks
 */
static int acpica_ddi_setwake(dev_info_t *dip, int level);

int
_init(void)
{
	int error = EBUSY;
	int	status;
	extern int (*acpi_fp_setwake)();
	extern kmutex_t cpu_map_lock;

	ffh_init();

	mutex_init(&acpica_module_lock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&cpu_map_lock, NULL, MUTEX_SPIN,
	    (ddi_iblock_cookie_t)ipltospl(DISP_LEVEL));

	if ((error = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&acpica_module_lock);
		goto load_error;
	}

	AcpiGbl_EnableInterpreterSlack = (acpica_enable_interpreter_slack != 0);

	/* global ACPI CA initialization */
	if (ACPI_FAILURE(status = AcpiInitializeSubsystem())) {
		cmn_err(CE_WARN, "!AcpiInitializeSubsystem failed: %d", status);
	}

	/* initialize table manager */
	if (ACPI_FAILURE(status = AcpiInitializeTables(NULL, 0, 0))) {
		cmn_err(CE_WARN, "!AcpiInitializeTables failed: %d", status);
	}

	/* seed PCI config space routing list from MCFG */
	acpica_pci_cfgspace_init();

	acpi_fp_setwake = acpica_ddi_setwake;

load_error:
	return (error);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	/*
	 * acpica module is never unloaded at run-time
	 */
	return (EBUSY);
}

/*
 * Install acpica-provided (default) address-space handlers
 * that may be needed before AcpiEnableSubsystem() runs.
 * See the comment in AcpiInstallAddressSpaceHandler().
 * Default handlers for remaining address spaces are
 * installed later, in AcpiEnableSubsystem.
 */
static int
acpica_install_handlers()
{
	ACPI_STATUS	rv = AE_OK;
	ACPI_STATUS	res;

	/*
	 * Install ACPI CA default handlers
	 */
	if ((res = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
	    ACPI_ADR_SPACE_SYSTEM_MEMORY,
	    ACPI_DEFAULT_HANDLER, NULL, NULL)) != AE_OK &&
	    res != AE_SAME_HANDLER) {
		cmn_err(CE_WARN, "!acpica: no default handler for"
		    " system memory");
		rv = AE_ERROR;
	}

	if ((res = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
	    ACPI_ADR_SPACE_SYSTEM_IO,
	    ACPI_DEFAULT_HANDLER, NULL, NULL)) != AE_OK &&
	    res != AE_SAME_HANDLER) {
		cmn_err(CE_WARN, "!acpica: no default handler for"
		    " system I/O");
		rv = AE_ERROR;
	}

	if ((res = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
	    ACPI_ADR_SPACE_PCI_CONFIG,
	    ACPI_DEFAULT_HANDLER, NULL, NULL)) != AE_OK &&
	    res != AE_SAME_HANDLER) {
		cmn_err(CE_WARN, "!acpica: no default handler for"
		    " PCI Config");
		rv = AE_ERROR;
	}

	if ((res = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
	    ACPI_ADR_SPACE_DATA_TABLE,
	    ACPI_DEFAULT_HANDLER, NULL, NULL)) != AE_OK &&
	    res != AE_SAME_HANDLER) {
		cmn_err(CE_WARN, "!acpica: no default handler for"
		    " Data Table");
		rv = AE_ERROR;
	}

	/*
	 * Install the Arm FFH (Fixed Function Hardware) address space
	 * handler for OpRegion access (DEN0048D).
	 */
	if (osl_ffh_install() != AE_OK) {
		rv = AE_ERROR;
	}

	return (rv);
}

/*
 * Initialize the CA subsystem if it hasn't been done already
 */
int
acpica_init()
{
	ACPI_STATUS status;

	mutex_enter(&acpica_module_lock);
	if (acpica_init_state == ACPICA_INITIALIZED) {
		mutex_exit(&acpica_module_lock);
		return (AE_OK);
	}

	if (ACPI_FAILURE(status = AcpiLoadTables())) {
		goto error;
	}

	if (ACPI_FAILURE(status = acpica_install_handlers())) {
		goto error;
	}

	status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status)) {
		goto error;
	}

	/*
	 * EC support is not yet standardised, so we don't do it yet.
	 */

	/* This runs all device _STA and _INI methods. */
	if (ACPI_FAILURE(status = AcpiInitializeObjects(0))) {
		goto error;
	}

	acpica_init_state = ACPICA_INITIALIZED;

	/*
	 * We don't evaluate _PRW, as that returns GPE reference, which is
	 * not a thing on hardware-reduced platforms.
	 */

	acpica_init_kstats();
error:
	if (acpica_init_state != ACPICA_INITIALIZED) {
		cmn_err(CE_NOTE, "!failed to initialize ACPI services");
	}

	/*
	 * Set acpi-status to 13 if acpica has been initialized successfully.
	 * This indicates that acpica is up and running.  This variable name
	 * and value were chosen in order to remain compatible with acpi_intp.
	 */
	e_ddi_prop_update_int(DDI_DEV_T_NONE, ddi_root_node(), "acpi-status",
	    (ACPI_SUCCESS(status)) ? (ACPI_BOOT_INIT | ACPI_BOOT_ENABLE |
	    ACPI_BOOT_BOOTCONF) : 0);

	/* Mark acpica subsystem as fully initialized. */
	if (ACPI_SUCCESS(status)) {
		acpica_set_core_feature(ACPI_FEATURE_FULL_INIT);
	}

	mutex_exit(&acpica_module_lock);
	return (status);
}

/*
 * No GPE on hardware-reduced platforms, so there is
 * nothing sensible we can do here.
 */
static int
acpica_ddi_setwake(dev_info_t *dip __unused, int level __unused)
{
	return (0);
}

/*
 * kstat access to a limited set of ACPI properties
 */
static void
acpica_init_kstats()
{
	ACPI_HANDLE	s3handle;
	ACPI_STATUS	status;
	ACPI_TABLE_FADT	*fadt;
	kstat_named_t *knp;

	/*
	 * Create a small set of named kstats; just return in the rare
	 * case of a failure, in which case, the kstats won't be present.
	 */
	if ((acpica_ksp = kstat_create("acpi", 0, "acpi", "misc",
	    KSTAT_TYPE_NAMED, 2, 0)) == NULL) {
		return;
	}

	/*
	 * initialize kstat 'S3' to reflect the presence of \_S3 in
	 * the ACPI namespace (1 = present, 0 = not present)
	 */
	knp = acpica_ksp->ks_data;
	knp->value.l = (AcpiGetHandle(NULL, "\\_S3", &s3handle) == AE_OK);
	kstat_named_init(knp, "S3", KSTAT_DATA_LONG);
	knp++;		/* advance to next named kstat */

	/*
	 * initialize kstat 'preferred_pm_profile' to the value
	 * contained in the (always present) FADT
	 */
	status = AcpiGetTable(ACPI_SIG_FADT, 1, (ACPI_TABLE_HEADER **)&fadt);
	knp->value.l = (status == AE_OK) ? fadt->PreferredProfile : -1;
	kstat_named_init(knp, "preferred_pm_profile", KSTAT_DATA_LONG);

	/*
	 * install the named kstats
	 */
	kstat_install(acpica_ksp);
}

/*
 * Attempt to save the current ACPI settings (_CRS) for the device
 * which corresponds to the supplied devinfo node.  The settings are
 * saved as a property on the dip.  If no ACPI object is found to be
 * associated with the devinfo node, no action is taken and no error
 * is reported.
 */
void
acpica_ddi_save_resources(dev_info_t *dip)
{
	ACPI_HANDLE	devobj;
	ACPI_BUFFER	resbuf;
	int		ret;

	resbuf.Length = ACPI_ALLOCATE_BUFFER;
	if (ACPI_FAILURE(acpica_get_handle(dip, &devobj)) ||
	    ACPI_FAILURE(AcpiGetCurrentResources(devobj, &resbuf))) {
		return;
	}

	ret = ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
	    "acpi-crs", resbuf.Pointer, resbuf.Length);

	ASSERT(ret == DDI_PROP_SUCCESS);

	AcpiOsFree(resbuf.Pointer);
}

/*
 * If the supplied devinfo node has an ACPI settings property attached,
 * restore them to the associated ACPI device using _SRS.  The property
 * is deleted from the devinfo node afterward.
 */
void
acpica_ddi_restore_resources(dev_info_t *dip)
{
	ACPI_HANDLE	devobj;
	ACPI_BUFFER	resbuf;
	uchar_t		*propdata;
	uint_t		proplen;

	if (ACPI_FAILURE(acpica_get_handle(dip, &devobj))) {
		return;
	}

	if (ddi_prop_lookup_byte_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "acpi-crs", &propdata, &proplen) != DDI_PROP_SUCCESS) {
		return;
	}

	resbuf.Pointer = propdata;
	resbuf.Length = proplen;
	(void) AcpiSetCurrentResources(devobj, &resbuf);
	ddi_prop_free(propdata);
	(void) ddi_prop_remove(DDI_DEV_T_NONE, dip, "acpi-crs");
}
