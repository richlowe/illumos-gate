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

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/acpidev.h>
#include <sys/acpidev_devprop.h>
#include <sys/acpidev_impl.h>
#include <sys/uuid.h>
#include <sys/obpdefs.h>

/*
 * Propagate ACPI Device Specific Data (_DSD) Device Properties UUID
 * information to properties on a device node in the device tree.
 */

static const char *banned_props[] = {
	OBP_NAME,
	OBP_UNIT_ADDRESS,
	OBP_REG,
	OBP_INTR,
	OBP_RANGES,
	OBP_DMA_RANGES,
	OBP_INTERRUPTS,
	OBP_COMPATIBLE,
	OBP_STATUS,
	OBP_BOARDNUM,
	OBP_BUS_RANGE,
	OBP_IDPROM,
	OBP_DEVICETYPE,
	OBP_ADDRESS_CELLS,
	OBP_SIZE_CELLS,
	OBP_INTERRUPT_CELLS,
	OBP_INTERRUPT_CONTROLLER,
	OBP_INTERRUPT_PARENT,
	OBP_INTERRUPT_PRIORITIES,
	OBP_INTERRUPT_MAP,
	OBP_INTERRUPT_MAP_MASK,
	OBP_MSI_CONTROLLER,
	OBP_MSI_CELLS,
};

/*
 * The _DSD Device Properties UUID is: daffd814-6eba-4d8c-8a91-bc9bbf4aa301.
 */
static const struct uuid devprops_uuid = {
	.time_low			= 0xdaffd814,
	.time_mid			= 0x6eba,
	.time_hi_and_version		= 0x4d8c,
	.clock_seq_hi_and_reserved	= 0x8a,
	.clock_seq_low			= 0x91,
	.node_addr			= {0xbc, 0x9b, 0xbf, 0x4a, 0xa3, 0x01}
};

/*
 * Return true if a name is found in a list.
 */
static boolean_t
acpidev_devprop_in_strlist(const char *name,
    const char **slist, unsigned int nlist)
{
	unsigned int i;

	for (i = 0; i < nlist; ++i) {
		if (strcmp(slist[i], name) == 0) {
			return (B_TRUE);
		}
	}

	return (B_FALSE);
}

/*
 * Return true if the passed property name is banned.
 *
 * Banned properties are created from namespace objects or by drivers and
 * should never be overridden by propagated properties.
 */
static boolean_t
acpidev_devprop_banned_name(const char *name)
{
	return (acpidev_devprop_in_strlist(name,
	    ACPIDEV_ARRAY_PARAM(banned_props)));
}

/*
 * Return true if an integer DSD entry should be propagated as a 32bit integer.
 *
 * The default is to propagate as a 64bit integer.
 */
static boolean_t
acpidev_devprop_downgrade_int_width(
    acpidev_walk_info_t *infop __unused, const char *name)
{
	if (strcmp(name, "clock-frequency") == 0) {
		return (B_TRUE);
	}

	return (B_FALSE);
}

/*
 * This function maps grandfathered-in, but deprecated, property names to
 * their preferred replacements.
 *
 * The code throughout this file will create properties with both the old
 * and new names when a substitution happens.
 */
static char *
acpidev_dsd_key_name(acpidev_walk_info_t *infop __unused, char *name)
{
	ASSERT(name != NULL);

	if (strcmp(name, "phy-channel") == 0) {
		return ("acpi-phy-channel");
	} else if (strcmp(name, "phy-mode") == 0) {
		return ("acpi-phy-mode");
	} else if (strcmp(name, "mac-address") == 0) {
		return ("acpi-mac-address");
	} else if (strcmp(name, "max-transfer-unit") == 0) {
		return ("acpi-max-transfer-unit");
	} else if (strcmp(name, "max-speed") == 0) {
		return ("acpi-max-speed");
	}

	return (name);
}

/*
 * Propagate a 32bit or 64bit integer property.
 */
static ACPI_STATUS
acpidev_devprop_process_integer(acpidev_walk_info_t *infop,
    const ACPI_OBJECT *name, const ACPI_OBJECT *val)
{
	dev_t dev;
	int rc;
	char *key_name;

	ASSERT(infop != NULL);
	ASSERT(infop->awi_dip != NULL);
	ASSERT(name != NULL);
	ASSERT(val != NULL);
	ASSERT(name->Type == ACPI_TYPE_STRING);
	ASSERT(val->Type == ACPI_TYPE_INTEGER);

	key_name = acpidev_dsd_key_name(infop, name->String.Pointer);
	ASSERT(key_name != NULL);
	dev = makedevice(DDI_MAJOR_T_UNKNOWN, ddi_get_instance(infop->awi_dip));

	if (acpidev_devprop_downgrade_int_width(infop, name->String.Pointer)) {
		uint32_t v;
		v = (uint32_t)val->Integer.Value;
		rc = ndi_prop_update_int(dev, infop->awi_dip, key_name, v);

		if (rc == DDI_SUCCESS && key_name != name->String.Pointer) {
			rc = ndi_prop_update_int(dev, infop->awi_dip,
			    name->String.Pointer, v);
		}
	} else {
		rc = ndi_prop_update_int64(dev, infop->awi_dip, key_name,
		    val->Integer.Value);

		if (rc == DDI_SUCCESS && key_name != name->String.Pointer) {
			rc = ndi_prop_update_int64(dev, infop->awi_dip,
			    name->String.Pointer, val->Integer.Value);
		}
	}

	if (rc != DDI_SUCCESS) {
		return (AE_ERROR);
	}

	return (AE_OK);
}

/*
 * Propagate a string property.
 */
static ACPI_STATUS
acpidev_devprop_process_string(acpidev_walk_info_t *infop,
    const ACPI_OBJECT *name, const ACPI_OBJECT *val)
{
	int rc;
	dev_t dev;
	char *key_name;

	ASSERT(infop != NULL);
	ASSERT(infop->awi_dip != NULL);
	ASSERT(name != NULL);
	ASSERT(val != NULL);
	ASSERT(name->Type == ACPI_TYPE_STRING);
	ASSERT(val->Type == ACPI_TYPE_STRING);

	key_name = acpidev_dsd_key_name(infop, name->String.Pointer);
	ASSERT(key_name != NULL);
	dev = makedevice(DDI_MAJOR_T_UNKNOWN, ddi_get_instance(infop->awi_dip));

	rc = ndi_prop_update_string(dev, infop->awi_dip, key_name,
	    val->String.Pointer);

	if (rc == DDI_SUCCESS && key_name != name->String.Pointer) {
		rc = ndi_prop_update_string(dev, infop->awi_dip,
		    name->String.Pointer, val->String.Pointer);
	}

	if (rc != DDI_SUCCESS) {
		return (AE_ERROR);
	}

	return (AE_OK);
}

/*
 * MAC Address is a special case of integer list, where we know the package
 * will be 6 integers, representing the 6 octets of a MAC address.
 *
 * We add this property as a simple byte array rather than an integer list,
 * doing some validation of the package values along the way.
 */
static ACPI_STATUS
acpidev_devprop_process_mac_address(acpidev_walk_info_t *infop,
    const ACPI_OBJECT *name, const ACPI_OBJECT *val)
{
	int rc;
	int i;
	dev_t dev;
	char *key_name;
	uchar_t bytes[6];
	uint_t nelements = 6;

	ASSERT3P(infop, !=, NULL);
	ASSERT3P(infop->awi_dip, !=, NULL);
	ASSERT3P(name, !=, NULL);
	ASSERT3P(val, !=, NULL);

	if (val->Type != ACPI_TYPE_PACKAGE) {
		return (AE_BAD_DATA);
	}

	if (val->Package.Count != nelements) {
		return (AE_BAD_DATA);
	}

	for (i = 0; i < nelements; ++i) {
		if (val->Package.Elements[i].Type != ACPI_TYPE_INTEGER) {
			return (AE_BAD_DATA);
		}

		if (val->Package.Elements[i].Integer.Value > 255) {
			return (AE_BAD_DATA);
		}

		bytes[i] = (uchar_t)val->Package.Elements[i].Integer.Value;
	}

	key_name = acpidev_dsd_key_name(infop, name->String.Pointer);
	ASSERT(key_name != NULL);
	dev = makedevice(DDI_MAJOR_T_UNKNOWN, ddi_get_instance(infop->awi_dip));

	rc = ddi_prop_update_byte_array(dev, infop->awi_dip, key_name,
	    bytes, nelements);

	if (rc == DDI_SUCCESS && key_name != name->String.Pointer) {
		rc = ddi_prop_update_byte_array(dev, infop->awi_dip,
		    name->String.Pointer, bytes, nelements);
	}

	if (rc != DDI_SUCCESS) {
		return (AE_ERROR);
	}

	return (AE_OK);
}

/*
 * Propagate a property containing an array of 32bit or 64bit integers.
 */
static ACPI_STATUS
acpidev_devprop_process_integer_list(acpidev_walk_info_t *infop,
    const ACPI_OBJECT *name, const ACPI_OBJECT *val)
{
	int rc;
	UINT32 n;
	dev_t dev;
	char *key_name;

	ASSERT(infop != NULL);
	ASSERT(infop->awi_dip != NULL);
	ASSERT(name != NULL);
	ASSERT(val != NULL);
	ASSERT(name->Type == ACPI_TYPE_STRING);
	ASSERT(val->Type == ACPI_TYPE_PACKAGE);

	key_name = acpidev_dsd_key_name(infop, name->String.Pointer);
	ASSERT(key_name != NULL);
	if (strcmp(key_name, "acpi-mac-address") == 0) {
		return (acpidev_devprop_process_mac_address(infop, name, val));
	}

	dev = makedevice(DDI_MAJOR_T_UNKNOWN, ddi_get_instance(infop->awi_dip));

	if (acpidev_devprop_downgrade_int_width(infop, name->String.Pointer)) {
		int32_t *ints;
		uint_t nints;
		size_t sz;

		nints = val->Package.Count;
		sz = sizeof (int32_t) * nints;
		ints = kmem_alloc(sz, KM_SLEEP);
		for (n = 0; n < val->Package.Count; ++n) {
			uint32_t v;
			v = val->Package.Elements[n].Integer.Value;
			ints[n] = v;
		}

		rc = ndi_prop_update_int_array(dev, infop->awi_dip, key_name,
		    ints, nints);

		if (rc == DDI_SUCCESS && key_name != name->String.Pointer) {
			rc = ndi_prop_update_int_array(dev, infop->awi_dip,
			    name->String.Pointer, ints, nints);
		}

		kmem_free(ints, sz);
	} else {
		int64_t *ints;
		uint_t nints;
		size_t sz;

		nints = val->Package.Count;
		sz = sizeof (int64_t) * nints;
		ints = kmem_alloc(sz, KM_SLEEP);
		for (n = 0; n < val->Package.Count; ++n) {
			ints[n] = val->Package.Elements[n].Integer.Value;
		}

		rc = ndi_prop_update_int64_array(dev, infop->awi_dip, key_name,
		    ints, nints);

		if (rc == DDI_SUCCESS && key_name != name->String.Pointer) {
			rc = ndi_prop_update_int64_array(dev, infop->awi_dip,
			    name->String.Pointer, ints, nints);
		}

		kmem_free(ints, sz);
	}

	if (rc != DDI_SUCCESS) {
		return (AE_ERROR);
	}

	return (AE_OK);
}

/*
 * Propagate a string array property.
 */
static ACPI_STATUS
acpidev_devprop_process_string_list(acpidev_walk_info_t *infop,
    const ACPI_OBJECT *name, const ACPI_OBJECT *val)
{
	UINT32 n;
	char **vals;
	uint_t nvals;
	size_t sz;
	dev_t dev;
	int rc;
	char *key_name;

	ASSERT(infop != NULL);
	ASSERT(infop->awi_dip != NULL);
	ASSERT(name != NULL);
	ASSERT(val != NULL);
	ASSERT(name->Type == ACPI_TYPE_STRING);
	ASSERT(val->Type == ACPI_TYPE_PACKAGE);

	key_name = acpidev_dsd_key_name(infop, name->String.Pointer);
	ASSERT(key_name != NULL);
	dev = makedevice(DDI_MAJOR_T_UNKNOWN, ddi_get_instance(infop->awi_dip));

	nvals = val->Package.Count;
	sz = sizeof (char *) * nvals;
	vals = kmem_zalloc(sz, KM_SLEEP);

	for (n = 0; n < val->Package.Count; ++n) {
		vals[n] = ddi_strdup(
		    val->Package.Elements[n].String.Pointer, KM_SLEEP);
	}

	rc = ndi_prop_update_string_array(dev, infop->awi_dip, key_name,
	    vals, nvals);

	if (rc == DDI_SUCCESS && key_name != name->String.Pointer) {
		rc = ndi_prop_update_string_array(dev, infop->awi_dip,
		    name->String.Pointer, vals, nvals);
	}

	for (n = 0; n < val->Package.Count; ++n) {
		strfree(vals[n]);
	}
	kmem_free(vals, sz);

	if (rc != DDI_SUCCESS) {
		return (AE_ERROR);
	}

	return (AE_OK);
}

/*
 * Checks that the value is a package (list of values) and that the package
 * contents are homogenous, then calls the appropriate typed list function.
 */
static ACPI_STATUS
acpidev_devprop_process_list(acpidev_walk_info_t *infop,
    const ACPI_OBJECT *name, const ACPI_OBJECT *val)
{
	UINT32 n;
	ACPI_OBJECT_TYPE t;

	ASSERT(infop != NULL);
	ASSERT(infop->awi_dip != NULL);
	ASSERT(name != NULL);
	ASSERT(val != NULL);
	ASSERT(name->Type == ACPI_TYPE_STRING);
	ASSERT(val->Type == ACPI_TYPE_PACKAGE);

	if (val->Package.Count < 1) {
		return (AE_BAD_DATA);
	}

	t = val->Package.Elements[0].Type;
	for (n = 1; n < val->Package.Count; ++n) {
		if (val->Package.Elements[n].Type != t) {
			return (AE_BAD_DATA);
		}
	}

	switch (t) {
	case ACPI_TYPE_INTEGER:
		return (acpidev_devprop_process_integer_list(infop, name, val));
	case ACPI_TYPE_STRING:
		return (acpidev_devprop_process_string_list(infop, name, val));
	/*
	 * XXXARM: We should handle reference here, but this is extremely
	 * property-specific in the device tree world.  We have no hosts with
	 * this type of property yet, so defer for now.
	 */
	default:
		ACPIDEV_DEBUG(CE_WARN, "!acpidev: unhandled list member type "
		    "%d in acpidev_devprop_process_list().", (int)t);
		break;
	}

	return (AE_ERROR);
}

/*
 * Walks the packages in a _DSD package looking for the Device Properties UUID
 * package and propagating properties from that package into device tree
 * properties on the passed device when properties are found.
 */
static ACPI_STATUS
acpidev_devprop_walker(acpidev_walk_info_t *infop, ACPI_OBJECT *dsd)
{
	UINT32			i;
	UINT32			j;
	ACPI_STATUS		rc;
	const ACPI_OBJECT	*guid;
	const ACPI_OBJECT	*dsd_pkg;

	rc = AE_OK;

	ASSERT(infop != NULL);
	ASSERT(dsd != NULL);
	ASSERT(dsd->Type == ACPI_TYPE_PACKAGE);
	if (infop == NULL || dsd == NULL || dsd->Type != ACPI_TYPE_PACKAGE) {
		return (AE_BAD_PARAMETER);
	}

	/*
	 * The _DSD is described as a package consisting of UUID/package pairs,
	 * where the UUID describes the contents of the associated package.
	 *
	 * Therefore there must be an even number of entries.
	 */
	if ((dsd->Package.Count & 0x1) == 0x1) {
		return (AE_BAD_DATA);
	}

	/*
	 * Now we iterate through the UUID/package pairs under the _DSD, looking
	 * for the device properties UUID.
	 */
	for (i = 0; i < dsd->Package.Count; i += 2) {
		guid = &dsd->Package.Elements[i];

		if (guid->Type != ACPI_TYPE_BUFFER ||
		    guid->Buffer.Length != sizeof (devprops_uuid)) {
			continue;
		}

		if (memcmp(guid->Buffer.Pointer, &devprops_uuid,
		    sizeof (devprops_uuid)) != 0) {
			continue;
		}

		/*
		 * We have the entry for the device properties GUID, now check
		 * that the payload is a package and that it looks reasonable,
		 * which simply means that it is itself a package.
		 */
		dsd_pkg = &dsd->Package.Elements[i + 1];

		if (dsd_pkg->Type != ACPI_TYPE_PACKAGE) {
			ACPIDEV_DEBUG(CE_WARN, "!acpidev: _DSD properties GUID "
			    "does not have a package payload in "
			    "acpidev_devprop_walker().");
			continue;
		}

		/*
		 * Seems reasonable so far. Now we iterate through the package
		 * pairs, checking that they are packages, that they are pairs
		 * and that the first element is a string (the property name).
		 *
		 * We skip any attempts to redefine well known properties
		 * that we populate from namespace information.
		 */
		for (j = 0; j < dsd_pkg->Package.Count; j++) {
			ACPI_OBJECT *pkg;
			ACPI_OBJECT *name;
			ACPI_OBJECT *val;

			pkg = &dsd_pkg->Package.Elements[j];
			if (pkg->Type != ACPI_TYPE_PACKAGE ||
			    pkg->Package.Count != 2) {
				ACPIDEV_DEBUG(CE_WARN, "!acpidev: _DSD item %u "
				    "is not a package in "
				    "acpidev_devprop_walker().", j);
				continue;
			}

			name = &pkg->Package.Elements[0];
			if (name->Type != ACPI_TYPE_STRING ||
			    name->String.Length < 1) {
				ACPIDEV_DEBUG(CE_WARN, "!acpidev: _DSD item %u "
				    "name is invalid in "
				    "acpidev_devprop_walker().", j);
				continue;
			}

			if (acpidev_devprop_banned_name(name->String.Pointer)) {
				continue;
			}

			val = &pkg->Package.Elements[1];
			switch (val->Type) {
			case ACPI_TYPE_INTEGER:
				if (ACPI_FAILURE(
				    acpidev_devprop_process_integer(
				    infop, name, val))) {
					ACPIDEV_DEBUG(CE_WARN, "!acpidev: "
					    "failed to set DSD property %s",
					    name->String.Pointer);
				}
				break;
			case ACPI_TYPE_STRING:
				if (ACPI_FAILURE(
				    acpidev_devprop_process_string(
				    infop, name, val))) {
					ACPIDEV_DEBUG(CE_WARN, "!acpidev: "
					    "failed to set DSD property %s",
					    name->String.Pointer);
				}
				break;
			case ACPI_TYPE_PACKAGE:
				if (ACPI_FAILURE(
				    acpidev_devprop_process_list(
				    infop, name, val))) {
					ACPIDEV_DEBUG(CE_WARN, "!acpidev: "
					    "failed to set DSD property %s",
					    name->String.Pointer);
				}
				break;
			/*
			 * XXXARM: We should handle reference here, but this
			 * is extremely property-specific in the device tree
			 * world.  We have no hosts with this type of property
			 * yet, so defer for now.
			 */
			default:
				ACPIDEV_DEBUG(CE_WARN, "!acpidev: unhandled "
				    "element type %d in "
				    "acpidev_devprop_walker().",
				    (int)val->Type);
				break;
			}
		}
	}

	return (rc);
}

/*
 * Locate and evaluate the ACPI Device Specific Data associated to a device
 * node (via the _DSD method/object). The resulting data, if present, is
 * integrated into the device tree as appropriate.
 */
static ACPI_STATUS
acpidev_devprop_process_dsd(acpidev_walk_info_t *infop)
{
	ACPI_BUFFER dsd;
	ACPI_STATUS st;

	/*
	 * Evaluate _DSD on the device.  AcpiEvaluateObjectTyped handles both
	 * static data objects (ACPI_TYPE_PACKAGE) and control methods that
	 * return a package, so there is no need to distinguish between the
	 * two cases.  AE_NOT_FOUND is not an error - the device simply has
	 * no _DSD.
	 */
	dsd.Length = ACPI_ALLOCATE_BUFFER;
	dsd.Pointer = NULL;
	st = AcpiEvaluateObjectTyped(infop->awi_hdl, METHOD_NAME__DSD, NULL,
	    &dsd, ACPI_TYPE_PACKAGE);
	if (st == AE_NOT_FOUND) {
		return (AE_OK);
	}
	if (ACPI_SUCCESS(st)) {
		st = acpidev_devprop_walker(infop, dsd.Pointer);
		AcpiOsFree(dsd.Pointer);
	}

	return (st);
}

ACPI_STATUS
acpidev_devprop_process(acpidev_walk_info_t *infop)
{
	ACPI_STATUS rc = AE_OK;
	char path[MAXPATHLEN];

	ASSERT3P(infop, !=, NULL);
	if (infop == NULL) {
		ACPIDEV_DEBUG(CE_WARN, "!acpidev: invalid parameter "
		    "in acpidev_devprop_process().");
		return (AE_BAD_PARAMETER);
	}

	/*
	 * Only run on the reprobe pass - at this point we have instances.
	 *
	 * Creation happens on the first pass.
	 */
	if (infop->awi_op_type != ACPIDEV_OP_BOOT_REPROBE) {
		ACPIDEV_DEBUG(CE_WARN, "!acpidev: acpidev_devprop_process() "
		    "can only be called in reprobe.");
		return (AE_ERROR);
	}

	(void) ddi_pathname(infop->awi_dip, path);
	rc = acpidev_devprop_process_dsd(infop);
	if (ACPI_FAILURE(rc)) {
		ACPIDEV_DEBUG(CE_WARN,
		    "!acpidev: failed to walk ACPI device-specific "
		    "data of %s (%s).", path, infop->awi_name);
		return (rc);
	}

	return (rc);
}
