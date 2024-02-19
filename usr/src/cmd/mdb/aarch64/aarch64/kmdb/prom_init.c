/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/ccompile.h>

/*
 *  Every standalone that wants to use this library must call
 *  prom_init() before any of the other routines can be called.
 *
 *  XXXKDI: The real prom_init() on arm loads the FDT among other things,
 *  which doesn't seem like something kmdb should be doing.  So we have a
 *  stub, here.  And don't end up using prom_node.c
 */
void
prom_init(char *pgmname __unused, void *cookie __unused)
{
	/* nothing to do */
}
