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
 * Copyright (c) 2013 by Delphix. All rights reserved.
 * Copyright (c) 2018, Joyent, Inc.
 */

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_gcore.h>
#include <mdb/mdb_debug.h>

#include <sys/psw.h>
#include <sys/privregs.h>

uintptr_t
gcore_prgetstackbase(mdb_proc_t *p)
{
	return (p->p_usrstack - p->p_stksize);
}

int
gcore_prfetchinstr(mdb_klwp_t *lwp, ulong_t *ip)
{
	*ip = (ulong_t)(instr_t)lwp->lwp_pcb.pcb_instr;
	return (lwp->lwp_pcb.pcb_flags & INSTR_VALID);
}

int
gcore_prisstep(mdb_klwp_t *lwp)
{
	return ((lwp->lwp_pcb.pcb_flags &
	    (NORMAL_STEP|WATCH_STEP|DEBUG_PENDING)) != 0);
}

void
gcore_getgregs(mdb_klwp_t *lwp, gregset_t grp)
{
	struct regs regs;

	if (mdb_vread(&regs, sizeof (regs), lwp->lwp_regs) != sizeof (regs)) {
		mdb_warn("Failed to read regs from %p\n", lwp->lwp_regs);
		return;
	}

	grp[REG_X0] = regs.r_x0;
	grp[REG_X1] = regs.r_x1;
	grp[REG_X2] = regs.r_x2;
	grp[REG_X3] = regs.r_x3;
	grp[REG_X4] = regs.r_x4;
	grp[REG_X5] = regs.r_x5;
	grp[REG_X6] = regs.r_x6;
	grp[REG_X7] = regs.r_x7;
	grp[REG_X8] = regs.r_x8;
	grp[REG_X9] = regs.r_x9;
	grp[REG_X10] = regs.r_x10;
	grp[REG_X11] = regs.r_x11;
	grp[REG_X12] = regs.r_x12;
	grp[REG_X13] = regs.r_x13;
	grp[REG_X14] = regs.r_x14;
	grp[REG_X15] = regs.r_x15;
	grp[REG_X16] = regs.r_x16;
	grp[REG_X17] = regs.r_x17;
	grp[REG_X18] = regs.r_x18;
	grp[REG_X19] = regs.r_x19;
	grp[REG_X20] = regs.r_x20;
	grp[REG_X21] = regs.r_x21;
	grp[REG_X22] = regs.r_x22;
	grp[REG_X23] = regs.r_x23;
	grp[REG_X24] = regs.r_x24;
	grp[REG_X25] = regs.r_x25;
	grp[REG_X26] = regs.r_x26;
	grp[REG_X27] = regs.r_x27;
	grp[REG_X28] = regs.r_x28;
	grp[REG_X29] = regs.r_x29;
	grp[REG_X30] = regs.r_x30;
	grp[REG_SP] = regs.r_sp;
	grp[REG_PC] = regs.r_pc;
	grp[REG_SPSR] = regs.r_spsr;
}

int
gcore_prgetrvals(mdb_klwp_t *lwp, long *rval1, long *rval2)
{
	struct regs *r = lwptoregs(lwp);

	if (r->r_spsr & PSR_C)
		return (r->r_x9);

	if (lwp->lwp_eosys == JUSTRETURN) {
		*rval1 = 0;
		*rval2 = 0;
	} else {
		*rval1 = r->r_r0;
		*rval2 = r->r_r1;
	}
	return (0);
}
