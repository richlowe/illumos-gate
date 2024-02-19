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

#ifndef _SYS_KDI_MACHIMPL_H
#define	_SYS_KDI_MACHIMPL_H


/*
 * The Kernel/Debugger interface.  The operations provided by the kdi_t,
 * defined below, comprise the Debugger -> Kernel portion of the interface,
 * and are to be used only when the system has been stopped.
 */

#include <sys/modctl.h>
#include <sys/types.h>
#include <sys/cpuvar.h>
#include <sys/kdi_regs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*kdi_main_t)(kdi_cpusave_t *);

typedef struct kdi_memrange {
	caddr_t mr_base;
	caddr_t mr_lim;
} kdi_memrange_t;

#define	KDI_MEMRANGES_MAX	2

typedef struct kdi_mach {
	void (*mkdi_activate)(kdi_main_t, kdi_cpusave_t *, uint_t);
	void (*mkdi_deactivate)(void);

	uintptr_t (*mkdi_get_userlimit)(void);

	void (*mkdi_stop_slaves)(int, int);

	void (*mkdi_start_slaves)(void);

	void (*mkdi_slave_wait)(void);

	void (*mkdi_memrange_add)(caddr_t, size_t);

	void (*mkdi_reboot)(void);

	void (*mkdi_set_exception_vector)(kdi_cpusave_t *);
	size_t (*mkdi_num_wapts)(void);
	void (*mkdi_update_waptreg)(kdi_waptreg_t *);
	void (*mkdi_read_waptreg)(int, kdi_waptreg_t *);
} kdi_mach_t;

#define	mkdi_activate			kdi_mach.mkdi_activate
#define	mkdi_deactivate			kdi_mach.mkdi_deactivate
#define	mkdi_get_userlimit		kdi_mach.mkdi_get_userlimit
#define	mkdi_get_cpuinfo		kdi_mach.mkdi_get_cpuinfo
#define	mkdi_stop_slaves		kdi_mach.mkdi_stop_slaves
#define	mkdi_start_slaves		kdi_mach.mkdi_start_slaves
#define	mkdi_slave_wait			kdi_mach.mkdi_slave_wait
#define	mkdi_memrange_add		kdi_mach.mkdi_memrange_add
#define	mkdi_reboot			kdi_mach.mkdi_reboot
#define	mkdi_update_waptreg		kdi_mach.mkdi_update_waptreg
#define	mkdi_read_waptreg		kdi_mach.mkdi_read_waptreg
#define	mkdi_num_wapts			kdi_mach.mkdi_num_wapts
#define	mkdi_set_exception_vector	kdi_mach.mkdi_set_exception_vector

extern void hat_kdi_init(void);

extern void kdi_cpu_debug_init(kdi_cpusave_t *);

extern void kdi_xc_others(int, void (*)(void));
extern void kdi_start_slaves(void);
extern void kdi_slave_wait(void);

extern void kdi_activate(kdi_main_t, kdi_cpusave_t *, uint_t);
extern void kdi_deactivate(void);
extern void kdi_stop_slaves(int, int);
extern void kdi_memrange_add(caddr_t, size_t);
extern void kdi_reboot(void);

extern void kmdb_enter(void);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_KDI_MACHIMPL_H */
