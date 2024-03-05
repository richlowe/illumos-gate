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
#include <saveargs.h>
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
Pissyscall_prev_indirect(struct ps_prochandle *P, uintptr_t addr,
    uintptr_t *dst)
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


/*
 * Read arguments from the frame indicated by regs into args, return the
 * number of arguments successfully read
 */
static int
read_args(struct ps_prochandle *P, uintptr_t fp, uintptr_t pc, prgreg_t *args,
    size_t argsize)
{
	GElf_Sym sym;
	ctf_file_t *ctfp = NULL;
	ctf_funcinfo_t finfo;
	prsyminfo_t si = {0};
	uint8_t ins[SAVEARGS_INSN_SEQ_LEN];
	size_t insnsize;
	int argc = 0;
	int args_style = 0;
	int i;
	ctf_id_t args_types[5];

	if (Pxlookup_by_addr(P, pc, NULL, 0, &sym, &si) != 0)
		return (0);

	if ((ctfp = Paddr_to_ctf(P, pc)) == NULL)
		return (0);

	if (ctf_func_info(ctfp, si.prs_id, &finfo) == CTF_ERR)
		return (0);

	argc = finfo.ctc_argc;

	if (argc == 0)
		return (0);

	/*
	 * If any of the first 7 arguments are a structure less than 16 bytes
	 * in size, it will be passed spread across two argument registers,
	 * and we will not cope.
	 */
	if (ctf_func_args(ctfp, si.prs_id, 7, args_types) == CTF_ERR)
		return (0);

	for (i = 0; i < MIN(7, finfo.ctc_argc); i++) {
		int t = ctf_type_kind(ctfp, args_types[i]);

		if (((t == CTF_K_STRUCT) || (t == CTF_K_UNION)) &&
		    ctf_type_size(ctfp, args_types[i]) <= 16)
			return (0);
	}

	/*
	 * The number of instructions to search for argument saving is limited
	 * such that only instructions prior to %pc are considered and we
	 * never read arguments from a function where the saving code has not
	 * in fact yet executed.
	 */
	insnsize = MIN(MIN(sym.st_size, SAVEARGS_INSN_SEQ_LEN),
	    pc - sym.st_value);

	if (Pread(P, ins, insnsize, sym.st_value) != insnsize)
		return (0);

	if ((argc != 0) &&
	    ((args_style = saveargs_has_args(ins, insnsize, argc,
	    0)) != SAVEARGS_NO_ARGS)) {
		int regargs = MIN(8, argc);
		size_t size = regargs * sizeof (long);

		if (Pread(P, args, size, fp + (2 * sizeof (long))) != size)
			return (0);

		/*
		 * XXXARM:
		 *
		 * Due to the ARM ABI dumping stacked args is a bit
		 * cumbersome.  The first stacked argument will be stored at
		 * %sp (at call time).
		 *
		 * We will then reserve some stack space (in an arbitrary
		 * manner), but even in the common case we have to disassemble
		 * the initial
		 *	stp x29, x30, [sp, #-128]!  to determine the
		 * immediate to subtract from %fp to find them on the stack.
		 */

		return (regargs);
	} else {
		return (0);
	}
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
				argc = read_args(P, fp, pc, args,
				    sizeof (args));
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
 * Set up our registers to make an indirect system call to `sysindex`.
 *
 * That is set %sp to `sp`, %pc to the address of an indirect system call
 * found by `Pscantext()`, and store the system call number `sysindex` in %x9.
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
