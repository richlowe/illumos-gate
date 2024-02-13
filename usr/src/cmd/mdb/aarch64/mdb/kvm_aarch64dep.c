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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2022 Richard Lowe
 */

/*
 * Libkvm Kernel Target AArch64 component
 *
 * This file provides the ISA-dependent portion of the libkvm kernel target.
 * For more details on the implementation refer to mdb_kvm.c.
 */

#include <mdb/mdb_target_impl.h>
#include <mdb/mdb_disasm.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_conf.h>
#include <mdb/mdb_kvm.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb.h>
#include <mdb/kvm_isadep.h>
#include <mdb/mdb_kreg.h>
#include <mdb/mdb_kreg_impl.h>

#include <sys/panic.h>

const mdb_tgt_regdesc_t mdb_aarch64_kregs[] = {
	{ "savfp", KREG_SAVFP, MDB_TGT_R_EXPORT },
	{ "savpc", KREG_SAVPC, MDB_TGT_R_EXPORT },
	{ "r0", KREG_X0, MDB_TGT_R_EXPORT },
	{ "x0", KREG_X0, MDB_TGT_R_EXPORT },
	{ "w0", KREG_X0, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r1", KREG_X1, MDB_TGT_R_EXPORT },
	{ "x1", KREG_X1, MDB_TGT_R_EXPORT },
	{ "w1", KREG_X1, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r2", KREG_X2, MDB_TGT_R_EXPORT },
	{ "x2", KREG_X2, MDB_TGT_R_EXPORT },
	{ "w2", KREG_X2, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r3", KREG_X3, MDB_TGT_R_EXPORT },
	{ "x3", KREG_X3, MDB_TGT_R_EXPORT },
	{ "w3", KREG_X3, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r4", KREG_X4, MDB_TGT_R_EXPORT },
	{ "x4", KREG_X4, MDB_TGT_R_EXPORT },
	{ "w4", KREG_X4, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r5", KREG_X5, MDB_TGT_R_EXPORT },
	{ "x5", KREG_X5, MDB_TGT_R_EXPORT },
	{ "w5", KREG_X5, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r6", KREG_X6, MDB_TGT_R_EXPORT },
	{ "x6", KREG_X6, MDB_TGT_R_EXPORT },
	{ "w6", KREG_X6, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r7", KREG_X7, MDB_TGT_R_EXPORT },
	{ "x7", KREG_X7, MDB_TGT_R_EXPORT },
	{ "w7", KREG_X7, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r8", KREG_X8, MDB_TGT_R_EXPORT },
	{ "x8", KREG_X8, MDB_TGT_R_EXPORT },
	{ "w8", KREG_X8, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r9", KREG_X9, MDB_TGT_R_EXPORT },
	{ "x9", KREG_X9, MDB_TGT_R_EXPORT },
	{ "w9", KREG_X9, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r10", KREG_X10, MDB_TGT_R_EXPORT },
	{ "x10", KREG_X10, MDB_TGT_R_EXPORT },
	{ "w10", KREG_X10, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r11", KREG_X11, MDB_TGT_R_EXPORT },
	{ "x11", KREG_X11, MDB_TGT_R_EXPORT },
	{ "w11", KREG_X11, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r12", KREG_X12, MDB_TGT_R_EXPORT },
	{ "x12", KREG_X12, MDB_TGT_R_EXPORT },
	{ "w12", KREG_X12, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r13", KREG_X13, MDB_TGT_R_EXPORT },
	{ "x13", KREG_X13, MDB_TGT_R_EXPORT },
	{ "w13", KREG_X13, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r14", KREG_X14, MDB_TGT_R_EXPORT },
	{ "x14", KREG_X14, MDB_TGT_R_EXPORT },
	{ "w14", KREG_X14, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r15", KREG_X15, MDB_TGT_R_EXPORT },
	{ "x15", KREG_X15, MDB_TGT_R_EXPORT },
	{ "w15", KREG_X15, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r16", KREG_X16, MDB_TGT_R_EXPORT },
	{ "x16", KREG_X16, MDB_TGT_R_EXPORT },
	{ "w16", KREG_X16, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r17", KREG_X17, MDB_TGT_R_EXPORT },
	{ "x17", KREG_X17, MDB_TGT_R_EXPORT },
	{ "w17", KREG_X17, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r18", KREG_X18, MDB_TGT_R_EXPORT },
	{ "x18", KREG_X18, MDB_TGT_R_EXPORT },
	{ "w18", KREG_X18, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r19", KREG_X19, MDB_TGT_R_EXPORT },
	{ "x19", KREG_X19, MDB_TGT_R_EXPORT },
	{ "w19", KREG_X19, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r20", KREG_X20, MDB_TGT_R_EXPORT },
	{ "x20", KREG_X20, MDB_TGT_R_EXPORT },
	{ "w20", KREG_X20, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r21", KREG_X21, MDB_TGT_R_EXPORT },
	{ "x21", KREG_X21, MDB_TGT_R_EXPORT },
	{ "w21", KREG_X21, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r22", KREG_X22, MDB_TGT_R_EXPORT },
	{ "x22", KREG_X22, MDB_TGT_R_EXPORT },
	{ "w22", KREG_X22, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r23", KREG_X23, MDB_TGT_R_EXPORT },
	{ "x23", KREG_X23, MDB_TGT_R_EXPORT },
	{ "w23", KREG_X23, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r24", KREG_X24, MDB_TGT_R_EXPORT },
	{ "x24", KREG_X24, MDB_TGT_R_EXPORT },
	{ "w24", KREG_X24, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r25", KREG_X25, MDB_TGT_R_EXPORT },
	{ "x25", KREG_X25, MDB_TGT_R_EXPORT },
	{ "w25", KREG_X25, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r26", KREG_X26, MDB_TGT_R_EXPORT },
	{ "x26", KREG_X26, MDB_TGT_R_EXPORT },
	{ "w26", KREG_X26, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r27", KREG_X27, MDB_TGT_R_EXPORT },
	{ "x27", KREG_X27, MDB_TGT_R_EXPORT },
	{ "w27", KREG_X27, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r28", KREG_X28, MDB_TGT_R_EXPORT },
	{ "x28", KREG_X28, MDB_TGT_R_EXPORT },
	{ "w28", KREG_X28, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r29", KREG_X29, MDB_TGT_R_EXPORT },
	{ "fp", KREG_X29, MDB_TGT_R_EXPORT },
	{ "x29", KREG_X29, MDB_TGT_R_EXPORT },
	{ "w29", KREG_X29, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "r30", KREG_X30, MDB_TGT_R_EXPORT },
	{ "lr", KREG_X30, MDB_TGT_R_EXPORT },
	{ "x30", KREG_X30, MDB_TGT_R_EXPORT },
	{ "w30", KREG_X30, MDB_TGT_R_EXPORT | MDB_TGT_R_32 },
	{ "sp", KREG_SP, MDB_TGT_R_EXPORT },
	{ "pc", KREG_PC, MDB_TGT_R_EXPORT },
	{ "psr", KREG_PSR, MDB_TGT_R_EXPORT },
	{ "tp", KREG_TP, MDB_TGT_R_EXPORT },
	{ NULL, 0, 0 }
};

int
kt_regs(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const kreg_t *grs = &((mdb_tgt_gregset_t *)addr)->kregs[0];

	mdb_printf("%%x0 = 0x%0?p\t%%x1 = 0x%0?p\n",
	    grs[KREG_X0], grs[KREG_X1]);
	mdb_printf("%%x2 = 0x%0?p\t%%x3 = 0x%0?p\n",
	    grs[KREG_X2], grs[KREG_X3]);
	mdb_printf("%%x4 = 0x%0?p\t%%x5 = 0x%0?p\n",
	    grs[KREG_X4], grs[KREG_X5]);
	mdb_printf("%%x6 = 0x%0?p\t%%x7 = 0x%0?p\n",
	    grs[KREG_X6], grs[KREG_X7]);
	mdb_printf("%%x8 = 0x%0?p\t%%x9 = 0x%0?p\n",
	    grs[KREG_X8], grs[KREG_X9]);
	mdb_printf("%%x10 = 0x%0?p\t%%x11 = 0x%0?p\n",
	    grs[KREG_X10], grs[KREG_X11]);
	mdb_printf("%%x12 = 0x%0?p\t%%x13 = 0x%0?p\n",
	    grs[KREG_X12], grs[KREG_X13]);
	mdb_printf("%%x14 = 0x%0?p\t%%x15 = 0x%0?p\n",
	    grs[KREG_X14], grs[KREG_X15]);
	mdb_printf("%%x16 = 0x%0?p\t%%x17 = 0x%0?p\n",
	    grs[KREG_X16], grs[KREG_X17]);
	mdb_printf("%%x18 = 0x%0?p\t%%x19 = 0x%0?p\n",
	    grs[KREG_X18], grs[KREG_X19]);
	mdb_printf("%%x20 = 0x%0?p\t%%x21 = 0x%0?p\n",
	    grs[KREG_X20], grs[KREG_X21]);
	mdb_printf("%%x22 = 0x%0?p\t%%x23 = 0x%0?p\n",
	    grs[KREG_X22], grs[KREG_X23]);
	mdb_printf("%%x24 = 0x%0?p\t%%x25 = 0x%0?p\n",
	    grs[KREG_X24], grs[KREG_X25]);
	mdb_printf("%%x26 = 0x%0?p\t%%x27 = 0x%0?p\n",
	    grs[KREG_X26], grs[KREG_X27]);
	mdb_printf("%%x28 = 0x%0?p\t%%x29 = 0x%0?p\n",
	    grs[KREG_X28], grs[KREG_X29]);
	mdb_printf("%%x30 = 0x%0?p\n", grs[KREG_X30]);
	mdb_printf("\n");

	mdb_printf("%%sp = 0x%0?p\t%%pc = 0x%0?p\n",
	    grs[KREG_SP], grs[KREG_PC]);
	mdb_printf("%%tp = 0x%0?p\t%%psr = 0x%0?p\n",
	    grs[KREG_TP], grs[KREG_PSR]);

	return (DCMD_OK);
}

void
kt_regs_to_kregs(struct regs *regs, mdb_tgt_gregset_t *gregs)
{
	gregs->kregs[KREG_SAVFP] = regs->r_savfp;
	gregs->kregs[KREG_SAVPC] = regs->r_savpc;

	gregs->kregs[KREG_X0] = regs->r_x0;
	gregs->kregs[KREG_X1] = regs->r_x1;
	gregs->kregs[KREG_X2] = regs->r_x2;
	gregs->kregs[KREG_X3] = regs->r_x3;
	gregs->kregs[KREG_X4] = regs->r_x4;
	gregs->kregs[KREG_X5] = regs->r_x5;
	gregs->kregs[KREG_X6] = regs->r_x6;
	gregs->kregs[KREG_X7] = regs->r_x7;
	gregs->kregs[KREG_X8] = regs->r_x8;
	gregs->kregs[KREG_X9] = regs->r_x9;
	gregs->kregs[KREG_X10] = regs->r_x10;
	gregs->kregs[KREG_X11] = regs->r_x11;
	gregs->kregs[KREG_X12] = regs->r_x12;
	gregs->kregs[KREG_X13] = regs->r_x13;
	gregs->kregs[KREG_X14] = regs->r_x14;
	gregs->kregs[KREG_X15] = regs->r_x15;
	gregs->kregs[KREG_X16] = regs->r_x16;
	gregs->kregs[KREG_X17] = regs->r_x17;
	gregs->kregs[KREG_X18] = regs->r_x18;
	gregs->kregs[KREG_X19] = regs->r_x19;
	gregs->kregs[KREG_X20] = regs->r_x20;
	gregs->kregs[KREG_X21] = regs->r_x21;
	gregs->kregs[KREG_X22] = regs->r_x22;
	gregs->kregs[KREG_X23] = regs->r_x23;
	gregs->kregs[KREG_X24] = regs->r_x24;
	gregs->kregs[KREG_X25] = regs->r_x25;
	gregs->kregs[KREG_X26] = regs->r_x26;
	gregs->kregs[KREG_X27] = regs->r_x27;
	gregs->kregs[KREG_X28] = regs->r_x28;
	gregs->kregs[KREG_X29] = regs->r_x29;
	gregs->kregs[KREG_X30] = regs->r_x30;
	gregs->kregs[KREG_SP] = regs->r_sp;
	gregs->kregs[KREG_PC] = regs->r_pc;
	gregs->kregs[KREG_PSR] = regs->r_spsr;
}

int
kt_putareg(mdb_tgt_t *t, mdb_tgt_tid_t tid, const char *rname, mdb_tgt_reg_t r)
{
	const mdb_tgt_regdesc_t *rdp;
	kt_data_t *kt = t->t_data;

	if (tid != kt->k_tid)
		return (set_errno(EMDB_NOREGS));

	for (rdp = kt->k_rds; rdp->rd_name != NULL; rdp++) {
		if (strcmp(rname, rdp->rd_name) == 0) {
			if (rdp->rd_flags & MDB_TGT_R_32)
				r &= 0xffffffffULL;

			kt->k_regs->kregs[rdp->rd_num] = (kreg_t)r;
			return (0);
		}
	}

	return (set_errno(EMDB_BADREG));
}

int
mdb_aarch64_kvm_stack_iter(mdb_tgt_t *t, const mdb_tgt_gregset_t *gsp,
    mdb_tgt_stack_f *func, void *arg)
{
	mdb_tgt_gregset_t gregs;
	kreg_t *kregs = &gregs.kregs[0];
	int got_pc = (gsp->kregs[KREG_PC] != 0);
	uint_t argc, reg_argc;
	long fr_argv[32];
	int start_index; /* index to save_instr where to start comparison */
	int err;
	int i;

	struct fr {
		uintptr_t fr_savfp;
		uintptr_t fr_savpc;
	} fr;

	uintptr_t fp = gsp->kregs[KREG_FP];
	uintptr_t pc = gsp->kregs[KREG_PC];

	ssize_t size;
	ssize_t insnsize;
#if 0				/* XXXARM: No saveargs */
	uint8_t ins[SAVEARGS_INSN_SEQ_LEN];
#endif

	GElf_Sym s;
	mdb_syminfo_t sip;
	mdb_ctf_funcinfo_t mfp;
	int xpv_panic = 0;
	int advance_tortoise = 1;
	uintptr_t tortoise_fp = 0;

	bcopy(gsp, &gregs, sizeof (gregs));

	while (fp != 0) {
		int args_style = 0;

		if (mdb_tgt_aread(t, MDB_TGT_AS_VIRT_S, &fr, sizeof (fr), fp) !=
		    sizeof (fr)) {
			err = EMDB_NOMAP;
			goto badfp;
		}

		if (tortoise_fp == 0) {
			tortoise_fp = fp;
		} else {
			/*
			 * Advance tortoise_fp every other frame, so we detect
			 * cycles with Floyd's tortoise/hare.
			 */
			if (advance_tortoise != 0) {
				struct fr tfr;

				if (mdb_tgt_aread(t, MDB_TGT_AS_VIRT_S, &tfr,
				    sizeof (tfr), tortoise_fp) !=
				    sizeof (tfr)) {
					err = EMDB_NOMAP;
					goto badfp;
				}

				tortoise_fp = tfr.fr_savfp;
			}

			if (fp == tortoise_fp) {
				err = EMDB_STKFRAME;
				goto badfp;
			}
		}

		advance_tortoise = !advance_tortoise;

		if ((mdb_tgt_lookup_by_addr(t, pc, MDB_TGT_SYM_FUZZY,
		    NULL, 0, &s, &sip) == 0) &&
		    (mdb_ctf_func_info(&s, &sip, &mfp) == 0)) {
#if 0				/* XXXARM: No saveargs */
			int return_type = mdb_ctf_type_kind(mfp.mtf_return);
			mdb_ctf_id_t args_types[5];

			argc = mfp.mtf_argc;

			/*
			 * If the function returns a structure or union
			 * greater than 16 bytes in size %rdi contains the
			 * address in which to store the return value rather
			 * than for an argument.
			 */
			if ((return_type == CTF_K_STRUCT ||
			    return_type == CTF_K_UNION) &&
			    mdb_ctf_type_size(mfp.mtf_return) > 16)
				start_index = 1;
			else
				start_index = 0;

			/*
			 * If any of the first 5 arguments are a structure
			 * less than 16 bytes in size, it will be passed
			 * spread across two argument registers, and we will
			 * not cope.
			 */
			if (mdb_ctf_func_args(&mfp, 5, args_types) == CTF_ERR)
				argc = 0;

			for (i = 0; i < MIN(5, argc); i++) {
				int t = mdb_ctf_type_kind(args_types[i]);

				if (((t == CTF_K_STRUCT) ||
				    (t == CTF_K_UNION)) &&
				    mdb_ctf_type_size(args_types[i]) <= 16) {
					argc = 0;
					break;
				}
			}
#else
			argc = 0;
#endif
		} else {
			argc = 0;
		}

#if 0				/* XXXARM: No saveargs */
		/*
		 * The number of instructions to search for argument saving is
		 * limited such that only instructions prior to %pc are
		 * considered such that we never read arguments from a
		 * function where the saving code has not in fact yet
		 * executed.
		 */
		insnsize = MIN(MIN(s.st_size, SAVEARGS_INSN_SEQ_LEN),
		    pc - s.st_value);

		if (mdb_tgt_aread(t, MDB_TGT_AS_VIRT_I, ins, insnsize,
		    s.st_value) != insnsize)
			argc = 0;

		if ((argc != 0) &&
		    ((args_style = saveargs_has_args(ins, insnsize, argc,
		    start_index)) != SAVEARGS_NO_ARGS)) {
			/* Up to 6 arguments are passed via registers */
			reg_argc = MIN((6 - start_index), mfp.mtf_argc);
			size = reg_argc * sizeof (long);

			/*
			 * If Studio pushed a structure return address as an
			 * argument, we need to read one more argument than
			 * actually exists (the addr) to make everything line
			 * up.
			 */
			if (args_style == SAVEARGS_STRUCT_ARGS)
				size += sizeof (long);

			if (mdb_tgt_aread(t, MDB_TGT_AS_VIRT_S, fr_argv, size,
			    (fp - size)) != size)
				return (-1);	/* errno has been set for us */

			/*
			 * Arrange the arguments in the right order for
			 * printing.
			 */
			for (i = 0; i < (reg_argc / 2); i++) {
				long t = fr_argv[i];

				fr_argv[i] = fr_argv[reg_argc - i - 1];
				fr_argv[reg_argc - i - 1] = t;
			}

			if (argc > reg_argc) {
				size = MIN((argc - reg_argc) * sizeof (long),
				    sizeof (fr_argv) -
				    (reg_argc * sizeof (long)));

				if (mdb_tgt_aread(t, MDB_TGT_AS_VIRT_S,
				    &fr_argv[reg_argc], size,
				    fp + sizeof (fr)) != size)
					return (-1); /* errno has been set */
			}
		} else {
			argc = 0;
		}
#else
		argc = 0;
#endif

		if (got_pc && func(arg, pc, argc, fr_argv, &gregs) != 0)
			break;

		kregs[KREG_SP] = kregs[KREG_FP];

		fp = fr.fr_savfp;

		kregs[KREG_FP] = fp;
		kregs[KREG_PC] = pc = fr.fr_savpc;

		got_pc = (pc != 0);
	}

	return (0);

badfp:
	mdb_printf("%p [%s]", fp, mdb_strerror(err));
	return (set_errno(err));
}

static int
mdb_aarch64_kvm_frame(void *arglim, uintptr_t pc, uint_t argc, const long *argv,
    const mdb_tgt_gregset_t *gregs)
{
	argc = MIN(argc, (uintptr_t)arglim);
	mdb_printf("%a(", pc);

	if (argc != 0) {
		mdb_printf("%lr", *argv++);
		for (argc--; argc != 0; argc--)
			mdb_printf(", %lr", *argv++);
	}

	mdb_printf(")\n");
	return (0);
}

int
mdb_aarch64_kvm_framev(void *arglim, uintptr_t pc, uint_t argc,
    const long *argv, const mdb_tgt_gregset_t *gregs)
{
	/*
	 * Historically adb limited stack trace argument display to a fixed-
	 * size number of arguments since no symbolic debugging info existed.
	 * On amd64 we can detect the true number of saved arguments so only
	 * respect an arglim of zero; otherwise display the entire argv[].
	 */
	if (arglim == 0)
		argc = 0;

	mdb_printf("%0?lr %a(", gregs->kregs[KREG_FP], pc);

	if (argc != 0) {
		mdb_printf("%lr", *argv++);
		for (argc--; argc != 0; argc--)
			mdb_printf(", %lr", *argv++);
	}

	mdb_printf(")\n");
	return (0);
}

static int
kt_stack_common(uintptr_t addr, uint_t flags, int argc,
    const mdb_arg_t *argv, mdb_tgt_stack_f *func)
{
	kt_data_t *kt = mdb.m_target->t_data;
	void *arg = (void *)(uintptr_t)mdb.m_nargs;
	mdb_tgt_gregset_t gregs, *grp;

	if (flags & DCMD_ADDRSPEC) {
		bzero(&gregs, sizeof (gregs));
		gregs.kregs[KREG_FP] = addr;
		grp = &gregs;
	} else
		grp = kt->k_regs;

	if (argc != 0) {
		if (argv->a_type == MDB_TYPE_CHAR || argc > 1)
			return (DCMD_USAGE);

		if (argv->a_type == MDB_TYPE_STRING)
			arg = (void *)mdb_strtoull(argv->a_un.a_str);
		else
			arg = (void *)argv->a_un.a_val;
	}

	(void) mdb_aarch64_kvm_stack_iter(mdb.m_target, grp, func, arg);
	return (DCMD_OK);
}

int
kt_stack(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (kt_stack_common(addr, flags, argc, argv,
	    mdb_aarch64_kvm_frame));
}

int
kt_stackv(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (kt_stack_common(addr, flags, argc, argv,
	    mdb_aarch64_kvm_framev));
}

const mdb_tgt_ops_t kt_aarch64_ops = {
	.t_setflags = kt_setflags,
	.t_setcontext = kt_setcontext,
	.t_activate = kt_activate,
	.t_deactivate = kt_deactivate,
	.t_periodic = (void (*)())(uintptr_t)mdb_tgt_nop,
	.t_destroy = kt_destroy,
	.t_name = kt_name,
	.t_isa = (const char *(*)())mdb_conf_isa,
	.t_platform = kt_platform,
	.t_uname = kt_uname,
	.t_dmodel = kt_dmodel,
	.t_aread = kt_aread,
	.t_awrite = kt_awrite,
	.t_vread = kt_vread,
	.t_vwrite = kt_vwrite,
	.t_pread = kt_pread,
	.t_pwrite = kt_pwrite,
	.t_fread = kt_fread,
	.t_fwrite = kt_fwrite,
	.t_ioread = (ssize_t (*)())mdb_tgt_notsup,
	.t_iowrite = (ssize_t (*)())mdb_tgt_notsup,
	.t_vtop = kt_vtop,
	.t_lookup_by_name = kt_lookup_by_name,
	.t_lookup_by_addr = kt_lookup_by_addr,
	.t_symbol_iter = kt_symbol_iter,
	.t_mapping_iter = kt_mapping_iter,
	.t_object_iter = kt_object_iter,
	.t_addr_to_map = kt_addr_to_map,
	.t_name_to_map = kt_name_to_map,
	.t_addr_to_ctf = kt_addr_to_ctf,
	.t_name_to_ctf = kt_name_to_ctf,
	.t_status = kt_status,
	.t_run = (int (*)())(uintptr_t)mdb_tgt_notsup,
	.t_step = (int (*)())(uintptr_t)mdb_tgt_notsup,
	.t_step_out = (int (*)())(uintptr_t)mdb_tgt_notsup,
	.t_next = (int (*)())(uintptr_t)mdb_tgt_notsup,
	.t_cont = (int (*)())(uintptr_t)mdb_tgt_notsup,
	.t_signal = (int (*)())(uintptr_t)mdb_tgt_notsup,
	.t_add_vbrkpt = (int (*)())(uintptr_t)mdb_tgt_null,
	.t_add_sbrkpt = (int (*)())(uintptr_t)mdb_tgt_null,
	.t_add_pwapt = (int (*)())(uintptr_t)mdb_tgt_null,
	.t_add_vwapt = (int (*)())(uintptr_t)mdb_tgt_null,
	.t_add_iowapt = (int (*)())(uintptr_t)mdb_tgt_null,
	.t_add_sysenter = (int (*)())(uintptr_t)mdb_tgt_null,
	.t_add_sysexit = (int (*)())(uintptr_t)mdb_tgt_null,
	.t_add_signal = (int (*)())(uintptr_t)mdb_tgt_null,
	.t_add_fault = (int (*)())(uintptr_t)mdb_tgt_null,
	.t_getareg = kt_getareg,
	.t_putareg = kt_putareg,
	.t_stack_iter = mdb_aarch64_kvm_stack_iter,
	.t_auxv = (int (*)())(uintptr_t)mdb_tgt_notsup
};

void
kt_aarch64_init(mdb_tgt_t *t)
{
	kt_data_t *kt = t->t_data;
	panic_data_t pd;
	struct regs regs;
	uintptr_t addr;

	/*
	 * Initialize the machine-dependent parts of the kernel target
	 * structure.  Once this is complete and we fill in the ops
	 * vector, the target is now fully constructed and we can use
	 * the target API itself to perform the rest of our initialization.
	 */
	kt->k_rds = mdb_aarch64_kregs;
	kt->k_regs = mdb_zalloc(sizeof (mdb_tgt_gregset_t), UM_SLEEP);
	kt->k_regsize = sizeof (mdb_tgt_gregset_t);
	kt->k_dcmd_regs = kt_regs;
	kt->k_dcmd_stack = kt_stack;
	kt->k_dcmd_stackv = kt_stackv;
	kt->k_dcmd_stackr = kt_stackv;
	kt->k_dcmd_cpustack = (int (*)())(uintptr_t)mdb_tgt_notsup;
	kt->k_dcmd_cpuregs = (int (*)())(uintptr_t)mdb_tgt_notsup;

	t->t_ops = &kt_aarch64_ops;

	(void) mdb_dis_select("a64");

	/*
	 * Don't attempt to load any thread or register information if
	 * we're examining the live operating system.
	 */
	if (kt->k_symfile != NULL && strcmp(kt->k_symfile, "/dev/ksyms") == 0)
		return;

	/*
	 * If the panicbuf symbol is present and we can consume a panicbuf
	 * header of the appropriate version from this address, then we can
	 * initialize our current register set based on its contents.
	 * Prior to the re-structuring of panicbuf, our only register data
	 * was the panic_regs label_t, into which a setjmp() was performed,
	 * or the panic_reg register pointer, which was only non-zero if
	 * the system panicked as a result of a trap calling die().
	 */
	if (mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, &pd, sizeof (pd),
	    MDB_TGT_OBJ_EXEC, "panicbuf") == sizeof (pd) &&
	    pd.pd_version == PANICBUFVERS) {

		size_t pd_size = MIN(PANICBUFSIZE, pd.pd_msgoff);
		panic_data_t *pdp = mdb_zalloc(pd_size, UM_SLEEP);
		uint_t i, n;

		(void) mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, pdp, pd_size,
		    MDB_TGT_OBJ_EXEC, "panicbuf");

		n = (pd_size - (sizeof (panic_data_t) -
		    sizeof (panic_nv_t))) / sizeof (panic_nv_t);

		for (i = 0; i < n; i++) {
			(void) kt_putareg(t, kt->k_tid,
			    pdp->pd_nvdata[i].pnv_name,
			    pdp->pd_nvdata[i].pnv_value);
		}

		mdb_free(pdp, pd_size);

		return;
	};

	if (mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, &addr, sizeof (addr),
	    MDB_TGT_OBJ_EXEC, "panic_reg") == sizeof (addr) && addr != 0 &&
	    mdb_tgt_vread(t, &regs, sizeof (regs), addr) == sizeof (regs)) {
		kt_regs_to_kregs(&regs, kt->k_regs);
		return;
	}

	warn("failed to read panicbuf and panic_reg -- "
	    "current register set will be unavailable\n");
}
