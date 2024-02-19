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
 *
 * Copyright 2018 Joyent, Inc.
 */

/*
 * Kernel/Debugger Interface (KDI) routines.  Called by the debugger under
 * various system states (boot, while running, while the debugger has control).
 * Functions intended for use while the debugger has control may not grab any
 * locks or perform any functions that assume the availability of other system
 * services.
 */

#include <sys/archsystm.h>
#include <sys/clock_impl.h>
#include <sys/controlregs.h>
#include <sys/cpuid.h>
#include <sys/kdi_impl.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>
#include <sys/smp_impldefs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/trap.h>

static void
kdi_system_claim(void)
{
	lbolt_debug_entry();
}

static void
kdi_system_release(void)
{
	lbolt_debug_return();
}

static uintptr_t
kdi_get_userlimit(void)
{
	return (_userlimit);
}

static void
kdi_plat_call(void (*platfn)(void))
{
	if (platfn != NULL)
		platfn();
}

void
kdi_set_exception_vector(kdi_cpusave_t *cpusave)
{
	extern void kdi_exception_vector(void);

	if (cpusave == NULL) {
		write_vbar((uintptr_t)&kdi_exception_vector);
	} else {
		write_vbar(cpusave->krs_exception_vector);
	}
}

/*
 * XXXKDI: Actually, the maximum is probably 64 where ID_AA64DFR1.WRPS is
 * non-0 but our assembler, etc, have no support for that yet (and in fact,
 * even the ARM isn't updated to account for it).
 */
#define	KDI_NWAPTS	16	/* Architectural max */
kdi_waptreg_t kdi_waptregs[KDI_NWAPTS];

size_t
kdi_num_wapts(void)
{
	uint32_t wrps = DFR0_WRPS(read_id_aa64dfr0()) + 1;
	return (MIN(wrps, KDI_NWAPTS));
}

void
kdi_update_waptreg(kdi_waptreg_t *waptreg)
{
	VERIFY3U(waptreg->kw_hwid, <, KDI_NWAPTS);

	/*
	 * Watchpoints not double-word aligned are deprecated, we should never
	 * create them
	 */
	VERIFY(IS_P2ALIGNED(waptreg->kw_addr, 8));

	kdi_waptregs[waptreg->kw_hwid] = *waptreg;
}

void
kdi_read_waptreg(int id, kdi_waptreg_t *out)
{
	VERIFY3U(id, <, KDI_NWAPTS);
	memcpy(out, &kdi_waptregs[id], sizeof (kdi_waptreg_t));
}

#define	DEF_WAPTREG_SET(n)					\
	void							\
	kdi_waptreg##n##_set(kdi_waptreg_t *waptreg)		\
	{							\
		VERIFY3U(n, ==, waptreg->kw_hwid);		\
		__asm__ __volatile__("msr dbgwvr" #n "_el1, %0"	\
		    :						\
		    : "r" (waptreg->kw_addr));			\
		__asm__ __volatile__("msr dbgwcr" #n "_el1, %0"	\
		    :						\
		    : "r" (waptreg->kw_ctl));			\
	}

/* BEGIN CSTYLED */
DEF_WAPTREG_SET(0)
DEF_WAPTREG_SET(1)
DEF_WAPTREG_SET(2)
DEF_WAPTREG_SET(3)
DEF_WAPTREG_SET(4)
DEF_WAPTREG_SET(5)
DEF_WAPTREG_SET(6)
DEF_WAPTREG_SET(7)
DEF_WAPTREG_SET(8)
DEF_WAPTREG_SET(9)
DEF_WAPTREG_SET(10)
DEF_WAPTREG_SET(11)
DEF_WAPTREG_SET(12)
DEF_WAPTREG_SET(13)
DEF_WAPTREG_SET(14)
DEF_WAPTREG_SET(15)
/* END CSTYLED */

void
kdi_waptreg_set(kdi_waptreg_t *waptreg)
{
	VERIFY3U(waptreg->kw_hwid, <, kdi_num_wapts());

	switch (waptreg->kw_hwid) {
	case 0:
		kdi_waptreg0_set(waptreg);
		break;
	case 1:
		kdi_waptreg1_set(waptreg);
		break;
	case 2:
		kdi_waptreg2_set(waptreg);
		break;
	case 3:
		kdi_waptreg3_set(waptreg);
		break;
	case 4:
		kdi_waptreg4_set(waptreg);
		break;
	case 5:
		kdi_waptreg5_set(waptreg);
		break;
	case 6:
		kdi_waptreg6_set(waptreg);
		break;
	case 7:
		kdi_waptreg7_set(waptreg);
		break;
	case 8:
		kdi_waptreg8_set(waptreg);
		break;
	case 9:
		kdi_waptreg9_set(waptreg);
		break;
	case 10:
		kdi_waptreg10_set(waptreg);
		break;
	case 11:
		kdi_waptreg11_set(waptreg);
		break;
	case 12:
		kdi_waptreg12_set(waptreg);
		break;
	case 13:
		kdi_waptreg13_set(waptreg);
		break;
	case 14:
		kdi_waptreg14_set(waptreg);
		break;
	case 15:
		kdi_waptreg15_set(waptreg);
		break;
	default:
		panic("Invalid watchpoint register: %d", waptreg->kw_hwid);
	}
}

void
kdi_restore_debugging_state(void)
{
	for (int i = 0; i < kdi_num_wapts(); i++) {
		kdi_waptreg_set(&kdi_waptregs[i]);
	}
}

/*
 * On AArch64, most of these are shared between platforms, so this is really
 * an arch_kdi_init().
 */
void
mach_kdi_init(kdi_t *kdi)
{
	for (int i = 0; i < KDI_NWAPTS; i++)
		kdi_waptregs[i].kw_hwid = i;

	kdi->kdi_plat_call = kdi_plat_call;
	kdi->kdi_kmdb_enter = kmdb_enter;
	kdi->mkdi_activate = kdi_activate;
	kdi->mkdi_deactivate = kdi_deactivate;
	kdi->mkdi_get_userlimit = kdi_get_userlimit;
	kdi->mkdi_stop_slaves = kdi_stop_slaves;
	kdi->mkdi_start_slaves = kdi_start_slaves;
	kdi->mkdi_slave_wait = kdi_slave_wait;
	kdi->mkdi_memrange_add = kdi_memrange_add;
	kdi->mkdi_reboot = kdi_reboot;
	kdi->mkdi_set_exception_vector = kdi_set_exception_vector;
	kdi->mkdi_num_wapts = kdi_num_wapts;
	kdi->mkdi_update_waptreg = kdi_update_waptreg;
	kdi->mkdi_read_waptreg = kdi_read_waptreg;
}

void
plat_kdi_init(kdi_t *kdi)
{
	kdi->pkdi_system_claim = kdi_system_claim;
	kdi->pkdi_system_release = kdi_system_release;
}
