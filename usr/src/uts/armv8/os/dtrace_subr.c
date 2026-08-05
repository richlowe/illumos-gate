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
 */

/*
 * Copyright (c) 2011, Joyent, Inc. All rights reserved.
 */

#include <sys/dtrace.h>
#include <sys/x_call.h>
#include <sys/cmn_err.h>
#include <sys/trap.h>
#include <sys/psw.h>
#include <sys/privregs.h>
#include <sys/machsystm.h>
#include <sys/controlregs.h>
#include <vm/seg_kmem.h>

typedef struct dtrace_invop_hdlr {
	int (*dtih_func)(uintptr_t, uintptr_t *, uintptr_t);
	struct dtrace_invop_hdlr *dtih_next;
} dtrace_invop_hdlr_t;

static dtrace_invop_hdlr_t *dtrace_invop_hdlr;

static volatile int reader_count;
int
dtrace_invop(uintptr_t addr, uintptr_t *stack, uintptr_t eax)
{
	dtrace_invop_hdlr_t *hdlr;
	int rval = 0;

	__sync_add_and_fetch(&reader_count, 1);

	for (hdlr = dtrace_invop_hdlr; hdlr != NULL; hdlr = hdlr->dtih_next) {
		rval = hdlr->dtih_func(addr, stack, eax);
		if (rval != 0)
			break;
	}

	__sync_add_and_fetch(&reader_count, -1);

	return (rval);
}

void
dtrace_invop_add(int (*func)(uintptr_t, uintptr_t *, uintptr_t))
{
	dtrace_invop_hdlr_t *hdlr;

	hdlr = kmem_alloc(sizeof (dtrace_invop_hdlr_t), KM_SLEEP);
	hdlr->dtih_func = func;
	hdlr->dtih_next = dtrace_invop_hdlr;
	dmb(ish);
	dtrace_invop_hdlr = hdlr;
}

void
dtrace_invop_remove(int (*func)(uintptr_t, uintptr_t *, uintptr_t))
{
	dtrace_invop_hdlr_t *hdlr = dtrace_invop_hdlr, *prev = NULL;

	for (;;) {
		if (hdlr == NULL)
			panic("attempt to remove non-existent invop handler");

		if (hdlr->dtih_func == func)
			break;

		prev = hdlr;
		hdlr = hdlr->dtih_next;
	}

	if (prev == NULL) {
		ASSERT(dtrace_invop_hdlr == hdlr);
		dtrace_invop_hdlr = hdlr->dtih_next;
	} else {
		ASSERT(dtrace_invop_hdlr != hdlr);
		prev->dtih_next = hdlr->dtih_next;
	}

	while (reader_count != 0) {}
	dmb(ish);

	kmem_free(hdlr, sizeof (dtrace_invop_hdlr_t));
}

int
dtrace_getipl(void)
{
	return (CPU->cpu_pri);
}

static int
dtrace_xcall_func(dtrace_xcall_t func, void *arg)
{
	(*func)(arg);

	return (0);
}

/*ARGSUSED*/
void
dtrace_xcall(processorid_t cpu, dtrace_xcall_t func, void *arg)
{
	cpuset_t set;

	CPUSET_ZERO(set);

	if (cpu == DTRACE_CPUALL) {
		CPUSET_ALL(set);
	} else {
		CPUSET_ADD(set, cpu);
	}

	kpreempt_disable();
	xc_sync((xc_arg_t)func, (xc_arg_t)arg, 0, CPUSET2BV(set),
	    (xc_func_t)(uintptr_t)dtrace_xcall_func); /* XXXARM */
	kpreempt_enable();
}

static void
dtrace_sync_func(void)
{
	membar_consumer();
}

void
dtrace_sync(void)
{
	membar_producer();
	dtrace_xcall(DTRACE_CPUALL, (dtrace_xcall_t)dtrace_sync_func, NULL);
}

void
dtrace_toxic_ranges(void (*func)(uintptr_t base, uintptr_t limit))
{
	if (hole_end > hole_start)
		(*func)((uintptr_t)hole_start, (uintptr_t)hole_end);
}

int (*dtrace_pid_probe_ptr)(struct regs *);
int (*dtrace_return_probe_ptr)(struct regs *);

void
dtrace_user_probe(struct regs *rp, int trapno)
{
	ASSERT(USERMODE(rp->r_spsr));

	krwlock_t *rwp;
	proc_t *p = curproc;
	if (curthread->t_cred != p->p_cred) {
		cred_t *oldcred = curthread->t_cred;
		/*
		 * DTrace accesses t_cred in probe context.  t_cred
		 * must always be either NULL, or point to a valid,
		 * allocated cred structure.
		 */
		curthread->t_cred = crgetcred();
		crfree(oldcred);
	}

	if (trapno == T_DTRACE_RET) {
		uint8_t step = curthread->t_dtrace_step;
		uint8_t ret = curthread->t_dtrace_ret;
		uintptr_t npc = curthread->t_dtrace_npc;

		if (curthread->t_dtrace_ast) {
			aston(curthread);
			curthread->t_sig_check = 1;
		}

		/*
		 * Clear all user tracing flags.
		 */
		curthread->t_dtrace_ft = 0;

		/*
		 * If we weren't expecting to take a return probe trap, kill
		 * the process as though it had just executed an unassigned
		 * trap instruction.
		 */
		if (step == 0) {
			tsignal(curthread, SIGILL);
			return;
		}

		/*
		 * If we hit this trap unrelated to a return probe, we're
		 * just here to reset the AST flag since we deferred a signal
		 * until after we logically single-stepped the instruction we
		 * copied out.
		 */
		if (ret == 0) {
			rp->r_pc = npc;
			return;
		}

		/*
		 * We need to wait until after we've called the
		 * dtrace_return_probe_ptr function pointer to set %pc.
		 */
		rwp = &CPU->cpu_ft_lock;
		rw_enter(rwp, RW_READER);
		if (dtrace_return_probe_ptr != NULL)
			(void) (*dtrace_return_probe_ptr)(rp);
		rw_exit(rwp);
		rp->r_pc = npc;

		return;
	} else if (trapno == T_DTRACE_PID) {
		rwp = &CPU->cpu_ft_lock;

		rw_enter(rwp, RW_READER);
		if (dtrace_pid_probe_ptr != NULL &&
		    (*dtrace_pid_probe_ptr)(rp) == 0) {
			rw_exit(rwp);
			return;
		}
		rw_exit(rwp);
	}

	tsignal(curthread, SIGSYS);
}

void
dtrace_safe_synchronous_signal(void)
{
	kthread_t *t = curthread;
	struct regs *rp = lwptoregs(ttolwp(t));
	size_t isz = t->t_dtrace_npc - t->t_dtrace_pc;

	ASSERT(t->t_dtrace_on);

	/*
	 * If we're not in the range of scratch addresses, we're not actually
	 * tracing user instructions so turn off the flags. If the instruction
	 * we copied out caused a synchonous trap, reset the pc back to its
	 * original value and turn off the flags.
	 */
	if (rp->r_pc < t->t_dtrace_scrpc ||
	    rp->r_pc > t->t_dtrace_astpc + isz) {
		t->t_dtrace_ft = 0;
	} else if (rp->r_pc == t->t_dtrace_scrpc ||
	    rp->r_pc == t->t_dtrace_astpc) {
		rp->r_pc = t->t_dtrace_pc;
		t->t_dtrace_ft = 0;
	}
}

int
dtrace_safe_defer_signal(void)
{
	kthread_t *t = curthread;
	struct regs *rp = lwptoregs(ttolwp(t));
	size_t isz = t->t_dtrace_npc - t->t_dtrace_pc;

	ASSERT(t->t_dtrace_on);

	/*
	 * If we're not in the range of scratch addresses, we're not actually
	 * tracing user instructions so turn off the flags.
	 */
	if (rp->r_pc < t->t_dtrace_scrpc ||
	    rp->r_pc > t->t_dtrace_astpc + isz) {
		t->t_dtrace_ft = 0;
		return (0);
	}

	/*
	 * If we've executed the original instruction, but haven't performed
	 * the jmp back to t->t_dtrace_npc or the clean up of any registers
	 * used to emulate %rip-relative instructions in 64-bit mode, do that
	 * here and take the signal right away. We detect this condition by
	 * seeing if the program counter is the range [scrpc + isz, astpc).
	 */
	if (t->t_dtrace_astpc - rp->r_pc <
	    t->t_dtrace_astpc - t->t_dtrace_scrpc - isz) {
		rp->r_pc = t->t_dtrace_npc;
		t->t_dtrace_ft = 0;
		return (0);
	}

	/*
	 * Otherwise, make sure we'll return to the kernel after executing
	 * the copied out instruction and defer the signal.
	 */
	if (!t->t_dtrace_step) {
		ASSERT(rp->r_pc < t->t_dtrace_astpc);
		rp->r_pc += t->t_dtrace_astpc - t->t_dtrace_scrpc;
		t->t_dtrace_step = 1;
	}

	t->t_dtrace_ast = 1;

	return (1);
}

int
dtrace_mach_aframes(void)
{
	return (0);
}
