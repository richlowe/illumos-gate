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

/* Copyright 2022 Richard Lowe */

#include <mdb/mdb_aarch64util.h>
#include <mdb/mdb_ctf.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_kreg_impl.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_target_impl.h>

#include <sys/errno.h>

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
	{ "spsr", KREG_SPSR, MDB_TGT_R_EXPORT },
	{ "tp", KREG_TP, MDB_TGT_R_EXPORT },
	{ "trapno", KREG_TRAPNO, MDB_TGT_R_EXPORT },
	{ "far", KREG_FAR, MDB_TGT_R_EXPORT },
	{ "esr", KREG_ESR, MDB_TGT_R_EXPORT },
	{ NULL, 0, 0 }
};

#define	BOP_MASK	0xfc000000
#define	BOP(op)		((op) & BOP_MASK)

/* Note that this mask has gaps, to account for authenticated BLR with PAC */
#define	BROP_MASK	0xfefff000
#define	BROP(op)	((op) & BROP_MASK)

enum aarch64_branch {
	BL_INSTR =  0x94000000,
	BLR_INSTR = 0xd63f0000
};

static boolean_t
mdb_aarch64_call_instr(mdb_instr_t instr)
{
	return ((BOP(instr) == BL_INSTR) || (BROP(instr) == BLR_INSTR));
}

/*
 * Put the address of the next instruction after pc in p if a call, or return -1
 * and set errno to EAGAIN if the target should just single-step.
 */
int
mdb_aarch64_next(mdb_tgt_t *t, uintptr_t *p, kreg_t pc, mdb_instr_t curinstr)
{
	if (mdb_aarch64_call_instr(curinstr)) {
		*p = (pc + 4);
		return (0);
	} else {
		return (set_errno(EAGAIN));
	}
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

int
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
	 * On aarch64 we can detect the true number of saved arguments so only
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
