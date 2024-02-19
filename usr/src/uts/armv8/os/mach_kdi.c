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
 */

/*
 * Kernel/Debugger Interface (KDI) routines.  Called during debugger under
 * various system states (boot, while running, while the debugger has control).
 * Functions intended for use while the debugger has control may not grab any
 * locks or perform any functions that assume the availability of other system
 * services.
 */

#include <sys/systm.h>
#include <sys/kdi_impl.h>
#include <sys/smp_impldefs.h>
#include <sys/archsystm.h>
#include <sys/controlregs.h>
#include <sys/trap.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>
#include <sys/x_call.h>
#include <sys/reboot.h>

kdi_cpusave_t *kdi_cpusave;
int kdi_ncpusave;

static kdi_main_t kdi_kmdb_main;

#define	KDI_MEMRANGES_MAX	2

kdi_memrange_t	kdi_memranges[KDI_MEMRANGES_MAX];
int		kdi_nmemranges;

extern void flush_data_cache_all(void);
extern void kdi_exception_vector(void);
extern void exception_vector(void);

void
kdi_flush_caches(void)
{
	flush_data_cache_all();
	dsb(ish);
	invalidate_instruction_cache_all();
	dsb(ish);
	isb();
}


/*
 * On ARM, slaves busy-loop, so we don't need to do anything here.
 */
void
kdi_start_slaves(void)
{
}

void
kdi_slave_wait(void)
{
}

static int
kdi_cpu_deactivate(xc_arg_t arg1 __unused, xc_arg_t arg2 __unused,
    xc_arg_t arg3 __unused)
{
	write_mdscr_el1(read_mdscr_el1() & ~(MDSCR_KDE | MDSCR_MDE));
	write_vbar((uintptr_t)&exception_vector);
	return (0);
}

void
kdi_deactivate(void)
{
	cpuset_t cpuset;
	CPUSET_ALL(cpuset);

	xc_call(0, 0, 0, CPUSET2BV(cpuset), kdi_cpu_deactivate);
	kdi_nmemranges = 0;
}

void
kdi_memrange_add(caddr_t base, size_t len)
{
	kdi_memrange_t *mr = &kdi_memranges[kdi_nmemranges];

	ASSERT(kdi_nmemranges != KDI_MEMRANGES_MAX);

	mr->mr_base = base;
	mr->mr_lim = base + len - 1;
	kdi_nmemranges++;
}

extern void kdi_slave_entry(void);

void
kdi_stop_slaves(int cpu, int doxc)
{
	if (doxc)
		kdi_xc_others(cpu, kdi_slave_entry);
}

extern void unlock_oslock(void);

/*
 * Activation for all CPUs for mod-loaded kmdb, i.e. a kmdb that wasn't
 * loaded at boot.
 */
static int
kdi_cpu_activate(xc_arg_t arg1 __unused, xc_arg_t arg2 __unused,
    xc_arg_t arg3 __unused)
{
	write_vbar((uintptr_t)&kdi_exception_vector);
	write_mdscr_el1(read_mdscr_el1() | MDSCR_KDE | MDSCR_MDE);
	clear_daif(DAIF_SETCLEAR_DEBUG);
	unlock_oslock();
	return (0);
}

void
kdi_cpu_init(void)
{
	kdi_cpu_activate(0, 0, 0);
}

void
kdi_activate(kdi_main_t main, kdi_cpusave_t *cpusave, uint_t ncpusave)
{
	cpuset_t cpuset;

	CPUSET_ALL(cpuset);

	kdi_cpusave = cpusave;
	kdi_ncpusave = ncpusave;
	kdi_kmdb_main = main;

	for (int i = 0; i < kdi_ncpusave; i++) {
		kdi_cpusave[i].krs_cpu_id = i;

		kdi_cpusave[i].krs_curcrumb =
		    &kdi_cpusave[i].krs_crumbs[KDI_NCRUMBS - 1];
		kdi_cpusave[i].krs_curcrumbidx = KDI_NCRUMBS - 1;
	}

	kdi_memranges[0].mr_base = kdi_segdebugbase;
	kdi_memranges[0].mr_lim = kdi_segdebugbase + kdi_segdebugsize - 1;
	kdi_nmemranges = 1;

	/*
	 * If we're boot-loaded we can't xcall yet so have to do this ourself
	 * to only ourself.
	 */
	if (boothowto & RB_KMDB) {
		(void) kdi_cpu_init();
	} else {
		xc_call(0, 0, 0, CPUSET2BV(cpuset), kdi_cpu_activate);
	}
}

void
kdi_debugger_entry(void *save)
{
	kdi_kmdb_main(save);
}
