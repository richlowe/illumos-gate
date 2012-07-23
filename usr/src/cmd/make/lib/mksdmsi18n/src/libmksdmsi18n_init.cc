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
 * Copyright 1996 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms.
 */
/*
 * @(#)libmksdmsi18n_init.cc 1.5 06/12/12
 */

#pragma	ident	"@(#)libmksdmsi18n_init.cc	1.5	06/12/12"

#include <avo/intl.h>
#include <stdio.h>
#include <stdlib.h>

nl_catd libmksdmsi18n_catd;
 
/*
 * Open the catalog file for libmksdmsi18n.  Users of this library must set
 * NSLPATH first.  See avo_18n_init().
 */
int
libmksdmsi18n_init()
{
	char		name[20];

	if (getenv(NOCATGETS("NLSPATH")) == NULL) {
		fprintf(stderr, NOCATGETS("Internal error: Set NLSPATH before opening catalogue file\n"));
		return 1;
	}
	sprintf(name, NOCATGETS("libmksdmsi18n_%d"), I18N_VERSION);
	libmksdmsi18n_catd = catopen(name, NL_CAT_LOCALE);
	return 0;
}
 
/*
 * Close the catalog file for libmksdmsi18n
 */
void
libmksdmsi18n_fini()
{
	catclose(libmksdmsi18n_catd);
}

