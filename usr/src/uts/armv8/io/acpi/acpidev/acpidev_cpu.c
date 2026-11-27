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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2009-2010, Intel Corporation.
 * All rights reserved.
 */
/*
 * Copyright 2016 Nexenta Systems, Inc.
 * Copyright 2026 Michael van der Westhuizen
 */

#include <sys/types.h>
#include <sys/atomic.h>
#include <sys/bootconf.h>
#include <sys/cpuvar.h>
#include <sys/machsystm.h>
#include <sys/note.h>
#include <sys/aarch64_archext.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/acpidev.h>
#include <sys/acpidev_impl.h>
#include <sys/cpuinfo.h>

struct acpidev_cpu_map_item {
	uint32_t	proc_id;
	uint64_t	mpidr;
};

struct acpidev_cpu_MAT_arg {
	boolean_t	found;
	boolean_t	enabled;
	uint32_t	proc_id;
	uint64_t	mpidr;
};

static ACPI_STATUS acpidev_cpu_pre_probe(acpidev_walk_info_t *infop);
static ACPI_STATUS acpidev_cpu_post_probe(acpidev_walk_info_t *infop);
static ACPI_STATUS acpidev_cpu_probe(acpidev_walk_info_t *infop);
static acpidev_filter_result_t acpidev_cpu_filter(acpidev_walk_info_t *infop,
    char *devname, int maxlen);
static ACPI_STATUS acpidev_cpu_init(acpidev_walk_info_t *infop);
static void acpidev_cpu_fini(ACPI_HANDLE hdl, acpidev_data_handle_t dhdl,
    acpidev_class_t *clsp);

static acpidev_filter_result_t acpidev_cpu_filter_func(
    acpidev_walk_info_t *infop, ACPI_HANDLE hdl, acpidev_filter_rule_t *afrp,
    char *devname, int len);
static int acpidev_cpu_create_dip(cpu_t *, dev_info_t **);

/*
 * Default class driver for ACPI processor/CPU objects.
 */
acpidev_class_t acpidev_class_cpu = {
	0,				/* adc_refcnt */
	ACPIDEV_CLASS_REV1,		/* adc_version */
	ACPIDEV_CLASS_ID_CPU,		/* adc_class_id */
	"ACPI CPU",			/* adc_class_name */
	ACPIDEV_TYPE_CPU,		/* adc_dev_type */
	NULL,				/* adc_private */
	acpidev_cpu_pre_probe,		/* adc_pre_probe */
	acpidev_cpu_post_probe,		/* adc_post_probe */
	acpidev_cpu_probe,		/* adc_probe */
	acpidev_cpu_filter,		/* adc_filter */
	acpidev_cpu_init,		/* adc_init */
	acpidev_cpu_fini,		/* adc_fini */
};

/*
 * List of class drivers which will be called in order when handling
 * children of ACPI cpu/processor objects.
 */
acpidev_class_list_t *acpidev_class_list_cpu = NULL;

/* Filter rule table for the first probe at boot time. */
static acpidev_filter_rule_t acpidev_cpu_filters[] = {
	{	/* Skip all processors under root node, should be there. */
		NULL,
		0,
		ACPIDEV_FILTER_SKIP,
		NULL,
		1,
		1,
		NULL,
		NULL,
	},
	{	/* Create and scan other processor objects */
		acpidev_cpu_filter_func,
		0,
		ACPIDEV_FILTER_DEFAULT,
		&acpidev_class_list_cpu,
		2,
		INT_MAX,
		NULL,
		ACPIDEV_NODE_NAME_CPU,
	}
};

/* ACPI/PNP hardware id for processor. */
static char *acpidev_processor_device_ids[] = {
	ACPIDEV_HID_CPU,
};

static char *acpidev_cpu_uid_formats[] = {
	"SCK%x-CPU%x",
};

static ACPI_HANDLE acpidev_cpu_map_hdl;
static uint32_t acpidev_cpu_map_count;
static struct acpidev_cpu_map_item *acpidev_cpu_map;

extern int (*psm_cpu_create_devinfo)(cpu_t *, dev_info_t **);

/* Count how many enabled CPUs are in the MADT table. */
static ACPI_STATUS
acpidev_cpu_count_MADT(ACPI_SUBTABLE_HEADER *ap, void *context)
{
	uint32_t *cntp;
	ACPI_MADT_GENERIC_INTERRUPT *gicc;

	cntp = (uint32_t *)context;
	switch (ap->Type) {
	case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
		gicc = (ACPI_MADT_GENERIC_INTERRUPT *)ap;
		if (gicc->Flags & ACPI_MADT_ENABLED) {
			(*cntp)++;
		}
		break;
	default:
		break;
	}

	return (AE_OK);
}

/* Extract information from the enabled CPUs using the MADT table. */
static ACPI_STATUS
acpidev_cpu_parse_MADT(ACPI_SUBTABLE_HEADER *ap, void *context)
{
	uint32_t *cntp;
	ACPI_MADT_GENERIC_INTERRUPT *gicc;

	cntp = (uint32_t *)context;
	switch (ap->Type) {
	case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
		gicc = (ACPI_MADT_GENERIC_INTERRUPT *)ap;
		if (gicc->Flags & ACPI_MADT_ENABLED) {
			ASSERT(*cntp < acpidev_cpu_map_count);
			acpidev_cpu_map[*cntp].proc_id = gicc->Uid;
			acpidev_cpu_map[*cntp].mpidr = gicc->ArmMpidr;
			(*cntp)++;
		}
		break;
	default:
		break;
	}

	return (AE_OK);
}

static ACPI_STATUS
acpidev_cpu_get_mpidr(uint32_t procid, uint64_t *apicidp)
{
	uint32_t i;

	for (i = 0; i < acpidev_cpu_map_count; i++) {
		if (acpidev_cpu_map[i].proc_id == procid) {
			*apicidp = acpidev_cpu_map[i].mpidr;
			return (AE_OK);
		}
	}

	return (AE_NOT_FOUND);
}

/*
 * Extract information for enabled CPUs from the buffer returned
 * by the _MAT method.
 */
static ACPI_STATUS
acpidev_cpu_query_MAT(ACPI_SUBTABLE_HEADER *ap, void *context)
{
	ACPI_MADT_GENERIC_INTERRUPT *gicc;
	struct acpidev_cpu_MAT_arg *rp;

	rp = (struct acpidev_cpu_MAT_arg *)context;
	switch (ap->Type) {
	case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
		gicc = (ACPI_MADT_GENERIC_INTERRUPT *)ap;
		rp->found = B_TRUE;
		rp->proc_id = gicc->Uid;
		rp->mpidr = gicc->ArmMpidr;
		rp->enabled =
		    (gicc->Flags & ACPI_MADT_ENABLED) ? B_TRUE : B_FALSE;
		return (AE_CTRL_TERMINATE);
	default:
		ACPIDEV_DEBUG(CE_NOTE,
		    "!acpidev: unknown MADT entry type %u in _MAT.", ap->Type);
		break;
	}

	return (AE_OK);
}

/*
 * Query ACPI processor ID by evaluating ACPI _MAT, _UID, and PROCESSOR
 * objects.
 */
static ACPI_STATUS
acpidev_cpu_get_procid(acpidev_walk_info_t *infop, uint32_t *idp)
{
	int id;
	ACPI_HANDLE hdl;
	struct acpidev_cpu_MAT_arg mat;

	if (infop->awi_info->Type != ACPI_TYPE_DEVICE) {
		ACPIDEV_DEBUG(CE_WARN,
		    "!acpidev: object %s is not DEVICE.",
		    infop->awi_name);
		return (AE_BAD_PARAMETER);
	}
	hdl = infop->awi_hdl;

	/*
	 * First try to evaluate _MAT.
	 *
	 * While _MAT is legal on processor devices I'd be surprised if we
	 * ever see it in the wild - the ACPI text steers people towards
	 * UID and MADT:
	 *   UID object values under a processor device are used to associate
	 *   processor devices with entries in the MADT.
	 *
	 * What we tend to see is entries in the MADT for all possible CPUs,
	 * but set to "not present" and "not online".  It's also totally
	 * legal to have these in the DSDT with _STA mentioning that they're
	 * "not present" and "not online" (until they're attached).
	 */
	bzero(&mat, sizeof (mat));
	(void) acpidev_walk_apic(NULL, hdl, ACPIDEV_METHOD_NAME_MAT,
	    acpidev_cpu_query_MAT, &mat);
	if (mat.found) {
		*idp = mat.proc_id;
		return (AE_OK);
	}


	/* Fall back to the _UID method. */
	if (ACPI_SUCCESS(acpica_eval_int(hdl, METHOD_NAME__UID, &id))) {
		*idp = id;
		return (AE_OK);
	}

	return (AE_NOT_FOUND);
}

static ACPI_STATUS
acpidev_cpu_get_proximity_id(ACPI_HANDLE hdl, uint32_t procid, uint32_t *pxmidp)
{
	int len, off;
	ACPI_SUBTABLE_HEADER *sp;
	ACPI_SRAT_GICC_AFFINITY *gp;

	ASSERT(hdl != NULL);
	ASSERT(pxmidp != NULL);
	*pxmidp = UINT32_MAX;

	if (ACPI_SUCCESS(acpidev_eval_pxm(hdl, pxmidp))) {
		return (AE_OK);
	}
	if (acpidev_srat_tbl_ptr == NULL) {
		return (AE_NOT_FOUND);
	}

	/* Search the static ACPI SRAT table for proximity domain id. */
	sp = (ACPI_SUBTABLE_HEADER *)(acpidev_srat_tbl_ptr + 1);
	len = acpidev_srat_tbl_ptr->Header.Length;
	off = sizeof (*acpidev_srat_tbl_ptr);
	while (off < len) {
		/*
		 * The subtable header must fit in the table before
		 * Type and Length can be read, and the entry must
		 * fit in the table.
		 */
		if (off + sizeof (*sp) > len ||
		    sp->Length < sizeof (*sp) ||
		    off + sp->Length > len) {
			break;
		}
		/*
		 * A GICC entry must cover the full GICC struct.
		 */
		if (sp->Type == ACPI_SRAT_TYPE_GICC_AFFINITY &&
		    sp->Length < sizeof (ACPI_SRAT_GICC_AFFINITY)) {
			break;
		}
		switch (sp->Type) {
		case ACPI_SRAT_TYPE_GICC_AFFINITY:
			gp = (ACPI_SRAT_GICC_AFFINITY *)sp;
			if ((gp->Flags & ACPI_SRAT_GICC_ENABLED) &&
			    gp->AcpiProcessorUid == procid) {
				*pxmidp = gp->ProximityDomain;
				return (AE_OK);
			}
			break;
		}
		off += sp->Length;
		sp = (ACPI_SUBTABLE_HEADER *)(((char *)sp) + sp->Length);
	}

	return (AE_NOT_FOUND);
}

static ACPI_STATUS
acpidev_cpu_pre_probe(acpidev_walk_info_t *infop)
{
	uint32_t count = 0;

	/* Parse and cache APIC info in MADT on the first probe at boot time. */
	ASSERT(infop != NULL);
	if (infop->awi_op_type == ACPIDEV_OP_BOOT_PROBE &&
	    acpidev_cpu_map_hdl == NULL) {
		/* Parse CPU relative information in the ACPI MADT table. */
		(void) acpidev_walk_apic(NULL, NULL, NULL,
		    acpidev_cpu_count_MADT, &acpidev_cpu_map_count);
		acpidev_cpu_map = kmem_zalloc(sizeof (acpidev_cpu_map[0])
		    * acpidev_cpu_map_count, KM_SLEEP);
		(void) acpidev_walk_apic(NULL, NULL, NULL,
		    acpidev_cpu_parse_MADT, &count);
		ASSERT(count == acpidev_cpu_map_count);
		acpidev_cpu_map_hdl = infop->awi_hdl;

		/* Cache pointer to the ACPI SRAT table. */
		if (ACPI_FAILURE(AcpiGetTable(ACPI_SIG_SRAT, 1,
		    (ACPI_TABLE_HEADER **)&acpidev_srat_tbl_ptr))) {
			acpidev_srat_tbl_ptr = NULL;
		}
	}

	return (AE_OK);
}

static ACPI_STATUS
acpidev_cpu_post_probe(acpidev_walk_info_t *infop)
{
	/* Free cached APIC info on the second probe at boot time. */
	ASSERT(infop != NULL);
	if (infop->awi_op_type == ACPIDEV_OP_BOOT_REPROBE &&
	    acpidev_cpu_map_hdl != NULL &&
	    infop->awi_hdl == acpidev_cpu_map_hdl) {
		if (acpidev_cpu_map != NULL && acpidev_cpu_map_count != 0) {
			kmem_free(acpidev_cpu_map, sizeof (acpidev_cpu_map[0])
			    * acpidev_cpu_map_count);
		}
		acpidev_cpu_map = NULL;
		acpidev_cpu_map_count = 0;
		acpidev_cpu_map_hdl = NULL;

		/* replace psm_cpu_create_devinfo with local implementation. */
		psm_cpu_create_devinfo = acpidev_cpu_create_dip;
	}

	return (AE_OK);
}

static ACPI_STATUS
acpidev_cpu_probe(acpidev_walk_info_t *infop)
{
	ACPI_STATUS rc = AE_OK;
	int flags;

	ASSERT(infop != NULL);
	ASSERT(infop->awi_hdl != NULL);
	ASSERT(infop->awi_info != NULL);
	ASSERT(infop->awi_class_curr == &acpidev_class_cpu);
	if (infop->awi_info->Type != ACPI_TYPE_DEVICE ||
	    acpidev_match_device_id(infop->awi_info,
	    ACPIDEV_ARRAY_PARAM(acpidev_processor_device_ids)) == 0) {
		return (AE_OK);
	}

	flags = ACPIDEV_PROCESS_FLAG_SCAN;
	switch (infop->awi_op_type) {
	case ACPIDEV_OP_BOOT_PROBE:
		/*
		 * Mark device as offline. It will be changed to online state
		 * when the corresponding CPU starts up.
		 */
		if (acpica_get_devcfg_feature(ACPI_DEVCFG_CPU)) {
			flags |= ACPIDEV_PROCESS_FLAG_CREATE |
			    ACPIDEV_PROCESS_FLAG_OFFLINE;
		}
		break;

	case ACPIDEV_OP_BOOT_REPROBE:
		break;

	case ACPIDEV_OP_HOTPLUG_PROBE:
		if (acpica_get_devcfg_feature(ACPI_DEVCFG_CPU)) {
			flags |= ACPIDEV_PROCESS_FLAG_CREATE |
			    ACPIDEV_PROCESS_FLAG_OFFLINE |
			    ACPIDEV_PROCESS_FLAG_SYNCSTATUS |
			    ACPIDEV_PROCESS_FLAG_HOLDBRANCH;
		}
		break;

	default:
		ACPIDEV_DEBUG(CE_WARN, "!acpidev: unknown operation type %u in "
		    "acpidev_cpu_probe().", infop->awi_op_type);
		rc = AE_BAD_PARAMETER;
		break;
	}

	if (rc == AE_OK) {
		rc = acpidev_process_object(infop, flags);
	}
	if (ACPI_FAILURE(rc) && rc != AE_NOT_EXIST && rc != AE_ALREADY_EXISTS) {
		cmn_err(CE_WARN,
		    "!acpidev: failed to process processor object %s.",
		    infop->awi_name);
	} else {
		rc = AE_OK;
	}

	return (rc);
}

static acpidev_filter_result_t
acpidev_cpu_filter_func(acpidev_walk_info_t *infop, ACPI_HANDLE hdl,
    acpidev_filter_rule_t *afrp, char *devname, int len)
{
	acpidev_filter_result_t res;

	ASSERT(afrp != NULL);
	if (infop->awi_op_type == ACPIDEV_OP_BOOT_PROBE ||
	    infop->awi_op_type == ACPIDEV_OP_BOOT_REPROBE) {
		uint32_t procid;
		uint64_t mpidr;

		if (ACPI_FAILURE(acpidev_cpu_get_procid(infop, &procid))) {
			ACPIDEV_DEBUG(CE_WARN,
			    "!acpidev: failed to query processor id for %s.",
			    infop->awi_name);
			return (ACPIDEV_FILTER_SKIP);
		} else if (ACPI_FAILURE(
		    acpidev_cpu_get_mpidr(procid, &mpidr))) {
			ACPIDEV_DEBUG(CE_WARN,
			    "!acpidev: failed to query mpidr for %s.",
			    infop->awi_name);
			return (ACPIDEV_FILTER_SKIP);
		}

		infop->awi_scratchpad[0] = procid;
		infop->awi_scratchpad[1] = mpidr;
	} else if (infop->awi_op_type == ACPIDEV_OP_HOTPLUG_PROBE) {
		struct acpidev_cpu_MAT_arg mat;

		bzero(&mat, sizeof (mat));
		(void) acpidev_walk_apic(NULL, hdl, ACPIDEV_METHOD_NAME_MAT,
		    acpidev_cpu_query_MAT, &mat);
		if (!mat.found) {
			cmn_err(CE_WARN,
			    "!acpidev: failed to walk apic resource for %s.",
			    infop->awi_name);
			return (ACPIDEV_FILTER_SKIP);
		} else if (!mat.enabled) {
			ACPIDEV_DEBUG(CE_NOTE,
			    "!acpidev: CPU %s has been disabled.",
			    infop->awi_name);
			return (ACPIDEV_FILTER_SKIP);
		}
		/* Save processor id and APIC id in scratchpad memory. */
		infop->awi_scratchpad[0] = mat.proc_id;
		infop->awi_scratchpad[1] = mat.mpidr;
	}

	res = acpidev_filter_default(infop, hdl, afrp, devname, len);

	return (res);
}

static acpidev_filter_result_t
acpidev_cpu_filter(acpidev_walk_info_t *infop, char *devname, int maxlen)
{
	acpidev_filter_result_t res;

	ASSERT(infop != NULL);
	ASSERT(devname == NULL || maxlen >= ACPIDEV_MAX_NAMELEN);
	if (infop->awi_op_type == ACPIDEV_OP_BOOT_PROBE ||
	    infop->awi_op_type == ACPIDEV_OP_BOOT_REPROBE ||
	    infop->awi_op_type == ACPIDEV_OP_HOTPLUG_PROBE) {
		res = acpidev_filter_device(infop, infop->awi_hdl,
		    ACPIDEV_ARRAY_PARAM(acpidev_cpu_filters), devname, maxlen);
	} else {
		res = ACPIDEV_FILTER_FAILED;
	}

	return (res);
}

static ACPI_STATUS
acpidev_cpu_init(acpidev_walk_info_t *infop)
{
	int count;
	uint32_t pxmid;
	dev_info_t *dip;
	ACPI_HANDLE hdl;
	char unitaddr[64];
	char **compatpp;
	static char *compatible[] = {
		ACPIDEV_TYPE_CPU,
		"cpu"
	};

	ASSERT(infop != NULL);
	dip = infop->awi_dip;
	hdl = infop->awi_hdl;

	/* Create "mpidr", "processor_id" and "proximity_id" properties. */
	if (ndi_prop_update_int(DDI_DEV_T_NONE, dip,
	    ACPIDEV_PROP_NAME_PROCESSOR_ID, infop->awi_scratchpad[0]) !=
	    NDI_SUCCESS) {
		cmn_err(CE_WARN,
		    "!acpidev: failed to set processor_id property for %s.",
		    infop->awi_name);
		return (AE_ERROR);
	}
	if (ndi_prop_update_int64(DDI_DEV_T_NONE, dip,
	    ACPIDEV_PROP_NAME_MPIDR, infop->awi_scratchpad[1]) !=
	    NDI_SUCCESS) {
		cmn_err(CE_WARN,
		    "!acpidev: failed to set %s property for %s.",
		    ACPIDEV_PROP_NAME_MPIDR, infop->awi_name);
		return (AE_ERROR);
	}

	if (ACPI_SUCCESS(acpidev_cpu_get_proximity_id(infop->awi_hdl,
	    infop->awi_scratchpad[0], &pxmid))) {
		if (ndi_prop_update_int(DDI_DEV_T_NONE, dip,
		    ACPIDEV_PROP_NAME_PROXIMITY_ID, pxmid) != NDI_SUCCESS) {
			cmn_err(CE_WARN, "!acpidev: failed to set proximity id "
			    "property for %s.", infop->awi_name);
			return (AE_ERROR);
		}
	}

	/*
	 * Set "compatible" property for CPU dip.
	 * acpidev_set_compatible() will handle HID/CID for CPU device.
	 */
	count = sizeof (compatible) / sizeof (compatible[0]);
	compatpp = compatible;
	if (ACPI_FAILURE(acpidev_set_compatible(infop, compatpp, count))) {
		return (AE_ERROR);
	}

	/*
	 * Set device unit-address property.
	 * First try to generate meaningful unit address from _UID,
	 * then use Processor Id if that fails.
	 */
	if ((infop->awi_info->Valid & ACPI_VALID_UID) == 0 ||
	    acpidev_generate_unitaddr(infop->awi_info->UniqueId.String,
	    ACPIDEV_ARRAY_PARAM(acpidev_cpu_uid_formats),
	    unitaddr, sizeof (unitaddr)) == NULL) {
		(void) snprintf(unitaddr, sizeof (unitaddr), "%u",
		    (uint32_t)infop->awi_scratchpad[0]);
	}
	if (ACPI_FAILURE(acpidev_set_unitaddr(infop, NULL, 0, unitaddr))) {
		return (AE_ERROR);
	}

	/*
	 * Build binding information for CPUs.
	 */
	if (infop->awi_op_type == ACPIDEV_OP_BOOT_PROBE ||
	    infop->awi_op_type == ACPIDEV_OP_BOOT_REPROBE ||
	    infop->awi_op_type == ACPIDEV_OP_HOTPLUG_PROBE) {
		uint32_t uid = (uint32_t)infop->awi_scratchpad[0];
		uint64_t mpidr = (uint64_t)infop->awi_scratchpad[1];
		processorid_t cpuid;

		if (ACPI_FAILURE(acpica_add_processor_to_map(
		    uid, hdl, mpidr))) {
			cmn_err(CE_WARN, "!acpidev: failed to bind processor "
			    "id/object handle for %s.", infop->awi_name);
			return (AE_ERROR);
		}

		cpuid = cpuinfo_id_for_mpidr(mpidr);
		if (cpuid == (processorid_t)-1) {
			cmn_err(CE_WARN, "!acpidev: failed to get processor ID "
			    "for %s.", infop->awi_name);
			return (AE_ERROR);
		}

		if (ACPI_FAILURE(acpica_map_cpu(cpuid, uid))) {
			cmn_err(CE_WARN, "!acpidev: failed to map CPU "
			    "for %s.", infop->awi_name);
			return (AE_ERROR);
		}
	} else {
		ACPIDEV_DEBUG(CE_WARN,
		    "!acpidev: unknown operation type %u in acpidev_cpu_init.",
		    infop->awi_op_type);
		return (AE_BAD_PARAMETER);
	}

	return (AE_OK);
}

static void
acpidev_cpu_fini(ACPI_HANDLE hdl, acpidev_data_handle_t dhdl __unused,
    acpidev_class_t *clsp __unused)
{
	int rc;
	uint32_t procid;

	rc = acpica_get_procid_by_object(hdl, &procid);
	ASSERT(ACPI_SUCCESS(rc));
	if (ACPI_SUCCESS(rc)) {
		rc = acpica_remove_processor_from_map(procid);
		ASSERT(ACPI_SUCCESS(rc));
		if (ACPI_FAILURE(rc)) {
			cmn_err(CE_WARN, "!acpidev: failed to remove "
			    "processor from ACPICA.");
		}
	}
}

/*
 * Lookup the dip for a CPU if ACPI CPU autoconfig is enabled.
 */
static int
acpidev_cpu_lookup_dip(cpu_t *cp, dev_info_t **dipp)
{
	uint64_t mpidr;
	ACPI_HANDLE hdl;
	dev_info_t *dip = NULL;

	*dipp = NULL;
	if (acpica_get_devcfg_feature(ACPI_DEVCFG_CPU)) {
		mpidr = cpuid_get_mpidr(cp);
		if (acpica_get_cpu_object_by_cpuid(cp->cpu_id, &hdl) == 0 ||
		    (mpidr != UINT64_MAX &&
		    acpica_get_cpu_object_by_mpidr(mpidr, &hdl) == 0)) {
			ASSERT(hdl != NULL);
			if (ACPI_SUCCESS(acpica_get_devinfo(hdl, &dip))) {
				ASSERT(dip != NULL);
				*dipp = dip;
				return (DDI_SUCCESS);
			}
		}
		ACPIDEV_DEBUG(CE_WARN,
		    "!acpidev: failed to lookup dip for cpu %d(%p).",
		    cp->cpu_id, (void *)cp);
	}

	return (DDI_FAILURE);
}

static int
acpidev_cpu_create_dip(cpu_t *cp, dev_info_t **dipp)
{
	if (acpidev_cpu_lookup_dip(cp, dipp) == DDI_SUCCESS) {
		ndi_hold_devi(*dipp);
		return (DDI_SUCCESS);
	}

	return (DDI_FAILURE);
}
