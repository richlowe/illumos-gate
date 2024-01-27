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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpuvar.h>
#include <sys/gic.h>
#include <sys/irq.h>
#include <sys/spl.h>
#include <sys/x_call.h>
#include <sys/xc_levels.h>
#include <sys/machsystm.h>

void
poke_cpu(int cpun)
{
	if (panicstr)
		return;

	send_dirint(cpun, IRQ_IPI_CPUPOKE);
}

void
cpu_call(cpu_t *cp, cpu_call_func_t func, uintptr_t arg1, uintptr_t arg2)
{
	cpuset_t set;

	if (panicstr)
		return;

	kpreempt_disable();

	if (CPU == cp) {
		int save_spl = splr(ipltospl(XC_HI_PIL));
		(*func)(arg1, arg2);
		splx(save_spl);
	} else {
		CPUSET_ONLY(set, cp->cpu_id);
		/* XXXARM */
		xc_call((xc_arg_t)arg1, (xc_arg_t)arg2, 0, set, (xc_func_t)(uintptr_t)func);
	}

	kpreempt_enable();
}
