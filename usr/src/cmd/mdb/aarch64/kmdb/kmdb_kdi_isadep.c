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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2018 Joyent, Inc.
 */

#include <sys/types.h>
#include <sys/kdi_impl.h>
#include <sys/cpuvar.h>

#include <mdb/mdb_debug.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_umem.h>
#include <kmdb/kmdb_dpi.h>
#include <mdb/mdb.h>

void
kmdb_kdi_stop_slaves(int my_cpuid, int doxc)
{
	/* Stop other CPUs if there are CPUs to stop */
	mdb.m_kdi->mkdi_stop_slaves(my_cpuid, doxc);
}

void
kmdb_kdi_start_slaves(void)
{
	mdb.m_kdi->mkdi_start_slaves();
}

void
kmdb_kdi_slave_wait(void)
{
	mdb.m_kdi->mkdi_slave_wait();
}

uintptr_t
kmdb_kdi_get_userlimit(void)
{
	return (mdb.m_kdi->mkdi_get_userlimit());
}

void
kmdb_kdi_init_isadep(kdi_t *kdi __unused, kmdb_auxv_t *kav __unused)
{
}

void
kmdb_kdi_activate(kdi_main_t main, kdi_cpusave_t *cpusave, int ncpusave)
{
	mdb.m_kdi->mkdi_activate(main, cpusave, ncpusave);
}

void
kmdb_kdi_deactivate(void)
{
	mdb.m_kdi->mkdi_deactivate();
}

size_t
kmdb_kdi_num_wapts(void)
{
	return (mdb.m_kdi->mkdi_num_wapts());
}

void
kmdb_kdi_update_waptreg(kdi_waptreg_t *waptreg)
{
	mdb.m_kdi->mkdi_update_waptreg(waptreg);
}

void
kmdb_kdi_read_waptreg(int hwid, kdi_waptreg_t *out)
{
	mdb.m_kdi->mkdi_read_waptreg(hwid, out);
}

void
kmdb_kdi_memrange_add(caddr_t base, size_t len)
{
	mdb.m_kdi->mkdi_memrange_add(base, len);
}

void
kmdb_kdi_reboot(void)
{
	mdb.m_kdi->mkdi_reboot();
}

void
kmdb_kdi_set_exception_vector(kdi_cpusave_t *cpusave)
{
	mdb.m_kdi->mkdi_set_exception_vector(cpusave);
}
