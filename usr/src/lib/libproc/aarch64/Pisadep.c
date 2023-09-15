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
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/frame.h>
#include <sys/machelf.h>
#include <sys/regset.h>
#include <sys/stack.h>
#include <sys/sysmacros.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Pcontrol.h"
#include "Pstack.h"

const char *
Ppltdest(struct ps_prochandle *P, uintptr_t pltaddr)
{
	map_info_t *mp = Paddr2mptr(P, pltaddr);
	file_info_t *fp;
	size_t i;
	uintptr_t r_addr;

	if (mp == NULL || (fp = mp->map_file) == NULL ||
	    fp->file_plt_base == 0 ||
	    pltaddr - fp->file_plt_base >= fp->file_plt_size) {
		errno = EINVAL;
		return (NULL);
	}

	i = (pltaddr - (fp->file_plt_base + M_PLT_RESERVSZ)) / M_PLT_ENTSIZE;

	Elf64_Rela r;

	r_addr = fp->file_jmp_rel + i * sizeof (r);

	if (Pread(P, &r, sizeof (r), r_addr) == sizeof (r) &&
	    (i = ELF64_R_SYM(r.r_info)) < fp->file_dynsym.sym_symn) {
		Elf_Data *data = fp->file_dynsym.sym_data_pri;
		Elf64_Sym *symp = &(((Elf64_Sym *)data->d_buf)[i]);

		return (fp->file_dynsym.sym_strs + symp->st_name);
	}

	return (NULL);
}

/*
 * See Arm® A64 Instruction Set for A-profile architecture
 *	DDI 0596 (ID070623)
 *	pp. 831
 */
#define	SVC_INSN	0xd4000001 /* svc #<imm16> */
#define	SVC_SYSN_SHIFT	5	   /* the immediate value */
#define	SVC_SYSN_MASK	(0xffff << SVC_SYSN_SHIFT)
#define	SVC_SYSN(x)	((x & SVC_SYSN_MASK) >> SVC_SYSN_SHIFT)

int
Pissyscall(struct ps_prochandle *P, uintptr_t addr)
{
	uint32_t instr;

	if (Pread(P, &instr, sizeof (instr), addr) != sizeof (instr))
		return (0);

	if ((instr & ~SVC_SYSN_MASK) == SVC_INSN) /* svc <imm16> */
		return (1);

	return (0);
}

int
Pissyscall_indirect(struct ps_prochandle *P, uintptr_t addr)
{
	uint32_t instr;

	if (Pread(P, &instr, sizeof (instr), addr) != sizeof (instr))
		return (0);

	return (((instr & ~SVC_SYSN_MASK) == SVC_INSN) &&
	    (SVC_SYSN(instr) == 0));
}

int
Pissyscall_prev(struct ps_prochandle *P, uintptr_t addr, uintptr_t *dst)
{
	int ret;

	if (ret = Pissyscall(P, addr - sizeof (uint32_t))) {
		if (dst != NULL)
			*dst = addr - sizeof (uint32_t);
		return (ret);
	}

	return (0);
}


int
Pissyscall_prev_indirect(struct ps_prochandle *P, uintptr_t addr, uintptr_t *dst)
{
	int ret;

	if (ret = Pissyscall_indirect(P, addr - sizeof (uint32_t))) {
		if (dst != NULL)
			*dst = addr - sizeof (uint32_t);
		return (ret);
	}

	return (0);
}

/*
 * This, unlike the Pissyscall(3PROC) family, must check specifically for the
 * indirect system call via a `svc #0` instruction, because we're going to
 * jump to this instruction (with our own register set) to make system calls.
 */
int
Pissyscall_text(struct ps_prochandle *P, const void *buf, size_t buflen)
{
	if (buflen < sizeof (uint32_t))
		return (0);

	const uint32_t *instr = buf;

	if (*instr == 0xd4000001) /* svc #0 */
		return (1);

	return (0);
}

static void
ucontext_n_to_prgregs(const ucontext_t *src, prgregset_t dst)
{
	(void) memcpy(dst, src->uc_mcontext.gregs, sizeof (gregset_t));
}

int
Pstack_iter(struct ps_prochandle *P, const prgregset_t regs,
    proc_stack_f *func, void *arg)
{
	struct frame frame;

	uint_t prevfpsize = 0;
	prgreg_t *prevfp = NULL;
	prgreg_t fp;
	prgreg_t pc;

	prgregset_t gregs;
	int nfp = 0;

	int rv = 0;
	int argc;

	uclist_t ucl;
	uintptr_t uc_addr;
	ucontext_t uc;

	/*
	 * Type definition for a structure corresponding to an AArch64
	 * signal frame.  Refer to the comments in Pstack.c for more info.
	 *
	 * This ABSOLUTELY MUST be kept in sync with the kernel's `sendsig`
	 * and libc's `walkcontext`.
	 */
	typedef struct {
		struct frame fr;
		prgreg_t signo;
		siginfo_t *sip;
	} sigframe_t;

	prgreg_t args[32] = {0};

	init_uclist(&ucl, P);

	(void) memcpy(gregs, regs, sizeof (gregs));

	fp = gregs[R_FP];
	pc = gregs[R_PC];

	while (fp != 0 || pc != 0) {
		if (stack_loop(fp, &prevfp, &nfp, &prevfpsize))
			break;

		if (fp != 0 &&
		    Pread(P, &frame, sizeof (frame), (uintptr_t)fp) ==
		    sizeof (frame)) {
			if (frame.fr_savpc == -1) {
				argc = 3;
				args[2] = frame.fr_savfp + sizeof (sigframe_t);
				if (Pread(P, &args, 2 * sizeof (prgreg_t),
				    frame.fr_savfp + 2 * sizeof (prgreg_t)) !=
				    2 * sizeof (prgreg_t)) {
					argc = 0;
				}
			} else {
#if 0				/* XXXARM: No saveargs yet */
				argc = read_args(P, fp, pc, args,
				    sizeof (args));
#else
				argc = 0;
#endif
			}
		} else {
			(void) memset(&frame, 0, sizeof (frame));
			argc = 0;
		}

		gregs[R_FP] = fp;
		gregs[R_PC] = pc;

		if ((rv = func(arg, gregs, argc, args)) != 0)
			break;

		fp = frame.fr_savfp;
		pc = frame.fr_savpc;

		if (pc == -1 && find_uclink(&ucl, fp + sizeof (sigframe_t))) {
			uc_addr = fp + sizeof (sigframe_t);

			if (Pread(P, &uc, sizeof (uc), uc_addr)
			    == sizeof (uc)) {
				ucontext_n_to_prgregs(&uc, gregs);
				fp = gregs[R_FP];
				pc = gregs[R_PC];
			}
		}
	}

	if (prevfp)
		free(prevfp);

	free_uclist(&ucl);
	return (rv);
}

/*
 * Set up our registers to make an indirect system call to `sysindex`,
 * That is set %sp to `sp`, %pc to the address of an indirect system call found by `Pscantext()`,
 * and the system call number `sysindex` in %x9.
 */
uintptr_t
Psyscall_setup(struct ps_prochandle *P, int nargs, int sysindex, uintptr_t sp)
{
	P->status.pr_lwp.pr_reg[REG_X9] = sysindex;
	P->status.pr_lwp.pr_reg[REG_PC] = P->sysaddr;
	P->status.pr_lwp.pr_reg[REG_SP] = sp;

	return (sp);
}

int
Psyscall_copyinargs(struct ps_prochandle *P, int nargs, argdes_t *argp,
    uintptr_t ap)
{
	int i;
	argdes_t *adp;

	for (i = 0, adp = argp; i < nargs; i++, adp++) {
		switch (i) {
		case 0:
			(void) Pputareg(P, REG_X0, adp->arg_value);
			break;
		case 1:
			(void) Pputareg(P, REG_X1, adp->arg_value);
			break;
		case 2:
			(void) Pputareg(P, REG_X2, adp->arg_value);
			break;
		case 3:
			(void) Pputareg(P, REG_X3, adp->arg_value);
			break;
		case 4:
			(void) Pputareg(P, REG_X4, adp->arg_value);
			break;
		case 5:
			(void) Pputareg(P, REG_X5, adp->arg_value);
			break;
		case 6:
			(void) Pputareg(P, REG_X6, adp->arg_value);
			break;
		case 7:
			(void) Pputareg(P, REG_X7, adp->arg_value);
			break;
		}
	}

	return (0);
}

int
Psyscall_copyoutargs(struct ps_prochandle *P, int nargs, argdes_t *argp,
    uintptr_t ap)
{
	int i;
	argdes_t *adp;

	for (i = 0, adp = argp; i < nargs; i++, adp++) {
		switch (i) {
		case 0:
			adp->arg_value =
			    P->status.pr_lwp.pr_reg[REG_X0];
			break;
		case 1:
			adp->arg_value =
			    P->status.pr_lwp.pr_reg[REG_X1];
			break;
		case 2:
			adp->arg_value =
			    P->status.pr_lwp.pr_reg[REG_X2];
			break;
		case 3:
			adp->arg_value =
			    P->status.pr_lwp.pr_reg[REG_X3];
			break;
		case 4:
			adp->arg_value =
			    P->status.pr_lwp.pr_reg[REG_X4];
			break;
		case 5:
			adp->arg_value =
			    P->status.pr_lwp.pr_reg[REG_X5];
			break;
		case 6:
			adp->arg_value =
			    P->status.pr_lwp.pr_reg[REG_X6];
			break;
		case 7:
			adp->arg_value =
			    P->status.pr_lwp.pr_reg[REG_X7];
			break;
		}
	}

	return (0);
}
