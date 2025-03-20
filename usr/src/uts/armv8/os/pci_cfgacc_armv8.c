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
 * Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
 */

#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/pci.h>
#include <sys/pci_cfgacc.h>
#include <sys/pci_cfgspace.h>

typedef void (*acc_impl_t)(pci_cfgacc_req_t *);

void
pci_cfgacc_acc(pci_cfgacc_req_t *req)
{
	acc_impl_t acc_impl;

	VERIFY3P(req, !=, NULL);
	VERIFY3P(req->rcdip, !=, NULL);

	/*
	 * XXXPCI: This is not how I would like to do this, but everything else
	 * in my brain is far worse
	 */
	acc_impl = (acc_impl_t)(uintptr_t)ddi_prop_get_int64(DDI_DEV_T_ANY,
	    req->rcdip, DDI_PROP_DONTPASS, OBP_CFGSPACE_HOOK, 0);

	if (acc_impl == NULL) {
		dev_err(req->rcdip, CE_WARN, "No '" OBP_CFGSPACE_HOOK "' on "
		    "PCI root complex");
		if (!req->write) {
			VAL64(req) = PCI_EINVAL64;
		}
		return;
	}

	acc_impl(req);
}
