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
 * Copyright 2024 Michael van der Westhuizen
 */

/*
 * Configure the cyclic backend by attaching the Arm Generic Timer driver when
 * `cbe_init' is called from `main'.
 */

#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/obpdefs.h>

static int
tmr_matcher(dev_info_t *rdip, void *arg)
{
	char		**data;
	uint_t		nelements;
	uint_t		n;
	dev_info_t	**result = arg;

	ASSERT3P(rdip, !=, NULL);
	ASSERT3P(result, !=, NULL);
	ASSERT3P(*result, ==, NULL);

	if (ddi_prop_lookup_string_array(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS,
	    OBP_COMPATIBLE, &data, &nelements) == DDI_PROP_SUCCESS) {
		for (n = 0; n < nelements; ++n) {
			if (strcmp(data[n], "arm,armv8-timer") == 0) {
				*result = rdip;
				break;
			}
		}

		ddi_prop_free(data);
	}

	if (*result != NULL)
		return (DDI_WALK_TERMINATE);

	return (DDI_WALK_CONTINUE);
}

void
cbe_init(void)
{
	static dev_info_t *dip = NULL;
	if (dip != NULL)
		return;

	ndi_devi_enter(ddi_root_node());
	ddi_walk_devs(ddi_get_child(ddi_root_node()), tmr_matcher, &dip);
	ndi_devi_exit(ddi_root_node());

	if (dip == NULL)
		panic("unable to locate architected timer node");

	ndi_hold_devi(dip);
	if (i_ddi_attach_node_hierarchy(dip) != DDI_SUCCESS)
		panic("unable to attach architected timer node");
	ndi_rele_devi(dip);
}

void
cbe_init_pre(void)
{
	/* unnecessary on aarch64 with the architected timer */
}
