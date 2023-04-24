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

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/psw.h>
#include <sys/pcb.h>
#include <sys/buf.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/cpuvar.h>

#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/cmn_err.h>
#include <sys/stack.h>
#include <sys/copyops.h>

#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/vmparam.h>
#include <sys/fp.h>
#include <sys/archsystm.h>
#include <sys/vmsystm.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_kp.h>
#include <vm/page.h>

#include <fs/proc/prdata.h>

int	prnwatch = 10000;	/* maximum number of watched areas */

/*
 * Force a thread into the kernel if it is not already there.
 * This is a no-op on uniprocessors.
 */
/* ARGSUSED */
void
prpokethread(kthread_t *t)
{
	if (t->t_state == TS_ONPROC && t->t_cpu != CPU)
		poke_cpu(t->t_cpu->cpu_id);
}

/*
 * Return general registers.
 */
void
prgetprregs(klwp_t *lwp, prgregset_t prp)
{
	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	getgregs(lwp, prp);
}

/*
 * Set general registers.
 * (Note: This can be an alias to setgregs().)
 */
void
prsetprregs(klwp_t *lwp, prgregset_t prp, int initial)
{
	if (initial)		/* set initial values */
		lwptoregs(lwp)->r_spsr = PSR_USERINIT;
	(void) setgregs(lwp, prp);
}

/*
 * Get the syscall return values for the lwp.
 */
int
prgetrvals(klwp_t *lwp, long *rval1, long *rval2)
{
	struct regs *r = lwptoregs(lwp);

	if (r->r_spsr & PSR_C)
		return (r->r_x9);
	if (lwp->lwp_eosys == JUSTRETURN) {
		*rval1 = 0;
		*rval2 = 0;
	} else {
		*rval1 = r->r_x0;
		*rval2 = r->r_x1;
	}
	return (0);
}

/*
 * Does the system support floating-point, either through hardware
 * or by trapping and emulating floating-point machine instructions?
 */
int
prhasfp(void)
{
	return 1;
}

/*
 * Get floating-point registers.
 */
void
prgetprfpregs(klwp_t *lwp, prfpregset_t *pfp)
{
	bzero(pfp, sizeof (prfpregset_t));
	getfpregs(lwp, pfp);
}

/*
 * Set floating-point registers.
 * (Note: This can be an alias to setfpregs().)
 */
void
prsetprfpregs(klwp_t *lwp, prfpregset_t *pfp)
{
	setfpregs(lwp, pfp);
}

/*
 * Does the system support extra register state?
 */
int
prhasx(proc_t *p __unused)
{
	return (0);
}

/*
 * Get the size of the extra registers.
 */
size_t
prgetprxregsize(proc_t *p __unused)
{
	return (0);
}

/*
 * Get extra registers.
 */
void
prgetprxregs(klwp_t *lwp __unused, prxregset_t *prx __unused)
{
	/* no extra registers */
}

/*
 * Set extra registers.
 */
int
prsetprxregs(klwp_t *lwp __unused, prxregset_t *prx __unused)
{
	/* no extra registers */
	return (0);
}

/*
 * Return the base (lower limit) of the process stack.
 */
caddr_t
prgetstackbase(proc_t *p)
{
	return (p->p_usrstack - p->p_stksize);
}

/*
 * Return the "addr" field for pr_addr in prpsinfo_t.
 * This is a vestige of the past, so whatever we return is OK.
 */
caddr_t
prgetpsaddr(proc_t *p)
{
	return ((caddr_t)p);
}

/*
 * Arrange to single-step the lwp.
 */
void
prstep(klwp_t *lwp, int watchstep)
{
	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	lwp->lwp_pcb.pcb_flags |= REQUEST_STEP;
	lwp->lwp_pcb.pcb_flags &= ~REQUEST_NOSTEP;

	if (watchstep)
		lwp->lwp_pcb.pcb_flags |= WATCH_STEP;
	else
		lwp->lwp_pcb.pcb_flags |= NORMAL_STEP;

	aston(lwptot(lwp));	/* let trap() set PS_T in rp->r_efl */
}

/*
 * Undo prstep().
 */
void
prnostep(klwp_t *lwp)
{
	ASSERT(ttolwp(curthread) == lwp ||
	    MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	/*
	 * flag LWP so that its r_efl trace bit (PS_T) will be cleared on
	 * next return to usermode.
	 */
	lwp->lwp_pcb.pcb_flags |= REQUEST_NOSTEP;

	lwp->lwp_pcb.pcb_flags &=
	    ~(REQUEST_STEP|NORMAL_STEP|WATCH_STEP|DEBUG_PENDING);

	aston(lwptot(lwp));	/* let trap() clear PS_T in rp->r_efl */
}

/*
 * Return non-zero if a single-step is in effect.
 */
int
prisstep(klwp_t *lwp)
{
	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	return ((lwp->lwp_pcb.pcb_flags &
	    (NORMAL_STEP|WATCH_STEP|DEBUG_PENDING)) != 0);
}

/*
 * Set the PC to the specified virtual address.
 */
void
prsvaddr(klwp_t *lwp, caddr_t vaddr)
{
	struct regs *r = lwptoregs(lwp);

	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	r->r_pc = (uintptr_t)vaddr;
}

/*
 * Map address "addr" in address space "as" into a kernel virtual address.
 * The memory is guaranteed to be resident and locked down.
 */
caddr_t
prmapin(struct as *as, caddr_t addr, int writing)
{
	page_t *pp;
	caddr_t kaddr;
	pfn_t pfnum;

	/*
	 * XXX - Because of past mistakes, we have bits being returned
	 * by getpfnum that are actually the page type bits of the pte.
	 * When the object we are trying to map is a memory page with
	 * a page structure everything is ok and we can use the optimal
	 * method, ppmapin.  Otherwise, we have to do something special.
	 */
	pfnum = hat_getpfnum(as->a_hat, addr);
	if (pf_is_memory(pfnum)) {
		pp = page_numtopp_nolock(pfnum);
		if (pp != NULL) {
			ASSERT(PAGE_LOCKED(pp));
			kaddr = ppmapin(pp, writing ?
			    (PROT_READ | PROT_WRITE) : PROT_READ, (caddr_t)-1);
			return (kaddr + ((uintptr_t)addr & PAGEOFFSET));
		}
	}

	/*
	 * Oh well, we didn't have a page struct for the object we were
	 * trying to map in; ppmapin doesn't handle devices, but allocating a
	 * heap address allows ppmapout to free virtual space when done.
	 */
	kaddr = vmem_alloc(heap_arena, PAGESIZE, VM_SLEEP);

	hat_devload(kas.a_hat, kaddr, MMU_PAGESIZE,  pfnum,
	    writing ? (PROT_READ | PROT_WRITE) : PROT_READ, 0);

	return (kaddr + ((uintptr_t)addr & PAGEOFFSET));
}

/*
 * Unmap address "addr" in address space "as"; inverse of prmapin().
 */
/* ARGSUSED */
void
prmapout(struct as *as, caddr_t addr, caddr_t vaddr, int writing)
{
	extern void ppmapout(caddr_t);

	vaddr = (caddr_t)((uintptr_t)vaddr & PAGEMASK);
	ppmapout(vaddr);
}

/*
 * Make sure the lwp is in an orderly state
 * for inspection by a debugger through /proc.
 *
 * This needs to be called only once while the current thread remains in the
 * kernel and needs to be called while holding no resources (mutex locks, etc).
 *
 * As a hedge against these conditions, if prstop() is called repeatedly
 * before prunstop() is called, it does nothing and just returns.
 *
 * prunstop() must be called before the thread returns to user level.
 */
/* ARGSUSED */
void
prstop(int why, int what)
{
	klwp_t *lwp = ttolwp(curthread);
	struct regs *r = lwptoregs(lwp);

	if (lwp->lwp_pcb.pcb_flags & PRSTOP_CALLED)
		return;

	/*
	 * Make sure we don't deadlock on a recursive call
	 * to prstop().  stop() tests the lwp_nostop flag.
	 */
	ASSERT(lwp->lwp_nostop == 0);
	lwp->lwp_nostop = 1;

	if (copyin_nowatch((caddr_t)r->r_pc, &lwp->lwp_pcb.pcb_instr,
	    sizeof (lwp->lwp_pcb.pcb_instr)) == 0)
		lwp->lwp_pcb.pcb_flags |= INSTR_VALID;
	else {
		lwp->lwp_pcb.pcb_flags &= ~INSTR_VALID;
		lwp->lwp_pcb.pcb_instr = 0;
	}

	(void) save_syscall_args();
	ASSERT(lwp->lwp_nostop == 1);
	lwp->lwp_nostop = 0;

	lwp->lwp_pcb.pcb_flags |= PRSTOP_CALLED;
	aston(curthread);	/* so prunstop() will be called */
}

/*
 * Inform prstop() that it should do its work again
 * the next time it is called.
 */
void
prunstop(void)
{
	ttolwp(curthread)->lwp_pcb.pcb_flags &= ~PRSTOP_CALLED;
}

/*
 * Fetch the user-level instruction on which the lwp is stopped.
 * It was saved by the lwp itself, in prstop().
 * Return non-zero if the instruction is valid.
 */
int
prfetchinstr(klwp_t *lwp, ulong_t *ip)
{
	*ip = (ulong_t)(instr_t)lwp->lwp_pcb.pcb_instr;
	return (lwp->lwp_pcb.pcb_flags & INSTR_VALID);
}

/*
 * Called from trap() when a load or store instruction
 * falls in a watched page but is not a watchpoint.
 * We emulate the instruction in the kernel.
 */
/* ARGSUSED */
int
pr_watch_emul(struct regs *rp, caddr_t addr, enum seg_rw rw)
{
#ifdef SOMEDAY
	int res;
	proc_t *p = curproc;
	char *badaddr = (caddr_t)(-1);
	int mapped;

	/* prevent recursive calls to pr_watch_emul() */
	ASSERT(!(curthread->t_flag & T_WATCHPT));
	curthread->t_flag |= T_WATCHPT;

	watch_disable_addr(addr, 8, rw);
	res = do_unaligned(rp, &badaddr);
	watch_enable_addr(addr, 8, rw);

	curthread->t_flag &= ~T_WATCHPT;
	if (res == SIMU_SUCCESS) {
		/* adjust the pc */
		return (1);
	}
#endif
	return (0);
}

/*
 * Return the minimum size that we need to determine the full size of a
 * prxregset_t.
 */
boolean_t
prwriteminxreg(size_t *sizep)
{
	*sizep = 0;
	return (B_TRUE);
}

boolean_t
prwritesizexreg(const void *argp __unused, size_t *sizep)
{
	*sizep = 0;
	return (B_TRUE);
}
