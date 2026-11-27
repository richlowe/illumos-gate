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
 * ACPI CPU container objects (ACPI0010, the Processor Container Device)
 * describe the processor hierarchy: a socket containing clusters containing
 * CPUs.  These get their own class, rather than sharing the generic container
 * class, because their naming follows the device pattern (lowercase ACPI name,
 * _UID as the unit address) and they bind acpinex as a nexus.
 *
 * The ACPI objects backing these nodes can contain _LPI functions, allowing
 * deeper sleep states to be exposed to the OS.
 */

#include <sys/types.h>
#include <sys/atomic.h>
#include <sys/note.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/acpidev.h>
#include <sys/acpidev_dr.h>
#include <sys/acpidev_impl.h>

static ACPI_STATUS acpidev_cpu_container_probe(acpidev_walk_info_t *infop);
static acpidev_filter_result_t acpidev_cpu_container_filter(
    acpidev_walk_info_t *infop, char *devname, int maxlen);
static ACPI_STATUS acpidev_cpu_container_init(acpidev_walk_info_t *infop);
static acpidev_filter_result_t acpidev_cpu_container_filter_func(
    acpidev_walk_info_t *infop, ACPI_HANDLE hdl, acpidev_filter_rule_t *rulep,
    char *devname, int devnamelen);

/*
 * Class driver for ACPI CPU container objects.
 */
acpidev_class_t acpidev_class_cpu_container = {
	0,				/* adc_refcnt */
	ACPIDEV_CLASS_REV1,		/* adc_version */
	ACPIDEV_CLASS_ID_CPU_CONTAINER,	/* adc_class_id */
	"ACPI CPU Container",		/* adc_class_name */
	ACPIDEV_TYPE_CPU_CONTAINER,	/* adc_dev_type */
	NULL,				/* adc_private */
	NULL,				/* adc_pre_probe */
	NULL,				/* adc_post_probe */
	acpidev_cpu_container_probe,	/* adc_probe */
	acpidev_cpu_container_filter,	/* adc_filter */
	acpidev_cpu_container_init,	/* adc_init */
	NULL,				/* adc_fini */
};

static char *acpidev_cpu_container_device_ids[] = {
	ACPIDEV_HID_CPU_CONTAINER,
};

static char *acpidev_cpu_container_uid_formats[] = {
	"%u",
};

/* Filter rule table for CPU container objects. */
static acpidev_filter_rule_t acpidev_cpu_container_filters[] = {
	{	/* Create node and scan child for all CPU container objects */
		acpidev_cpu_container_filter_func,
		0,
		ACPIDEV_FILTER_DEFAULT,
		&acpidev_class_list_device,
		1,
		INT_MAX,
		NULL,
		ACPIDEV_NODE_NAME_CPU_CONTAINER,
	}
};

static ACPI_STATUS
acpidev_cpu_container_probe(acpidev_walk_info_t *infop)
{
	ACPI_STATUS rc = AE_OK;
	int flags;

	ASSERT(infop != NULL);
	ASSERT(infop->awi_hdl != NULL);
	ASSERT(infop->awi_info != NULL);

	if (infop->awi_info->Type != ACPI_TYPE_DEVICE ||
	    acpidev_match_device_id(infop->awi_info,
	    ACPIDEV_ARRAY_PARAM(acpidev_cpu_container_device_ids)) == 0) {
		return (AE_OK);
	}

	flags = ACPIDEV_PROCESS_FLAG_SCAN;
	switch (infop->awi_op_type) {
	case ACPIDEV_OP_BOOT_PROBE:
		if (acpica_get_devcfg_feature(ACPI_DEVCFG_CONTAINER)) {
			flags |= ACPIDEV_PROCESS_FLAG_CREATE;
			acpidev_dr_check(infop);
		}
		break;

	case ACPIDEV_OP_BOOT_REPROBE:
		break;

	case ACPIDEV_OP_HOTPLUG_PROBE:
		if (acpica_get_devcfg_feature(ACPI_DEVCFG_CONTAINER)) {
			flags |= ACPIDEV_PROCESS_FLAG_CREATE |
			    ACPIDEV_PROCESS_FLAG_SYNCSTATUS |
			    ACPIDEV_PROCESS_FLAG_HOLDBRANCH;
		}
		break;

	default:
		ACPIDEV_DEBUG(CE_WARN, "!acpidev: unknown operation type %u in "
		    "acpidev_cpu_container_probe().", infop->awi_op_type);
		rc = AE_BAD_PARAMETER;
		break;
	}

	if (rc == AE_OK) {
		rc = acpidev_process_object(infop, flags);
	}

	if (ACPI_FAILURE(rc) && rc != AE_NOT_EXIST && rc != AE_ALREADY_EXISTS) {
		cmn_err(CE_WARN,
		    "!acpidev: failed to process CPU container object %s.",
		    infop->awi_name);
	} else {
		rc = AE_OK;
	}

	return (rc);
}

static acpidev_filter_result_t
acpidev_cpu_container_filter_func(acpidev_walk_info_t *infop, ACPI_HANDLE hdl,
    acpidev_filter_rule_t *rulep, char *devname, int devnamelen)
{
	ACPI_BUFFER buf;
	acpidev_filter_result_t res;

	res = acpidev_filter_default(infop, hdl, rulep, devname, devnamelen);
	/* Return if we don't need to generate a device name. */
	if (devname == NULL || res == ACPIDEV_FILTER_FAILED ||
	    res == ACPIDEV_FILTER_SKIP) {
		return (res);
	}

	/*
	 * Name the node after the ACPI object, cleaned up the same
	 * way device nodes are named.
	 */
	buf.Pointer = devname;
	buf.Length = devnamelen;
	if (ACPI_FAILURE(AcpiGetName(infop->awi_hdl,
	    ACPI_SINGLE_NAME, &buf))) {
		(void) strlcpy(devname, ACPIDEV_NODE_NAME_CPU_CONTAINER,
		    devnamelen);
	}
	acpidev_trim_devname(infop, devname);

	return (res);
}

static acpidev_filter_result_t
acpidev_cpu_container_filter(acpidev_walk_info_t *infop,
    char *devname, int maxlen)
{
	acpidev_filter_result_t res;

	ASSERT(infop != NULL);
	if (infop->awi_op_type == ACPIDEV_OP_BOOT_PROBE ||
	    infop->awi_op_type == ACPIDEV_OP_BOOT_REPROBE ||
	    infop->awi_op_type == ACPIDEV_OP_HOTPLUG_PROBE) {
		res = acpidev_filter_device(infop, infop->awi_hdl,
		    ACPIDEV_ARRAY_PARAM(acpidev_cpu_container_filters),
		    devname, maxlen);
	} else {
		res = ACPIDEV_FILTER_FAILED;
	}

	return (res);
}

static ACPI_STATUS
acpidev_cpu_container_init(acpidev_walk_info_t *infop)
{
	static char *compatible[] = {
		ACPIDEV_TYPE_CPU_CONTAINER,
		ACPIDEV_HID_VIRTNEX,
		ACPIDEV_TYPE_VIRTNEX,
	};

	ASSERT(infop != NULL);
	ASSERT(infop->awi_hdl != NULL);
	ASSERT(infop->awi_dip != NULL);

	if (ACPI_FAILURE(acpidev_set_compatible(infop,
	    ACPIDEV_ARRAY_PARAM(compatible)))) {
		return (AE_ERROR);
	}

	if (ddi_prop_update_int(DDI_DEV_T_NONE, infop->awi_dip,
	    OBP_ADDRESS_CELLS, ACPIDEV_ROOTNEX_ADDRESS_CELLS) != DDI_SUCCESS) {
		return (AE_ERROR);
	}

	if (ddi_prop_update_int(DDI_DEV_T_NONE, infop->awi_dip,
	    OBP_SIZE_CELLS, ACPIDEV_ROOTNEX_SIZE_CELLS) != DDI_SUCCESS) {
		return (AE_ERROR);
	}

	if (ACPI_FAILURE(acpidev_set_unitaddr(infop,
	    ACPIDEV_ARRAY_PARAM(acpidev_cpu_container_uid_formats), NULL))) {
		return (AE_ERROR);
	}

	return (AE_OK);
}
