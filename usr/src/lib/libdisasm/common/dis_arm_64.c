/*
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */
/*
 * Copyright (c) 2017, Joyent, Inc.  All rights reserved.
 * Copyright 2024 OmniOS Community Edition (OmniOSce) Association.
 */

/*
 * This provides support for disassembling AArch64 instructions, derived from
 * the ARMv8 Architecture Reference Manual, ARMv8, for ARMv8-A architecture
 * profile, version A.a dated September 2013, chapters C1-C6.
 *
 * XXX Update cross reference for newer versions that renumbered things.
 *
 * All instructions come in as uint32_t's.
 */

#include <libdisasm.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <stdbool.h>
#include <sys/byteorder.h>
#include <sys/bitext.h>

#include "libdisasm_impl.h"

extern size_t strlen(const char *);
extern size_t strlcat(char *, const char *, size_t);

/* Generates a 32-bit mask with bits [l,h] set, given 31 >= h >= l >= 0 */
#define	BITS(h, l) (((unsigned) -1 >> (31 - (h))) & ~((1U << (l)) - 1))
#define	BIT(b) (1U << b)

/*
 * AArch64 instructions are not uniformly encoded, so we are left with little
 * option other than checking individual bits to see which instruction group
 * and class an instruction is part of.  To make things worse, the datasheets
 * re-use variable names for different parts, even within the same group, so
 * for example "op" or "size" may not be the same between classes.
 *
 * Thus, while it is not ideal, the clearest way to refer to sections XXX
 */
#define	A64_BIT_31_MASK		BIT(31)
#define	A64_BIT_31_SHIFT	31
#define	A64_BIT_30_MASK		BIT(30)
#define	A64_BIT_30_SHIFT	30
#define	A64_BIT_29_MASK		BIT(29)
#define	A64_BIT_29_SHIFT	29
#define	A64_BIT_28_MASK		BIT(28)
#define	A64_BIT_28_SHIFT	28
#define	A64_BIT_27_MASK		BIT(27)
#define	A64_BIT_27_SHIFT	27
#define	A64_BIT_26_MASK		BIT(26)
#define	A64_BIT_26_SHIFT	26
#define	A64_BIT_25_MASK		BIT(25)
#define	A64_BIT_25_SHIFT	25
#define	A64_BIT_24_MASK		BIT(24)
#define	A64_BIT_24_SHIFT	24
#define	A64_BIT_23_MASK		BIT(23)
#define	A64_BIT_23_SHIFT	23
#define	A64_BIT_22_MASK		BIT(22)
#define	A64_BIT_22_SHIFT	22
#define	A64_BIT_21_MASK		BIT(21)
#define	A64_BIT_21_SHIFT	21
#define	A64_BIT_20_MASK		BIT(20)
#define	A64_BIT_20_SHIFT	20
#define	A64_BIT_19_MASK		BIT(19)
#define	A64_BIT_19_SHIFT	19
#define	A64_BIT_16_MASK		BIT(16)
#define	A64_BIT_16_SHIFT	16
#define	A64_BIT_12_MASK		BIT(12)
#define	A64_BIT_12_SHIFT	12
#define	A64_BIT_11_MASK		BIT(11)
#define	A64_BIT_11_SHIFT	11
#define	A64_BIT_10_SHIFT	10
#define	A64_BIT_5_SHIFT		5
#define	A64_BIT_0_SHIFT		0

#define	A64_BITS_29_23_MASK	BITS(29, 23)
#define	A64_BITS_23_22_MASK	BITS(23, 22)
#define	A64_BITS_22_19_MASK	BITS(22, 19)
#define	A64_BITS_20_16_MASK	BITS(20, 16)
#define	A64_BITS_18_16_MASK	BITS(18, 16)
#define	A64_BITS_15_12_MASK	BITS(15, 12)
#define	A64_BITS_15_13_MASK	BITS(15, 13)
#define	A64_BITS_14_11_MASK	BITS(14, 11)
#define	A64_BITS_11_10_MASK	BITS(11, 10)
#define	A64_BITS_9_5_MASK	BITS(9, 5)
#define	A64_BITS_4_0_MASK	BITS(4, 0)

/*
 * Definitions for the Data Processing Instruction groups.
 */
#define	A64_DPI_CLASS_MASK	0x1f800000
#define	A64_DPI_CLASS_SHIFT	23

/*
 * Some instruction classes use bit23 for their own purposes, hence the 0/1
 * definitions for those classes so we match both cases.
 */
#define	A64_DPI_CLASS_LOGSHIFT0	0x14	/* Logical (shifted register) */
#define	A64_DPI_CLASS_LOGSHIFT1	0x15
#define	A64_DPI_CLASS_ADDSUBR0	0x16	/* Add/Sub (shift/ext register) */
#define	A64_DPI_CLASS_ADDSUBR1	0x17
#define	A64_DPI_CLASS_PCREL0	0x20	/* PC relative addressing */
#define	A64_DPI_CLASS_PCREL1	0x21
#define	A64_DPI_CLASS_ADDSUBI0	0x22	/* Add/Sub (immediate) */
#define	A64_DPI_CLASS_ADDSUBI1	0x23
#define	A64_DPI_CLASS_LOGIMM	0x24	/* Logical (immediate) */
#define	A64_DPI_CLASS_MOVWIDE	0x25	/* Move wide Immediate */
#define	A64_DPI_CLASS_BITFIELD	0x26	/* Bitfield */
#define	A64_DPI_CLASS_EXTRACT	0x27	/* Extract */
#define	A64_DPI_CLASS_CONDCOMP	0x34	/* Conditional compare + A/s (carry) */
#define	A64_DPI_CLASS_CONDSEL	0x35	/* Conditional select + D-proc[12] */
#define	A64_DPI_CLASS_DPROC30	0x36	/* Data processing (3 source) */
#define	A64_DPI_CLASS_DPROC31	0x37

/* Order by masks */
#define	A64_DPI_LOGSREG_MASK	0x1f000000
#define	A64_DPI_LOGSREG_TARG	0x0a000000
#define	A64_DPI_PCREL_MASK	0x1f000000
#define	A64_DPI_PCREL_TARG	0x10000000
#define	A64_DPI_ADSBIMM_MASK	0x1f000000
#define	A64_DPI_ADSBIMM_TARG	0x11000000
#define	A64_DPI_DP3_MASK	0x1f000000
#define	A64_DPI_DP3_TARG	0x1b000000

#define	A64_DPI_ADSBSREG_MASK	0x1f200000
#define	A64_DPI_ADSBSREG_TARG	0x0b000000
#define	A64_DPI_ADSBXREG_MASK	0x1f200000
#define	A64_DPI_ADSBXREG_TARG	0x0b200000

#define	A64_DPI_LOGIMM_MASK	0x1f800000
#define	A64_DPI_LOGIMM_TARG	0x12000000
#define	A64_DPI_MOVIMM_MASK	0x1f800000
#define	A64_DPI_MOVIMM_TARG	0x12800000
#define	A64_DPI_BITFIELD_MASK	0x1f800000
#define	A64_DPI_BITFIELD_TARG	0x13000000
#define	A64_DPI_EXTRACT_MASK	0x1f800000
#define	A64_DPI_EXTRACT_TARG	0x13800000

#define	A64_DPI_ADSBCARRY_MASK	0x1fe00000
#define	A64_DPI_ADSBCARRY_TARG	0x1a000000
#define	A64_DPI_CONDCOMP_MASK	0x1fe00000
#define	A64_DPI_CONDCOMP_TARG	0x1a400000
#define	A64_DPI_CONDSEL_MASK	0x1fe00000
#define	A64_DPI_CONDSEL_TARG	0x1a800000

#define	A64_DPI_DP2_MASK	0x5fe00000
#define	A64_DPI_DP2_TARG	0x1ac00000
#define	A64_DPI_DP1_MASK	0x5fe00000
#define	A64_DPI_DP1_TARG	0x5ac00000


/*
 * Definitions for portions of the bit space used to determine opcode and
 * operands.
 */
#define	A64_DPI_SF_MASK		0x80000000
#define	A64_DPI_SF_SHIFT	31
#define	A64_DPI_OP_MASK		0x40000000
#define	A64_DPI_OP_SHIFT	30
#define	A64_DPI_OPC_MASK	0x60000000
#define	A64_DPI_OPC_SHIFT	29
#define	A64_DPI_IMMLO_MASK	0x60000000
#define	A64_DPI_IMMLO_SHIFT	29
#define	A64_DPI_S_MASK		0x20000000
#define	A64_DPI_S_SHIFT		29
#define	A64_DPI_IMMHI_MASK	0x00ffffe0
#define	A64_DPI_IMMHI_SHIFT	5
#define	A64_DPI_OP31_MASK	0x00e00000
#define	A64_DPI_OP31_SHIFT	21
#define	A64_DPI_SHIFT_MASK	0x00c00000
#define	A64_DPI_SHIFT_SHIFT	22
#define	A64_DPI_HW_MASK		0x00600000
#define	A64_DPI_HW_SHIFT	21
#define	A64_DPI_IMM12_MASK	0x003ffc00
#define	A64_DPI_IMM12_SHIFT	10
#define	A64_DPI_IMMR_MASK	0x003f0000
#define	A64_DPI_IMMR_SHIFT	16
#define	A64_DPI_N22_MASK	0x00400000	/* Confusingly N is defined */
#define	A64_DPI_N22_SHIFT	22
#define	A64_DPI_N21_MASK	0x00200000	/*  for both bits 21 and 22 */
#define	A64_DPI_N21_SHIFT	21
#define	A64_DPI_IMM16_MASK	0x001fffe0
#define	A64_DPI_IMM16_SHIFT	5
#define	A64_DPI_IMM_MASK	0x001fffe0
#define	A64_DPI_IMM_SHIFT	5
#define	A64_DPI_RM_MASK		0x001f0000
#define	A64_DPI_RM_SHIFT	16
#define	A64_DPI_IMM5_MASK	0x001f0000
#define	A64_DPI_IMM5_SHIFT	16
#define	A64_DPI_IMMS_MASK	0x0000fc00
#define	A64_DPI_IMMS_SHIFT	10
#define	A64_DPI_IMM6_MASK	0x0000fc00
#define	A64_DPI_IMM6_SHIFT	10
#define	A64_DPI_OPCODE_MASK	0x0000fc00
#define	A64_DPI_OPCODE_SHIFT	10
#define	A64_DPI_COND_MASK	0x0000f000
#define	A64_DPI_COND_SHIFT	12
#define	A64_DPI_OPTION_MASK	0x0000e000
#define	A64_DPI_OPTION_SHIFT	13
#define	A64_DPI_IMM3_MASK	0x00001c00
#define	A64_DPI_IMM3_SHIFT	10
#define	A64_DPI_O0_MASK		0x00008000
#define	A64_DPI_O0_SHIFT	15
#define	A64_DPI_RA_MASK		0x00007c00
#define	A64_DPI_RA_SHIFT	10
#define	A64_DPI_OP2_MASK	0x00000c00
#define	A64_DPI_OP2_SHIFT	10
#define	A64_DPI_CCMI_MASK	0x00000400
#define	A64_DPI_RN_MASK		0x000003e0
#define	A64_DPI_RN_SHIFT	5
#define	A64_DPI_RD_MASK		0x0000001f
#define	A64_DPI_RD_SHIFT	0
#define	A64_DPI_NZCV_MASK	0x0000000f

/*
 * C1.2.3 Condition Code
 */
static const char *a64_cond_names[] = {
	"eq",		/* Equal */
	"ne",		/* Not Equal */
	"cs",		/* Carry set/unsigned higher or same */
	"cc",		/* Carry clear/unsigned lower */
	"mi",		/* Minus/negative */
	"pl",		/* Plus/positive or zero */
	"vs",		/* Overflow */
	"vc",		/* No overflow */
	"hi",		/* Unsigned higher */
	"ls",		/* Unsigned lower or same */
	"ge",		/* Signed greater than or equal */
	"lt",		/* Signed less than */
	"gt",		/* Signed greater than */
	"le",		/* Signed less than or equal */
	"al",		/* AL - Always (unconditional) */
	"nv"		/* NV - Always (unconditional) */
};
typedef enum a64_cond_code {
	A64_COND_EQ,
	A64_COND_NE,
	A64_COND_CSHS,
	A64_COND_CCLO,
	A64_COND_MI,
	A64_COND_PL,
	A64_COND_VS,
	A64_COND_VC,
	A64_COND_HI,
	A64_COND_LS,
	A64_COND_GE,
	A64_COND_LT,
	A64_COND_GT,
	A64_COND_LE,
	A64_COND_AL,
	A64_COND_NV
} a64_cond_code_t;

/* XXX: document */
static const char *a64_prefetch_ops[] = {
	"pldl1keep",
	"pldl1strm",
	"pldl2keep",
	"pldl2strm",
	"pldl3keep",
	"pldl3strm",
	NULL,
	NULL,
	"plil1keep",
	"plil1strm",
	"plil2keep",
	"plil2strm",
	"plil3keep",
	"plil3strm",
	NULL,
	NULL,
	"pstl1keep",
	"pstl1strm",
	"pstl2keep",
	"pstl2strm",
	"pstl3keep",
	"pstl3strm",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

/*
 * C1.2.4 Register names
 *
 * There are 31 general purpose registers R0-R30, printed based on the data
 * size, either Wn (32-bit) or Xn (64-bit).  R31 represents either the stack
 * pointer or the zero register.  We default to the zero register, and support
 * printing it as the stack pointer via DPI_SPREGS*.
 */
#define	A64_NUM_REGS	32
static const struct {
	const char *r32name;
	const char *r64name;
	const char *simd8name;
	const char *simd16name;
	const char *simd32name;
	const char *simd64name;
	const char *simd128name;
	const char *simdvname;
} a64_reg_names[] = {
	{  "w0",  "x0",  "b0",  "h0",  "s0",  "d0",  "q0",  "v0"},
	{  "w1",  "x1",  "b1",  "h1",  "s1",  "d1",  "q1",  "v1"},
	{  "w2",  "x2",  "b2",  "h2",  "s2",  "d2",  "q2",  "v2"},
	{  "w3",  "x3",  "b3",  "h3",  "s3",  "d3",  "q3",  "v3"},
	{  "w4",  "x4",  "b4",  "h4",  "s4",  "d4",  "q4",  "v4"},
	{  "w5",  "x5",  "b5",  "h5",  "s5",  "d5",  "q5",  "v5"},
	{  "w6",  "x6",  "b6",  "h6",  "s6",  "d6",  "q6",  "v6"},
	{  "w7",  "x7",  "b7",  "h7",  "s7",  "d7",  "q7",  "v7"},
	{  "w8",  "x8",  "b8",  "h8",  "s8",  "d8",  "q8",  "v8"},
	{  "w9",  "x9",  "b9",  "h9",  "s9",  "d9",  "q9",  "v9"},
	{ "w10", "x10", "b10", "h10", "s10", "d10", "q10", "v10"},
	{ "w11", "x11", "b11", "h11", "s11", "d11", "q11", "v11"},
	{ "w12", "x12", "b12", "h12", "s12", "d12", "q12", "v12"},
	{ "w13", "x13", "b13", "h13", "s13", "d13", "q13", "v13"},
	{ "w14", "x14", "b14", "h14", "s14", "d14", "q14", "v14"},
	{ "w15", "x15", "b15", "h15", "s15", "d15", "q15", "v15"},
	{ "w16", "x16", "b16", "h16", "s16", "d16", "q16", "v16"},
	{ "w17", "x17", "b17", "h17", "s17", "d17", "q17", "v17"},
	{ "w18", "x18", "b18", "h18", "s18", "d18", "q18", "v18"},
	{ "w19", "x19", "b19", "h19", "s19", "d19", "q19", "v19"},
	{ "w20", "x20", "b20", "h20", "s20", "d20", "q20", "v20"},
	{ "w21", "x21", "b21", "h21", "s21", "d21", "q21", "v21"},
	{ "w22", "x22", "b22", "h22", "s22", "d22", "q22", "v22"},
	{ "w23", "x23", "b23", "h23", "s23", "d23", "q23", "v23"},
	{ "w24", "x24", "b24", "h24", "s24", "d24", "q24", "v24"},
	{ "w25", "x25", "b25", "h25", "s25", "d25", "q25", "v25"},
	{ "w26", "x26", "b26", "h26", "s26", "d26", "q26", "v26"},
	{ "w27", "x27", "b27", "h27", "s27", "d27", "q27", "v27"},
	{ "w28", "x28", "b28", "h28", "s28", "d28", "q28", "v28"},
	{ "w29", "x29", "b29", "h29", "s29", "d29", "q29", "v29"},
	{ "w30", "x30", "b30", "h30", "s30", "d30", "q30", "v30"},
	{ "wzr", "xzr", "b31", "h31", "s31", "d31", "q31", "v31"}
};

/*
 * SIMD register arrangement.  These are in specific order so that 8b-1d and
 * 16b-2d can be calculated based on a size offset.
 */
static const char *a64_reg_arr_names[] = {
	"b",
	"h",
	"s",
	"d",
	"8b",
	"4h",
	"2s",
	"1d",
	"16b",
	"8h",
	"4s",
	"2d"
};
typedef enum a64_reg_arr {
	A64_REG_ARR_B,
	A64_REG_ARR_H,
	A64_REG_ARR_S,
	A64_REG_ARR_D,
	A64_REG_ARR_8B,
	A64_REG_ARR_4H,
	A64_REG_ARR_2S,
	A64_REG_ARR_1D,
	A64_REG_ARR_16B,
	A64_REG_ARR_8H,
	A64_REG_ARR_4S,
	A64_REG_ARR_2D
} a64_reg_arr_t;

/*
 * Arrangement specifiers for multiple structure variants.  Lookup is based
 * on [Q][size].
 */
const char *a64_simd_arrange[2][4] = {
	{ "8b", "4h", "2s", "1d"},
	{"16b", "8h", "4s", "2d"},
};

typedef enum a64_reg_id {
	A64_REG_0,  A64_REG_1,  A64_REG_2,  A64_REG_3,
	A64_REG_4,  A64_REG_5,  A64_REG_6,  A64_REG_7,
	A64_REG_8,  A64_REG_9,  A64_REG_10, A64_REG_11,
	A64_REG_12, A64_REG_13, A64_REG_14, A64_REG_15,
	A64_REG_16, A64_REG_17, A64_REG_18, A64_REG_19,
	A64_REG_20, A64_REG_21, A64_REG_22, A64_REG_23,
	A64_REG_24, A64_REG_25, A64_REG_26, A64_REG_27,
	A64_REG_28, A64_REG_29, A64_REG_30, A64_REG_ZR,
	A64_REG_SP = A64_REG_ZR
} a64_reg_id_t;

typedef enum a64_reg_width {
	A64_REGWIDTH_32,	/* <Wn> */
	A64_REGWIDTH_64,	/* <Xn> */
	A64_REGWIDTH_SIMD_8,	/* <Bn> */
	A64_REGWIDTH_SIMD_16,	/* <Hn> */
	A64_REGWIDTH_SIMD_32,	/* <Sn> */
	A64_REGWIDTH_SIMD_64,	/* <Dn> */
	A64_REGWIDTH_SIMD_128,	/* <Qn> */
	A64_REGWIDTH_SIMD_V,	/* <Vn> */
} a64_reg_width_t;

/*
 * Register flags for determining output.
 */
typedef enum a64_reg_flags {
	A64_REGFLAGS_NONE = 0x00,
	A64_REGFLAGS_32 = 0x01,		/* For 64-bit ops, <R><m> == W<m> */
	A64_REGFLAGS_SP = 0x02,		/* Use SP instead of ZR */
	A64_REGFLAGS_SIMD = 0x04,	/* SIMD/FP register */
} a64_reg_flags_t;

typedef struct a64_reg {
	a64_reg_id_t id;		/* register id */
	a64_reg_width_t width;		/* register width */
	a64_reg_flags_t flags;		/* register flags */
	a64_reg_arr_t arr;		/* Vector arrangement */
} a64_reg_t;

typedef struct dis_handle_arm_64 {
	uint64_t	dha_adrp_addr[A64_NUM_REGS];
	uint64_t	dha_adrp_imm[A64_NUM_REGS];
} dis_handle_arm_64_t;

static const char *
a64_reg_name(a64_reg_t reg)
{
	switch (reg.width) {
	case A64_REGWIDTH_32:
		return ((reg.id == A64_REG_SP && (reg.flags & A64_REGFLAGS_SP))
		    ? "wsp" : a64_reg_names[reg.id].r32name);
		break;
	case A64_REGWIDTH_64:
		return ((reg.id == A64_REG_SP && (reg.flags & A64_REGFLAGS_SP))
		    ? "sp" : a64_reg_names[reg.id].r64name);
		break;
	case A64_REGWIDTH_SIMD_8:
		return (a64_reg_names[reg.id].simd8name);
		break;
	case A64_REGWIDTH_SIMD_16:
		return (a64_reg_names[reg.id].simd16name);
		break;
	case A64_REGWIDTH_SIMD_32:
		return (a64_reg_names[reg.id].simd32name);
		break;
	case A64_REGWIDTH_SIMD_64:
		return (a64_reg_names[reg.id].simd64name);
		break;
	case A64_REGWIDTH_SIMD_128:
		return (a64_reg_names[reg.id].simd128name);
		break;
	case A64_REGWIDTH_SIMD_V:
		return (a64_reg_names[reg.id].simdvname);
		break;
	}

	return (NULL);
}

/*
 * Handle printing of <Vn>.<Ta> or {<Vn>.<Ta> ...} sections based on the number
 * of registers and their arrangements.
 */
static size_t
a64_print_vregs(char *buf, size_t buflen, a64_reg_t v1, int nregs)
{
	a64_reg_t v2, v3, v4;
	size_t len;

	v2 = v3 = v4 = v1;
	v2.id = ((v1.id + 1) % 32);
	v3.id = ((v1.id + 2) % 32);
	v4.id = ((v1.id + 3) % 32);

	switch (nregs) {
	case 1:
		len = dis_snprintf(buf, buflen, "{%s.%s}",
		    a64_reg_name(v1), a64_reg_arr_names[v1.arr]);
		break;
	case 2:
		len = dis_snprintf(buf, buflen, "{%s.%s, %s.%s}",
		    a64_reg_name(v1), a64_reg_arr_names[v1.arr],
		    a64_reg_name(v2), a64_reg_arr_names[v2.arr]);
		break;
	case 3:
		if (v3.id > v1.id) {
			len = dis_snprintf(buf, buflen, "{%s.%s-%s.%s}",
			    a64_reg_name(v1), a64_reg_arr_names[v1.arr],
			    a64_reg_name(v3), a64_reg_arr_names[v3.arr]);
		} else {
			len = dis_snprintf(buf, buflen, "{%s.%s, %s.%s, %s.%s}",
			    a64_reg_name(v1), a64_reg_arr_names[v1.arr],
			    a64_reg_name(v2), a64_reg_arr_names[v2.arr],
			    a64_reg_name(v3), a64_reg_arr_names[v3.arr]);
		}
		break;
	case 4:
		if (v4.id > v1.id) {
			len = dis_snprintf(buf, buflen, "{%s.%s-%s.%s}",
			    a64_reg_name(v1), a64_reg_arr_names[v1.arr],
			    a64_reg_name(v4), a64_reg_arr_names[v4.arr]);
		} else {
			len = dis_snprintf(buf, buflen,
			    "{%s.%s, %s.%s, %s.%s, %s.%s}",
			    a64_reg_name(v1), a64_reg_arr_names[v1.arr],
			    a64_reg_name(v2), a64_reg_arr_names[v2.arr],
			    a64_reg_name(v3), a64_reg_arr_names[v3.arr],
			    a64_reg_name(v4), a64_reg_arr_names[v4.arr]);
		}
		break;
	}

	return (len);
}

/*
 * Data Processing Instruction registers.  This does not take into account
 * registers which have a variable width based on the operand, for example
 * <R><m>.  Those are handled by DPI_WDREGS_*.
 */
typedef enum a64_dataproc_reg {
	DPI_REGS_d = 0x01,	/* <Rd> */
	DPI_REGS_n = 0x02,	/* <Rn> */
	DPI_REGS_m = 0x04,	/* <Rm> */
	DPI_REGS_a = 0x08	/* <Ra> */
} a64_dataproc_reg_t;
#define	DPI_REGS_dn	(DPI_REGS_d|DPI_REGS_n)
#define	DPI_REGS_dnm	(DPI_REGS_dn|DPI_REGS_m)
#define	DPI_REGS_dnma	(DPI_REGS_dnm|DPI_REGS_a)
#define	DPI_REGS_dm	(DPI_REGS_d|DPI_REGS_m)
#define	DPI_REGS_nm	(DPI_REGS_n|DPI_REGS_m)

/*
 * Instruction operand type.
 */
typedef enum a64_dataproc_optype {
	DPI_OPTYPE_NONE,	/* No additional operands */
	DPI_OPTYPE_IMM,		/* #<imm> */
	DPI_OPTYPE_IMMX,	/* 0x<imm> */
	DPI_OPTYPE_IMM2,	/* #<immr>, #<imms> */
	DPI_OPTYPE_SIMM,	/* #<imm>{, lsl #<shift>} */
	DPI_OPTYPE_XREG,	/* <extend> {#<amount>} */
	DPI_OPTYPE_SREG,	/* <shift> #<amount> */
	DPI_OPTYPE_IMMS,	/* #<imm>{, <shift>} */
	DPI_OPTYPE_IMM_MOV,	/* #<imm> (0x%x) */
	DPI_OPTYPE_COND,	/* <cond> */
	DPI_OPTYPE_COND_IMM,	/* #<imm>, #<nzcv>, <cond> */
	DPI_OPTYPE_COND_REG,	/* #<nzcv>, <cond> */
	DPI_OPTYPE_COND_INV,	/* invert(<cond>) */
	DPI_OPTYPE_LABEL,	/* 0x<imm> <label> */
} a64_dataproc_optype_t;

/*
 * Operand per-register flags.
 */
typedef enum a64_dataproc_flags {
	DPI_SPREGS_d = 0x01,	/* <Rd|SP> */
	DPI_SPREGS_n = 0x02,	/* <Rn|SP> */
	DPI_SPREGS_m = 0x04,	/* <Rm|SP> */
	DPI_SPREGS_a = 0x08,	/* <Ra|SP> */
	DPI_WDREGS_d = 0x10,	/* <R><d> */
	DPI_WDREGS_n = 0x20,	/* <R><n> */
	DPI_WDREGS_m = 0x40,	/* <R><m> */
	DPI_WDREGS_a = 0x80,	/* <R><a> */
	DPI_WREGS_d = 0x100,	/* <Wd> */
	DPI_WREGS_n = 0x200,	/* <Wn> */
	DPI_WREGS_m = 0x400,	/* <Wm> */
	DPI_WREGS_a = 0x800,	/* <Wa> */
	DPI_ADRP_F = 0x1000,
} a64_dataproc_flags_t;
#define	DPI_SPREGS_dn		(DPI_SPREGS_d|DPI_SPREGS_n)
#define	DPI_WDREGS_dn		(DPI_WDREGS_d|DPI_WDREGS_n)
#define	DPI_WDREGS_dnm		(DPI_WDREGS_dn|DPI_WDREGS_m)
#define	DPI_WREGS_nm		(DPI_WREGS_n|DPI_WREGS_m)
#define	DPI_WREGS_dn		(DPI_WREGS_d|DPI_WREGS_n)

/*
 * A Data Processing Instruction opcode entry.
 */
typedef struct a64_dataproc_opcode_entry {
	const char *name;
	a64_dataproc_reg_t regs;
	a64_dataproc_optype_t optype;
	a64_dataproc_flags_t flags;
} a64_dataproc_opcode_entry_t;

/*
 * The Data Processing Instruction opcode table, indexed by a64_dataproc_opcode.
 */
static a64_dataproc_opcode_entry_t a64_dataproc_opcodes[] = {
	/* C3.4.1 Add/subtract (immediate) */
	{"add",		DPI_REGS_dn,	DPI_OPTYPE_IMMS,	DPI_SPREGS_dn|
								DPI_ADRP_F},
	{"mov",		DPI_REGS_dn,	DPI_OPTYPE_NONE,	DPI_SPREGS_dn},
	{"adds",	DPI_REGS_dn,	DPI_OPTYPE_IMMS,	DPI_SPREGS_n},
	{"cmn",		DPI_REGS_n,	DPI_OPTYPE_IMMS,	DPI_SPREGS_n},
	{"sub",		DPI_REGS_dn,	DPI_OPTYPE_IMMS,	DPI_SPREGS_dn},
	{"subs",	DPI_REGS_dn,	DPI_OPTYPE_IMMS,	DPI_SPREGS_n},
	{"cmp",		DPI_REGS_n,	DPI_OPTYPE_IMMS,	DPI_SPREGS_n},
	/* C3.4.2 Bitfield */
	{"sbfm",	DPI_REGS_dn,	DPI_OPTYPE_IMM2,	0},
	{"asr",		DPI_REGS_dn,	DPI_OPTYPE_IMM,		0},
	{"sbfiz",	DPI_REGS_dn,	DPI_OPTYPE_IMM2,	0},
	{"sbfx",	DPI_REGS_dn,	DPI_OPTYPE_IMM2,	0},
	{"sxtb",	DPI_REGS_dn,	DPI_OPTYPE_NONE,	DPI_WREGS_n},
	{"sxth",	DPI_REGS_dn,	DPI_OPTYPE_NONE,	DPI_WREGS_n},
	{"sxtw",	DPI_REGS_dn,	DPI_OPTYPE_NONE,	DPI_WREGS_n},
	{"bfm",		DPI_REGS_dn,	DPI_OPTYPE_IMM2,	0},
	{"bfi",		DPI_REGS_dn,	DPI_OPTYPE_IMM2,	0},
	{"bfxil",	DPI_REGS_dn,	DPI_OPTYPE_IMM2,	0},
	{"ubfm",	DPI_REGS_dn,	DPI_OPTYPE_IMM2,	0},
	{"lsl",		DPI_REGS_dn,	DPI_OPTYPE_IMM,		0},
	{"lsr",		DPI_REGS_dn,	DPI_OPTYPE_IMM,		0},
	{"ubfiz",	DPI_REGS_dn,	DPI_OPTYPE_IMM2,	0},
	{"ubfx",	DPI_REGS_dn,	DPI_OPTYPE_IMM2,	0},
	{"uxtb",	DPI_REGS_dn,	DPI_OPTYPE_NONE,	0},
	{"uxth",	DPI_REGS_dn,	DPI_OPTYPE_NONE,	0},
	/* C3.4.3 Extract */
	{"extr",	DPI_REGS_dnm,	DPI_OPTYPE_IMM,		0},
	{"ror",		DPI_REGS_dn,	DPI_OPTYPE_IMM,		0},
	/* C3.4.4 Logical (immediate) */
	{"and",		DPI_REGS_dn,	DPI_OPTYPE_IMM_MOV,	DPI_SPREGS_d},
	{"orr",		DPI_REGS_dn,	DPI_OPTYPE_IMM_MOV,	DPI_SPREGS_d},
	{"mov",		DPI_REGS_d,	DPI_OPTYPE_IMM_MOV,	DPI_SPREGS_d},
	{"eor",		DPI_REGS_dn,	DPI_OPTYPE_IMM_MOV,	DPI_SPREGS_d},
	{"ands",	DPI_REGS_dn,	DPI_OPTYPE_IMM_MOV,	0},
	{"tst",		DPI_REGS_n,	DPI_OPTYPE_IMM_MOV,	0},
	/* C3.4.5 Move wide (immediate) */
	{"movn",	DPI_REGS_d,	DPI_OPTYPE_SIMM,	0},
	{"mov",		DPI_REGS_d,	DPI_OPTYPE_IMM_MOV,	0},
	{"movz",	DPI_REGS_d,	DPI_OPTYPE_SIMM,	0},
	{"mov",		DPI_REGS_d,	DPI_OPTYPE_IMM_MOV,	0},
	{"movk",	DPI_REGS_d,	DPI_OPTYPE_SIMM,	0},
	/* C3.4.6 PC-rel. addressing */
	{"adr",		DPI_REGS_d,	DPI_OPTYPE_LABEL,	0},
	{"adrp",	DPI_REGS_d,	DPI_OPTYPE_IMMX,	0},
	/* C3.5.1 Add/subtract (extended register) */
	{"add",		DPI_REGS_dnm,	DPI_OPTYPE_XREG,	DPI_SPREGS_dn|
								DPI_WDREGS_m},
	{"adds",	DPI_REGS_dnm,	DPI_OPTYPE_XREG,	DPI_SPREGS_n|
								DPI_WDREGS_m},
	{"cmn",		DPI_REGS_nm,	DPI_OPTYPE_XREG,	DPI_SPREGS_n|
								DPI_WDREGS_m},
	{"sub",		DPI_REGS_dnm,	DPI_OPTYPE_XREG,	DPI_SPREGS_dn|
								DPI_WDREGS_m},
	{"subs",	DPI_REGS_dnm,	DPI_OPTYPE_XREG,	DPI_SPREGS_n|
								DPI_WDREGS_m},
	{"cmp",		DPI_REGS_nm,	DPI_OPTYPE_XREG,	DPI_SPREGS_n|
								DPI_WDREGS_m},
	/* C3.5.2 Add/subtract (shifted register) */
	{"add",		DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"adds",	DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"cmn",		DPI_REGS_nm,	DPI_OPTYPE_SREG,	0},
	{"sub",		DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"neg",		DPI_REGS_dm,	DPI_OPTYPE_SREG,	0},
	{"subs",	DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"cmp",		DPI_REGS_nm,	DPI_OPTYPE_SREG,	0},
	{"negs",	DPI_REGS_dm,	DPI_OPTYPE_SREG,	0},
	/* C3.5.3 Add/subtract (with carry) */
	{"adc",		DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"adcs",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"sbc",		DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"ngc",		DPI_REGS_dm,	DPI_OPTYPE_NONE,	0},
	{"sbcs",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"ngcs",	DPI_REGS_dm,	DPI_OPTYPE_NONE,	0},
	/* C3.5.4 Conditional compare (immediate) */
	{"ccmn",	DPI_REGS_n,	DPI_OPTYPE_COND_IMM,	0},
	{"ccmp",	DPI_REGS_n,	DPI_OPTYPE_COND_IMM,	0},
	/* C3.5.5 Conditional compare (register) */
	{"ccmn",	DPI_REGS_nm,	DPI_OPTYPE_COND_REG,	0},
	{"ccmp",	DPI_REGS_nm,	DPI_OPTYPE_COND_REG,	0},
	/* C3.5.6 Conditional select */
	{"csel",	DPI_REGS_dnm,	DPI_OPTYPE_COND,	0},
	{"csinc",	DPI_REGS_dnm,	DPI_OPTYPE_COND,	0},
	{"cinc",	DPI_REGS_dn,	DPI_OPTYPE_COND_INV,	0},
	{"cset",	DPI_REGS_d,	DPI_OPTYPE_COND_INV,	0},
	{"csinv",	DPI_REGS_dnm,	DPI_OPTYPE_COND,	0},
	{"cinv",	DPI_REGS_dn,	DPI_OPTYPE_COND_INV,	0},
	{"csetm",	DPI_REGS_d,	DPI_OPTYPE_COND_INV,	0},
	{"csneg",	DPI_REGS_dnm,	DPI_OPTYPE_COND,	0},
	{"cneg",	DPI_REGS_dn,	DPI_OPTYPE_COND_INV,	0},
	/* C3.5.7 Data-processing (1 source) */
	{"rbit",	DPI_REGS_dn,	DPI_OPTYPE_NONE,	0},
	{"rev16",	DPI_REGS_dn,	DPI_OPTYPE_NONE,	0},
	{"rev32",	DPI_REGS_dn,	DPI_OPTYPE_NONE,	0},
	{"rev",		DPI_REGS_dn,	DPI_OPTYPE_NONE,	0},
	{"clz",		DPI_REGS_dn,	DPI_OPTYPE_NONE,	0},
	{"cls",		DPI_REGS_dn,	DPI_OPTYPE_NONE,	0},
	/* C3.5.8 Data-processing (2 source) */
	{"udiv",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"sdiv",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"lslv",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"lsl",		DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"lsrv",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"lsr",		DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"asrv",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"asr",		DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"rorv",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"ror",		DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"crc32b",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WDREGS_dnm},
	{"crc32h",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WDREGS_dnm},
	{"crc32w",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WDREGS_dnm},
	{"crc32x",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WREGS_dn},
	{"crc32cb",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WDREGS_dnm},
	{"crc32ch",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WDREGS_dnm},
	{"crc32cw",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WDREGS_dnm},
	{"crc32cx",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WREGS_dn},
	/* C3.5.9 Data-processing (3 source) */
	{"madd",	DPI_REGS_dnma,	DPI_OPTYPE_NONE,	0},
	{"mul",		DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"msub",	DPI_REGS_dnma,	DPI_OPTYPE_NONE,	0},
	{"mneg",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"smaddl",	DPI_REGS_dnma,	DPI_OPTYPE_NONE,	DPI_WREGS_nm},
	{"smull",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WREGS_nm},
	{"smsubl",	DPI_REGS_dnma,	DPI_OPTYPE_NONE,	DPI_WREGS_nm},
	{"smnegl",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WREGS_nm},
	{"smulh",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	{"umaddl",	DPI_REGS_dnma,	DPI_OPTYPE_NONE,	DPI_WREGS_nm},
	{"umull",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WREGS_nm},
	{"umsubl",	DPI_REGS_dnma,	DPI_OPTYPE_NONE,	DPI_WREGS_nm},
	{"umnegl",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	DPI_WREGS_nm},
	{"umulh",	DPI_REGS_dnm,	DPI_OPTYPE_NONE,	0},
	/* C3.5.10 Logical (shifted register) */
	{"and",		DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"bic",		DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"orr",		DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"mov",		DPI_REGS_dm,	DPI_OPTYPE_NONE,	0},
	{"orn",		DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"mvn",		DPI_REGS_dm,	DPI_OPTYPE_SREG,	0},
	{"eor",		DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"eon",		DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"ands",	DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
	{"tst",		DPI_REGS_nm,	DPI_OPTYPE_SREG,	0},
	{"bics",	DPI_REGS_dnm,	DPI_OPTYPE_SREG,	0},
};

typedef enum a64_dataproc_opcode {
	/* C3.4.1 Add/subtract (immediate) */
	DPI_OP_ADD_I,
	DPI_OP_MOV_SP,
	DPI_OP_ADDS_I,
	DPI_OP_CMN_I,
	DPI_OP_SUB_I,
	DPI_OP_SUBS_I,
	DPI_OP_CMP_I,
	/* C3.4.2 Bitfield */
	DPI_OP_SBFM,
	DPI_OP_ASR_I,
	DPI_OP_SBFIZ,
	DPI_OP_SBFX,
	DPI_OP_SXTB,
	DPI_OP_SXTH,
	DPI_OP_SXTW,
	DPI_OP_BFM,
	DPI_OP_BFI,
	DPI_OP_BFXIL,
	DPI_OP_UBFM,
	DPI_OP_LSL_I,
	DPI_OP_LSR_I,
	DPI_OP_UBFIZ,
	DPI_OP_UBFX,
	DPI_OP_UXTB,
	DPI_OP_UXTH,
	/* C3.4.3 Extract */
	DPI_OP_EXTR,
	DPI_OP_ROR_I,
	/* C3.4.4 Logical (immediate) */
	DPI_OP_AND_I,
	DPI_OP_ORR_I,
	DPI_OP_MOV_BI,
	DPI_OP_EOR_I,
	DPI_OP_ANDS_I,
	DPI_OP_TST_I,
	/* C3.4.5 Move wide (immediate) */
	DPI_OP_MOVN,
	DPI_OP_MOV_IWI,
	DPI_OP_MOVZ,
	DPI_OP_MOV_WI,
	DPI_OP_MOVK,
	/* C3.4.6 PC-rel. addressing */
	DPI_OP_ADR,
	DPI_OP_ADRP,
	/* C3.5.1 Add/subtract (extended register) */
	DPI_OP_ADD_XR,
	DPI_OP_ADDS_XR,
	DPI_OP_CMN_XR,
	DPI_OP_SUB_XR,
	DPI_OP_SUBS_XR,
	DPI_OP_CMP_XR,
	/* C3.5.2 Add/subtract (shifted register) */
	DPI_OP_ADD_SR,
	DPI_OP_ADDS_SR,
	DPI_OP_CMN_SR,
	DPI_OP_SUB_SR,
	DPI_OP_NEG,
	DPI_OP_SUBS_SR,
	DPI_OP_CMP_SR,
	DPI_OP_NEGS,
	/* C3.5.3 Add/subtract (with carry) */
	DPI_OP_ADC,
	DPI_OP_ADCS,
	DPI_OP_SBC,
	DPI_OP_NGC,
	DPI_OP_SBCS,
	DPI_OP_NGCS,
	/* C3.5.4 Conditional compare (immediate) */
	DPI_OP_CCMN_I,
	DPI_OP_CCMP_I,
	/* C3.5.5 Conditional compare (register) */
	DPI_OP_CCMN_R,
	DPI_OP_CCMP_R,
	/* C3.5.6 Conditional select */
	DPI_OP_CSEL,
	DPI_OP_CSINC,
	DPI_OP_CINC,
	DPI_OP_CSET,
	DPI_OP_CSINV,
	DPI_OP_CINV,
	DPI_OP_CSETM,
	DPI_OP_CSNEG,
	DPI_OP_CNEG,
	/* C3.5.7 Data-processing (1 source) */
	DPI_OP_RBIT,
	DPI_OP_REV16,
	DPI_OP_REV32,
	DPI_OP_REV,
	DPI_OP_CLZ,
	DPI_OP_CLS,
	/* C3.5.8 Data-processing (2 source) */
	DPI_OP_UDIV,
	DPI_OP_SDIV,
	DPI_OP_LSLV,
	DPI_OP_LSL_R,
	DPI_OP_LSRV,
	DPI_OP_LSR_R,
	DPI_OP_ASRV,
	DPI_OP_ASR_R,
	DPI_OP_RORV,
	DPI_OP_ROR_R,
	DPI_OP_CRC32B,
	DPI_OP_CRC32H,
	DPI_OP_CRC32W,
	DPI_OP_CRC32X,
	DPI_OP_CRC32CB,
	DPI_OP_CRC32CH,
	DPI_OP_CRC32CW,
	DPI_OP_CRC32CX,
	/* C3.5.9 Data-processing (3 source) */
	DPI_OP_MADD,
	DPI_OP_MUL,
	DPI_OP_MSUB,
	DPI_OP_MNEG,
	DPI_OP_SMADDL,
	DPI_OP_SMULL,
	DPI_OP_SMSUBL,
	DPI_OP_SMNEGL,
	DPI_OP_SMULH,
	DPI_OP_UMADDL,
	DPI_OP_UMULL,
	DPI_OP_UMSUBL,
	DPI_OP_UMNEGL,
	DPI_OP_UMULH,
	/* C3.5.10 Logical (shifted register) */
	DPI_OP_AND_SR,
	DPI_OP_BIC_SR,
	DPI_OP_ORR_SR,
	DPI_OP_MOV_R,
	DPI_OP_ORN_SR,
	DPI_OP_MVN,
	DPI_OP_EOR_SR,
	DPI_OP_EON_SR,
	DPI_OP_ANDS_SR,
	DPI_OP_TST_SR,
	DPI_OP_BICS_SR,
} a64_dataproc_opcode_t;

const char *a64_dataproc_shifts[] = {
	"lsl",	/* Logical shift left */
	"lsr",	/* Logical shift right */
	"asr",	/* Arithmetic shift right */
	"ror"	/* Rotate right */
};

typedef enum a64_dataproc_shift_type {
	DPI_S_LSL,	/* Logical shift left */
	DPI_S_LSR,	/* Logical shift right */
	DPI_S_ASR,	/* Arithmetic shift right */
	DPI_S_ROR	/* RESERVED */
} a64_dataproc_shift_type_t;

const char *a64_dataproc_extends[] = {
	"uxtb",		/* Unsigned extend byte */
	"uxth",		/* Unsigned extend halfword */
	"uxtw",		/* Unsigned extend word */
	"uxtx",
	"sxtb",		/* Sign-extend byte */
	"sxth",		/* Sign-extend halfword */
	"sxtw",		/* Sign-extend word */
	"sxtx",
	"lsl"		/* Special form of uxtw(32)/uxtx(64) for SP */
};

typedef enum a64_dataproc_extend_type {
	DPI_E_UXTB,
	DPI_E_UXTH,
	DPI_E_UXTW,
	DPI_E_UXTX,
	DPI_E_SXTB,
	DPI_E_SXTH,
	DPI_E_SXTW,
	DPI_E_SXTX,
	DPI_E_LSL
} a64_dataproc_extend_type_t;

typedef struct a64_dataproc_shifter_imm {
	uint64_t dataprocsi_imm;		/* Immediate value */
	uint8_t dataprocsi_shift;		/* Shift amount */
	uint8_t dataprocsi_inv;			/* Whether to NOT(imm) */
} a64_dataproc_shifter_imm_t;

typedef struct a64_dataproc_shifter_imm2 {
	uint32_t dataprocsi2_imm1;
	uint32_t dataprocsi2_imm2;
} a64_dataproc_shifter_imm2_t;

/* {, <extend> {#<amount>}} */
typedef struct a64_dataproc_shifter_xreg {
	a64_dataproc_extend_type_t dataprocss_type;	/* Extend type */
	uint16_t dataprocss_imm;			/* Shift value */
} a64_dataproc_shifter_xreg_t;

/* sort this one out first */
/* {, <shift> #<amount>} */
typedef struct a64_dataproc_shifter_sreg {
	a64_dataproc_shift_type_t dataprocsr_type;	/* Shift type */
	uint8_t dataprocsr_imm;				/* Shift value */
} a64_dataproc_shifter_sreg_t;

/* immediate */
#define	dpimm_imm	dataproci_un.dataproci_im.dataprocsi_imm
#define	dpimm_shift	dataproci_un.dataproci_im.dataprocsi_shift
#define	imm_inv		dataproci_un.dataproci_im.dataprocsi_inv
/* immediate 2 */
#define	imm2_imm1	dataproci_un.dataproci_im2.dataprocsi2_imm1
#define	imm2_imm2	dataproci_un.dataproci_im2.dataprocsi2_imm2
/* extended register */
#define	xreg_type	dataproci_un.dataproci_si.dataprocss_type
#define	xreg_imm	dataproci_un.dataproci_si.dataprocss_imm
/* shifted register */
#define	sreg_type	dataproci_un.dataproci_ri.dataprocsr_type
#define	sreg_imm	dataproci_un.dataproci_ri.dataprocsr_imm

/*
 * Taken from G.1.4 aarch64/instrs/integer/logical/movwpreferred
 */
static int __unused
a64_dis_movwpreferred(int sf, int nbit, int imms, int immr)
{
	int width;

	width = (sf == 1) ? 64 : 32;

	if (sf == 1 && nbit != 1)
		return (0);

	if (sf == 0 && nbit != 0 && (imms & 0x20) != 0)
		return (0);

	if (imms < 16)
		return ((-immr % 16) <= (15 - imms));

	if (imms >= (width - 15))
		return ((immr % 16) <= (imms - (width - 15)));

	return (0);
}

/*
 * Taken from G.1.4 aarch64/instrs/integer/bitfield/bfxpreferred
 */
static int
a64_dis_bfxpref(int sf, int uns, int imms, int immr)
{
	if (imms < immr)
		return (0);

	if (imms == (sf << 6) + 31)
		return (0);

	if (immr == 0) {
		if (sf == 0 && (imms == 7 || imms == 15))
			return (0);
		if (sf == 1 && uns == 0 &&
		    (imms == 7 || imms == 15 || imms == 31))
			return (0);
	}
	return (1);
}
/*
 * G.1.4 aarch64/instrs/integer/bitmasks DecodeBitMasks()
 *
 * The immediate value is constructed from a sub-pattern, repeated as needed to
 * produce the number of bits required for the register.
 * The initial sub-pattern is a consecutive sequence of zeros followed by
 * a number of consecutive ones (think a regex pattern of '0+1+').
 *
 * The sub-pattern size and the number of consecutive ones is determed by N and
 * imms as shown in the following table. The value specified by the x places
 * determines the number of consecutive ones. 0 means there is one 1 in the
 * pattern, 3 means there are four, and so on.
 *
 *	N	imms		sub-pattern size
 *	0	1 1 1 1 0 x	2 bits
 *	0	1 1 1 0 x x	4 bits
 *	0	1 1 0 x x x	8 bits
 *	0	1 0 x x x x	16 bits
 *	0	0 x x x x x	32 bits
 *	1	x x x x x x	64 bits
 *
 * Once this initial sub-pattern has been determined, it will then be rotated
 * right 'immr' times to produce the final sub-pattern.
 * This final sub-pattern will be repeated as many times as necessary to
 * produce the register value.
 *
 * A couple of examples
 *
 * 	0	0 0 1 1 1 1	immr=0
 *
 * is:
 * 	a 32-bit sub-pattern
 * 	has 16 consecutive ones at the end
 * 	the sub-pattern is therefore 0000ffff
 * 	when used for a 64-bit register, imm will be 0000ffff0000ffff
 *
 * 	0	1 1 0 0 1 0	immr=4
 *
 * is:
 * 	an 8-bit sub-pattern
 * 	has 3 consecutive ones at the end
 * 	the initial sub-pattern is therefore 07
 * 	the four rotations change this to 70
 * 	when used for a 32-bit register, imm will be 70707070
 */
static uint64_t
a64_dis_decodebitmasks(uint8_t sfbit, uint8_t nbit, uint8_t imms, uint8_t immr)
{
	uint_t i, size;
	int topbit;
	uint8_t r, s;
	uint64_t imm;

	/*
	 * Determine the size of the element by concatenating N and a
	 * bit-flipped imms, and then looking for the highest set bit.
	 */
	topbit = fls((nbit << 6) | (~imms & 0x3f));
	if (topbit == 0)
		return (0);
	size = 1 << (topbit - 1);

	r = immr & (size - 1);
	s = imms & (size - 1);

	imm = (1ULL << (s + 1)) - 1;


	/* Rotate right as specified by immr. */
	for (i = 0; i < r; ++i)
		imm = ((imm & 1) << (size - 1)) | (imm >> 1);

	/* Repeat the sub-pattern to fill the register. */
	for (i = (sfbit ? 64 : 32) / size; i > 1; i--)
		imm |= (imm << size);

	return (imm);
}

/*
 * Data Processing Instructions
 * ============================
 *
 *   Data Processing - Immediate
 *
 * 31|30|29|28 24|23|22|21|20     16|15         10|9  5|4     0|
 * --+--+--+-----+--+--+--+---------+-------------+----+-------+
 * sf|op| S|10001|shift|          imm12           | Rn |   Rd  | Add/Sub
 * sf| opc |10011  0| N|    immr    |     imms    | Rn |   Rd  | Bitfield
 * sf| op21|10011  1| N|oO|   Rm    |     imms    | Rn |   Rd  | Extract
 * sf| opc |10010  0| N|    immr    |     imms    | Rn |   Rd  | Logical
 * sf| opc |10010  1|  hw |           imm16            |   Rd  | Move wide
 * op|immlo|10000|                immhi                |   Rd  | PC-Rel Addr
 *
 *   Data Processing - Register
 *
 * 31|30|29|28 24|23 22|21|20 16|15|14 13|12|11|10|9  5|4|3   0|
 * --+--+--+-----+-----+--+-----+--+-----+--+--+--+----+-------+
 * sf|op| S|01011| opt | 1|  Rm | option |  imm3  | Rn |   Rd  | Add/Sub (extrg)
 * sf|op| S|01011|shift| 0|  Rm |       imm6      | Rn |   Rd  | Add/Sub (shift)
 * sf|op| S|11010  0  0  0|  Rm |     opcode2     | Rn |   Rd  | Add/Sub (carry)
 * sf|op| S|11010  0  1  0| imm5|   cond    | 1|o2| Rn |o3|nzcv| CondComp (imm)
 * sf|op| S|11010  0  1  0|  Rm |   cond    | 0|o2| Rn |o3|nzcv| CondComp (reg)
 * sf|op| S|11010  1  0  0|  Rm |   cond    | op2 | Rn |   Rd  | Cond Select
 * sf| 1| S|11010  1  1  0| opc2|     opcode      | Rn |   Rd  | DataProc (1src)
 * sf| 0| S|11010  1  1  0|  Rm |     opcode      | Rn |   Rd  | DataProc (2src)
 * sf| op54|11011|  op31  |  Rm |o0|      Ra      | Rn |   Rd  | DataProc (3src)
 * sf| opc |01010|shift| N|  Rm |       imm6      | Rn |   Rd  | Logical Shifted
 *
 * Used by most:
 *
 * 	31:	sf
 * 	30:	op
 * 	30-29:	opc	(op21, op54, immlo)
 * 	29:	s
 * 	23-22:	shift
 * 	20-16:	rm	(imm5, opc2)
 * 	9-5:	rn
 * 	4-0:	rd
 * 	3-0:	nzcv
 */

/*
 * A data processing instruction.
 */
typedef struct a64_dataproc {
	a64_dataproc_opcode_t opcode;	/* opcode */
	a64_cond_code_t cond;		/* condition code */
	a64_reg_t rd;			/* destination register */
	a64_reg_t rn;			/* first source register */
	a64_reg_t rm;			/* second source register */
	a64_reg_t ra;			/* third source register */
	uint8_t sfbit, opbit, sbit;	/* bits useful for determining opcode */
	union {				/* shifter values */
		a64_dataproc_shifter_imm_t dataproci_im;
		a64_dataproc_shifter_imm2_t dataproci_im2;
		a64_dataproc_shifter_xreg_t dataproci_si;
		a64_dataproc_shifter_sreg_t dataproci_ri;
	} dataproci_un;
} a64_dataproc_t;

/*
 * C3.4.1 Add/subtract (immediate)
 *
 * 31|30|29|28 24|23 22|21         10|9  5|4  0|
 * --+--+--+-----+-----+-------------+----+----+
 * sf|op| S|10001|shift|    imm12    | Rn | Rd |
 */
static void
a64_dis_dataproc_addsubimm(uint32_t in, a64_dataproc_t *dpi)
{
	uint16_t imm12;
	uint8_t shift;

	shift = (in & A64_DPI_SHIFT_MASK) >> A64_DPI_SHIFT_SHIFT;
	imm12 = (in & A64_DPI_IMM12_MASK) >> A64_DPI_IMM12_SHIFT;

	if (dpi->opbit == 0 && dpi->sbit == 0 && imm12 == 0 && shift == 0 &&
	    (dpi->rd.id == A64_REG_SP || dpi->rn.id == A64_REG_SP)) {
		dpi->opcode = DPI_OP_MOV_SP;
	} else if (dpi->sbit && dpi->rd.id == A64_REG_SP) {
		dpi->opcode = dpi->opbit ? DPI_OP_CMP_I : DPI_OP_CMN_I;
	} else if (dpi->sbit) {
		dpi->opcode = dpi->opbit ? DPI_OP_SUBS_I : DPI_OP_ADDS_I;
	} else {
		dpi->opcode = dpi->opbit ? DPI_OP_SUB_I : DPI_OP_ADD_I;
	}

	dpi->dpimm_imm = imm12;
	dpi->dpimm_shift = shift;
}

/*
 * C3.4.2 Bitfield
 *
 * 31|30 29|28  23|22|21  16|15  10|9  5|4  0|
 * --+-----+------+--+------+------+----+----+
 * sf| opc |100110| N| immr | imms | Rn | Rd |
 */
static void
a64_dis_dataproc_bitfield(uint32_t in, a64_dataproc_t *dpi)
{
	uint8_t opc, immr, imms, tmp;

	opc = (in & A64_DPI_OPC_MASK) >> A64_DPI_OPC_SHIFT;
	immr = (in & A64_DPI_IMMR_MASK) >> A64_DPI_IMMR_SHIFT;
	imms = (in & A64_DPI_IMMS_MASK) >> A64_DPI_IMMS_SHIFT;

	switch (opc) {
	case 0:
		if ((dpi->sfbit == 0 && imms == 31) ||
		    (dpi->sfbit == 1 && imms == 63)) {
			dpi->opcode = DPI_OP_ASR_I;
		} else if (imms < immr) {
			dpi->opcode = DPI_OP_SBFIZ;
		} else if (a64_dis_bfxpref(dpi->sfbit, (opc & 2), imms, immr)) {
			dpi->opcode = DPI_OP_SBFX;
		} else if (immr == 0 && imms == 7) {
			dpi->opcode = DPI_OP_SXTB;
		} else if (immr == 0 && imms == 15) {
			dpi->opcode = DPI_OP_SXTH;
		} else if (immr == 0 && imms == 31) {
			dpi->opcode = DPI_OP_SXTW;
		} else {
			dpi->opcode = DPI_OP_SBFM;
		}
		break;
	case 1:
		if (imms < immr)
			dpi->opcode = DPI_OP_BFI;
		else if (imms >= immr)
			dpi->opcode = DPI_OP_BFXIL;
		else
			dpi->opcode = DPI_OP_BFM;
		break;
	case 2:
		if ((!dpi->sfbit && imms != 31 && (imms + 1) == immr) ||
		    (dpi->sfbit && imms != 63 && (imms + 1) == immr)) {
			dpi->opcode = DPI_OP_LSL_I;
		} else if ((!dpi->sfbit && imms == 31) ||
		    (dpi->sfbit && imms == 63)) {
			dpi->opcode = DPI_OP_LSR_I;
		} else if (imms < immr) {
			dpi->opcode = DPI_OP_UBFIZ;
		} else if (a64_dis_bfxpref(dpi->sfbit, (opc & 2), imms, immr)) {
			dpi->opcode = DPI_OP_UBFX;
		} else if (immr == 0 && imms == 7) {
			dpi->opcode = DPI_OP_UXTB;
		} else if (immr == 0 && imms == 15) {
			dpi->opcode = DPI_OP_UXTH;
		} else {
			dpi->opcode = DPI_OP_UBFM;
		}
		break;
	}
	/* Set shifter type and values based on operand */
	/* XXX: abstract this further? */
	switch (dpi->opcode) {
	case DPI_OP_ASR_I:
	case DPI_OP_LSR_I:
		dpi->dpimm_imm = immr;
		break;
	case DPI_OP_LSL_I:
		tmp = dpi->sfbit ? 63 : 31;
		dpi->dpimm_imm = tmp - imms;
		break;
	case DPI_OP_SBFM:
	case DPI_OP_BFM:
	case DPI_OP_UBFM:
		dpi->imm2_imm1 = immr;
		dpi->imm2_imm2 = imms;
		break;
	case DPI_OP_SBFIZ:
	case DPI_OP_BFI:
	case DPI_OP_UBFIZ:
		tmp = dpi->sfbit ? 64 : 32;
		dpi->imm2_imm1 =
		    (tmp - immr) & (tmp - 1);
		dpi->imm2_imm2 = imms + 1;
		break;
	case DPI_OP_SBFX:
	case DPI_OP_BFXIL:
	case DPI_OP_UBFX:
		dpi->imm2_imm1 = immr;
		dpi->imm2_imm2 = imms + 1 - immr;
		break;
	default:
		break;
	}
}

/*
 * C3.4.3 Extract
 *
 * 31|30  29|28  23|22|21|20  16|15  10|9  5|4  0|
 * --+------+------+--+--+------+------+----+----+
 * sf| op21 |100111| N|o0|  Rm  | imms | Rn | Rd |
 *
 * C5.6.67  EXTR <Rd>, <Rn>, <Rm>, #<lsb>
 * C5.6.152 ROR  <Rd>, <Rs>,       #<shift>
 */
static void
a64_dis_dataproc_extract(uint32_t in, a64_dataproc_t *dpi)
{
	dpi->dpimm_imm = (in & A64_DPI_IMMS_MASK) >> A64_DPI_IMMS_SHIFT;
	dpi->opcode = (dpi->rn.id == dpi->rm.id) ? DPI_OP_ROR_I : DPI_OP_EXTR;
}

/*
 * C3.4.4 Logical (immediate)
 *
 * 31|30 29|28  23|22|21  16|15  10|9  5|4  0|
 * --+-----+------+--+------+------+----+----+
 * sf| opc |100100| N| immr | imms | Rn | Rd |
 */
static void
a64_dis_dataproc_logimm(uint32_t in, a64_dataproc_t *dpi)
{
	uint8_t opc, nbit, immr, imms;

	opc = (in & A64_DPI_OPC_MASK) >> A64_DPI_OPC_SHIFT;
	nbit = (in & A64_DPI_N22_MASK) >> A64_DPI_N22_SHIFT;
	immr = (in & A64_DPI_IMMR_MASK) >> A64_DPI_IMMR_SHIFT;
	imms = (in & A64_DPI_IMMS_MASK) >> A64_DPI_IMMS_SHIFT;

	switch (opc) {
	case 0:
		dpi->opcode = DPI_OP_AND_I;
		break;
	case 1:
		/*
		 * ARM says that the preferred disassembly is actually
		 * based on MoveWidePreferred(), but we follow gobjdump
		 * here and just compare SP, which also matches the
		 * decoding of TST below.
		 */
		dpi->opcode = (dpi->rn.id == A64_REG_SP)
		    ? DPI_OP_MOV_BI : DPI_OP_ORR_I;
		break;
	case 2:
		dpi->opcode = DPI_OP_EOR_I;
		break;
	case 3:
		dpi->opcode = (dpi->rd.id == A64_REG_ZR)
		    ? DPI_OP_TST_I : DPI_OP_ANDS_I;
		break;
	}
	dpi->dpimm_imm = a64_dis_decodebitmasks(dpi->sfbit, nbit, imms, immr);
}

/*
 * C3.4.5 Move wide (immediate)
 *
 * 31|30 29|28  23|22 21|20    5|4  0|
 * --+-----+------+-----+-------+----+
 * sf| opc |100101|  hw | imm16 | Rd |
 */
static void
a64_dis_dataproc_movimm(uint32_t in, a64_dataproc_t *dpi)
{
	uint16_t imm16;
	uint8_t opc, shift;

	opc = (in & A64_DPI_OPC_MASK) >> A64_DPI_OPC_SHIFT;
	shift = (in & A64_DPI_HW_MASK) >> A64_DPI_HW_SHIFT;
	imm16 = (in & A64_DPI_IMM16_MASK) >> A64_DPI_IMM16_SHIFT;

	switch (opc) {
	case 0:
		if (!(imm16 == 0 && shift != 0) &&
		    (dpi->sfbit || imm16 != 0xffff))
			dpi->opcode = DPI_OP_MOV_IWI;
		else
			dpi->opcode = DPI_OP_MOVN;
		break;
	case 2:
		if (!(imm16 == 0 && shift != 0))
			dpi->opcode = DPI_OP_MOV_WI;
		else
			dpi->opcode = DPI_OP_MOVZ;
		break;
	case 3:
		dpi->opcode = DPI_OP_MOVK;
		break;
	}

	/* XXX: merge these */
	if (dpi->opcode == DPI_OP_MOV_IWI) {
		dpi->dpimm_shift = shift;
		dpi->dpimm_imm = imm16;
		dpi->imm_inv = 1;
	} else if (dpi->opcode == DPI_OP_MOV_WI) {
		dpi->dpimm_shift = shift;
		dpi->dpimm_imm = imm16;
	} else {
		dpi->xreg_imm = imm16;
		dpi->xreg_type = shift;
	}
}

/*
 * C3.4.6 PC-rel. addressing
 *
 * 31|30 29|28 24|23            5|4  0|
 * --+-----+-----+---------------+----+
 * op|immlo|10000|     immhi     | Rd |
 *
 * Note that "op" is in a different place to all other instructions,
 * so we reuse "sfbit" instead (bit 31).
 */
static void
a64_dis_dataproc_pcrel(dis_handle_t *dhp, uint32_t in, a64_dataproc_t *dpi)
{
	uint32_t immhi, imm;
	uint8_t immlo;

	immhi = (in & A64_DPI_IMMHI_MASK) >> A64_DPI_IMMHI_SHIFT;
	immlo = (in & A64_DPI_IMMLO_MASK) >> A64_DPI_IMMLO_SHIFT;

	if (dpi->sfbit) {
		dis_handle_arm_64_t *dhx = dhp->dh_arch_private;

		dpi->opcode = DPI_OP_ADRP;
		imm = (immlo + (immhi << 2)) << 12;
		dpi->dpimm_imm = (int)imm;
		dpi->dpimm_imm += dhp->dh_addr & ~0xfff;

		/*
		 * Cache information in the disassembly handle so that it can
		 * be cross referenced with an upcoming 'add' in order to
		 * try and annotate it with a symbol label.
		 */
		dhx->dha_adrp_addr[dpi->rd.id] = dhp->dh_addr;
		dhx->dha_adrp_imm[dpi->rd.id] = dpi->dpimm_imm;
	} else {
		dpi->opcode = DPI_OP_ADR;
		imm = (immlo + (immhi << 2));
		dpi->dpimm_imm = (int)imm;
		dpi->dpimm_imm += dhp->dh_addr;
		/*
		 * Set the sfbit so that the register argument is interpreted
		 * as having 64-bit width. For adr/adrp, sfbit is used to
		 * indicate page (adrp) versus not (adr).
		 */
		dpi->sfbit = 1;
	}
}

/*
 * C3.5.1 Add/subtract (extended register)
 *
 * 31|30|29|28 24|23 22|21|20 16|15    13|12  10|9  5|4  0|
 * --+--+--+-----+-----+--+-----+--------+------+----+----+
 * sf|op| S|01011| opt | 1|  Rm | option | imm3 | Rn | Rd |
 */
static void
a64_dis_dataproc_addsubxreg(uint32_t in, a64_dataproc_t *dpi)
{
	if (dpi->sbit == 0)
		dpi->opcode = dpi->opbit ? DPI_OP_SUB_XR : DPI_OP_ADD_XR;
	else if (dpi->sbit && dpi->rd.id == A64_REG_SP)
		dpi->opcode = dpi->opbit ? DPI_OP_CMP_XR : DPI_OP_CMN_XR;
	else
		dpi->opcode = dpi->opbit ? DPI_OP_SUBS_XR : DPI_OP_ADDS_XR;

	dpi->xreg_imm = (in & A64_DPI_IMM3_MASK) >> A64_DPI_IMM3_SHIFT;
	dpi->xreg_type = (in & A64_DPI_OPTION_MASK) >> A64_DPI_OPTION_SHIFT;

	/* XXX: move this to later handling? */
	if (dpi->xreg_type != 3 && dpi->xreg_type != 7)
		dpi->rm.flags |= A64_REGFLAGS_32;
}

/*
 * C3.5.2 Add/subtract (shifted register)
 *
 * 31|30|29|28 24|23 22|21|20 16|15    13|12  10|9  5|4  0|
 * --+--+--+-----+-----+--+-----+--------+------+----+----+
 * sf|op| S|01011|shift| 0|  Rm |      imm6     | Rn | Rd |
 */
static void
a64_dis_dataproc_addsubsreg(uint32_t in, a64_dataproc_t *dpi)
{
	if (dpi->opbit && dpi->rn.id == A64_REG_SP)
		dpi->opcode = dpi->sbit ? DPI_OP_NEGS : DPI_OP_NEG;
	else if (dpi->sbit && dpi->rd.id == A64_REG_SP)
		dpi->opcode = dpi->opbit ? DPI_OP_CMP_SR : DPI_OP_CMN_SR;
	else if (dpi->sbit)
		dpi->opcode = dpi->opbit ? DPI_OP_SUBS_SR : DPI_OP_ADDS_SR;
	else
		dpi->opcode = dpi->opbit ? DPI_OP_SUB_SR : DPI_OP_ADD_SR;

	dpi->sreg_imm = (in & A64_DPI_IMM6_MASK) >> A64_DPI_IMM6_SHIFT;
	dpi->sreg_type = (in & A64_DPI_SHIFT_MASK) >> A64_DPI_SHIFT_SHIFT;
}

/*
 * C3.5.3 Add/subtract (with carry)
 *
 * 31|30|29|28    21|20  16|15     10|9  5|4  0|
 * --+--+--+--------+------+---------+----+----+
 * sf|op| S|11010000|  Rm  | opcode2 | Rn | Rd |
 */
static void
a64_dis_dataproc_addsubcarry(uint32_t in, a64_dataproc_t *dpi)
{
	uint8_t opc;

	opc = (in & A64_DPI_OPC_MASK) >> A64_DPI_OPC_SHIFT;

	switch (opc) {
	case 0:
		dpi->opcode = DPI_OP_ADC;
		break;
	case 1:
		dpi->opcode = DPI_OP_ADCS;
		break;
	case 2:
		dpi->opcode = (dpi->rn.id == A64_REG_ZR)
		    ? DPI_OP_NGC : DPI_OP_SBC;
		break;
	case 3:
		dpi->opcode = (dpi->rn.id == A64_REG_ZR)
		    ? DPI_OP_NGCS : DPI_OP_SBCS;
		break;
	}
}

/*
 * These two are small enough to handle together.
 *
 * C3.5.4 Conditional compare (immediate)
 * C3.5.5 Conditional compare (register)
 *
 * 31|30|29|28    21|20  16|15  12|11|10|9  5| 4|3    0|
 * --+--+--+--------+------+------+--+--+----+--+------+
 * sf|op| S|11010010| imm5 | cond | 1|o2| Rn |o3| nzcv |
 * sf|op| S|11010010|  Rm  | cond | 0|o2| Rn |o3| nzcv |
 */
static void
a64_dis_dataproc_condcomp(uint32_t in, a64_dataproc_t *dpi)
{
	if (in & A64_BIT_11_MASK)
		dpi->opcode = dpi->opbit ? DPI_OP_CCMP_I : DPI_OP_CCMN_I;
	else
		dpi->opcode = dpi->opbit ? DPI_OP_CCMP_R : DPI_OP_CCMN_R;
}

/*
 * C3.5.6 Conditional select
 *
 * 31|30|29|28    21|20  16|15  12|11 10|9  5|4  0|
 * --+--+--+--------+------+------+-----+----+----+
 * sf|op| S|11010100|  Rm  | cond | op2 | Rn | Rd |
 */
static void
a64_dis_dataproc_condsel(uint32_t in, a64_dataproc_t *dpi)
{
	uint8_t alias, op2, tmp;

	op2 = (in & A64_DPI_OP2_MASK) >> A64_DPI_OP2_SHIFT;
	tmp = op2 + (dpi->opbit << 1);

	/* XXX: revisit this and name better */
	/* One of the common alias conditions. */
	alias = ((dpi->cond & 0xe) != 0xe);

	switch (tmp) {
	case 0:
		dpi->opcode = DPI_OP_CSEL;
		break;
	case 1:
		if (dpi->rn.id == dpi->rm.id && dpi->rn.id != A64_REG_ZR &&
		    alias) {
			dpi->opcode = DPI_OP_CINC;
		} else if (dpi->rn.id == dpi->rm.id &&
		    dpi->rn.id == A64_REG_ZR && alias) {
			dpi->opcode = DPI_OP_CSET;
		} else {
			dpi->opcode = DPI_OP_CSINC;
		}
		break;
	case 2:
		if (dpi->rn.id == dpi->rm.id && dpi->rn.id != A64_REG_ZR &&
		    alias) {
			dpi->opcode = DPI_OP_CINV;
		} else if (dpi->rn.id == dpi->rm.id &&
		    dpi->rn.id == A64_REG_ZR && alias) {
			dpi->opcode = DPI_OP_CSETM;
		} else {
			dpi->opcode = DPI_OP_CSINV;
		}
		break;
	case 3:
		if (dpi->rn.id == dpi->rm.id && alias)
			dpi->opcode = DPI_OP_CNEG;
		else
			dpi->opcode = DPI_OP_CSNEG;
		break;
	}
}

/*
 * C3.5.7 Data-processing (1 source)
 *
 * 31|30|29|28    21|20     16|15    10|9  5|4  0|
 * --+--+--+--------+---------+--------+----+----+
 * sf| 1| S|11010110| opcode2 | opcode | Rn | Rd |
 */
static void
a64_dis_dataproc_dp1(uint32_t in, a64_dataproc_t *dpi)
{
	uint8_t tmp;

	tmp = (in & A64_DPI_OPCODE_MASK) >> A64_DPI_OPCODE_SHIFT;
	switch (tmp) {
	case 0:
		dpi->opcode = DPI_OP_RBIT;
		break;
	case 1:
		dpi->opcode = DPI_OP_REV16;
		break;
	case 2:
		dpi->opcode = dpi->sfbit ? DPI_OP_REV32 : DPI_OP_REV;
		break;
	case 3:
		if (dpi->sfbit)
			dpi->opcode = DPI_OP_REV;
		break;
	case 4:
		dpi->opcode = DPI_OP_CLZ;
		break;
	case 5:
		dpi->opcode = DPI_OP_CLS;
		break;
	}
}

/*
 * C3.5.8 Data-processing (2 source)
 *
 * 31|30|29|28    21|20 16|15    10|9  5|4  0|
 * --+--+--+--------+-----+--------+----+----+
 * sf| 0| S|11010110|  Rm | opcode | Rn | Rd |
 */
static void
a64_dis_dataproc_dp2(uint32_t in, a64_dataproc_t *dpi)
{
	uint8_t tmp;

	tmp = (in & A64_DPI_OPCODE_MASK) >> A64_DPI_OPCODE_SHIFT;

	switch (tmp) {
	case 2:
		dpi->opcode = DPI_OP_UDIV;
		break;
	case 3:
		dpi->opcode = DPI_OP_SDIV;
		break;
	case 8:
		dpi->opcode = DPI_OP_LSL_R;
		break;
	case 9:
		dpi->opcode = DPI_OP_LSR_R;
		break;
	case 10:
		dpi->opcode = DPI_OP_ASR_R;
		break;
	case 11:
		dpi->opcode = DPI_OP_ROR_R;
		break;
	case 16:
		if (dpi->sfbit == 0)
			dpi->opcode = DPI_OP_CRC32B;
		break;
	case 17:
		if (dpi->sfbit == 0)
			dpi->opcode = DPI_OP_CRC32H;
		break;
	case 18:
		if (dpi->sfbit == 0)
			dpi->opcode = DPI_OP_CRC32W;
		break;
	case 19:
		if (dpi->sfbit)
			dpi->opcode = DPI_OP_CRC32X;
		break;
	case 20:
		if (dpi->sfbit == 0)
			dpi->opcode = DPI_OP_CRC32CB;
		break;
	case 21:
		if (dpi->sfbit == 0)
			dpi->opcode = DPI_OP_CRC32CH;
		break;
	case 22:
		if (dpi->sfbit == 0)
			dpi->opcode = DPI_OP_CRC32CW;
		break;
	case 23:
		if (dpi->sfbit)
			dpi->opcode = DPI_OP_CRC32CX;
		break;
	}
}

/*
 * C3.5.9 Data-processing (3 source)
 *
 * 31|30  29|28 24|23  21|20  16|15|14  10|9  5|4  0|
 * --+------+-----+------+------+--+------+----+----+
 * sf| op54 |11011| op31 |  Rm  |o0|  Ra  | Rn | Rd |
 */
static void
a64_dis_dataproc_dp3(uint32_t in, a64_dataproc_t *dpi)
{
	uint8_t o0, op31, tmp;

	o0 = (in & A64_DPI_O0_MASK) >> A64_DPI_O0_SHIFT;
	op31 = (in & A64_DPI_OP31_MASK) >> A64_DPI_OP31_SHIFT;
	tmp = o0 + (op31 << 1);

	switch (tmp) {
	case 0:
		dpi->opcode = (dpi->ra.id == A64_REG_ZR)
		    ? DPI_OP_MUL : DPI_OP_MADD;
		break;
	case 1:
		dpi->opcode = (dpi->ra.id == A64_REG_ZR)
		    ? DPI_OP_MNEG : DPI_OP_MSUB;
		break;
	case 2:
		if (dpi->sfbit)
			dpi->opcode = (dpi->ra.id == A64_REG_ZR)
			    ? DPI_OP_SMULL : DPI_OP_SMADDL;
		break;
	case 3:
		if (dpi->sfbit)
			dpi->opcode = (dpi->ra.id == A64_REG_ZR)
			    ? DPI_OP_SMNEGL : DPI_OP_SMSUBL;
		break;
	case 4:
		if (dpi->sfbit)
			dpi->opcode = DPI_OP_SMULH;
		break;
	case 10:
		if (dpi->sfbit)
			dpi->opcode = (dpi->ra.id == A64_REG_ZR)
			    ? DPI_OP_UMULL : DPI_OP_UMADDL;
		break;
	case 11:
		if (dpi->sfbit)
			dpi->opcode = (dpi->ra.id == A64_REG_ZR)
			    ? DPI_OP_UMNEGL : DPI_OP_UMSUBL;
		break;
	case 12:
		if (dpi->sfbit)
			dpi->opcode = DPI_OP_UMULH;
		break;
	}
}

/*
 * C3.5.10 Logical (shifted register)
 *
 * 31|30 29|28 24|23 22|21|20 16|15  10|9  5|4  0|
 * --+-----+-----+-----+--+-----+------+----+----+
 * sf| opc |01010|shift| N|  Rm | imm6 | Rn | Rd |
 */
static void
a64_dis_dataproc_logsreg(uint32_t in, a64_dataproc_t *dpi)
{
	uint8_t imm6, nbit, opc, shift, tmp;

	opc = (in & A64_DPI_OPC_MASK) >> A64_DPI_OPC_SHIFT;
	shift = (in & A64_DPI_SHIFT_MASK) >> A64_DPI_SHIFT_SHIFT;
	nbit = (in & A64_DPI_N21_MASK) >> A64_DPI_N21_SHIFT;
	imm6 = (in & A64_DPI_IMM6_MASK) >> A64_DPI_IMM6_SHIFT;
	tmp = nbit + (opc << 1);

	switch (tmp) {
	case 0:
		dpi->opcode = DPI_OP_AND_SR;
		break;
	case 1:
		dpi->opcode = DPI_OP_BIC_SR;
		break;
	case 2:
		/* XXX: gobjdump only compares rn == ZR */
		dpi->opcode = (dpi->rn.id == A64_REG_ZR && shift == 0 &&
		    imm6 == 0) ? DPI_OP_MOV_R : DPI_OP_ORR_SR;
		break;
	case 3:
		dpi->opcode = (dpi->rn.id == A64_REG_ZR)
		    ? DPI_OP_MVN : DPI_OP_ORN_SR;
		break;
	case 4:
		dpi->opcode = DPI_OP_EOR_SR;
		break;
	case 5:
		dpi->opcode = DPI_OP_EON_SR;
		break;
	case 6:
		dpi->opcode = (dpi->rd.id == A64_REG_ZR)
		    ? DPI_OP_TST_SR : DPI_OP_ANDS_SR;
		break;
	case 7:
		dpi->opcode = DPI_OP_BICS_SR;
		break;
	}

	if (dpi->opcode != DPI_OP_MOV_R) {
		dpi->sreg_imm = imm6;
		dpi->sreg_type = shift;
	}
}

static void
a64_dis_addlabel(dis_handle_t *dhp, uint64_t addr, char *buf, size_t buflen)
{
	size_t len = 0;

	if (dhp->dh_lookup(dhp->dh_data, addr, NULL, 0, NULL, NULL) != 0)
		return;

	len = dis_snprintf(buf, buflen, "\t<");
	if (len >= buflen)
		return;

	dhp->dh_lookup(dhp->dh_data, addr, buf + len, buflen - len, NULL, NULL);
	(void) strlcat(buf, ">", buflen);
}

static int
a64_dis_dataproc(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen)
{
	a64_dataproc_opcode_entry_t *dop;
	a64_dataproc_t dpi;
	const char *opname;
	uint64_t imm;
	size_t len;

	bzero(&dpi, sizeof (dpi));

	/*
	 * Extract certain sections which are common across multiple classes
	 * and are required for determining the opcode.
	 */
	dpi.sfbit = (in & A64_DPI_SF_MASK) >> A64_DPI_SF_SHIFT;
	dpi.opbit = (in & A64_DPI_OP_MASK) >> A64_DPI_OP_SHIFT;
	dpi.sbit = (in & A64_DPI_S_MASK) >> A64_DPI_S_SHIFT;
	dpi.cond = (in & A64_DPI_COND_MASK) >> A64_DPI_COND_SHIFT;
	dpi.rd.id = (in & A64_DPI_RD_MASK) >> A64_DPI_RD_SHIFT;
	dpi.rm.id = (in & A64_DPI_RM_MASK) >> A64_DPI_RM_SHIFT;
	dpi.rn.id = (in & A64_DPI_RN_MASK) >> A64_DPI_RN_SHIFT;
	dpi.ra.id = (in & A64_DPI_RA_MASK) >> A64_DPI_RA_SHIFT;

	if ((in & A64_DPI_ADSBIMM_MASK) == A64_DPI_ADSBIMM_TARG)
		a64_dis_dataproc_addsubimm(in, &dpi);

	else if ((in & A64_DPI_BITFIELD_MASK) == A64_DPI_BITFIELD_TARG)
		a64_dis_dataproc_bitfield(in, &dpi);

	else if ((in & A64_DPI_EXTRACT_MASK) == A64_DPI_EXTRACT_TARG)
		a64_dis_dataproc_extract(in, &dpi);

	else if ((in & A64_DPI_LOGIMM_MASK) == A64_DPI_LOGIMM_TARG)
		a64_dis_dataproc_logimm(in, &dpi);

	else if ((in & A64_DPI_MOVIMM_MASK) == A64_DPI_MOVIMM_TARG)
		a64_dis_dataproc_movimm(in, &dpi);

	else if ((in & A64_DPI_PCREL_MASK) == A64_DPI_PCREL_TARG)
		a64_dis_dataproc_pcrel(dhp, in, &dpi);

	else if ((in & A64_DPI_ADSBXREG_MASK) == A64_DPI_ADSBXREG_TARG)
		a64_dis_dataproc_addsubxreg(in, &dpi);

	else if ((in & A64_DPI_ADSBSREG_MASK) == A64_DPI_ADSBSREG_TARG)
		a64_dis_dataproc_addsubsreg(in, &dpi);

	else if ((in & A64_DPI_ADSBCARRY_MASK) == A64_DPI_ADSBCARRY_TARG)
		a64_dis_dataproc_addsubcarry(in, &dpi);

	else if ((in & A64_DPI_CONDCOMP_MASK) == A64_DPI_CONDCOMP_TARG)
		a64_dis_dataproc_condcomp(in, &dpi);

	else if ((in & A64_DPI_CONDSEL_MASK) == A64_DPI_CONDSEL_TARG)
		a64_dis_dataproc_condsel(in, &dpi);

	else if ((in & A64_DPI_DP1_MASK) == A64_DPI_DP1_TARG)
		a64_dis_dataproc_dp1(in, &dpi);

	else if ((in & A64_DPI_DP2_MASK) == A64_DPI_DP2_TARG)
		a64_dis_dataproc_dp2(in, &dpi);

	else if ((in & A64_DPI_DP3_MASK) == A64_DPI_DP3_TARG)
		a64_dis_dataproc_dp3(in, &dpi);

	else if ((in & A64_DPI_LOGSREG_MASK) == A64_DPI_LOGSREG_TARG)
		a64_dis_dataproc_logsreg(in, &dpi);

	else
		return (-1);

	/*
	 * Now that we have the operand we can set up the output variables
	 * based on its a64_dataproc_opcodes entry.
	 */
	dop = &a64_dataproc_opcodes[dpi.opcode];
	opname = dop->name;

	/*
	 * C5.6.199 SUBS (shifted register)
	 * CMP is preferred when Rd == '11111'
	 */
	if ((dop->regs & DPI_REGS_d) && dpi.rd.id == 0x1f &&
	    strcmp(opname, "negs") == 0) {
		opname = "cmp";
	}

	/*
	 * Determine ZR|SP and width for each valid register.  In order of
	 * precedence the options for specifying the register width are:
	 *
	 *   DPI_WREGS_*:	Use W<reg>.
	 *   DPI_WDREGS_*:	Use W<reg> if A64_REGFLAGS_32 is set.
	 *   Default:		Use [WX]<reg> based on the SF bit.
	 */
	if (dop->regs & DPI_REGS_d) {
		if (dop->flags & DPI_SPREGS_d)
			dpi.rd.flags |= A64_REGFLAGS_SP;
		if (dop->flags & DPI_WREGS_d ||
		    (dop->flags & DPI_WDREGS_d &&
		    dpi.rd.flags & A64_REGFLAGS_32)) {
			dpi.rd.width = A64_REGWIDTH_32;
		} else {
			dpi.rd.width = (dpi.sfbit) ?
			    A64_REGWIDTH_64 : A64_REGWIDTH_32;
		}
	}
	if (dop->regs & DPI_REGS_n) {
		if (dop->flags & DPI_SPREGS_n)
			dpi.rn.flags |= A64_REGFLAGS_SP;
		if (dop->flags & DPI_WREGS_n ||
		    (dop->flags & DPI_WDREGS_n &&
		    dpi.rn.flags & A64_REGFLAGS_32)) {
			dpi.rn.width = A64_REGWIDTH_32;
		} else {
			dpi.rn.width = (dpi.sfbit) ?
			    A64_REGWIDTH_64 : A64_REGWIDTH_32;
		}
	}
	if (dop->regs & DPI_REGS_m) {
		if (dop->flags & DPI_SPREGS_m)
			dpi.rm.flags |= A64_REGFLAGS_SP;
		if (dop->flags & DPI_WREGS_m ||
		    (dop->flags & DPI_WDREGS_m &&
		    dpi.rm.flags & A64_REGFLAGS_32)) {
			dpi.rm.width = A64_REGWIDTH_32;
		} else {
			dpi.rm.width = (dpi.sfbit) ?
			    A64_REGWIDTH_64 : A64_REGWIDTH_32;
		}
	}
	if (dop->regs & DPI_REGS_a) {
		if (dop->flags & DPI_SPREGS_a)
			dpi.ra.flags |= A64_REGFLAGS_SP;
		if (dop->flags & DPI_WREGS_a ||
		    (dop->flags & DPI_WDREGS_a &&
		    dpi.ra.flags & A64_REGFLAGS_32)) {
			dpi.ra.width = A64_REGWIDTH_32;
		} else {
			dpi.ra.width = (dpi.sfbit) ?
			    A64_REGWIDTH_64 : A64_REGWIDTH_32;
		}
	}

	/*
	 * Print opcode and register arguments.
	 */
	if (dop->regs == DPI_REGS_d) {
		len = dis_snprintf(buf, buflen, "%s %s", opname,
		    a64_reg_name(dpi.rd));
	} else if (dop->regs == DPI_REGS_dn) {
		len = dis_snprintf(buf, buflen, "%s %s, %s", opname,
		    a64_reg_name(dpi.rd),
		    a64_reg_name(dpi.rn));
	} else if (dop->regs == DPI_REGS_dnm) {
		len = dis_snprintf(buf, buflen, "%s %s, %s, %s", opname,
		    a64_reg_name(dpi.rd),
		    a64_reg_name(dpi.rn),
		    a64_reg_name(dpi.rm));
	} else if (dop->regs == DPI_REGS_dnma) {
		len = dis_snprintf(buf, buflen, "%s %s, %s, %s, %s", opname,
		    a64_reg_name(dpi.rd),
		    a64_reg_name(dpi.rn),
		    a64_reg_name(dpi.rm),
		    a64_reg_name(dpi.ra));
	} else if (dop->regs == DPI_REGS_dm) {
		len = dis_snprintf(buf, buflen, "%s %s, %s", opname,
		    a64_reg_name(dpi.rd),
		    a64_reg_name(dpi.rm));
	} else if (dop->regs == DPI_REGS_n) {
		len = dis_snprintf(buf, buflen, "%s %s", opname,
		    a64_reg_name(dpi.rn));
	} else if (dop->regs == DPI_REGS_nm) {
		len = dis_snprintf(buf, buflen, "%s %s, %s", opname,
		    a64_reg_name(dpi.rn),
		    a64_reg_name(dpi.rm));
	}

	if (len >= buflen)
		return (-1);
	buflen -= len;
	buf += len;

	/*
	 * Print any additional operands based on the optype.
	 */
	switch (dop->optype) {
	case DPI_OPTYPE_IMM:
		/* #<imm> */
		len = dis_snprintf(buf, buflen, ", #%" PRIu64, dpi.dpimm_imm);
		break;
	case DPI_OPTYPE_IMMX:
		/* 0x<imm> */
		len = dis_snprintf(buf, buflen, ", 0x%" PRIx64, dpi.dpimm_imm);
		break;
	case DPI_OPTYPE_IMM2:
		/* #<immr>, #<imms> */
		len = dis_snprintf(buf, buflen, ", #%" PRIu32 ", #%" PRIu32,
		    dpi.imm2_imm1,
		    dpi.imm2_imm2);
		break;
	case DPI_OPTYPE_IMMS:
		imm = dpi.dpimm_imm;
		switch (dpi.dpimm_shift) {
		/* XXX: use DPI_OPTYPE_SIMM and calculate */
		case 0:
			len = dis_snprintf(buf, buflen, ", #0x%" PRIx64, imm);
			break;
		case 1:
			len = dis_snprintf(buf, buflen,
			    ", #0x%" PRIx64 ", lsl #12", imm);
			break;
		}
		if ((dop->flags & DPI_ADRP_F) != 0 &&
		    dpi.rd.width == A64_REGWIDTH_64) {
			dis_handle_arm_64_t *dhx = dhp->dh_arch_private;
			uint_t i = dpi.rd.id;

			/*
			 * If we recently (in the last 10 instructions - this
			 * is pretty arbitrary) saw an `adrp` on this register,
			 * then assume that the result after this `add` is to
			 * set up an address of a symbol and try to print the
			 * label.
			 */
			if (dhx->dha_adrp_imm[i] != 0 &&
			    dhp->dh_addr > dhx->dha_adrp_addr[i] &&
			    dhp->dh_addr - dhx->dha_adrp_addr[i] <
			    sizeof (uint32_t) * 10) {
				a64_dis_addlabel(dhp,
				    dpi.dpimm_imm + dhx->dha_adrp_imm[i],
				    buf + len, buflen - len);
			}
			dhx->dha_adrp_imm[i] = 0;
		}

		break;
	case DPI_OPTYPE_IMM_MOV:
		imm = dpi.dpimm_imm;
		imm <<= (dpi.dpimm_shift * 16);
		if (dpi.imm_inv)
			imm = ~imm;
		if (dpi.sfbit) {
			len = dis_snprintf(buf, buflen, ", #0x%" PRIx64,
			    imm);
		} else {
			len = dis_snprintf(buf, buflen, ", #0x%" PRIx32,
			    (uint32_t)imm);
		}
		break;
	case DPI_OPTYPE_SIMM:
		if (dpi.xreg_type) {
			len = dis_snprintf(buf, buflen, ", #0x%x, lsl #%d",
			    dpi.xreg_imm,
			    dpi.xreg_type * 16);
		} else {
			len = dis_snprintf(buf, buflen, ", #0x%x",
			    dpi.xreg_imm);
		}
		break;
	/* {, <shift> #<amount>} */
	case DPI_OPTYPE_SREG:
		if (!(dpi.sreg_imm == 0 &&
		    dpi.sreg_type == DPI_S_LSL)) {
			len = dis_snprintf(buf, buflen, ", %s #%d",
			    a64_dataproc_shifts[dpi.sreg_type],
			    dpi.sreg_imm);
		}
		break;
	/* {, <extend> {#<amount>}} */
	case DPI_OPTYPE_XREG:
		/* Use the LSL form under certain circumstances. */
		if (((!dpi.sfbit && dpi.xreg_type == DPI_E_UXTW) ||
		    (dpi.sfbit && dpi.xreg_type == DPI_E_UXTX)) &&
		    ((!dpi.sbit && (dpi.rn.id == A64_REG_SP ||
		    dpi.rd.id == A64_REG_SP)) ||
		    (dpi.sbit && dpi.rn.id == A64_REG_SP))) {
			dpi.xreg_type = DPI_E_LSL;
		}

		/* The default lsl #0 can be elided. */
		if (dpi.xreg_imm == 0) {
			if (dpi.xreg_type != DPI_E_LSL) {
				len = dis_snprintf(buf, buflen, ", %s",
				    a64_dataproc_extends[dpi.xreg_type]);
			}
		} else {
			len = dis_snprintf(buf, buflen, ", %s #%d",
			    a64_dataproc_extends[dpi.xreg_type],
			    dpi.xreg_imm);
		}
		break;
	case DPI_OPTYPE_COND:
		len = dis_snprintf(buf, buflen, ", %s",
		    a64_cond_names[dpi.cond]);
		break;
	case DPI_OPTYPE_COND_INV:
		len = dis_snprintf(buf, buflen, ", %s",
		    a64_cond_names[dpi.cond ^ 0x1]);
		break;
	case DPI_OPTYPE_COND_IMM:
		/* #<imm>, #<nzcv>, <cond> */
		/* XXX: should probably not re-use rm here */
		len = dis_snprintf(buf, buflen, ", #0x%x, #0x%x, %s", dpi.rm.id,
		    (in & A64_DPI_NZCV_MASK),
		    a64_cond_names[dpi.cond]);
		break;
	case DPI_OPTYPE_COND_REG:
		/* #<nzcv>, <cond> */
		len = dis_snprintf(buf, buflen, ", #0x%x, %s",
		    (in & A64_DPI_NZCV_MASK),
		    a64_cond_names[dpi.cond]);
		break;
	case DPI_OPTYPE_LABEL:
		len = dis_snprintf(buf, buflen, ", 0x%" PRIx64,
		    dpi.dpimm_imm);
		if (len >= buflen)
			break;
		a64_dis_addlabel(dhp, dpi.dpimm_imm, buf + len, buflen - len);
	default:
		break;
	}

	return (len < buflen ? 0 : -1);
}

/*
 * Load/store
 */

#define	LDSTR_SIZE_MASK		0xc0000000
#define	LDSTR_SIZE_SHIFT	30
#define	LDSTR_CLASS_MASK	0x38000000
#define	LDSTR_CLASS_SHIFT	27
#define	LDSTR_V_MASK		0x04000000
#define	LDSTR_V_SHIFT		26
#define	LDSTR_O2_MASK		0x00800000
#define	LDSTR_O2_SHIFT		23
#define	LDSTR_L_MASK		0x00400000
#define	LDSTR_L_SHIFT		22
#define	LDSTR_O1_MASK		0x00200000
#define	LDSTR_O1_SHIFT		21
#define	LDSTR_OPC_MASK		0x00c00000
#define	LDSTR_OPC_SHIFT		22
#define	LDSTR_IMM19_MASK	0x00ffffe0
#define	LDSTR_IMM19_SHIFT	5
#define	LDSTR_IMM12_MASK	0x003ffc00
#define	LDSTR_IMM12_SHIFT	10
#define	LDSTR_O0_MASK		0x00008000
#define	LDSTR_O0_SHIFT		15
#define	LDSTR_IMM7_MASK		0x003f8000
#define	LDSTR_IMM7_SHIFT	15
#define	LDSTR_IMM9_MASK		0x001ff000
#define	LDSTR_IMM9_SHIFT	12
#define	LDSTR_S_MASK		0x00001000
#define	LDSTR_S_SHIFT		12
#define	LDSTR_OPTION_MASK	0x0000e000
#define	LDSTR_OPTION_SHIFT	13
#define	LDSTR_RM_MASK		0x001f0000
#define	LDSTR_RM_SHIFT		16
#define	LDSTR_RT2_MASK		0x00007c00
#define	LDSTR_RT2_SHIFT		10
#define	LDSTR_RN_MASK		0x000003e0
#define	LDSTR_RN_SHIFT		5
#define	LDSTR_RT_MASK		0x0000001f
#define	LDSTR_RT_SHIFT		0

#define	LDSTR_IMM19_SIGN_MASK	0x00040000
#define	LDSTR_IMM19_POS_SIGN	0x0002ffff
#define	LDSTR_IMM19_NEG_SIGN	0xfffd0000
#define	LDSTR_IMM12_SIGN_MASK	0x00000800
#define	LDSTR_IMM12_POS_SIGN	0x000007ff
#define	LDSTR_IMM12_NEG_SIGN	0xfffff700
#define	LDSTR_IMM9_SIGN_MASK	0x00000100
#define	LDSTR_IMM9_POS_SIGN	0x000001ff
#define	LDSTR_IMM9_NEG_SIGN	0xfffffe00
#define	LDSTR_IMM7_SIGN_MASK	0x00000040
#define	LDSTR_IMM7_POS_SIGN	0x0000003f
#define	LDSTR_IMM7_NEG_SIGN	0xffffffc0

/* XXX: prfm not valid for 8/9, only 10/13 */
typedef struct a64_ldstr_opcode_entry {
	uint32_t targ;
	/* C3.3.5 Load register (literal) */
	const char *name_literal;
	/* C3.3.6 Load/store exclusive */
	const char *name_exclusive;
	/* C3.3.7 Load/store no-allocate pair (offset) */
	const char *name_noalloc;
	/* C3.3.8 Load/store register (immediate post-indexed) */
	/* C3.3.9 Load/store register (immediate pre-indexed) */
	/* C3.3.10 Load/store register (register offset) */
	/* C3.3.13 Load/store register (unsigned immediate) */
	const char *name_reg;
	/* C3.3.11 Load/store register (unprivileged) */
	const char *name_unpriv;
	/* C3.3.12 Load/store register (unscaled immediate) */
	const char *name_unscaled;
	/* C3.3.14 Load/store register pair (offset) */
	/* C3.3.15 Load/store register pair (post-indexed) */
	/* C3.3.16 Load/store register pair (pre-indexed) */
	const char *name_regpair;
	a64_reg_flags_t regflags;
} a64_ldstr_opcode_entry_t;

/* BEGIN CSTYLED */
/*
 * 31  30|29   27| 26|25 24|23                                    5|4  0| Reg (lit)
 * ------+-------+---+-----+---------------------------------------+----+
 *   opc | 0 1 1 | V | 0 0 |                 imm19                 | Rt | C3.3.5
 * ------+-------+---+-----+---------------------------------------+----+
 * 31  30|29             24|23|22| 21|20  16|15|14          10|9  5|4  0| Exclusive
 * ------+-----------------+--+--+---+------+--+--------------+----+----+
 *  size | 0 0 1   0   0 0 |o2| L| o1|   Rs |o0|      Rt2     | Rn | Rt | C3.3.6
 * ------+-----------------+--+--+---+------+--+--------------+----+----+
 * 31  30|29   27| 26|25 24|23 22| 21|20  16|15    13|12|11 10|9  5|4  0| Reg
 * ------+-------+---+-----+-----+---+------+--------+--+-----+----+----+
 *  size | 1 1 1 | V | 0 0 | opc | 0 |       imm9       | 0 1 | Rn | Rt | C3.3.8
 *  size | 1 1 1 | V | 0 0 | opc | 0 |       imm9       | 1 1 | Rn | Rt | C3.3.9
 *  size | 1 1 1 | V | 0 0 | opc | 1 |  Rm  | option | S| 1 0 | Rn | Rt | C3.3.10
 *  size | 1 1 1 | V | 0 0 | opc | 0 |       imm9       | 1 0 | Rn | Rt | C3.3.11
 *  size | 1 1 1 | V | 0 0 | opc | 0 |       imm9       | 0 0 | Rn | Rt | C3.3.12
 *  size | 1 1 1 | V | 0 1 | opc |            imm12           | Rn | Rt | C3.3.13
 * ------+-------+---+-----+-+---+---+------+-+------+--+-----+----+----+
 * 31  30|29   27| 26|25   23| 22|21         15|14          10|9  5|4  0| Reg pair
 * ------+-------+---+-------+---+-------------+--------------+----+----+
 *   opc | 1 0 1 | V | 0 0 0 | L |     imm7    |      Rt2     | Rn | Rt | C3.3.7
 *   opc | 1 0 1 | V | 0 1 0 | L |     imm7    |      Rt2     | Rn | Rt | C3.3.14
 *   opc | 1 0 1 | V | 0 0 1 | L |     imm7    |      Rt2     | Rn | Rt | C3.3.15
 *   opc | 1 0 1 | V | 0 1 1 | L |     imm7    |      Rt2     | Rn | Rt | C3.3.16
 *
 *    opc  v
 *  31-30 26			3
 *
 *    opc  v     l
 *  31-30 26    22		4
 *
 *   size  v  opc
 *  31-30 26 23-22		5
 *
 *   size    o2  l o1 o0
 *  31-30    23 22 21 15	6	mask = 0xc0e08000
 */
/* END CSTYLED */

/* BEGIN CSTYLED */
static a64_ldstr_opcode_entry_t a64_ldstr_opcodes[] = {
	/* c4e08000	excl	noalloc	  reg	  unpriv    unscale   regpair */
	{0x00000000,   "ldr",  "stxrb", "stnp",  "strb",  "sttrb",  "sturb",   "stp", 0},
	{0x00008000,    NULL, "stlxrb",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x00400000,    NULL,  "ldxrb", "ldnp",  "ldrb",  "ldtrb",  "ldurb",   "ldp", 0},
	{0x00408000,    NULL, "ldaxrb",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x00800000,    NULL,     NULL,   NULL, "ldrsb", "ldtrsb", "ldursb",    NULL, 0},
	{0x00808000,    NULL,  "stlrb",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x00c00000,    NULL,     NULL,   NULL, "ldrsb", "ldtrsb", "ldursb",    NULL, 0},
	{0x00c08000,    NULL,  "ldarb",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x04000000,   "ldr",     NULL, "stnp",   "str",     NULL,   "stur",   "stp", A64_REGFLAGS_SIMD},
	{0x04400000,    NULL,     NULL, "ldnp",   "ldr",     NULL,   "ldur",   "ldp", A64_REGFLAGS_SIMD},
	{0x04800000,    NULL,     NULL,   NULL,   "str",     NULL,   "stur",    NULL, A64_REGFLAGS_SIMD},
	{0x04c00000,    NULL,     NULL,   NULL,   "ldr",     NULL,   "ldur",    NULL, A64_REGFLAGS_SIMD},
	{0x40000000,   "ldr",  "stxrh",   NULL,  "strh",  "sttrh",  "sturh",    NULL, 0},
	{0x40008000,    NULL, "stlxrh",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x40400000,    NULL,  "ldxrh",   NULL,  "ldrh",  "ldtrh",  "ldurh", "ldpsw", 0},
	{0x40408000,    NULL, "ldaxrh",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x40800000,    NULL,     NULL,   NULL, "ldrsh", "ldtrsh", "ldursh",    NULL, 0},
	{0x40808000,    NULL,  "stlrh",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x40c00000,    NULL,     NULL,   NULL, "ldrsh", "ldtrsh", "ldursh",    NULL, 0},
	{0x40c08000,    NULL,  "ldarh",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x44000000,   "ldr",     NULL, "stnp",   "str",     NULL,   "stur",   "stp", A64_REGFLAGS_SIMD},
	{0x44400000,    NULL,     NULL, "ldnp",   "ldr",     NULL,   "ldur",   "ldp", A64_REGFLAGS_SIMD},
	{0x80000000, "ldrsw",   "stxr", "stnp",   "str",   "sttr",   "stur",   "stp", 0},
	{0x80008000,    NULL,  "stlxr",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x80200000,    NULL,   "stxp",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x80208000,    NULL,  "stlxp",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x80400000,    NULL,   "ldxr", "ldnp",   "ldr",   "ldtr",   "ldur",   "ldp", 0},
	{0x80408000,    NULL,  "ldaxr",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x80600000,    NULL,   "ldxp",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x80608000,    NULL,  "ldaxp",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x80800000,    NULL,     NULL,   NULL, "ldrsw", "ldtrsw", "ldursw",    NULL, 0},
	{0x80808000,    NULL,   "stlr",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x80c08000,    NULL,   "ldar",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0x84000000,   "ldr",     NULL, "stnp",   "str",     NULL,   "stur",   "stp", A64_REGFLAGS_SIMD},
	{0x84400000,    NULL,     NULL, "ldnp",   "ldr",     NULL,   "ldur",   "ldp", A64_REGFLAGS_SIMD},
	{0xc0000000,  "prfm",   "stxr",   NULL,   "str",   "sttr",   "stur",    NULL, 0},
	{0xc0008000,    NULL,  "stlxr",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0xc0200000,    NULL,   "stxp",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0xc0208000,    NULL,  "stlxp",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0xc0400000,    NULL,   "ldxr",   NULL,   "ldr",   "ldtr",   "ldur",    NULL, 0},
	{0xc0408000,    NULL,  "ldaxr",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0xc0600000,    NULL,   "ldxp",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0xc0608000,    NULL,  "ldaxp",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0xc0800000,    NULL,     NULL,   NULL,  "prfm",     NULL,  "prfum",    NULL, 0},
	{0xc0808000,    NULL,   "stlr",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0xc0c08000,    NULL,   "ldar",   NULL,    NULL,     NULL,     NULL,    NULL, 0},
	{0xc4000000,    NULL,     NULL,   NULL,   "str",     NULL,   "stur",    NULL, A64_REGFLAGS_SIMD},
	{0xc4400000,    NULL,     NULL,   NULL,   "ldr",     NULL,   "ldur",    NULL, A64_REGFLAGS_SIMD},
	{0xffffffff,    NULL,     NULL,   NULL,    NULL,     NULL,     NULL,    NULL, 0},
};
/* END CSTYLED */

#define	LDSTR_OPCODE_MASK_EXCLUSIVE	0xc0e08000
#define	LDSTR_OPCODE_MASK_REG_LIT	0xc4000000
#define	LDSTR_OPCODE_MASK_REG_PAIR	0xc4400000
#define	LDSTR_OPCODE_MASK_REG		0xc4c00000
#define	LDSTR_OPCODE_TARG_INVALID	0xffffffff

/* BEGIN CSTYLED */
/*
 * 31  30|29   27| 26|25 24|23                                    5|4  0| Reg (lit)
 * ------+-------+---+-----+---------------------------------------+----+
 *   opc | 0 1 1 | V | 0 0 |                 imm19                 | Rt | C3.3.5
 * ------+-------+---+-----+---------------------------------------+----+
 * 31  30|29             24|23|22| 21|20  16|15|14          10|9  5|4  0| Exclusive
 * ------+-----------------+--+--+---+------+--+--------------+----+----+
 *  size | 0 0 1   0   0 0 |o2| L| o1|   Rs |o0|      Rt2     | Rn | Rt | C3.3.6
 * ------+-----------------+--+--+---+------+--+--------------+----+----+
 * 31  30|29   27| 26|25 24|23 22| 21|20  16|15    13|12|11 10|9  5|4  0| Reg
 * ------+-------+---+-----+-----+---+------+--------+--+-----+----+----+
 *  size | 1 1 1 | V | 0 0 | opc | 0 |       imm9       | 0 1 | Rn | Rt | C3.3.8
 *  size | 1 1 1 | V | 0 0 | opc | 0 |       imm9       | 1 1 | Rn | Rt | C3.3.9
 *  size | 1 1 1 | V | 0 0 | opc | 1 |  Rm  | option | S| 1 0 | Rn | Rt | C3.3.10
 *  size | 1 1 1 | V | 0 0 | opc | 0 |       imm9       | 1 0 | Rn | Rt | C3.3.11
 *  size | 1 1 1 | V | 0 0 | opc | 0 |       imm9       | 0 0 | Rn | Rt | C3.3.12
 *  size | 1 1 1 | V | 0 1 | opc |            imm12           | Rn | Rt | C3.3.13
 * ------+-------+---+-----+-+---+---+------+-+------+--+-----+----+----+
 * 31  30|29   27| 26|25   23| 22|21         15|14          10|9  5|4  0| Reg pair
 * ------+-------+---+-------+---+-------------+--------------+----+----+
 *   opc | 1 0 1 | V | 0 0 0 | L |     imm7    |      Rt2     | Rn | Rt | C3.3.7
 *   opc | 1 0 1 | V | 0 1 0 | L |     imm7    |      Rt2     | Rn | Rt | C3.3.14
 *   opc | 1 0 1 | V | 0 0 1 | L |     imm7    |      Rt2     | Rn | Rt | C3.3.15
 *   opc | 1 0 1 | V | 0 1 1 | L |     imm7    |      Rt2     | Rn | Rt | C3.3.16
 */
/* END CSTYLED */

/* Used to identify instruction classes within a group */
#define	A64_LDSTR_NOALLOC_MASK	0x01800000
#define	A64_LDSTR_NOALLOC_TARG	0x00000000
#define	A64_LDSTR_UNPRIV_MASK	0x01200c00
#define	A64_LDSTR_UNPRIV_TARG	0x00000800
#define	A64_LDSTR_UNSCALE_MASK	0x01200c00
#define	A64_LDSTR_UNSCALE_TARG	0x00000000

#define	LDSTR_CLASS_REG_TYPE_MASK	0x00000c00
#define	LDSTR_CLASS_REG_TYPE_SHIFT	10

#define	LDSTR_CLASS_EXCLUSIVE	1
#define	LDSTR_CLASS_REG_LITERAL	3
#define	LDSTR_CLASS_REG_PAIR	5
#define	LDSTR_CLASS_REG		7

typedef struct a64_ldstr_insn {
	a64_ldstr_opcode_entry_t *op;
	const char *opname;
	a64_reg_t rm;
	a64_reg_t rn;
	a64_reg_t rt;
	a64_reg_t rt2;
	uint32_t imm;
	uint8_t opc;
	uint8_t size;
} a64_ldstr_insn_t;

/*
 * C3.3.6 Load/store exclusive
 */
static int
a64_dis_ldstr_excl(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen,
	a64_ldstr_insn_t *lsin)
{
	size_t len;
	uint8_t l, o1, o2, size, tmp;

	for (lsin->op = a64_ldstr_opcodes;
	    lsin->op->targ != LDSTR_OPCODE_TARG_INVALID; lsin->op++) {
		if ((in & LDSTR_OPCODE_MASK_EXCLUSIVE) == lsin->op->targ)
			break;
	}

	lsin->opname = lsin->op->name_exclusive;
	if (lsin->opname == NULL)
		return (-1);

	size = (in & LDSTR_SIZE_MASK) >> LDSTR_SIZE_SHIFT;
	o2 = (in & LDSTR_O2_MASK) >> LDSTR_O2_SHIFT;
	o1 = (in & LDSTR_O1_MASK) >> LDSTR_O1_SHIFT;
	l = (in & LDSTR_L_MASK) >> LDSTR_L_SHIFT;

	if (size == 3)
		lsin->rt.width = lsin->rt2.width = A64_REGWIDTH_64;

	tmp = o1 | (l << 1) | (o2 << 2);
	switch (tmp) {
	case 0:
		len = dis_snprintf(buf, buflen, "%s %s, %s, [%s]", lsin->opname,
		    a64_reg_name(lsin->rm), a64_reg_name(lsin->rt),
		    a64_reg_name(lsin->rn));
		break;
	case 1:
		len = dis_snprintf(buf, buflen, "%s %s, %s, %s, [%s]",
		    lsin->opname, a64_reg_name(lsin->rm),
		    a64_reg_name(lsin->rt), a64_reg_name(lsin->rt2),
		    a64_reg_name(lsin->rn));
		break;
	case 3:
		len = dis_snprintf(buf, buflen, "%s %s, %s, [%s]", lsin->opname,
		    a64_reg_name(lsin->rt), a64_reg_name(lsin->rt2),
		    a64_reg_name(lsin->rn));
		break;
	default:
		len = dis_snprintf(buf, buflen, "%s %s, [%s]", lsin->opname,
		    a64_reg_name(lsin->rt), a64_reg_name(lsin->rn));
		break;
	}

	return (len < buflen ? 0 : -1);
}

/*
 * C3.3.5 Load register (literal)
 */
static int
a64_dis_ldstr_reg_lit(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen,
    a64_ldstr_insn_t *lsin)
{
	uint32_t imm;
	uint8_t opc;
	size_t len;

	/* reg literal */
	imm = (in & LDSTR_IMM19_MASK) >> LDSTR_IMM19_SHIFT;
	opc = (in & LDSTR_SIZE_MASK) >> LDSTR_SIZE_SHIFT;
	for (lsin->op = a64_ldstr_opcodes;
	    lsin->op->targ != LDSTR_OPCODE_TARG_INVALID; lsin->op++) {
		if ((in & LDSTR_OPCODE_MASK_REG_LIT) == lsin->op->targ)
			break;
	}
	lsin->opname = lsin->op->name_literal;
	if (lsin->opname == NULL)
		return (-1);
	switch (lsin->op->regflags) {
	case A64_REGFLAGS_NONE:
		if (opc)
			lsin->rt.width = A64_REGWIDTH_64;
		break;
	case A64_REGFLAGS_SIMD:
		switch (opc) {
		case 0:
			lsin->rt.width = A64_REGWIDTH_SIMD_32;
			break;
		case 1:
			lsin->rt.width = A64_REGWIDTH_SIMD_64;
			break;
		case 2:
			lsin->rt.width = A64_REGWIDTH_SIMD_128;
			break;
		default:
			return (-1);
		}
		break;
	default:
		return (-1);
		break;
	}
	if (imm & LDSTR_IMM19_SIGN_MASK)
		imm |= LDSTR_IMM19_NEG_SIGN;
	else
		imm &= LDSTR_IMM19_POS_SIGN;
	imm <<= 2;
	/* XXX: abstract away, prfm */
	if (opc == 3) {
		if (a64_prefetch_ops[lsin->rt.id]) {
			len = dis_snprintf(buf, buflen, "%s %s, %x",
			    lsin->opname, a64_prefetch_ops[lsin->rt.id],
			    dhp->dh_addr + imm);
		} else {
			len = dis_snprintf(buf, buflen, "%s #0x%02x, %x",
			    lsin->opname, lsin->rt.id, dhp->dh_addr + imm);
		}
	} else {
		len = dis_snprintf(buf, buflen, "%s %s, %x", lsin->opname,
		    a64_reg_name(lsin->rt), dhp->dh_addr + imm);
	}
	buflen -= len;
	buf += len;

	a64_dis_addlabel(dhp, dhp->dh_addr + (int)imm, buf, buflen);

	return (len < buflen ? 0 : -1);
}

static int
a64_dis_ldstr_reg_pair(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen,
    a64_ldstr_insn_t *lsin)
{
	uint8_t opc, v;
	uint32_t imm;
	size_t len;

	imm = (in & LDSTR_IMM7_MASK) >> LDSTR_IMM7_SHIFT;
	opc = (in & LDSTR_SIZE_MASK) >> LDSTR_SIZE_SHIFT;
	v = (in & LDSTR_V_MASK) >> LDSTR_V_SHIFT;

	for (lsin->op = a64_ldstr_opcodes;
	    lsin->op->targ != LDSTR_OPCODE_TARG_INVALID; lsin->op++) {
		if ((in & LDSTR_OPCODE_MASK_REG_PAIR) == lsin->op->targ)
			break;
	}

	if ((in & A64_LDSTR_NOALLOC_MASK) == A64_LDSTR_NOALLOC_TARG)
		lsin->opname = lsin->op->name_noalloc;
	else
		lsin->opname = lsin->op->name_regpair;

	if (lsin->opname == NULL)
		return (-1);

	switch (lsin->op->regflags) {
	case A64_REGFLAGS_NONE:
		if (opc != 0)
			lsin->rt.width = A64_REGWIDTH_64;
		break;
	case A64_REGFLAGS_SIMD:
		switch (opc) {
		case 0:
			lsin->rt.width = A64_REGWIDTH_SIMD_32;
			break;
		case 1:
			lsin->rt.width = A64_REGWIDTH_SIMD_64;
			break;
		case 2:
			lsin->rt.width = A64_REGWIDTH_SIMD_128;
			break;
		default:
			return (-1);
		}
		break;
	default:
		return (-1);
	}
	lsin->rt2.width = lsin->rt.width;
	if ((imm & LDSTR_IMM7_SIGN_MASK) != 0)
		imm |= LDSTR_IMM7_NEG_SIGN;
	else
		imm &= LDSTR_IMM7_POS_SIGN;

	if (v != 0) {
		switch (lsin->rt.width) {
		case A64_REGWIDTH_SIMD_32:
			imm <<= 2;
			break;
		case A64_REGWIDTH_SIMD_64:
			imm <<= 3;
			break;
		case A64_REGWIDTH_SIMD_128:
			imm <<= 4;
			break;
		default:
			break;
		}
	} else if (opc == 1) {
		imm <<= 2;
	} else {
		imm <<= (lsin->rt.width == A64_REGWIDTH_64) ? 3 : 2;
	}

	if ((in & A64_BIT_23_MASK) && (in & A64_BIT_24_MASK)) {
		len = dis_snprintf(buf, buflen,
		    "%s %s, %s, [%s, #%d]!",
		    lsin->opname,
		    a64_reg_name(lsin->rt), a64_reg_name(lsin->rt2),
		    a64_reg_name(lsin->rn), imm);
	} else if (in & A64_BIT_23_MASK) {
		len = dis_snprintf(buf, buflen,
		    "%s %s, %s, [%s], #%d",
		    lsin->opname,
		    a64_reg_name(lsin->rt), a64_reg_name(lsin->rt2),
		    a64_reg_name(lsin->rn), imm);
	} else if (imm) {
		len = dis_snprintf(buf, buflen,
		    "%s %s, %s, [%s, #%d]",
		    lsin->opname,
		    a64_reg_name(lsin->rt), a64_reg_name(lsin->rt2),
		    a64_reg_name(lsin->rn), imm);
	} else {
		len = dis_snprintf(buf, buflen,
		    "%s %s, %s, [%s]",
		    lsin->opname,
		    a64_reg_name(lsin->rt), a64_reg_name(lsin->rt2),
		    a64_reg_name(lsin->rn));
	}

	return (len < buflen ? 0 : -1);
}

static int
a64_dis_ldstr_reg(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen,
    a64_ldstr_insn_t *lsin)
{
	uint8_t size, opc, v, regtype, option, sbit;
	uint32_t imm;
	size_t len;
	int amount = -1;

	size = (in & LDSTR_SIZE_MASK) >> LDSTR_SIZE_SHIFT;
	opc = (in & LDSTR_OPC_MASK) >> LDSTR_OPC_SHIFT;
	v = (in & LDSTR_V_MASK) >> LDSTR_V_SHIFT;

	for (lsin->op = a64_ldstr_opcodes;
	    lsin->op->targ != LDSTR_OPCODE_TARG_INVALID; lsin->op++) {
		if ((in & LDSTR_OPCODE_MASK_REG) == lsin->op->targ)
			break;
	}

	if ((in & A64_LDSTR_UNPRIV_MASK) == A64_LDSTR_UNPRIV_TARG) {
		lsin->opname = lsin->op->name_unpriv;
	} else if ((in & A64_LDSTR_UNSCALE_MASK) == A64_LDSTR_UNSCALE_TARG) {
		lsin->opname = lsin->op->name_unscaled;
	} else {
		lsin->opname = lsin->op->name_reg;
	}

	switch (lsin->op->regflags) {
	case A64_REGFLAGS_NONE:
		if (opc == 2 || size == 3)
			lsin->rt.width = A64_REGWIDTH_64;
		break;
	case A64_REGFLAGS_SIMD:
		switch (size) {
		case 0:
			lsin->rt.width = opc < 2 ? A64_REGWIDTH_SIMD_8 :
			    A64_REGWIDTH_SIMD_128;
			break;
		case 1:
			lsin->rt.width = A64_REGWIDTH_SIMD_16;
			break;
		case 2:
			lsin->rt.width = A64_REGWIDTH_SIMD_32;
			break;
		case 3:
			lsin->rt.width = A64_REGWIDTH_SIMD_64;
			break;
		}
		break;
	default:
		return (-1);
	}

	if (in & A64_BIT_24_MASK) {
		imm = (in & LDSTR_IMM12_MASK) >> LDSTR_IMM12_SHIFT;
		// 00 0 00	0
		// 00 0 10	0
		// 00 1 00	<< simd size 1,2,3,4,5
		// 00 1 10	same
		// 01 0 00	<< 1
		// 01 0 10	<< 1
		// 01 1 00	simd
		// 10 0 00	<< 32/64 2,3
		// 10 0 10	<< 2
		// 10 1 00	simd
		// 11 0 00	<< 32/64 2,3
		// 11 0 10	<< 3
		// 11 1 00	simd
		if (v != 0) {
			switch (lsin->rt.width) {
			case A64_REGWIDTH_SIMD_16:
				imm <<= 1;
				break;
			case A64_REGWIDTH_SIMD_32:
				imm <<= 2;
				break;
			case A64_REGWIDTH_SIMD_64:
				imm <<= 3;
				break;
			case A64_REGWIDTH_SIMD_128:
				imm <<= 4;
				break;
			default:
				break;
			}
		} else if (size == 1) {
			imm <<= 1;
		} else if (size == 2) {
			if (opc) {
				imm <<= 2;
			} else {
				imm <<= (lsin->rt.width == A64_REGWIDTH_64) ?
				    3 : 2;
			}
		} else if (size == 3) {
			imm <<= (lsin->rt.width == A64_REGWIDTH_64) ? 3 : 2;
		}

#if 0
		if (imm & LDSTR_IMM12_SIGN_MASK)
			imm |= LDSTR_IMM12_NEG_SIGN;
		else
			imm &= LDSTR_IMM12_POS_SIGN;
#endif
		/* XXX: prfm */
		if (size == 3 && opc == 2) {
			if (imm) {
				if (a64_prefetch_ops[lsin->rt.id]) {
					len = dis_snprintf(buf, buflen,
					    "%s %s, [%s, #%d]",
					    lsin->opname,
					    a64_prefetch_ops[lsin->rt.id],
					    a64_reg_name(lsin->rn), imm);
				} else {
					len = dis_snprintf(buf, buflen,
					    "%s #0x%02x, [%s, #%d]",
					    lsin->opname, lsin->rt.id,
					    a64_reg_name(lsin->rn), imm);
				}
			} else {
				if (a64_prefetch_ops[lsin->rt.id]) {
					len = dis_snprintf(buf, buflen,
					    "%s %s, [%s]",
					    lsin->opname,
					    a64_prefetch_ops[lsin->rt.id],
					    a64_reg_name(lsin->rn));
				} else {
					len = dis_snprintf(buf, buflen,
					    "%s #0x%02x, [%s]",
					    lsin->opname, lsin->rt.id,
					    a64_reg_name(lsin->rn));
				}
			}
		} else {
			if (imm) {
				len = dis_snprintf(buf, buflen,
				    "%s %s, [%s, #%d]",
				    lsin->opname, a64_reg_name(lsin->rt),
				    a64_reg_name(lsin->rn), imm);
			} else {
				len = dis_snprintf(buf, buflen,
				    "%s %s, [%s]",
				    lsin->opname, a64_reg_name(lsin->rt),
				    a64_reg_name(lsin->rn));
			}
		}
		return (len < buflen ? 0 : -1);
	}

	imm = (in & LDSTR_IMM9_MASK) >> LDSTR_IMM9_SHIFT;
	if (imm & LDSTR_IMM9_SIGN_MASK)
		imm |= LDSTR_IMM9_NEG_SIGN;
	else
		imm &= LDSTR_IMM9_POS_SIGN;

	if ((in & A64_LDSTR_UNPRIV_MASK) == A64_LDSTR_UNPRIV_TARG) {
		if (imm) {
			len = dis_snprintf(buf, buflen,
			    "%s %s, [%s, #%d]",
			    lsin->opname, a64_reg_name(lsin->rt),
			    a64_reg_name(lsin->rn), imm);
		} else {
			len = dis_snprintf(buf, buflen,
			    "%s %s, [%s]",
			    lsin->opname, a64_reg_name(lsin->rt),
			    a64_reg_name(lsin->rn), imm);
		}
		return (len < buflen ? 0 : -1);
	}

	regtype = (in & LDSTR_CLASS_REG_TYPE_MASK) >>
	    LDSTR_CLASS_REG_TYPE_SHIFT;

	switch (regtype) {
	case 0:
		/* XXX: prfum */
		if (size == 3 && opc == 2) {
			if (imm) {
				if (a64_prefetch_ops[lsin->rt.id]) {
					len = dis_snprintf(buf, buflen,
					    "%s %s, [%s, #%d]",
					    lsin->opname,
					    a64_prefetch_ops[lsin->rt.id],
					    a64_reg_name(lsin->rn), imm);
				} else {
					len = dis_snprintf(buf, buflen,
					    "%s #0x%02x, [%s, #%d]",
					    lsin->opname, lsin->rt.id,
					    a64_reg_name(lsin->rn), imm);
				}
			} else {
				if (a64_prefetch_ops[lsin->rt.id]) {
					len = dis_snprintf(buf, buflen,
					    "%s %s, [%s]",
					    lsin->opname,
					    a64_prefetch_ops[lsin->rt.id],
					    a64_reg_name(lsin->rn), imm);
				} else {
					len = dis_snprintf(buf, buflen,
					    "%s #0x%02x, [%s]",
					    lsin->opname, lsin->rt.id,
					    a64_reg_name(lsin->rn), imm);
				}
			}
		} else {
			if (imm) {
				len = dis_snprintf(buf, buflen,
				    "%s %s, [%s, #%d]",
				    lsin->opname, a64_reg_name(lsin->rt),
				    a64_reg_name(lsin->rn), imm);
			} else {
				len = dis_snprintf(buf, buflen,
				    "%s %s, [%s]",
				    lsin->opname, a64_reg_name(lsin->rt),
				    a64_reg_name(lsin->rn), imm);
			}
		}
		break;
	case 1:
		/*
		 *  C3.3.8 Load/store register (immediate post-indexed)
		 */
		len = dis_snprintf(buf, buflen,
		    "%s %s, [%s], #%d", lsin->opname,
		    a64_reg_name(lsin->rt), a64_reg_name(lsin->rn), imm);
		break;
	case 2:
		/*
		 * C3.3.10 Load/store register (register offset)
		 */
		option = (in & LDSTR_OPTION_MASK) >> LDSTR_OPTION_SHIFT;
		sbit = (in & LDSTR_S_MASK) >> LDSTR_S_SHIFT;

		if (option == 3 || option == 7)
			lsin->rm.width = 1;
		else
			lsin->rm.width = 0;

		if (sbit) {
			switch (lsin->rt.width) {
			case A64_REGWIDTH_32:
			case A64_REGWIDTH_64:
				amount = size;
				break;
			case A64_REGWIDTH_SIMD_8:
				amount = 0;
				break;
			case A64_REGWIDTH_SIMD_16:
				amount = 1;
				break;
			case A64_REGWIDTH_SIMD_32:
				amount = 2;
				break;
			case A64_REGWIDTH_SIMD_64:
				amount = 3;
				break;
			case A64_REGWIDTH_SIMD_128:
				amount = 4;
				break;
			default:
				break;
			}
		}
		if (option == DPI_E_UXTX)
			option = DPI_E_LSL;

		/* XXX: abstract prfm and reduce dups */
		if (size == 3 && opc == 2) {
			if (sbit)
				amount = 3;
			if (amount >= 0) {
				if (a64_prefetch_ops[lsin->rt.id]) {
					len = dis_snprintf(buf, buflen,
					    "%s %s, [%s, %s, %s #%d]",
					    lsin->opname,
					    a64_prefetch_ops[lsin->rt.id],
					    a64_reg_name(lsin->rn),
					    a64_reg_name(lsin->rm),
					    a64_dataproc_extends[option],
					    amount);
				} else {
					len = dis_snprintf(buf, buflen,
					    "%s #0x%02x, [%s, %s, %s #%d]",
					    lsin->opname, lsin->rt.id,
					    a64_reg_name(lsin->rn),
					    a64_reg_name(lsin->rm),
					    a64_dataproc_extends[option],
					    amount);
				}
			} else if (option != DPI_E_LSL) {
				if (a64_prefetch_ops[lsin->rt.id]) {
					len = dis_snprintf(buf, buflen,
					    "%s %s, [%s, %s, %s]",
					    lsin->opname,
					    a64_prefetch_ops[lsin->rt.id],
					    a64_reg_name(lsin->rn),
					    a64_reg_name(lsin->rm),
					    a64_dataproc_extends[option]);
				} else {
					len = dis_snprintf(buf, buflen,
					    "%s #0x%02x, [%s, %s, %s]",
					    lsin->opname, lsin->rt.id,
					    a64_reg_name(lsin->rn),
					    a64_reg_name(lsin->rm),
					    a64_dataproc_extends[option]);
				}
			} else {
				if (a64_prefetch_ops[lsin->rt.id]) {
					len = dis_snprintf(buf, buflen,
					    "%s %s, [%s, %s]",
					    lsin->opname,
					    a64_prefetch_ops[lsin->rt.id],
					    a64_reg_name(lsin->rn),
					    a64_reg_name(lsin->rm));
				} else {
					len = dis_snprintf(buf, buflen,
					    "%s #0x%02x, [%s, %s]",
					    lsin->opname, lsin->rt.id,
					    a64_reg_name(lsin->rn),
					    a64_reg_name(lsin->rm));
				}
			}
		} else {
			if (amount >= 0) {
				len = dis_snprintf(buf, buflen,
				    "%s %s, [%s, %s, %s #%d]",
				    lsin->opname, a64_reg_name(lsin->rt),
				    a64_reg_name(lsin->rn),
				    a64_reg_name(lsin->rm),
				    a64_dataproc_extends[option],
				    amount);
			} else if (option != DPI_E_LSL) {
				len = dis_snprintf(buf, buflen,
				    "%s %s, [%s, %s, %s]",
				    lsin->opname, a64_reg_name(lsin->rt),
				    a64_reg_name(lsin->rn),
				    a64_reg_name(lsin->rm),
				    a64_dataproc_extends[option]);
			} else {
				len = dis_snprintf(buf, buflen,
				    "%s %s, [%s, %s]",
				    lsin->opname, a64_reg_name(lsin->rt),
				    a64_reg_name(lsin->rn),
				    a64_reg_name(lsin->rm));
			}
		}
		break;
	case 3:
		/*
		 * C3.3.9 Load/store register (immediate pre-indexed)
		 */
		len = dis_snprintf(buf, buflen,
		    "%s %s, [%s, #%d]!", lsin->opname,
		    a64_reg_name(lsin->rt), a64_reg_name(lsin->rn), imm);
		break;
	}

	return (len < buflen ? 0 : -1);
}

static int
a64_dis_ldstr(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen)
{
	a64_ldstr_insn_t lsin;
	uint8_t class;
	size_t len;

	bzero(&lsin, sizeof (a64_ldstr_insn_t));

	class = (in & LDSTR_CLASS_MASK) >> LDSTR_CLASS_SHIFT;

	/*
	 * Not all classes use all registers but it's simpler to extract them
	 * all regardless.
	 */
	lsin.rm.id = (in & LDSTR_RM_MASK) >> LDSTR_RM_SHIFT;
	lsin.rn.id = (in & LDSTR_RN_MASK) >> LDSTR_RN_SHIFT;
	lsin.rt.id = (in & LDSTR_RT_MASK) >> LDSTR_RT_SHIFT;
	lsin.rt2.id = (in & LDSTR_RT2_MASK) >> LDSTR_RT2_SHIFT;

	lsin.rn.flags = A64_REGFLAGS_SP;
	lsin.rn.width = A64_REGWIDTH_64;

	switch (class) {
	case LDSTR_CLASS_EXCLUSIVE:
		return (a64_dis_ldstr_excl(dhp, in, buf, buflen, &lsin));
	case LDSTR_CLASS_REG_LITERAL:
		return (a64_dis_ldstr_reg_lit(dhp, in, buf, buflen, &lsin));
	case LDSTR_CLASS_REG_PAIR:
		return (a64_dis_ldstr_reg_pair(dhp, in, buf, buflen, &lsin));
	case LDSTR_CLASS_REG:
		return (a64_dis_ldstr_reg(dhp, in, buf, buflen, &lsin));
	default:
		len = dis_snprintf(buf, buflen, "class=%d\n", class);
		break;
	}

	return (len < buflen ? 0 : -1);
}

/*
 * ===========================================================================
 *
 * C2.1 Branches, Exception generating, and System instructions
 *
 * We have already excluded the exception and system instructions at this
 * point, so we are only interested in handling branches.
 */
#define	BR_SF_MASK		0x80000000
#define	BR_SF_SHIFT		31
#define	BR_B5_MASK		BR_SF_MASK
#define	BR_B5_SHIFT		BR_SF_SHIFT
#define	BR_B40_MASK		0x00f80000
#define	BR_B40_SHIFT		19
#define	BR_RN_MASK		0x000003e0
#define	BR_RN_SHIFT		5
#define	BR_RT_MASK		0x0000001f
#define	BR_RT_SHIFT		0
#define	BR_COND_MASK		0x0000000f
#define	BR_COND_SHIFT		0

/*
 * Masks and offsets for labels encoded in branch immediates.  The sign values
 * are adjusted to match the immediate after it has been shifted into position.
 */
#define	BR_IMM14_MASK		0x0007ffe0
#define	BR_IMM14_SHIFT		5
#define	BR_IMM14_SIGN_MASK	0x00002000
#define	BR_IMM14_POS_SIGN	0x00003fff
#define	BR_IMM14_NEG_SIGN	0xffffc000
#define	BR_IMM19_MASK		0x00ffffe0
#define	BR_IMM19_SHIFT		5
#define	BR_IMM19_SIGN_MASK	0x00040000
#define	BR_IMM19_POS_SIGN	0x0007ffff
#define	BR_IMM19_NEG_SIGN	0xfff80000
#define	BR_IMM26_MASK		0x03ffffff
#define	BR_IMM26_SHIFT		0
#define	BR_IMM26_SIGN_MASK	0x02000000
#define	BR_IMM26_POS_SIGN	0x03ffffff
#define	BR_IMM26_NEG_SIGN	0xfc000000

#define	BR_IMM(val)		BR_IMM##val##_MASK, BR_IMM##val##_SHIFT, \
				BR_IMM##val##_SIGN_MASK, \
				BR_IMM##val##_POS_SIGN, BR_IMM##val##_NEG_SIGN

typedef enum a64_branch_type {
	BR_TYPE_COMP_I,		/* <Wt>, <label> */
	BR_TYPE_COND_I,		/* <cond> <label> */
	BR_TYPE_TEST_I,		/* <R><t>, #<imm>, <label> */
	BR_TYPE_UNCOND_I,	/* <label> */
	BR_TYPE_UNCOND_R,	/* <Xn> */
	BR_TYPE_UNCOND_R_OPT,	/* {<Xn>} */
	BR_TYPE_DIRECT,		/* No operands */
} a64_branch_type_t;

typedef struct a64_branch_opcode_entry {
	const char *name;
	uint32_t mask;
	uint32_t val;
	a64_branch_type_t type;
	uint32_t imm_mask;
	uint32_t imm_shift;
	uint32_t sign_mask;
	uint32_t pos_sign;
	uint32_t neg_sign;
} a64_branch_opcode_entry_t;

/*
 * As there aren't many branch instructions we can afford a linear scan through
 * the lookup table to find the target instruction in order to simplify things.
 */
static a64_branch_opcode_entry_t a64_branch_opcodes[] = {
	/* C3.2.1 Compare & branch (immediate) */
	{"cbz",	 0x7f000000, 0x34000000, BR_TYPE_COMP_I, BR_IMM(19)},
	{"cbnz", 0x7f000000, 0x35000000, BR_TYPE_COMP_I, BR_IMM(19)},
	/* C3.2.2 Conditional branch (immediate) */
	{"b.",   0xff000010, 0x54000000, BR_TYPE_COND_I, BR_IMM(19)},
	/* C3.2.5 Test & branch (immediate) */
	{"tbz",  0x7f000000, 0x36000000, BR_TYPE_TEST_I, BR_IMM(14)},
	{"tbnz", 0x7f000000, 0x37000000, BR_TYPE_TEST_I, BR_IMM(14)},
	/* C3.2.6 Unconditional branch (immediate) */
	{"b",    0xfc000000, 0x14000000, BR_TYPE_UNCOND_I, BR_IMM(26)},
	{"bl",   0xfc000000, 0x94000000, BR_TYPE_UNCOND_I, BR_IMM(26)},
	/* C3.2.7 Unconditional branch (register) */
	{"br",   0xffe00000, 0xd6000000, BR_TYPE_UNCOND_R, 0, 0, 0, 0, 0},
	{"blr",  0xffe00000, 0xd6200000, BR_TYPE_UNCOND_R, 0, 0, 0, 0, 0},
	{"ret",  0xffe00000, 0xd6400000, BR_TYPE_UNCOND_R_OPT, 0, 0, 0, 0, 0},
	{"eret", 0xffe00000, 0xd6800000, BR_TYPE_DIRECT, 0, 0, 0, 0, 0},
	{"drps", 0xffe00000, 0xd6a00000, BR_TYPE_DIRECT, 0, 0, 0, 0, 0},
	{NULL, 0, 0, 0, 0, 0, 0, 0, 0}
};

static int
a64_dis_branch(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen)
{
	a64_branch_opcode_entry_t *op;
	a64_cond_code_t cc;
	a64_reg_t reg;
	uint32_t addr, imm;
	size_t len;
	uint8_t b40, b5; // , neg;

	for (op = a64_branch_opcodes; op->name != NULL; op++) {
		if ((in & op->mask) == op->val)
			break;
	}

	if (op->name == NULL)
		return (-1);

	/*
	 * First print the opcode.
	 */
	if ((len = dis_snprintf(buf, buflen, "%s", op->name)) >= buflen)
		return (-1);

	buflen -= len;
	buf += len;

	/*
	 * Next, print any non-label operands.  Return early from any
	 * branch types which do not include a label.
	 */
	switch (op->type) {
	case BR_TYPE_DIRECT:
		return (0);
		break;
	case BR_TYPE_COMP_I:
		reg.id = (in & BR_RT_MASK) >> BR_RT_SHIFT;
		reg.width = (in & BR_SF_MASK) >> BR_SF_SHIFT;
		if ((len = dis_snprintf(buf, buflen, " %s,",
		    a64_reg_name(reg))) >= buflen)
			return (-1);
		break;
	case BR_TYPE_UNCOND_R:
		reg.id = (in & BR_RN_MASK) >> BR_RN_SHIFT;
		reg.width = (in & BR_SF_MASK) >> BR_SF_SHIFT;
		len = dis_snprintf(buf, buflen, " %s", a64_reg_name(reg));
		return (len < buflen ? 0 : -1);
		break;
	case BR_TYPE_UNCOND_R_OPT:
		reg.id = (in & BR_RN_MASK) >> BR_RN_SHIFT;
		reg.width = (in & BR_SF_MASK) >> BR_SF_SHIFT;
		/* x30 is the default and not printed. */
		if (reg.id != A64_REG_30)
			if ((len = dis_snprintf(buf, buflen, " %s",
			    a64_reg_name(reg))) >= buflen)
				return (-1);
		return (0);
		break;
	case BR_TYPE_COND_I:
		cc = (in & BR_COND_MASK) >> BR_COND_SHIFT;
		/* The condition comes immediately after "b." */
		if ((len = dis_snprintf(buf, buflen, "%s",
		    a64_cond_names[cc])) >= buflen)
			return (-1);
		break;
	case BR_TYPE_TEST_I:
		reg.id = (in & BR_RT_MASK) >> BR_RT_SHIFT;
		reg.width = (in & BR_SF_MASK) >> BR_SF_SHIFT;
		b40 = (in & BR_B40_MASK) >> BR_B40_SHIFT;
		b5 = (in & BR_B5_MASK) >> BR_B5_SHIFT;
		/* imm is encoded as <b5:b40>, b40 is 4 bits wide */
		imm = b40 + (b5 << 5);
		if ((len = dis_snprintf(buf, buflen, " %s, #%d,",
		    a64_reg_name(reg), imm)) >= buflen)
			return (-1);
		break;
	case BR_TYPE_UNCOND_I:
		len = 0;
		break;
	}

	buflen -= len;
	buf += len;

	/*
	 * Finally, print the address and label.
	 */
	addr = (in & op->imm_mask) >> op->imm_shift;
	if (addr & op->sign_mask) {
		addr |= op->neg_sign;
		// neg = 1;
	} else {
		addr &= op->pos_sign;
		// neg = 0;
	}
	/* All addresses are encoded as imm * 4 */
	addr <<= 2;
	len = dis_snprintf(buf, buflen, " 0x%" PRIx64,
	    dhp->dh_addr + (int)addr);
	if (len >= buflen)
		return (-1);
#if 0
	if (neg) {
		if ((len = dis_snprintf(buf, buflen, " -0x%x", -(int)addr))
		    >= buflen)
			return (-1);
	} else {
		if ((len = dis_snprintf(buf, buflen, " +0x%x", (int)addr))
		    >= buflen)
			return (-1);
	}
#endif
	buflen -= len;
	buf += len;
	a64_dis_addlabel(dhp, dhp->dh_addr + (int)addr, buf, buflen);

	return (len < buflen ? 0 : -1);
}

/*
 * C3.2.3 Exception generation
 *
 * +-----------------+-------+-------------------------+-------+----+
 * |31             24|23   21|20                      5|4     2|1  0|
 * +-----------------+-------+-------------------------+-------+----+
 * | 1 1 0 1 0 1 0 0 |  opc  |          imm16          |  op2  | LL |
 * +-----------------+-------+-------------------------+-------+----+
 */
#define	A64_EXCEPTION_OPCODE_MASK	0x00e0001f
#define	A64_EXCEPTION_IMM16_MASK	0x001fffe0
#define	A64_EXCEPTION_IMM16_SHIFT	5

typedef struct a64_exception_opcode_entry {
	uint32_t targ;		/* mask target value */
	const char *name;	/* opcode */
	uint8_t immopt;		/* whether #<imm> is optional */
} a64_exception_opcode_entry_t;

static a64_exception_opcode_entry_t a64_exception_opcodes[] = {
	{0x00000001,	"svc",		0},
	{0x00000002,	"hvc",		0},
	{0x00000003,	"smc",		0},
	{0x00200000,	"brk",		0},
	{0x00400000,	"hlt",		0},
	{0x00a00001,	"dcps1",	1},
	{0x00a00002,	"dcps2",	1},
	{0x00a00003,	"dcps3",	1},
	{0xffffffff,	NULL,		0}
};

static int
a64_dis_exception(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen)
{
	a64_exception_opcode_entry_t *op;
	uint16_t imm;
	size_t len;

	for (op = a64_exception_opcodes; op->targ != 0xffffffff; op++) {
		if ((in & A64_EXCEPTION_OPCODE_MASK) == op->targ)
			break;
	}

	if (op->name == NULL)
		return (-1);

	imm = (in & A64_EXCEPTION_IMM16_MASK) >> A64_EXCEPTION_IMM16_SHIFT;

	if (op->immopt && imm == 0)
		len = dis_snprintf(buf, buflen, "%s", op->name);
	else
		len = dis_snprintf(buf, buflen, "%s #0x%x", op->name, imm);

	return (len < buflen ? 0 : -1);
}

/*
 * C3.2.4 System
 *
 * +---------------------+--+-----+-----+-------+-------+-----+------+
 * |31                 22|21|20 19|18 16|15   12|11    8|7   5|4    0|
 * +---------------------+--+-----+-----+-------+-------+-----+------+
 * | 1 1 0 1 0 1 0 1 0 0 | L| op0 | op1 |  CRn  |  CRm  | op2 |  Rt  |
 * +---------------------+--+-----+-----+-------+-------+-----+------+
 */
typedef enum a64_system_type {
	A64_SYS_TYPE_DIRECT,	/* OP */
	A64_SYS_TYPE_IMM1,	/* OP #<imm> (CRm:op2) */
	A64_SYS_TYPE_IMM2,	/* OP {#<imm>} (CRm) */
	A64_SYS_TYPE_IMMOPT,	/* OP <option>|#<imm> (CRm) */
	A64_SYS_TYPE_PSTATE,	/* OP <pstate>, #imm */
	A64_SYS_TYPE_SYS,	/* OP #<op1>, <Cn>, <Cm>, #<op2>{, <Xt>} */
	A64_SYS_TYPE_SYSL,	/* OP <Xt>, #<op1>, <Cn>, <Cm>, #<op2> */
	A64_SYS_TYPE_MSR,	/* OP <systemreg>, <Xt> */
	A64_SYS_TYPE_MRS,	/* OP <Xt>, <systemreg> */
} a64_system_type_t;

typedef struct a64_system_opcode_entry {
	uint32_t mask;		/* mask value */
	uint32_t targ;		/* target value */
	const char *name;	/* opcode */
	a64_system_type_t type;	/* operand type */
} a64_system_opcode_entry_t;

typedef enum a64_system_regopt {
	A64_SYS_REGOPT_NEVER,	/* Don't print <Xt> */
	A64_SYS_REGOPT_DEFAULT,	/* Print <Xt> if not default */
	A64_SYS_REGOPT_ALWAYS	/* Print <Xt> regardless */
} a64_system_regopt_t;

typedef struct a64_system_sysalias_entry {
	const char *opname;
	const char *opalias;
	uint8_t op1;
	uint8_t crn;
	uint8_t crm;
	uint8_t op2;
	a64_system_regopt_t regopt;
} a64_system_sysalias_entry_t;

/* BEGIN CSTYLED */
static a64_system_sysalias_entry_t a64_system_sysaliases[] = {
	{"s1e1r",		"at",	0x0,	0x7,	0x8,	0x0,	A64_SYS_REGOPT_ALWAYS},
	{"s1e2r",		"at",	0x4,	0x7,	0x8,	0x0,	A64_SYS_REGOPT_ALWAYS},
	{"s1e3r",		"at",	0x6,	0x7,	0x8,	0x0,	A64_SYS_REGOPT_ALWAYS},
	{"s1e1w",		"at",	0x0,	0x7,	0x8,	0x1,	A64_SYS_REGOPT_ALWAYS},
	{"s1e2w",		"at",	0x4,	0x7,	0x8,	0x1,	A64_SYS_REGOPT_ALWAYS},
	{"s1e3w",		"at",	0x6,	0x7,	0x8,	0x1,	A64_SYS_REGOPT_ALWAYS},
	{"s1e0r",		"at",	0x0,	0x7,	0x8,	0x2,	A64_SYS_REGOPT_ALWAYS},
	{"s1e0w",		"at",	0x0,	0x7,	0x8,	0x3,	A64_SYS_REGOPT_ALWAYS},
	{"s12e1r",		"at",	0x4,	0x7,	0x8,	0x4,	A64_SYS_REGOPT_ALWAYS},
	{"s12e1w",		"at",	0x4,	0x7,	0x8,	0x5,	A64_SYS_REGOPT_ALWAYS},
	{"s12e0r",		"at",	0x4,	0x7,	0x8,	0x6,	A64_SYS_REGOPT_ALWAYS},
	{"s12e0w",		"at",	0x4,	0x7,	0x8,	0x7,	A64_SYS_REGOPT_ALWAYS},

	{"zva",			"dc",	0x3,	0x7,	0x4,	0x1,	A64_SYS_REGOPT_ALWAYS},
	{"ivac",		"dc",	0x0,	0x7,	0x6,	0x1,	A64_SYS_REGOPT_ALWAYS},
	{"isw",			"dc",	0x0,	0x7,	0x6,	0x2,	A64_SYS_REGOPT_ALWAYS},
	{"cvac",		"dc",	0x3,	0x7,	0xa,	0x1,	A64_SYS_REGOPT_ALWAYS},
	{"csw",			"dc",	0x0,	0x7,	0xa,	0x2,	A64_SYS_REGOPT_ALWAYS},
	{"cvau",		"dc",	0x3,	0x7,	0xb,	0x1,	A64_SYS_REGOPT_ALWAYS},
	{"civac",		"dc",	0x3,	0x7,	0xe,	0x1,	A64_SYS_REGOPT_ALWAYS},
	{"cisw",		"dc",	0x0,	0x7,	0xe,	0x2,	A64_SYS_REGOPT_ALWAYS},

	{"ialluis",		"ic",	0x0,	0x7,	0x1,	0x0,	A64_SYS_REGOPT_NEVER},
	{"iallu",		"ic",	0x0,	0x7,	0x5,	0x0,	A64_SYS_REGOPT_NEVER},
	{"ivau",		"ic",	0x3,	0x7,	0x5,	0x1,	A64_SYS_REGOPT_DEFAULT},

	{"ipas2e1is",		"tlbi",	0x4,	0x8,	0x0,	0x1,	A64_SYS_REGOPT_DEFAULT},
	{"ipas2le1is",		"tlbi",	0x4,	0x8,	0x0,	0x5,	A64_SYS_REGOPT_DEFAULT},
	{"vmalle1is",		"tlbi",	0x0,	0x8,	0x3,	0x0,	A64_SYS_REGOPT_NEVER},
	{"alle2is",		"tlbi",	0x4,	0x8,	0x3,	0x0,	A64_SYS_REGOPT_NEVER},
	{"alle3is",		"tlbi",	0x6,	0x8,	0x3,	0x0,	A64_SYS_REGOPT_NEVER},
	{"vae1is",		"tlbi",	0x0,	0x8,	0x3,	0x1,	A64_SYS_REGOPT_DEFAULT},
	{"vae2is",		"tlbi",	0x4,	0x8,	0x3,	0x1,	A64_SYS_REGOPT_DEFAULT},
	{"vae3is",		"tlbi",	0x6,	0x8,	0x3,	0x1,	A64_SYS_REGOPT_DEFAULT},
	{"aside1is",		"tlbi",	0x0,	0x8,	0x3,	0x2,	A64_SYS_REGOPT_DEFAULT},
	{"vaae1is",		"tlbi",	0x0,	0x8,	0x3,	0x3,	A64_SYS_REGOPT_DEFAULT},
	{"alle1is",		"tlbi",	0x4,	0x8,	0x3,	0x4,	A64_SYS_REGOPT_NEVER},
	{"vale1is",		"tlbi",	0x0,	0x8,	0x3,	0x5,	A64_SYS_REGOPT_DEFAULT},
	{"vale2is",		"tlbi",	0x4,	0x8,	0x3,	0x5,	A64_SYS_REGOPT_DEFAULT},
	{"vale3is",		"tlbi",	0x6,	0x8,	0x3,	0x5,	A64_SYS_REGOPT_DEFAULT},
	{"vmalls12e1is",	"tlbi",	0x4,	0x8,	0x3,	0x6,	A64_SYS_REGOPT_NEVER},
	{"vaale1is",		"tlbi",	0x0,	0x8,	0x3,	0x7,	A64_SYS_REGOPT_DEFAULT},
	{"ipas2e1",		"tlbi",	0x4,	0x8,	0x4,	0x1,	A64_SYS_REGOPT_DEFAULT},
	{"ipas2le1",		"tlbi",	0x4,	0x8,	0x4,	0x5,	A64_SYS_REGOPT_DEFAULT},
	{"vmalle1",		"tlbi",	0x0,	0x8,	0x7,	0x0,	A64_SYS_REGOPT_NEVER},
	{"alle2",		"tlbi",	0x4,	0x8,	0x7,	0x0,	A64_SYS_REGOPT_NEVER},
	{"alle3",		"tlbi",	0x6,	0x8,	0x7,	0x0,	A64_SYS_REGOPT_NEVER},
	{"vae1",		"tlbi",	0x0,	0x8,	0x7,	0x1,	A64_SYS_REGOPT_DEFAULT},
	{"vae2",		"tlbi",	0x4,	0x8,	0x7,	0x1,	A64_SYS_REGOPT_DEFAULT},
	{"vae3",		"tlbi",	0x6,	0x8,	0x7,	0x1,	A64_SYS_REGOPT_DEFAULT},
	{"aside1",		"tlbi",	0x0,	0x8,	0x7,	0x2,	A64_SYS_REGOPT_DEFAULT},
	{"vaae1",		"tlbi",	0x0,	0x8,	0x7,	0x3,	A64_SYS_REGOPT_DEFAULT},
	{"alle1",		"tlbi",	0x4,	0x8,	0x7,	0x4,	A64_SYS_REGOPT_NEVER},
	{"vale1",		"tlbi",	0x0,	0x8,	0x7,	0x5,	A64_SYS_REGOPT_DEFAULT},
	{"vale2",		"tlbi",	0x4,	0x8,	0x7,	0x5,	A64_SYS_REGOPT_DEFAULT},
	{"vale3",		"tlbi",	0x6,	0x8,	0x7,	0x5,	A64_SYS_REGOPT_DEFAULT},
	{"vmalls12e1",		"tlbi",	0x4,	0x8,	0x7,	0x6,	A64_SYS_REGOPT_NEVER},
	{"vaale1",		"tlbi",	0x0,	0x8,	0x7,	0x7,	A64_SYS_REGOPT_DEFAULT},
	{NULL,			NULL,	0,	0,	0,	0,	0}
};
/* END CSTYLED */

static const char *a64_system_option_names[] = {
	NULL,
	"oshld",
	"oshst",
	"osh",
	NULL,
	"nshld",
	"nshst",
	"nsh",
	NULL,
	"ishld",
	"ishst",
	"ish",
	NULL,
	"ld",
	"st",
	"sy"
};

/* BEGIN CSTYLED */
static a64_system_opcode_entry_t a64_system_opcodes[] = {
	{0x0038f01f,	0x0000401f,	"msr",		A64_SYS_TYPE_PSTATE},	/* MSR <pstatefield>, #<imm> (op1:op2, CRm) */
	/* HINT aliases come first */
	{0x003fffff,	0x0003201f,	"nop",		A64_SYS_TYPE_DIRECT},	/* NOP */
	{0x003fffff,	0x0003203f,	"yield",	A64_SYS_TYPE_DIRECT},	/* YIELD */
	{0x003fffff,	0x0003205f,	"wfe",		A64_SYS_TYPE_DIRECT},	/* WFE */
	{0x003fffff,	0x0003207f,	"wfi",		A64_SYS_TYPE_DIRECT},	/* WFI */
	{0x003fffff,	0x0003209f,	"sev",		A64_SYS_TYPE_DIRECT},	/* SEV */
	{0x003fffff,	0x000320bf,	"sevl",		A64_SYS_TYPE_DIRECT},	/* SEVL */
	{0x003ff01f,	0x0003201f,	"hint",		A64_SYS_TYPE_IMM1},	/* HINT #<imm> (CRm:op2) */
	{0x003ff0ff,	0x0003305f,	"clrex",	A64_SYS_TYPE_IMM2},	/* CLREX {#<imm>} (CRm, default 15) */
	{0x003ff0ff,	0x0003309f,	"dsb",		A64_SYS_TYPE_IMMOPT},	/* DSB <option>|#<imm> (CRm) */
	{0x003ff0ff,	0x000330bf,	"dmb",		A64_SYS_TYPE_IMMOPT},	/* DMB <option>|#<imm> (CRm) */
	{0x003ff0ff,	0x000330df,	"isb",		A64_SYS_TYPE_IMM2},	/* ISB {<option>|#<imm>} (CRm) */
	/* SYS has at/dc/ic/tlbi aliases which are looked up in a64_system_sysaliases */
	{0x00380000,	0x00080000,	"sys",		A64_SYS_TYPE_SYS},	/* SYS #<op1>, <Cn>, <Cm>, #<op2>{, <Xt>} */
	{0x00300000,	0x00100000,	"msr",		A64_SYS_TYPE_MSR},	/* MSR <systemreg>, <Xt> (o0:op1:CRn:CRm:op2) */
	{0x00380000,	0x00280000,	"sysl",		A64_SYS_TYPE_SYSL},	/* SYSL <Xt>, #<op1>, <Cn>, <Cm>, #<op2> */
	{0x00300000,	0x00300000,	"mrs",		A64_SYS_TYPE_MRS},	/* MRS <Xt>, <systemreg> (o0:op1:CRn:CRm:op2 */
	{0xffffffff,	0xffffffff,	NULL,		0}
};
/* END CSTYLED */

typedef struct a64_system_sysreg_entry {
	const char *regname;
	uint8_t crn;
	uint8_t op1;
	uint8_t crm;
	uint8_t op2;
} a64_system_sysreg_entry_t;

static a64_system_sysreg_entry_t a64_system_sysregs_debug[] = {
	{"osdtrrx_el1",		0,	0,	0,	2},
	{"mdccint_el1",		0,	0,	2,	0},
	{"mdscr_el1",		0,	0,	2,	2},
	{"osdtrtx_el1",		0,	0,	3,	2},
	{"oseccr_el1",		0,	0,	6,	2},
	{"dbgbvr0_el1",		0,	0,	0,	4},
	{"dbgbvr1_el1",		0,	0,	1,	4},
	{"dbgbvr2_el1",		0,	0,	2,	4},
	{"dbgbvr3_el1",		0,	0,	3,	4},
	{"dbgbvr4_el1",		0,	0,	4,	4},
	{"dbgbvr5_el1",		0,	0,	5,	4},
	{"dbgbvr6_el1",		0,	0,	6,	4},
	{"dbgbvr7_el1",		0,	0,	7,	4},
	{"dbgbvr8_el1",		0,	0,	8,	4},
	{"dbgbvr9_el1",		0,	0,	9,	4},
	{"dbgbvr10_el1",	0,	0,	10,	4},
	{"dbgbvr11_el1",	0,	0,	11,	4},
	{"dbgbvr12_el1",	0,	0,	12,	4},
	{"dbgbvr13_el1",	0,	0,	13,	4},
	{"dbgbvr14_el1",	0,	0,	14,	4},
	{"dbgbvr15_el1",	0,	0,	15,	4},
	{"dbgbcr0_el1",		0,	0,	0,	5},
	{"dbgbcr1_el1",		0,	0,	1,	5},
	{"dbgbcr2_el1",		0,	0,	2,	5},
	{"dbgbcr3_el1",		0,	0,	3,	5},
	{"dbgbcr4_el1",		0,	0,	4,	5},
	{"dbgbcr5_el1",		0,	0,	5,	5},
	{"dbgbcr6_el1",		0,	0,	6,	5},
	{"dbgbcr7_el1",		0,	0,	7,	5},
	{"dbgbcr8_el1",		0,	0,	8,	5},
	{"dbgbcr9_el1",		0,	0,	9,	5},
	{"dbgbcr10_el1",	0,	0,	10,	5},
	{"dbgbcr11_el1",	0,	0,	11,	5},
	{"dbgbcr12_el1",	0,	0,	12,	5},
	{"dbgbcr13_el1",	0,	0,	13,	5},
	{"dbgbcr14_el1",	0,	0,	14,	5},
	{"dbgbcr15_el1",	0,	0,	15,	5},
	{"dbgwvr0_el1",		0,	0,	0,	6},
	{"dbgwvr1_el1",		0,	0,	1,	6},
	{"dbgwvr2_el1",		0,	0,	2,	6},
	{"dbgwvr3_el1",		0,	0,	3,	6},
	{"dbgwvr4_el1",		0,	0,	4,	6},
	{"dbgwvr5_el1",		0,	0,	5,	6},
	{"dbgwvr6_el1",		0,	0,	6,	6},
	{"dbgwvr7_el1",		0,	0,	7,	6},
	{"dbgwvr8_el1",		0,	0,	8,	6},
	{"dbgwvr9_el1",		0,	0,	9,	6},
	{"dbgwvr10_el1",	0,	0,	10,	6},
	{"dbgwvr11_el1",	0,	0,	11,	6},
	{"dbgwvr12_el1",	0,	0,	12,	6},
	{"dbgwvr13_el1",	0,	0,	13,	6},
	{"dbgwvr14_el1",	0,	0,	14,	6},
	{"dbgwvr15_el1",	0,	0,	15,	6},
	{"dbgwcr0_el1",		0,	0,	0,	7},
	{"dbgwcr1_el1",		0,	0,	1,	7},
	{"dbgwcr2_el1",		0,	0,	2,	7},
	{"dbgwcr3_el1",		0,	0,	3,	7},
	{"dbgwcr4_el1",		0,	0,	4,	7},
	{"dbgwcr5_el1",		0,	0,	5,	7},
	{"dbgwcr6_el1",		0,	0,	6,	7},
	{"dbgwcr7_el1",		0,	0,	7,	7},
	{"dbgwcr8_el1",		0,	0,	8,	7},
	{"dbgwcr9_el1",		0,	0,	9,	7},
	{"dbgwcr10_el1",	0,	0,	10,	7},
	{"dbgwcr11_el1",	0,	0,	11,	7},
	{"dbgwcr12_el1",	0,	0,	12,	7},
	{"dbgwcr13_el1",	0,	0,	13,	7},
	{"dbgwcr14_el1",	0,	0,	14,	7},
	{"dbgwcr15_el1",	0,	0,	15,	7},
	{"mdrar_el1",		1,	0,	0,	0},
	{"oslar_el1",		1,	0,	0,	4},
	{"oslsr_el1",		1,	0,	1,	4},
	{"osdlr_el1",		1,	0,	3,	4},
	{"dbgprcr_el1",		1,	0,	4,	4},
	{"dbgclaimset_el1",	7,	0,	8,	6},
	{"dbgclaimclr_el1",	7,	0,	9,	6},
	{"dbgauthstatus_el1",	7,	0,	14,	6},
	{"mdccsr_el0",		0,	3,	1,	0},
	{"dbgdtr_el0",		0,	3,	4,	0},
	{"dbgdtrrx_el0",	0,	3,	5,	0},
	{"dbgvcr32_el2",	0,	4,	7,	0},
	/* AArch32 Execution environment registers */
	{"teecr32_el1",		0,	2,	0,	0},
	{"teehbr32_el1",	1,	2,	0,	0},
	{NULL,			0,	0,	0,	0}
};

static a64_system_sysreg_entry_t a64_system_sysregs_nondebug[] = {
	{"midr_el1",		0,	0,	0,	0},
	{"mpidr_el1",		0,	0,	0,	5},
	{"revidr_el1",		0,	0,	0,	6},
	{"id_pfr0_el1",		0,	0,	1,	0},
	{"id_pfr1_el1",		0,	0,	1,	1},
	{"id_dfr0_el1",		0,	0,	1,	2},
	{"id_afr0_el1",		0,	0,	1,	3},
	{"id_mmfr0_el1",	0,	0,	1,	4},
	{"id_mmfr1_el1",	0,	0,	1,	5},
	{"id_mmfr2_el1",	0,	0,	1,	6},
	{"id_mmfr3_el1",	0,	0,	1,	7},
	{"id_isar0_el1",	0,	0,	2,	0},
	{"id_isar1_el1",	0,	0,	2,	1},
	{"id_isar2_el1",	0,	0,	2,	2},
	{"id_isar3_el1",	0,	0,	2,	3},
	{"id_isar4_el1",	0,	0,	2,	4},
	{"id_isar5_el1",	0,	0,	2,	5},
	{"id_mmfr4_el1",	0,	0,	2,	6},
	{"mvfr0_el1",		0,	0,	3,	0},
	{"mvfr1_el1",		0,	0,	3,	1},
	{"mvfr2_el1",		0,	0,	3,	2},
	{"id_aa64pfr0_el1",	0,	0,	4,	0},
	{"id_aa64pfr1_el1",	0,	0,	4,	1},
	{"id_aa64dfr0_el1",	0,	0,	5,	0},
	{"id_aa64dfr1_el1",	0,	0,	5,	1},
	{"id_aa64afr0_el1",	0,	0,	5,	4},
	{"id_aa64afr1_el1",	0,	0,	5,	5},
	{"id_aa64isar0_el1",	0,	0,	6,	0},
	{"id_aa64isar1_el1",	0,	0,	6,	1},
	{"id_aa64mmfr0_el1",	0,	0,	7,	0},
	{"id_aa64mmfr1_el1",	0,	0,	7,	1},
	{"ccsidr_el1",		0,	1,	0,	0},
	{"clidr_el1",		0,	1,	0,	1},
	{"aidr_el1",		0,	1,	0,	7},
	{"csselr_el1",		0,	2,	0,	0},
	{"ctr_el0",		0,	3,	0,	1},
	{"dczid_el0",		0,	3,	0,	7},
	{"vpidr_el2",		0,	4,	0,	0},
	{"vmpidr_el2",		0,	4,	0,	5},
	{"sctlr_el1",		1,	0,	0,	0},
	{"actlr_el1",		1,	0,	0,	1},
	{"cpacr_el1",		1,	0,	0,	2},
	{"sctlr_el2",		1,	4,	0,	0},
	{"actlr_el2",		1,	4,	0,	1},
	{"hcr_el2",		1,	4,	1,	0},
	{"mdcr_el2",		1,	4,	1,	1},
	{"cptr_el2",		1,	4,	1,	2},
	{"hstr_el2",		1,	4,	1,	3},
	{"hacr_el2",		1,	4,	1,	7},
	{"sctlr_el3",		1,	6,	0,	0},
	{"actlr_el3",		1,	6,	0,	1},
	{"scr_el3",		1,	6,	1,	0},
	{"sder32_el3",		1,	6,	1,	1},
	{"cptr_el3",		1,	6,	1,	2},
	{"mdcr_el3",		1,	6,	3,	1},
	{"ttbr0_el1",		2,	0,	0,	0},
	{"ttbr1_el1",		2,	0,	0,	1},
	{"tcr_el1",		2,	0,	0,	2},
	{"ttbr0_el2",		2,	4,	0,	0},
	{"tcr_el2",		2,	4,	0,	2},
	{"vttbr_el2",		2,	4,	1,	0},
	{"vtcr_el2",		2,	4,	1,	2},
	{"ttbr0_el3",		2,	6,	0,	0},
	{"tcr_el3",		2,	6,	0,	2},
	{"dacr32_el2",		3,	4,	0,	0},
	{"spsr_el1",		4,	0,	0,	0},
	{"elr_el1",		4,	0,	0,	1},
	{"sp_el0",		4,	0,	1,	0},
	{"spsel",		4,	0,	2,	0},
	{"currentel",		4,	0,	2,	2},
	{"nzcv",		4,	3,	2,	0},
	{"daif",		4,	3,	2,	1},
	{"fpcr",		4,	3,	4,	0},
	{"fpsr",		4,	3,	4,	1},
	{"dspsr_el0",		4,	3,	5,	0},
	{"dlr_el0",		4,	3,	5,	1},
	{"spsr_el2",		4,	4,	0,	0},
	{"elr_el2",		4,	4,	0,	1},
	{"sp_el1",		4,	4,	1,	0},
	{"spsr_irq",		4,	4,	3,	0},
	{"spsr_abt",		4,	4,	3,	1},
	{"spsr_und",		4,	4,	3,	2},
	{"spsr_fiq",		4,	4,	3,	3},
	{"spsr_el3",		4,	6,	0,	0},
	{"elr_el3",		4,	6,	0,	1},
	{"sp_el2",		4,	6,	1,	0},
	{"ifsr32_el2",		5,	4,	0,	1},
	{"fpexc32_el2",		5,	4,	3,	0},
	{"afsr0_el1",		5,	0,	1,	0},
	{"afsr1_el1",		5,	0,	1,	1},
	{"esr_el1",		5,	0,	2,	0},
	{"afsr0_el2",		5,	4,	1,	0},
	{"afsr1_el2",		5,	4,	1,	1},
	{"esr_el2",		5,	4,	2,	0},
	{"afsr0_el3",		5,	6,	1,	0},
	{"afsr1_el3",		5,	6,	1,	1},
	{"esr_el3",		5,	6,	2,	0},
	{"far_el1",		6,	0,	0,	0},
	{"far_el2",		6,	4,	0,	0},
	{"hpfar_el2",		6,	4,	0,	4},
	{"far_el3",		6,	6,	0,	0},
	{"par_el1",		7,	0,	4,	0},
	{"pmintenset_el1",	9,	0,	14,	1},
	{"pmintenclr_el1",	9,	0,	14,	2},
	{"pmcr_el0",		9,	3,	12,	0},
	{"pmcntenset_el0",	9,	3,	12,	1},
	{"pmcntenclr_el0",	9,	3,	12,	2},
	{"pmovsclr_el0",	9,	3,	12,	3},
	{"pmswinc_el0",		9,	3,	12,	4},
	{"pmselr_el0",		9,	3,	12,	5},
	{"pmceid0_el0",		9,	3,	12,	6},
	{"pmceid1_el0",		9,	3,	12,	7},
	{"pmccntr_el0",		9,	3,	13,	0},
	{"pmxevtyper_el0",	9,	3,	13,	1},
	{"pmxevcntr_el0",	9,	3,	13,	2},
	{"pmuserenr_el0",	9,	3,	14,	0},
	{"pmovsset_el0",	9,	3,	14,	3},
	{"mair_el1",		10,	0,	2,	0},
	{"amair_el1",		10,	0,	3,	0},
	{"mair_el2",		10,	4,	2,	0},
	{"amair_el2",		10,	4,	3,	0},
	{"mair_el3",		10,	6,	2,	0},
	{"amair_el3",		10,	6,	3,	0},
	{"vbar_el1",		12,	0,	0,	0},
	{"rvbar_el1",		12,	0,	0,	1},
	{"rmr_el1",		12,	0,	0,	2},
	{"isr_el1",		12,	0,	1,	0},
	{"vbar_el2",		12,	4,	0,	0},
	{"rvbar_el2",		12,	4,	0,	1},
	{"rmr_el2",		12,	4,	0,	2},
	{"vbar_el3",		12,	6,	0,	0},
	{"rvbar_el3",		12,	6,	0,	1},
	{"rmr_el3",		12,	6,	0,	2},
	{"contextidr_el1",	13,	0,	0,	1},
	{"tpidr_el1",		13,	0,	0,	4},
	{"tpidr_el0",		13,	3,	0,	2},
	{"tpidrro_el0",		13,	3,	0,	3},
	{"tpidr_el2",		13,	4,	0,	2},
	{"tpidr_el3",		13,	6,	0,	2},
	{"cntkctl_el1",		14,	0,	1,	0},
	{"cntfrq_el0",		14,	3,	0,	0},
	{"cntpct_el0",		14,	3,	0,	1},
	{"cntvct_el0",		14,	3,	0,	2},
	{"cntp_tval_el0",	14,	3,	2,	0},
	{"cntp_ctl_el0",	14,	3,	2,	1},
	{"cntp_cval_el0",	14,	3,	2,	2},
	{"cntv_tval_el0",	14,	3,	3,	0},
	{"cntv_ctl_el0",	14,	3,	3,	1},
	{"cntv_cval_el0",	14,	3,	3,	2},
	{"cnthctl_el2",		14,	4,	1,	0},
	{"cnthp_tval_el2",	14,	4,	2,	0},
	{"cnthp_ctl_el2",	14,	4,	2,	1},
	{"cnthp_cval_el2",	14,	4,	2,	2},
	{"cntvoff_el2",		14,	4,	0,	3},
	{"cntps_tval_el1",	14,	7,	2,	0},
	{"cntps_ctl_el1",	14,	7,	2,	1},
	{"cntps_cval_el1",	14,	7,	2,	2},
	{"pmevcntr0_el0",	14,	3,	8,	0},
	{"pmevcntr1_el0",	14,	3,	8,	1},
	{"pmevcntr2_el0",	14,	3,	8,	2},
	{"pmevcntr3_el0",	14,	3,	8,	3},
	{"pmevcntr4_el0",	14,	3,	8,	4},
	{"pmevcntr5_el0",	14,	3,	8,	5},
	{"pmevcntr6_el0",	14,	3,	8,	6},
	{"pmevcntr7_el0",	14,	3,	8,	7},
	{"pmevcntr8_el0",	14,	3,	9,	0},
	{"pmevcntr9_el0",	14,	3,	9,	1},
	{"pmevcntr10_el0",	14,	3,	9,	2},
	{"pmevcntr11_el0",	14,	3,	9,	3},
	{"pmevcntr12_el0",	14,	3,	9,	4},
	{"pmevcntr13_el0",	14,	3,	9,	5},
	{"pmevcntr14_el0",	14,	3,	9,	6},
	{"pmevcntr15_el0",	14,	3,	9,	7},
	{"pmevcntr16_el0",	14,	3,	10,	0},
	{"pmevcntr17_el0",	14,	3,	10,	1},
	{"pmevcntr18_el0",	14,	3,	10,	2},
	{"pmevcntr19_el0",	14,	3,	10,	3},
	{"pmevcntr20_el0",	14,	3,	10,	4},
	{"pmevcntr21_el0",	14,	3,	10,	5},
	{"pmevcntr22_el0",	14,	3,	10,	6},
	{"pmevcntr23_el0",	14,	3,	10,	7},
	{"pmevcntr24_el0",	14,	3,	11,	0},
	{"pmevcntr25_el0",	14,	3,	11,	1},
	{"pmevcntr26_el0",	14,	3,	11,	2},
	{"pmevcntr27_el0",	14,	3,	11,	3},
	{"pmevcntr28_el0",	14,	3,	11,	4},
	{"pmevcntr29_el0",	14,	3,	11,	5},
	{"pmevcntr30_el0",	14,	3,	11,	6},
	{"pmevtyper0_el0",	14,	3,	12,	0},
	{"pmevtyper1_el0",	14,	3,	12,	1},
	{"pmevtyper2_el0",	14,	3,	12,	2},
	{"pmevtyper3_el0",	14,	3,	12,	3},
	{"pmevtyper4_el0",	14,	3,	12,	4},
	{"pmevtyper5_el0",	14,	3,	12,	5},
	{"pmevtyper6_el0",	14,	3,	12,	6},
	{"pmevtyper7_el0",	14,	3,	12,	7},
	{"pmevtyper8_el0",	14,	3,	13,	0},
	{"pmevtyper9_el0",	14,	3,	13,	1},
	{"pmevtyper10_el0",	14,	3,	13,	2},
	{"pmevtyper11_el0",	14,	3,	13,	3},
	{"pmevtyper12_el0",	14,	3,	13,	4},
	{"pmevtyper13_el0",	14,	3,	13,	5},
	{"pmevtyper14_el0",	14,	3,	13,	6},
	{"pmevtyper15_el0",	14,	3,	13,	7},
	{"pmevtyper16_el0",	14,	3,	14,	0},
	{"pmevtyper17_el0",	14,	3,	14,	1},
	{"pmevtyper18_el0",	14,	3,	14,	2},
	{"pmevtyper19_el0",	14,	3,	14,	3},
	{"pmevtyper20_el0",	14,	3,	14,	4},
	{"pmevtyper21_el0",	14,	3,	14,	5},
	{"pmevtyper22_el0",	14,	3,	14,	6},
	{"pmevtyper23_el0",	14,	3,	14,	7},
	{"pmevtyper24_el0",	14,	3,	15,	0},
	{"pmevtyper25_el0",	14,	3,	15,	1},
	{"pmevtyper26_el0",	14,	3,	15,	2},
	{"pmevtyper27_el0",	14,	3,	15,	3},
	{"pmevtyper28_el0",	14,	3,	15,	4},
	{"pmevtyper29_el0",	14,	3,	15,	5},
	{"pmevtyper30_el0",	14,	3,	15,	6},
	{"pmccfiltr_el0",	14,	3,	15,	7},
	{NULL,			0,	0,	0,	0}
};

#define	A64_SYS_OP0_MASK	0x00180000
#define	A64_SYS_OP0_SHIFT	19
#define	A64_SYS_OP1_MASK	0x00070000
#define	A64_SYS_OP1_SHIFT	16
#define	A64_SYS_CRN_MASK	0x0000f000
#define	A64_SYS_CRN_SHIFT	12
#define	A64_SYS_CRM_MASK	0x00000f00
#define	A64_SYS_CRM_SHIFT	8
#define	A64_SYS_OP2_MASK	0x000000e0
#define	A64_SYS_OP2_SHIFT	5
#define	A64_SYS_RT_MASK		0x0000001f
#define	A64_SYS_RT_SHIFT	0

static int
a64_dis_system(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen)
{
	a64_system_opcode_entry_t *op;
	a64_system_sysalias_entry_t *sa;
	a64_system_sysreg_entry_t *sr;
	a64_reg_t rt;
	const char *optname;
	uint16_t imm;
	uint8_t op0, op1, op2, crn, crm;
	size_t len;

	for (op = a64_system_opcodes; op->mask != 0xffffffff; op++) {
		if ((in & op->mask) == op->targ)
			break;
	}

	if (op->name == NULL) {
		len = dis_snprintf(buf, buflen, "nope");
		return (0);
	}

	/* Extract sections used by most operation types. */
	op0 = (in & A64_SYS_OP0_MASK) >> A64_SYS_OP0_SHIFT;
	op1 = (in & A64_SYS_OP1_MASK) >> A64_SYS_OP1_SHIFT;
	op2 = (in & A64_SYS_OP2_MASK) >> A64_SYS_OP2_SHIFT;
	crm = (in & A64_SYS_CRM_MASK) >> A64_SYS_CRM_SHIFT;
	crn = (in & A64_SYS_CRN_MASK) >> A64_SYS_CRN_SHIFT;

	bzero(&rt, sizeof (rt));
	rt.id = (in & A64_SYS_RT_MASK) >> A64_SYS_RT_SHIFT;
	rt.width = A64_REGWIDTH_64;

	switch (op->type) {
	case A64_SYS_TYPE_DIRECT:
		len = dis_snprintf(buf, buflen, "%s", op->name);
		break;
	case A64_SYS_TYPE_PSTATE:
		if (op1 == 0 && op2 == 5)
			optname = "spsel";
		else if (op1 == 3 && op2 == 6)
			optname = "daifset";
		else if (op1 == 3 && op2 == 7)
			optname = "daifclr";
		else
			return (-1);
		len = dis_snprintf(buf, buflen, "%s %s, #0x%x", op->name,
		    optname, crm);
		break;
	case A64_SYS_TYPE_IMM1:
		imm = (in & (A64_SYS_CRM_MASK|A64_SYS_OP2_MASK))
		    >> A64_SYS_OP2_SHIFT;
		len = dis_snprintf(buf, buflen, "%s #0x%x", op->name, imm);
		break;
	case A64_SYS_TYPE_IMM2:
		/* CRm == 15 is the default */
		if (crm == 0xf) {
			len = dis_snprintf(buf, buflen, "%s", op->name);
		} else {
			len = dis_snprintf(buf, buflen, "%s #0x%x", op->name,
			    crm);
		}
		break;
	case A64_SYS_TYPE_IMMOPT:
		optname = a64_system_option_names[crm];
		if (optname) {
			len = dis_snprintf(buf, buflen, "%s %s", op->name,
			    optname);
		} else {
			len = dis_snprintf(buf, buflen, "%s #0x%02x", op->name,
			    crm);
		}
		break;
	case A64_SYS_TYPE_SYS:
		/* Look for SYS aliases and print accordingly if found */
		for (sa = a64_system_sysaliases; sa->opname != NULL; sa++) {
			if (sa->op1 == op1 && sa->op2 == op2 &&
			    sa->crm == crm && sa->crn == crn) {
				break;
			}
		}
		if (sa->opname) {
			if (sa->regopt == A64_SYS_REGOPT_NEVER ||
			    (sa->regopt == A64_SYS_REGOPT_DEFAULT &&
			    rt.id == A64_REG_ZR)) {
				len = dis_snprintf(buf, buflen, "%s %s",
				    sa->opalias, sa->opname);
			} else {
				len = dis_snprintf(buf, buflen, "%s %s, %s",
				    sa->opalias, sa->opname, a64_reg_name(rt));
			}
			break;
		}

		if (rt.id != A64_REG_ZR) {
			len = dis_snprintf(buf, buflen,
			    "%s #%d, C%d, C%d, #%d, %s",
			    op->name, op1, crn, crm, op2, a64_reg_name(rt));
		} else {
			len = dis_snprintf(buf, buflen,
			    "%s #%d, C%d, C%d, #%d",
			    op->name, op1, crn, crm, op2);
		}
		break;
	case A64_SYS_TYPE_MSR:
		if (op0 == 2)
			sr = a64_system_sysregs_debug;
		else
			sr = a64_system_sysregs_nondebug;
		for (; sr->regname != NULL; sr++) {
			if (sr->crn == crn && sr->op1 == op1 &&
			    sr->crm == crm && sr->op2 == op2)
				break;
		}
		if (sr->regname == NULL) {
			/* GNU binutils compatibility for impl-def registers */
			len = dis_snprintf(buf, buflen,
			    "%s s%u_%u_c%u_c%u_%u, %s",
			    op->name, op0, op1, crn, crm, op2,
			    a64_reg_name(rt));
		} else {
			len = dis_snprintf(buf, buflen,
			    "%s %s, %s",
			    op->name, sr->regname, a64_reg_name(rt));
		}
		break;
	case A64_SYS_TYPE_MRS:
		if (op0 == 2)
			sr = a64_system_sysregs_debug;
		else
			sr = a64_system_sysregs_nondebug;
		for (; sr->regname != NULL; sr++) {
			if (sr->crn == crn && sr->op1 == op1 &&
			    sr->crm == crm && sr->op2 == op2) {
				break;
			}
		}
		if (sr->regname == NULL) {
			/* GNU binutils compatibility for impl-def registers */
			len = dis_snprintf(buf, buflen,
			    "%s %s, s%u_%u_c%u_c%u_%u",
			    op->name, a64_reg_name(rt), op0, op1, crn, crm,
			    op2);
		} else {
			len = dis_snprintf(buf, buflen,
			    "%s %s, %s", op->name,
			    a64_reg_name(rt), sr->regname);
		}
		break;
	case A64_SYS_TYPE_SYSL:
		len = dis_snprintf(buf, buflen, "%s %s, #%d, C%d, C%d, #%d",
		    op->name, a64_reg_name(rt), op1, crn, crm, op2);
		break;
	}

	return (len < buflen ? 0 : -1);
}

/*
 * The A64 instruction set packs into a uint32_t with variable length encoding,
 * making our job somewhat convoluted to decode.
 *
 * Our first job is to decode the current Encoding Group based on bits 28:25.
 *
 *   28 27 26 25	Encoding Group
 *    0  0  -  -	UNALLOCATED
 *    1  0  0  -	Data processing - immediate
 *    1  0  1  -	Branch, exceptions, system
 *    -  1  -  0	Loads and stores
 *    -  1  0  1	Data processing - register
 *    0  1  1  1	Data processing - SIMD/FP
 *    1  1  1  1	Data processing - SIMD/FP
 */
#define	A64_ENCGRP_DPIMM_MASK		0x1c000000
#define	A64_ENCGRP_DPIMM_TARG		0x10000000
#define	A64_ENCGRP_BRANCH_MASK		0x1c000000
#define	A64_ENCGRP_BRANCH_TARG		0x14000000
#define	A64_ENCGRP_EXCPTN_MASK		0xff000000
#define	A64_ENCGRP_EXCPTN_TARG		0xd4000000
#define	A64_ENCGRP_SYSTEM_MASK		0xff000000
#define	A64_ENCGRP_SYSTEM_TARG		0xd5000000
#define	A64_ENCGRP_LDSTR_MASK		0x0a000000
#define	A64_ENCGRP_LDSTR_TARG		0x08000000
#define	A64_ENCGRP_LSSIMD_MASK		0x3c000000
#define	A64_ENCGRP_LSSIMD_TARG		0x0c000000
#define	A64_ENCGRP_DPREG_MASK		0x0e000000
#define	A64_ENCGRP_DPREG_TARG		0x0a000000
#define	A64_ENCGRP_DPSIMD_MASK		0x0e000000
#define	A64_ENCGRP_DPSIMD_TARG		0x0e000000


/*
 * AdvSIMD load/store instructions, covering the following classes:
 *
 *   C3.3.1 AdvSIMD load/store multiple structures
 *   C3.3.2 AdvSIMD load/store multiple structures (post-indexed)
 *   C3.3.3 AdvSIMD load/store single structure
 *   C3.3.4 AdvSIMD load/store single structure (post-indexed)
 *
 * Instruction encoding:
 *
 *   |31|30|29         23|22|21        16|15     12|11  10|9    5|4    0|
 *   +--+--+-------------+--+------------+---------+------+------+------+
 *   | 0| Q|0 0 1 1 0 0 0| L| 0 0 0 0 0 0|  opcode | size |  Rn  |  Rt  |C3.3.1
 *   +--+--+-------------+--+------------+---------+------+------+------+
 *   |31|30|29         23|22|21|20     16|15     12|11  10|9    5|4    0|
 *   +--+--+-------------+--+--+---------+---------+------+------+------+
 *   | 0| Q|0 0 1 1 0 0 1| L| 0|    Rm   |  opcode | size |  Rn  |  Rt  |C3.3.2
 *   +--+--+-------------+--+--+---------+------+--+------+------+------+
 *   |31|30|29         23|22|21|20     16|15  13|12|11  10|9    5|4    0|
 *   +--+--+-------------+--+--+---------+------+--+------+------+------+
 *   | 0| Q|0 0 1 1 0 1 0| L| R|0 0 0 0 0|opcode| S| size |  Rn  |  Rt  |C3.3.3
 *   +--+--+-------------+--+--+---------+------+--+------+------+------+
 *   |31|30|29         23|22|21|20     16|15  13|12|11  10|9    5|4    0|
 *   +--+--+-------------+--+--+---------+------+--+------+------+------+
 *   | 0| Q|0 0 1 1 0 1 1| L| R|    Rm   |opcode| S| size |  Rn  |  Rt  |C3.3.4
 *
 * To handle these classes we have a lookup table (a64_ldstr_simd_opcodes)
 * which contains enough information for us to print everything at the end.
 */

/*
 * Output flags for the various types of instruction.
 */
typedef enum a64_ldstr_simd_flags {
	A64_LDSTR_SIMD_IX8  = 0x01,	/* 8-bit element index */
	A64_LDSTR_SIMD_IX16 = 0x02,	/* 16-bit element index */
	A64_LDSTR_SIMD_IX32 = 0x04,	/* 32-bit element index */
	A64_LDSTR_SIMD_IX64 = 0x08,	/* 64-bit element index */
	A64_LDSTR_SIMD_ISIX = 0x0f,	/* Mask to check if index is used */
	A64_LDSTR_SIMD_PIB  = 0x10,	/* Post-index based on bit size */
	A64_LDSTR_SIMD_PIQ  = 0x20,	/* Post-index based on "Q" bit */
	A64_LDSTR_SIMD_PIS  = 0x40,	/* Post-index based on "size" */
	A64_LDSTR_SIMD_ISPI = 0xf0,	/* Mask to check if P-i is used */
} a64_ldstr_simd_flags_t;
/* Combined flags to reduce lookup table formatting size */
#define	A64_LDSTR_SIMD_PIB8	(A64_LDSTR_SIMD_PIB|A64_LDSTR_SIMD_IX8)
#define	A64_LDSTR_SIMD_PIB16	(A64_LDSTR_SIMD_PIB|A64_LDSTR_SIMD_IX16)
#define	A64_LDSTR_SIMD_PIB32	(A64_LDSTR_SIMD_PIB|A64_LDSTR_SIMD_IX32)
#define	A64_LDSTR_SIMD_PIB64	(A64_LDSTR_SIMD_PIB|A64_LDSTR_SIMD_IX64)

typedef struct a64_ldstr_simd_opcode_entry {
	uint32_t mask;			/* opcode mask */
	uint32_t targ;			/* target value */
	const char *name;		/* opcode name */
	uint8_t nregs;			/* number of registers */
	a64_ldstr_simd_flags_t flags;	/* output flags */
} a64_ldstr_simd_opcode_entry_t;

static a64_ldstr_simd_opcode_entry_t a64_ldstr_simd_opcodes[] = {
	/* C3.3.1 AdvSIMD load/store multiple structures */
	{0x01c0f000,	0x00000000,	"st4",	4,	0},
	{0x01c0f000,	0x00002000,	"st1",	4,	0},
	{0x01c0f000,	0x00004000,	"st3",	3,	0},
	{0x01c0f000,	0x00006000,	"st1",	3,	0},
	{0x01c0f000,	0x00007000,	"st1",	1,	0},
	{0x01c0f000,	0x00008000,	"st2",	2,	0},
	{0x01c0f000,	0x0000a000,	"st1",	2,	0},
	{0x01c0f000,	0x00400000,	"ld4",	4,	0},
	{0x01c0f000,	0x00402000,	"ld1",	4,	0},
	{0x01c0f000,	0x00404000,	"ld3",	3,	0},
	{0x01c0f000,	0x00406000,	"ld1",	3,	0},
	{0x01c0f000,	0x00407000,	"ld1",	1,	0},
	{0x01c0f000,	0x00408000,	"ld2",	2,	0},
	{0x01c0f000,	0x0040a000,	"ld1",	2,	0},
	/* C3.3.2 AdvSIMD load/store multiple structures (post-indexed) */
	{0x01c0f000,	0x00800000,	"st4",	4,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00802000,	"st1",	4,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00804000,	"st3",	3,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00806000,	"st1",	3,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00807000,	"st1",	1,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00808000,	"st2",	2,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x0080a000,	"st1",	2,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00c00000,	"ld4",	4,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00c02000,	"ld1",	4,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00c04000,	"ld3",	3,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00c06000,	"ld1",	3,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00c07000,	"ld1",	1,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00c08000,	"ld2",	2,	A64_LDSTR_SIMD_PIQ},
	{0x01c0f000,	0x00c0a000,	"ld1",	2,	A64_LDSTR_SIMD_PIQ},
	/* C3.3.3 AdvSIMD load/store single structure */
	{0x01e0e000,	0x01000000,	"st1",	1,	A64_LDSTR_SIMD_IX8},
	{0x01e0e000,	0x01002000,	"st3",	3,	A64_LDSTR_SIMD_IX8},
	{0x01e0e400,	0x01004000,	"st1",	1,	A64_LDSTR_SIMD_IX16},
	{0x01e0e400,	0x01006000,	"st3",	3,	A64_LDSTR_SIMD_IX16},
	{0x01e0ec00,	0x01008000,	"st1",	1,	A64_LDSTR_SIMD_IX32},
	{0x01e0fc00,	0x01008400,	"st1",	1,	A64_LDSTR_SIMD_IX64},
	{0x01e0ec00,	0x0100a000,	"st3",	3,	A64_LDSTR_SIMD_IX32},
	{0x01e0fc00,	0x0100a400,	"st3",	3,	A64_LDSTR_SIMD_IX64},
	{0x01e0e000,	0x01200000,	"st2",	2,	A64_LDSTR_SIMD_IX8},
	{0x01e0e000,	0x01202000,	"st4",	4,	A64_LDSTR_SIMD_IX8},
	{0x01e0e400,	0x01204000,	"st2",	2,	A64_LDSTR_SIMD_IX16},
	{0x01e0e400,	0x01206000,	"st4",	4,	A64_LDSTR_SIMD_IX16},
	{0x01e0ec00,	0x01208000,	"st2",	2,	A64_LDSTR_SIMD_IX32},
	{0x01e0fc00,	0x01208400,	"st2",	2,	A64_LDSTR_SIMD_IX64},
	{0x01e0ec00,	0x0120a000,	"st4",	4,	A64_LDSTR_SIMD_IX32},
	{0x01e0fc00,	0x0120a400,	"st4",	4,	A64_LDSTR_SIMD_IX64},
	{0x01e0e000,	0x01400000,	"ld1",	1,	A64_LDSTR_SIMD_IX8},
	{0x01e0e000,	0x01402000,	"ld3",	3,	A64_LDSTR_SIMD_IX8},
	{0x01e0e400,	0x01404000,	"ld1",	1,	A64_LDSTR_SIMD_IX16},
	{0x01e0e400,	0x01406000,	"ld3",	3,	A64_LDSTR_SIMD_IX16},
	{0x01e0ec00,	0x01408000,	"ld1",	1,	A64_LDSTR_SIMD_IX32},
	{0x01e0fc00,	0x01408400,	"ld1",	1,	A64_LDSTR_SIMD_IX64},
	{0x01e0ec00,	0x0140a000,	"ld3",	3,	A64_LDSTR_SIMD_IX32},
	{0x01e0fc00,	0x0140a400,	"ld3",	3,	A64_LDSTR_SIMD_IX64},
	{0x01e0f000,	0x0140c000,	"ld1r",	1,	0},
	{0x01e0f000,	0x0140e000,	"ld3r",	3,	0},
	{0x01e0e000,	0x01600000,	"ld2",	2,	A64_LDSTR_SIMD_IX8},
	{0x01e0e000,	0x01602000,	"ld4",	4,	A64_LDSTR_SIMD_IX8},
	{0x01e0e400,	0x01604000,	"ld2",	2,	A64_LDSTR_SIMD_IX16},
	{0x01e0e400,	0x01606000,	"ld4",	4,	A64_LDSTR_SIMD_IX16},
	{0x01e0ec00,	0x01608000,	"ld2",	2,	A64_LDSTR_SIMD_IX32},
	{0x01e0fc00,	0x01608400,	"ld2",	2,	A64_LDSTR_SIMD_IX64},
	{0x01e0ec00,	0x0160a000,	"ld4",	4,	A64_LDSTR_SIMD_IX32},
	{0x01e0fc00,	0x0160a400,	"ld4",	4,	A64_LDSTR_SIMD_IX64},
	{0x01e0f000,	0x0160c000,	"ld2r",	2,	0},
	{0x01e0f000,	0x0160e000,	"ld4r",	4,	0},
	/* C3.3.4 AdvSIMD load/store single structure (post-indexed) */
	{0x01e0e000,	0x01800000,	"st1",	1,	A64_LDSTR_SIMD_PIB8},
	{0x01e0e000,	0x01802000,	"st3",	3,	A64_LDSTR_SIMD_PIB8},
	{0x01e0e400,	0x01804000,	"st1",	1,	A64_LDSTR_SIMD_PIB16},
	{0x01e0e400,	0x01806000,	"st3",	3,	A64_LDSTR_SIMD_PIB16},
	{0x01e0ec00,	0x01808000,	"st1",	1,	A64_LDSTR_SIMD_PIB32},
	{0x01e0fc00,	0x01808400,	"st1",	1,	A64_LDSTR_SIMD_PIB64},
	{0x01e0ec00,	0x0180a000,	"st3",	3,	A64_LDSTR_SIMD_PIB32},
	{0x01e0fc00,	0x0180a400,	"st3",	3,	A64_LDSTR_SIMD_PIB64},
	{0x01e0e000,	0x01a00000,	"st2",	2,	A64_LDSTR_SIMD_PIB8},
	{0x01e0e000,	0x01a02000,	"st4",	4,	A64_LDSTR_SIMD_PIB8},
	{0x01e0e400,	0x01a04000,	"st2",	2,	A64_LDSTR_SIMD_PIB16},
	{0x01e0e400,	0x01a06000,	"st4",	4,	A64_LDSTR_SIMD_PIB16},
	{0x01e0ec00,	0x01a08000,	"st2",	2,	A64_LDSTR_SIMD_PIB32},
	{0x01e0fc00,	0x01a08400,	"st2",	2,	A64_LDSTR_SIMD_PIB64},
	{0x01e0ec00,	0x01a0a000,	"st4",	4,	A64_LDSTR_SIMD_PIB32},
	{0x01e0fc00,	0x01a0a400,	"st4",	4,	A64_LDSTR_SIMD_PIB64},
	{0x01e0e000,	0x01c00000,	"ld1",	1,	A64_LDSTR_SIMD_PIB8},
	{0x01e0e000,	0x01c02000,	"ld3",	3,	A64_LDSTR_SIMD_PIB8},
	{0x01e0e400,	0x01c04000,	"ld1",	1,	A64_LDSTR_SIMD_PIB16},
	{0x01e0e400,	0x01c06000,	"ld3",	3,	A64_LDSTR_SIMD_PIB16},
	{0x01e0ec00,	0x01c08000,	"ld1",	1,	A64_LDSTR_SIMD_PIB32},
	{0x01e0fc00,	0x01c08400,	"ld1",	1,	A64_LDSTR_SIMD_PIB64},
	{0x01e0ec00,	0x01c0a000,	"ld3",	3,	A64_LDSTR_SIMD_PIB32},
	{0x01e0fc00,	0x01c0a400,	"ld3",	3,	A64_LDSTR_SIMD_PIB64},
	{0x01e0f000,	0x01c0c000,	"ld1r",	1,	A64_LDSTR_SIMD_PIS},
	{0x01e0f000,	0x01c0e000,	"ld3r",	3,	A64_LDSTR_SIMD_PIS},
	{0x01e0e000,	0x01e00000,	"ld2",	2,	A64_LDSTR_SIMD_PIB8},
	{0x01e0e000,	0x01e02000,	"ld4",	4,	A64_LDSTR_SIMD_PIB8},
	{0x01e0e400,	0x01e04000,	"ld2",	2,	A64_LDSTR_SIMD_PIB16},
	{0x01e0e400,	0x01e06000,	"ld4",	4,	A64_LDSTR_SIMD_PIB16},
	{0x01e0ec00,	0x01e08000,	"ld2",	2,	A64_LDSTR_SIMD_PIB32},
	{0x01e0fc00,	0x01e08400,	"ld2",	2,	A64_LDSTR_SIMD_PIB64},
	{0x01e0ec00,	0x01e0a000,	"ld4",	4,	A64_LDSTR_SIMD_PIB32},
	{0x01e0fc00,	0x01e0a400,	"ld4",	4,	A64_LDSTR_SIMD_PIB64},
	{0x01e0f000,	0x01e0c000,	"ld2r",	2,	A64_LDSTR_SIMD_PIS},
	{0x01e0f000,	0x01e0e000,	"ld4r",	4,	A64_LDSTR_SIMD_PIS},
	{0xffffffff,	0xffffffff,	NULL,	0,	0}
};

static int
a64_dis_ldstr_simd(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen)
{
	a64_ldstr_simd_opcode_entry_t *op;
	a64_reg_t rm, rn, v;
	uint8_t idx, imm, q, s, size;
	size_t len;

	bzero(&rm, sizeof (rm));
	bzero(&rn, sizeof (rn));
	bzero(&v, sizeof (v));

	for (op = a64_ldstr_simd_opcodes; op->mask != 0xffffffff; op++) {
		if ((in & op->mask) == op->targ)
			break;
	}
	if (op->name == NULL)
		return (-1);

	q = bitx32(in, 30, 30);
	s = bitx32(in, 12, 12);
	size = bitx32(in, 11, 10);
	rm.id = bitx32(in, 20, 16);
	rn.id = bitx32(in, 9, 5);
	v.id = bitx32(in, 4, 0);

	rm.width = rn.width = A64_REGWIDTH_64;
	rm.flags = rn.flags = A64_REGFLAGS_SP;
	v.width = A64_REGWIDTH_SIMD_V;

	/*
	 * Look up arrangement type and calculate index.
	 */
	if (op->flags & A64_LDSTR_SIMD_IX8) {
		v.arr = A64_REG_ARR_B;
		idx = ((q << 3) | (s << 2) | size);
	} else if (op->flags & A64_LDSTR_SIMD_IX16) {
		v.arr = A64_REG_ARR_H;
		idx = ((q << 2) | (s << 1) | (size & 0x2) >> 1);
	} else if (op->flags & A64_LDSTR_SIMD_IX32) {
		v.arr = A64_REG_ARR_S;
		idx = ((q << 1) | s);
	} else if (op->flags & A64_LDSTR_SIMD_IX64) {
		v.arr = A64_REG_ARR_D;
		idx = q;
	} else
		v.arr = q ? A64_REG_ARR_16B + size : A64_REG_ARR_8B + size;

	/*
	 * Print opname and registers first.
	 */
	if ((len = dis_snprintf(buf, buflen, "%s ", op->name)) >= buflen)
		return (-1);
	buflen -= len;
	buf += len;

	if ((len = a64_print_vregs(buf, buflen, v, op->nregs)) >= buflen)
		return (-1);
	buflen -= len;
	buf += len;

	/*
	 * Print index if required.
	 */
	if (op->flags & A64_LDSTR_SIMD_ISIX) {
		if ((len = dis_snprintf(buf, buflen, "[%d]", idx)) >= buflen)
			return (-1);
		buflen -= len;
		buf += len;
	}

	/*
	 * Print base register.
	 */
	if ((len = dis_snprintf(buf, buflen, ", [%s]", a64_reg_name(rn)))
	    >= buflen) {
		return (-1);
	}
	buflen -= len;
	buf += len;

	/*
	 * Post-index register offset variant.
	 */
	if (op->flags & A64_LDSTR_SIMD_ISPI && rm.id != A64_REG_ZR)
		len = dis_snprintf(buf, buflen, ", %s", a64_reg_name(rm));

	/*
	 * Post-index immediate offset variants.
	 */
	else if (op->flags & A64_LDSTR_SIMD_ISPI) {

		/*
		 * Offset based on "Q" value.
		 */
		if (op->flags & A64_LDSTR_SIMD_PIQ)
			imm = (op->nregs * 8) << q;

		/*
		 * Offset based on bit size.
		 */
		else if (op->flags & A64_LDSTR_SIMD_PIB) {
			switch (op->flags & A64_LDSTR_SIMD_ISIX) {
			case A64_LDSTR_SIMD_IX8:
				imm = op->nregs << 0;
				break;
			case A64_LDSTR_SIMD_IX16:
				imm = op->nregs << 1;
				break;
			case A64_LDSTR_SIMD_IX32:
				imm = op->nregs << 2;
				break;
			case A64_LDSTR_SIMD_IX64:
				imm = op->nregs << 3;
				break;
			}
		/*
		 * Offset based on "size" value.
		 */
		} else if (op->flags & A64_LDSTR_SIMD_PIS)
			imm = op->nregs << size;

		len = dis_snprintf(buf, buflen, ", #%d", imm);
	}

	return (len < buflen ? 0 : -1);
}

/*
 *	.1|1|1|...5..|...3..|
 *	 0 Q 1  01110 op2  0 Rm 0 imm4 0 Rn Rd
 *	 0 Q 0  01110 op2  0 Rm 0 len op 00 Rn Rd
 *	 0 Q 0  01110 size 0 Rm 0 opcode 10 Rn Rd
 *	 0 Q U  01110 size 1 1000 opcode 10 Rn Rd
 *	 0 Q op 01110 00   0 imm5 0 imm4 1 Rn Rd
 *	 0 Q op 01111 00   0 00 a b c cmode o2 1 d e f g h Rd
 *	 0 1 op 11110 00   0 imm5 0 imm4 1 Rn Rd
 *	 0 1 U  11110 size 1 1000 opcode 10 Rn Rd
 *
 *	 0 1 U  11111 0 immh immb opcode 1 Rn Rd
 *
 *	 0 1 U  11110 size 1 Rm opcode 00 Rn Rd
 *	 0 1 U  11110 size 1 Rm opcode 1 Rn Rd
 *	 0 1 U  11110 size 1 0000 opcode 10 Rn Rd
 *
 *	 0 1 U  11111 size L M Rm opcode H 0 Rn Rd
 *
 *	 0 Q U  01111 0 immh immb opcode 1 Rn Rd
 *	 0 Q U  01110 size 1 Rm opcode 00 Rn Rd
 *	 0 Q U  01110 size 1 Rm opcode 1 Rn Rd
 *	 0 Q U  01110 size 1 0000 opcode 10 Rn Rd
 *	 0 Q U  01111 size L M Rm opcode H 0 Rn Rd
 *
 *	crypto
 *	01001110 size 10100 opcode 10 Rn Rd
 *	01011110 size 0 Rm 0 opcode 00 Rn Rd
 *	01011110 size 10100 opcode 10 Rn Rd
 *
 *	fp
 *	M 0 S 11110 type 1 Rm op 1000 Rn opcode2
 *	M 0 S 11110 type 1 Rm cond 01 Rn op nzcv
 *	M 0 S 11110 type 1 Rm cond 11 Rn Rd
 *	M 0 S 11110 type 1 opcode 10000 Rn Rd
 *	M 0 S 11110 type 1 Rm opcode 10 Rn Rd
 *	M 0 S 11111 type o1 Rm o0 Ra Rn Rd
 *	M 0 S 11110 type 1 imm8 100 imm5 Rd
 *	sf 0 S 11110 type 0 rmode opcode scale Rn Rd
 *	sf 0 S 11110 type 1 rmode opcode 000000 Rn Rd
 */

/* VFPExpandImm */
static uint64_t
a64_vfp_expand_imm(uint8_t imm8, a64_reg_width_t width)
{
	uint64_t result = 0;
	uint8_t bit6 = bitx32(imm8, 6, 6);

	if (width == A64_REGWIDTH_SIMD_32) {
		result = bitx32(imm8, 7, 7) << 31 | bitx32(imm8, 5, 0) << 19;
		if (bit6)
			result |= 0x1f << 25;
		else
			result |= 0x1 << 30;
	} else if (width == A64_REGWIDTH_SIMD_64) {
		result = (uint64_t)bitx32(imm8, 7, 7) << 63 |
		    (uint64_t)bitx32(imm8, 5, 0) << 48;
		if (bit6)
			result |= 0xffULL << 54;
		else
			result |= 0x1ULL << 62;
	}

	return (result);
}

static uint64_t
a64_simd_float(uint64_t imm, a64_reg_width_t width, char *buf, size_t buflen)
{
#ifdef DIS_STANDALONE
	return (dis_snprintf(buf, buflen, "<FP value>"));
#else
	double val;

	if (width == A64_REGWIDTH_SIMD_32)
		val = (double)*(float *)&imm;
	else
		val = *(double *)&imm;

	return (dis_snprintf(buf, buflen, "%.8f", val));
#endif
}

typedef enum a64_simd_flags {
	SIMD_F_SKIP	= BIT(0),
	SIMD_F_VAR1	= BIT(1),
	SIMD_F_VAR2	= BIT(2),
	SIMD_F_VAR3	= BIT(3),
	SIMD_F_VAR4	= BIT(4),
	SIMD_F_VAR5	= BIT(5),
	SIMD_F_VAR6	= BIT(6),
	SIMD_F_B	= BIT(7),
	SIMD_F_H	= BIT(8),
	SIMD_F_S	= BIT(9),
	SIMD_F_D	= BIT(10),
	SIMD_F_MIM	= BIT(11),
	SIMD_F_MIM1	= BIT(12),
	SIMD_F_ZERO	= BIT(13),
} a64_simd_flags_t;

/* Don't care value, short so it fits into the tables */
#define	DC	UINT32_MAX

typedef struct a64_simd_class a64_simd_class_t;
typedef struct a64_simd_opcode_entry a64_simd_opcode_entry_t;
typedef struct a64_simd_data a64_simd_data_t;
typedef int (*a64_simd_handler_f)(const dis_handle_t *,
    const a64_simd_class_t *, const a64_simd_opcode_entry_t *,
    a64_simd_data_t *, uint32_t, char *, size_t);

typedef struct a64_simd_opcode_entry {
	uint32_t			d1v;
	uint32_t			d2v;
	uint32_t			d3v;
	uint32_t			d4v;
	uint32_t			d5v;
	const char			*name;
	a64_simd_flags_t		flags;
	a64_simd_handler_f		handler;
} a64_simd_opcode_entry_t;

typedef struct a64_simd_data {
	a64_reg_t			rm;
	a64_reg_t			rn;
	a64_reg_t			rd;
	uint8_t				q;
	uint8_t				size;
} a64_simd_data_t;

typedef struct a64_simd_class {
	uint32_t			mask;
	uint32_t			targ;
	const a64_simd_opcode_entry_t	*entries;
	a64_simd_handler_f		handler;
	/*
	 * Discriminator masks and shifts, used to distinguish between
	 * instructions in this class.
	 */
	uint32_t			d1m;
	uint32_t			d2m;
	uint32_t			d3m;
	uint32_t			d4m;
	uint32_t			d5m;
	uint8_t				d1s;
	uint8_t				d2s;
	uint8_t				d3s;
	uint8_t				d4s;
	uint8_t				d5s;
} a64_simd_class_t;

/* 3.6.1 AdvSIMD EXT */
static int
a64_simd_handle_ext(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	uint8_t imm4 = bitx32(in, 14, 11);
	const char *arr = a64_simd_arrange[data->q][0];
	size_t len;

	if (data->q == 0 && (imm4 & 0x8))
		return (-1);
	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s, %s.%s, #%d",
	    a64_reg_name(data->rd), arr, a64_reg_name(data->rn), arr,
	    a64_reg_name(data->rm), arr, imm4);

	return (len < buflen ? 0 : -1);
}

/* 3.6.2 AdvSIMD TBL/TBX */
static int
a64_simd_handle_tbltbx(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	uint8_t nregs;
	size_t len;

	data->rd.arr = data->q ? A64_REG_ARR_16B : A64_REG_ARR_8B;
	data->rm.arr = data->rd.arr;
	data->rn.arr = A64_REG_ARR_16B;

	nregs = bitx32(in, 14, 13) + 1;

	/* <Vd>.<Ta> */
	if ((len = dis_snprintf(buf, buflen, " %s.%s, ",
	    a64_reg_name(data->rd),
	    a64_reg_arr_names[data->rd.arr])) >= buflen) {
		return (-1);
	}
	buflen -= len;
	buf += len;

	/* {<Vn>.16B ... } */
	if ((len = a64_print_vregs(buf, buflen, data->rn, nregs)) >= buflen)
		return (-1);
	buflen -= len;
	buf += len;

	/* <Vm>.<Ta> */
	len = dis_snprintf(buf, buflen, ", %s.%s", a64_reg_name(data->rm),
	    a64_reg_arr_names[data->rm.arr]);

	return (len < buflen ? 0 : -1);
}

/* 3.6.3 AdvSIMD ZIP/UZP/TRN */
static int
a64_simd_handle_trn(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	const char *arr = a64_simd_arrange[data->q][data->size];

	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s, %s.%s",
	    a64_reg_name(data->rd), arr, a64_reg_name(data->rn), arr,
	    a64_reg_name(data->rm), arr);

	return (len < buflen ? 0 : -1);
}

/* 3.6.4 ADVSIMD across lanes */
static int
a64_simd_handle_acr(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	data->rn.arr = data->q ? A64_REG_ARR_16B : A64_REG_ARR_8B;
	data->rn.arr += data->size;
	if (op->flags & SIMD_F_VAR1)
		data->rd.width = A64_REGWIDTH_SIMD_8 + data->size;
	else if (op->flags & SIMD_F_VAR2)
		data->rd.width = A64_REGWIDTH_SIMD_16 + data->size;
	else if (op->flags & SIMD_F_VAR3) {
		data->rd.width = A64_REGWIDTH_SIMD_32;
		data->rn.arr = A64_REG_ARR_4S;
	}
	len = dis_snprintf(buf, buflen, " %s, %s.%s",
	    a64_reg_name(data->rd),
	    a64_reg_name(data->rn), a64_reg_arr_names[data->rn.arr]);

	return (len < buflen ? 0 : -1);
}

/* 3.6.5 AdvSIMD copy */
/* 3.6.7 AdvSIMD scalar copy */
static int
a64_simd_handle_cpy(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	uint8_t i, idx, imm4, imm5;

	imm4 = bitx32(in, 14, 11);
	imm5 = bitx32(in, 20, 16);

	/*
	 * Output formats are based on first set bit in imm5.  We use
	 * this to compute the offsets in our register tables.
	 */
	i = ffs(imm5);

	if (op->flags & SIMD_F_VAR1) {
		/* <Vd>.<T>, <Vn>.<Ts>[<index>] */
		data->rd.arr = data->q ? A64_REG_ARR_16B + (i - 1)
		    : A64_REG_ARR_8B + (i - 1);
		data->rn.arr = A64_REG_ARR_B + (i - 1);
		idx = (imm5 >> i);
		len = dis_snprintf(buf, buflen, " %s.%s, %s.%s[%d]",
		    a64_reg_name(data->rd), a64_reg_arr_names[data->rd.arr],
		    a64_reg_name(data->rn),
		    a64_reg_arr_names[data->rn.arr], idx);
	} else if (op->flags & SIMD_F_VAR2) {
		/* <Vd>.<T>, <R><n> */
		data->rd.arr = data->q ? A64_REG_ARR_16B + (i - 1)
		    : A64_REG_ARR_8B + (i - 1);
		data->rn.width = (i > 3) ? A64_REGWIDTH_64 : A64_REGWIDTH_32;
		len = dis_snprintf(buf, buflen, " %s.%s, %s",
		    a64_reg_name(data->rd), a64_reg_arr_names[data->rd.arr],
		    a64_reg_name(data->rn));
	} else if (op->flags & SIMD_F_VAR3) {
		/* <Rd>, <Vn>.<Ts>[<index>] */
		data->rd.width = data->q ? A64_REGWIDTH_64 : A64_REGWIDTH_32;
		data->rn.arr = A64_REG_ARR_B + (i - 1);
		idx = (imm5 >> i);
		len = dis_snprintf(buf, buflen, " %s, %s.%s[%d]",
		    a64_reg_name(data->rd), a64_reg_name(data->rn),
		    a64_reg_arr_names[data->rn.arr], idx);
	} else if (op->flags & SIMD_F_VAR4) {
		/* <Vd>.<Ts>[<index>], <R><n> */
		data->rd.arr = A64_REG_ARR_B + (i - 1);
		data->rn.width = (i > 3) ? A64_REGWIDTH_64 : A64_REGWIDTH_32;
		idx = (imm5 >> i);
		len = dis_snprintf(buf, buflen, " %s.%s[%d], %s",
		    a64_reg_name(data->rd), a64_reg_arr_names[data->rd.arr],
		    idx, a64_reg_name(data->rn));
	} else if (op->flags & SIMD_F_VAR5) {
		/* <Vd>.<Ts>[<index1>], <Vn>.<Ts>[<index2>] */
		data->rd.arr = data->rn.arr = A64_REG_ARR_B + (i - 1);
		idx = (imm5 >> i);
		len = dis_snprintf(buf, buflen, " %s.%s[%d], %s.%s[%d]",
		    a64_reg_name(data->rd), a64_reg_arr_names[data->rd.arr],
		    idx,
		    a64_reg_name(data->rn), a64_reg_arr_names[data->rn.arr],
		    (imm4 >> i));
	} else if (op->flags & SIMD_F_VAR6) {
		/* <V><d>, <Vn>.<T>[<index>] */
		data->rd.width = A64_REGWIDTH_SIMD_8 + (i - 1);
		data->rn.arr = A64_REG_ARR_B + (i - 1);
		idx = (imm5 >> i);
		len = dis_snprintf(buf, buflen, " %s, %s.%s[%d]",
		    a64_reg_name(data->rd), a64_reg_name(data->rn),
		    a64_reg_arr_names[data->rn.arr], idx);
	}

	return (len < buflen ? 0 : -1);
}

/* 3.6.6 AdvSIMD modified immediate */
static int
a64_simd_handle_mmi(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	/* Shift a:b:c into position to make up a:b:c:d:e:f:g:h */
	uint8_t imm8 = bitx32(in, 18, 16) << 5 | bitx32(in, 9, 5);
	uint8_t cmode = bitx32(in, 15, 12);
	uint8_t shift = 0;

	if (op->flags & SIMD_F_B) {
		data->rd.arr = data->q ? A64_REG_ARR_16B : A64_REG_ARR_8B;
		shift = 0;
	} else if (op->flags & SIMD_F_H) {
		data->rd.arr = data->q ? A64_REG_ARR_8H : A64_REG_ARR_4H;
		shift = (cmode & 0x2) ? 8 : 0;
	} else if (op->flags & SIMD_F_S) {
		data->rd.arr = data->q ? A64_REG_ARR_4S : A64_REG_ARR_2S;
		if (op->flags & SIMD_F_MIM1)
			shift = 8 << (cmode & 0x1);
		else
			shift = 8 * ((cmode & 0x6) >> 1);
	} else if (op->flags & SIMD_F_D) {
		/*
		 * Build imm based on imm8, where "a:b:c:d:e:f:g:h"
		 * becomes "aaaaaaaa:bbbbbbbb:cccccccc:...".  No doubt
		 * there is a fancier way to compute this.
		 */
		uint64_t imm = 0;

		for (uint8_t i = 0; i < 8; i++) {
			imm |= ((imm8 & 0x01) ? 1ULL : 0) << i;
			imm |= ((imm8 & 0x02) ? 1ULL : 0) << i + 8;
			imm |= ((imm8 & 0x04) ? 1ULL : 0) << i + 16;
			imm |= ((imm8 & 0x08) ? 1ULL : 0) << i + 24;
			imm |= ((imm8 & 0x10) ? 1ULL : 0) << i + 32;
			imm |= ((imm8 & 0x20) ? 1ULL : 0) << i + 40;
			imm |= ((imm8 & 0x40) ? 1ULL : 0) << i + 48;
			imm |= ((imm8 & 0x80) ? 1ULL : 0) << i + 56;
		}
		/* Special formats, handle and exit early. */
		if (data->q) {
			data->rd.arr = A64_REG_ARR_2D;
			len = dis_snprintf(buf, buflen,
			    " %s.%s, #0x%" PRIx64 "",
			    a64_reg_name(data->rd),
			    a64_reg_arr_names[data->rd.arr], imm);
		} else {
			data->rd.width = A64_REGWIDTH_SIMD_64;
			len = dis_snprintf(buf, buflen,
			    " %s, #0x%" PRIx64 "",
			    a64_reg_name(data->rd), imm);
		}
		return (len < buflen ? 0 : -1);
	} else {
		uint64_t imm;

		data->rd.arr = data->q ? A64_REG_ARR_4S : A64_REG_ARR_2S;
		len = dis_snprintf(buf, buflen, " %s.%s, #",
		    a64_reg_name(data->rd), a64_reg_arr_names[data->rd.arr],
		    imm8);

		if (len >= buflen)
			return (-1);
		buf += len;
		buflen -= len;

		imm = a64_vfp_expand_imm(imm8, data->rd.width);
		len = a64_simd_float(imm, data->rd.width, buf, buflen);
	}
	if (shift != 0) {
		len = dis_snprintf(buf, buflen, " %s.%s, #0x%x, %s #%d",
		    a64_reg_name(data->rd), a64_reg_arr_names[data->rd.arr],
		    imm8, op->flags & SIMD_F_MIM1 ? "msl" : "lsl", shift);
	} else {
		len = dis_snprintf(buf, buflen, " %s.%s, #0x%x",
		    a64_reg_name(data->rd), a64_reg_arr_names[data->rd.arr],
		    imm8);
	}
	return (len < buflen ? 0 : -1);
}

/* 3.6.8 AdvSIMD scalar pairwise */
static int
a64_simd_handle_spr(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	uint8_t sz = data->size & 0x1;

	data->rd.width = sz ? A64_REGWIDTH_SIMD_64 : A64_REGWIDTH_SIMD_32;
	data->rn.arr = sz ? A64_REG_ARR_2D : A64_REG_ARR_2S;
	len = dis_snprintf(buf, buflen, " %s, %s.%s",
	    a64_reg_name(data->rd), a64_reg_name(data->rn),
	    a64_reg_arr_names[data->rn.arr]);

	return (len < buflen ? 0 : -1);
}

/* 3.6.9 AdvSIMD scalar shift by immediate */
static int
a64_simd_handle_ssh(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	uint8_t shift;
	size_t len;
	int i;

	uint8_t immh = bitx32(in, 22, 19);
	uint8_t immb = bitx32(in, 18, 16);
	uint8_t imm = immh << 3 | immb;

	/*
	 * Output formats are based on the last set bit in immh. We use
	 * this to compute the offsets in our register tables.
	 */
	i = fls(immh);

	data->rd.width = data->rn.width = A64_REGWIDTH_SIMD_8 + (i - 1);
	if (op->flags & SIMD_F_VAR1) {
		shift = imm - (1 << (i + 2));
	} else if (op->flags & SIMD_F_VAR2) {
		data->rn.width++;
		shift = (1 << (i + 3)) - imm;
	} else {
		shift = (1 << (i + 3)) - imm;
	}

	len = dis_snprintf(buf, buflen, " %s, %s, #%d",
	    a64_reg_name(data->rd), a64_reg_name(data->rn), shift);

	return (len < buflen ? 0 : -1);
}

/* 3.6.10 AdvSIMD scalar three different */
static int
a64_simd_handle_astd(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	if ((data->size & 1) == 1) {
		data->rd.width = A64_REGWIDTH_SIMD_32;
		data->rn.width = A64_REGWIDTH_SIMD_16;
	} else {
		data->rd.width = A64_REGWIDTH_SIMD_64;
		data->rn.width = A64_REGWIDTH_SIMD_32;
	}
	data->rm.width = data->rn.width;

	len = dis_snprintf(buf, buflen, " %s, %s, %s",
	    a64_reg_name(data->rd), a64_reg_name(data->rn),
	    a64_reg_name(data->rm));

	return (len < buflen ? 0 : -1);
}

/* 3.6.11 AdvSIMD scalar three same */
static int
a64_simd_handle_asts(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	/*
	 * Some instructions in this group have only a 1 bit size. In those
	 * cases we must index into the register table as if the top bit is
	 * set. We can derive this from our opcode table by looking at whether
	 * the top bit is ignored.
	 */
	if (op->d2v != DC)
		data->size |= 2;

	data->rn.width = A64_REGWIDTH_SIMD_8 + data->size;
	data->rd.width = data->rm.width = data->rn.width;

	len = dis_snprintf(buf, buflen, " %s, %s, %s",
	    a64_reg_name(data->rd), a64_reg_name(data->rn),
	    a64_reg_name(data->rm));

	return (len < buflen ? 0 : -1);
}

/* 3.6.12 AdvSIMD scalar two-reg misc */
static int
a64_simd_handle_astrm(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	uint8_t sz = bitx32(in, 22, 22);

	data->rn.width = data->rd.width = sz == 0 ? A64_REGWIDTH_SIMD_32 :
	    A64_REGWIDTH_SIMD_64;
	len = dis_snprintf(buf, buflen, " %s, %s",
	    a64_reg_name(data->rd), a64_reg_name(data->rn));

	if (len >= buflen)
		return (-1);
	buflen -= len;
	buf += len;

	if (op->flags & SIMD_F_ZERO)
		len = dis_snprintf(buf, buflen, ", #0");

	return (len < buflen ? 0 : -1);
}

/* 3.6.13 AdvSIMD scalar x indexed element */
static int
a64_simd_handle_sxie(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	uint8_t idx;

	uint8_t h = bitx32(in, 11, 11);
	uint8_t l = bitx32(in, 21, 21);
	uint8_t sz = bitx32(in, 22, 22);

	if (sz == 0) {
		data->rm.width = A64_REGWIDTH_SIMD_32;
		data->rm.arr = A64_REG_ARR_S;
		idx = h << 1 | l;
	} else {
		data->rm.width = A64_REGWIDTH_SIMD_64;
		data->rm.arr = A64_REG_ARR_D;
		idx = h;
	}
	data->rd.width = data->rn.width = data->rm.width;

	/* <V><d>, <V><n>, <Vm>.<Ts>[<index>] */
	len = dis_snprintf(buf, buflen, " %s, %s, %s.%s[%d]",
	    a64_reg_name(data->rd), a64_reg_name(data->rn),
	    a64_reg_name(data->rm), a64_reg_arr_names[data->rm.arr], idx);

	return (len < buflen ? 0 : -1);
}

/* 3.6.14 AdvSIMD shift by immediate */
static int
a64_simd_handle_asbi(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	const char *arr1, *arr2;
	uint8_t shift;
	size_t len;
	int i;

	uint8_t immh = bitx32(in, 22, 19);
	uint8_t immb = bitx32(in, 18, 16);
	uint8_t imm = immh << 3 | immb;

	/*
	 * Output formats are based on the last set bit in immh. We use
	 * this to compute the offsets in our arrangement tables.
	 */
	i = fls(immh);

	/*
	 * If no bits are set, then this is an AdvSIMD modified immediate
	 * instruction that should not have matched this class.
	 */
	if (i == 0)
		return (-1);

	if (op->flags & SIMD_F_VAR3) {
		arr1 = a64_simd_arrange[1][i];
		arr2 = a64_simd_arrange[data->q][i - 1];
		shift = imm - (1 << (i + 2));
	} else if (op->flags & SIMD_F_VAR2) {
		arr1 = a64_simd_arrange[data->q][i - 1];
		arr2 = a64_simd_arrange[1][i];
		shift = (1 << (i + 3)) - imm;
	} else if (op->flags & SIMD_F_VAR1) {
		arr1 = arr2 = a64_simd_arrange[data->q][i - 1];
		shift = imm - (1 << (i + 2));
	} else {
		arr1 = arr2 = a64_simd_arrange[data->q][i - 1];
		shift = (1 << (i + 3)) - imm;
	}

	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s, #%d",
	    a64_reg_name(data->rd), arr1, a64_reg_name(data->rn), arr2,
	    shift);

	return (len < buflen ? 0 : -1);
}

/* 3.6.15 AdvSIMD three different */
static int
a64_simd_handle_atd(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	const char *arra = a64_reg_arr_names[data->size + A64_REG_ARR_8H];
	const char *arrb = a64_simd_arrange[data->q][data->size];
	const char *arr1, *arr2, *arr3;

	if (op->flags & SIMD_F_VAR1) {
		arr1 = arr2 = arra;
		arr3 = arrb;
	} else if (op->flags & SIMD_F_VAR2) {
		arr1 = arrb;
		arr2 = arr3 = arra;
	} else {
		arr1 = arra;
		arr2 = arr3 = arrb;
	}

	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s, %s.%s",
	    a64_reg_name(data->rd), arr1, a64_reg_name(data->rn), arr2,
	    a64_reg_name(data->rm), arr3);

	return (len < buflen ? 0 : -1);
}

/* 3.6.16 AdvSIMD three same */
static int
a64_simd_handle_ats(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	const char *arr;
	size_t len;

	/*
	 * Some instructions in this group have only a 1 bit size. In those
	 * cases we must index into the arrangement table as if the top bit is
	 * set. We can derive this from our opcode table by looking at whether
	 * the bottom bit is ignored when the top one is not.
	 */
	if (op->d3v == DC && op->d2v != DC)
		data->size |= 2;

	/*
	 * Special case for the opcodes where both bits of 'size' are used for
	 * 'opc2' - we always need to index from zero.
	 */
	if (op->d3v != DC && op->d2v != DC)
		data->size = 0;

	arr = a64_simd_arrange[data->q][data->size];

	/*
	 * As a special case, "orr" should be replaced with "mov" if
	 * Rm == Rn
	 */
	if (op->d1v == 0 && op->d2v == 1 && op->d3v == 0 && op->d4v == 3 &&
	    data->rm.id == data->rn.id) {
		buf -= 3;
		buflen += 3;

		len = dis_snprintf(buf, buflen, "mov %s.%s, %s.%s",
		    a64_reg_name(data->rd), arr, a64_reg_name(data->rn), arr);
	} else {
		/* <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
		len = dis_snprintf(buf, buflen, " %s.%s, %s.%s, %s.%s",
		    a64_reg_name(data->rd), arr, a64_reg_name(data->rn), arr,
		    a64_reg_name(data->rm), arr);
	}

	return (len < buflen ? 0 : -1);
}

/* 3.6.17 AdvSIMD two-reg misc */
static int
a64_simd_handle_atrm(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	const char *arr;
	size_t len;

	/*
	 * This group is a bit of a grab bag; I suppose the "misc" should have
	 * been a clue. The good news is that these are all vector operations
	 * that use rd and rn in that order and the following general rules
	 * cover most of the instructions in this group:
	 *
	 * - If there are two size bits available, the arrangement is indexed
	 *   from 8B, in the usual [q][size] way.
	 * - If there is only one size bit available, the arrangement is
	 *   indexed as if the top bit is set. That's convenient.
	 *
	 * The exceptions are then these few remaining instructions which use
	 * two different arrangements. shll also has an extra #shift argument.
	 * These have separate handlers below this function.
	 */

	/* size[0] DC && size[1] != DC */
	if (op->d3v == DC && op->d2v != DC)
		data->size |= 2;

	arr = a64_simd_arrange[data->q][data->size];

	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s",
	    a64_reg_name(data->rd), arr,
	    a64_reg_name(data->rn), arr);

	if (len >= buflen)
		return (-1);
	buflen -= len;
	buf += len;

	if (op->flags & SIMD_F_ZERO)
		len = dis_snprintf(buf, buflen, ", #0");

	return (len < buflen ? 0 : -1);
}

static int
a64_simd_handle_atrm_4h8b(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	if (data->size > 2)
		return (-1);

	const char *arr1 = a64_simd_arrange[data->q][data->size + 1];
	const char *arr2 = a64_simd_arrange[data->q][data->size];

	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s",
	    a64_reg_name(data->rd), arr1,
	    a64_reg_name(data->rn), arr2);

	return (len < buflen ? 0 : -1);
}

static int
a64_simd_handle_atrm_8b8h(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	if (data->size > 2)
		return (-1);

	const char *arr1 = a64_simd_arrange[data->q][data->size];
	const char *arr2 = a64_simd_arrange[1][data->size + 1];

	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s",
	    a64_reg_name(data->rd), arr1,
	    a64_reg_name(data->rn), arr2);

	return (len < buflen ? 0 : -1);
}

static int
a64_simd_handle_atrm_shll(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	if (data->size > 2)
		return (-1);

	const char *arr1 = a64_simd_arrange[1][data->size + 1];
	const char *arr2 = a64_simd_arrange[data->q][data->size];

	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s, #%d",
	    a64_reg_name(data->rd), arr1,
	    a64_reg_name(data->rn), arr2,
	    8 << data->size);

	return (len < buflen ? 0 : -1);
}

static int
a64_simd_handle_atrm_fcvt(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	const char *arr1, *arr2;
	uint8_t size = bitx32(in, 22, 22);
	size_t len;

	switch (op->name[4]) {
	case 'n':
	case 'x':
		/* fcvtn[2]	4H	4S */
		/* fcvtxn[2]	4H	4S */
		arr1 = a64_simd_arrange[data->q][size + 1];
		arr2 = a64_simd_arrange[1][size + 2];
		break;
	case 'l':
		/* fcvtl[2]	4S	4H */
		arr1 = a64_simd_arrange[1][size + 2];
		arr2 = a64_simd_arrange[data->q][size + 1];
		break;
	default:
		return (-1);
	}

	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s",
	    a64_reg_name(data->rd), arr1,
	    a64_reg_name(data->rn), arr2);

	return (len < buflen ? 0 : -1);
}

/* 3.6.18 AdvSIMD vector x indexed element */
static int
a64_simd_handle_avxie(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	/*
	 * There are three groups of instructions in this class. There doesn't
	 * appear to be a way of detecting which group a particular instruction
	 * is in from the instruction fields so we default to group 3 and
	 * select the others based on SIMD_F_VAR variant flags attached to the
	 * opcodes.
	 *
	 * 1. The fused multiply instructions
	 *	<Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>]
	 * with the same arrangement for Vd and Vn and Rm is always 20:16
	 *
	 * 2. The remaining multiply instructions
	 *	<Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>]
	 * with the same arrangement for Vd and Vn, but differently calculated,
	 * and Rm is either 20:16 or 19:16 depending on what is in bit 23.
	 *
	 * 3. This is the largest group
	 *	<Vd>.<Ta>, <Vn>.<Tb>, <Vm>.<Ts>[<index>]
	 * with two different arrangments for Vd and Vn and the value for Rm is
	 * either 20:16 or 19:16 depending on what is in bit 23.
	 *
	 * In the end, we can collapse groups 1 and 2 because bit 23 is always
	 * set for group 1, meaning that the usual 2-bit size field can be used
	 * for naturally indexing into the arrangements.
	 */

	uint8_t h = bitx32(in, 11, 11);
	uint8_t m = bitx32(in, 20, 20);
	uint8_t l = bitx32(in, 21, 21);

	const char *arr1, *arr2;
	const char *ts;
	uint8_t idx;

	if (op->flags != 0)	/* Group 1 or 2 */
		arr1 = a64_simd_arrange[data->q][data->size];
	else
		arr1 = a64_simd_arrange[1][data->size + 1];

	arr2 = a64_simd_arrange[data->q][data->size];
	ts = a64_reg_arr_names[data->size];

	if ((data->size & 0x10) != 0) {
		idx = h << 1 | l;
	} else {
		data->rm.id = bitx32(in, 19, 16);
		idx = h << 2 | l << 1 | m;
	}

	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s, %s.%s[%d]",
	    a64_reg_name(data->rd), arr1, a64_reg_name(data->rn), arr2,
	    a64_reg_name(data->rm), ts, idx);

	return (len < buflen ? 0 : -1);
}

/* 3.6.19 Crypto AES */
static int
a64_simd_handle_aes(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	const char *arr;
	size_t len;

	/* <Vd>.16B, <Vn>.16B */

	arr = a64_reg_arr_names[A64_REG_ARR_16B];
	len = dis_snprintf(buf, buflen, " %s.%s, %s.%s",
	    a64_reg_name(data->rd), arr, a64_reg_name(data->rn), arr);

	return (len < buflen ? 0 : -1);
}

/* 3.6.20 Crypto three-reg SHA */
static int
a64_simd_handle_3rsha(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	const char *arr = a64_reg_arr_names[A64_REG_ARR_4S];
	size_t len;

	if (op->flags & SIMD_F_VAR2) {
		/* <Vd>.4S, <Vn>.4S, <Vm>.4S */

		len = dis_snprintf(buf, buflen, " %s.%s, %s.%s, %s.%s",
		    a64_reg_name(data->rd), arr, a64_reg_name(data->rn), arr,
		    a64_reg_name(data->rm), arr);
	} else {
		/* <Qd>, <Sn>, <Vm>.4S */
		data->rd.width = A64_REGWIDTH_SIMD_128;
		data->rn.width = A64_REGWIDTH_SIMD_32;

		if (op->flags & SIMD_F_VAR1) {
			/* <Qd>, <Qn>, <Vm>.4S */
			data->rn.width = A64_REGWIDTH_SIMD_128;
		}

		len = dis_snprintf(buf, buflen, " %s, %s, %s.%s",
		    a64_reg_name(data->rd), a64_reg_name(data->rn),
		    a64_reg_name(data->rm), arr);
	}

	return (len < buflen ? 0 : -1);
}

/* 3.6.21 Crypto two-reg SHA */
static int
a64_simd_handle_2rsha(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	if (op->flags & SIMD_F_VAR1) {
		/* <Sd>, <Sn> */
		data->rd.width = data->rn.width = A64_REGWIDTH_SIMD_32;

		len = dis_snprintf(buf, buflen, " %s, %s",
		    a64_reg_name(data->rd), a64_reg_name(data->rn));
	} else {
		/* <Vd>.4S, <Vn>.4S */
		const char *arr = a64_reg_arr_names[A64_REG_ARR_4S];

		len = dis_snprintf(buf, buflen, " %s.%s, %s.%s",
		    a64_reg_name(data->rd), arr, a64_reg_name(data->rn), arr);
	}

	return (len < buflen ? 0 : -1);
}

/* 3.6.22 Floating-point compare */
static int
a64_simd_handle_fpc(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	uint8_t opcode2 = bitx32(in, 4, 0);
	bool dbl = (data->size & 0x1) != 0;
	bool zero = bitx8(opcode2, 3, 3) == 1;
	size_t len;

	data->rn.width = dbl ? A64_REGWIDTH_SIMD_64 : A64_REGWIDTH_SIMD_32;
	data->rm.width = data->rn.width;
	if (zero) {
		len = dis_snprintf(buf, buflen, " %s, #0.0",
		    a64_reg_name(data->rn));
	} else {
		len = dis_snprintf(buf, buflen, " %s, %s",
		    a64_reg_name(data->rn), a64_reg_name(data->rm));
	}

	return (len < buflen ? 0 : -1);
}

/* 3.6.22 Floating-point conditional compare */
static int
a64_simd_handle_fpcc(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	bool dbl = (data->size & 0x1) != 0;
	uint8_t nzcv = bitx32(in, 3, 0);
	uint8_t cond = bitx32(in, 15, 12);
	size_t len;

	data->rn.width = dbl ? A64_REGWIDTH_SIMD_64 : A64_REGWIDTH_SIMD_32;
	data->rm.width = data->rn.width;

	/*
	 * <Sn>, <Sm>, #<nzcv>, <cond>
	 * <Dn>, <Dm>, #<nzcv>, <cond>
	 */
	len = dis_snprintf(buf, buflen, " %s, %s, #%d, %s",
	    a64_reg_name(data->rn), a64_reg_name(data->rm),
	    nzcv, a64_cond_names[cond]);

	return (len < buflen ? 0 : -1);
}

/* 3.6.23 Floating-point conditional select */
static int
a64_simd_handle_fpcs(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	bool dbl = (data->size & 0x1) != 0;
	uint8_t cond = bitx32(in, 15, 12);
	size_t len;

	data->rn.width = dbl ? A64_REGWIDTH_SIMD_64 : A64_REGWIDTH_SIMD_32;
	data->rd.width = data->rm.width = data->rn.width;

	/* <Sd>, <Sn>, <Sm>, <cond> */
	len = dis_snprintf(buf, buflen, " %s, %s, %s, %s",
	    a64_reg_name(data->rd), a64_reg_name(data->rn),
	    a64_reg_name(data->rm), a64_cond_names[cond]);

	return (len < buflen ? 0 : -1);
}

/* 3.6.25 Floating-point data-processing (1 source) */
static int
a64_simd_handle_fpdp1s(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	switch (data->size) {
	case 0:	/* single-precision */
		data->rd.width = data->rn.width = A64_REGWIDTH_SIMD_32;
		break;
	case 1: /* double-precision */
		data->rd.width = data->rn.width = A64_REGWIDTH_SIMD_64;
		break;
	default:
		return (-1);
	}

	len = dis_snprintf(buf, buflen, " %s, %s",
	    a64_reg_name(data->rd), a64_reg_name(data->rn));

	return (len < buflen ? 0 : -1);
}

static int
a64_simd_handle_fpdp1s_fcvt(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	const a64_reg_width_t map[] = {
		[0] = A64_REGWIDTH_SIMD_32,
		[1] = A64_REGWIDTH_SIMD_64,
		[3] = A64_REGWIDTH_SIMD_16,
	};
	uint8_t opc = bitx32(in, 16, 15);

	if (data->size == 2 || opc == 2)
		return (-1);

	data->rn.width = map[data->size];
	data->rd.width = map[opc];

	len = dis_snprintf(buf, buflen, " %s, %s",
	    a64_reg_name(data->rd), a64_reg_name(data->rn));

	return (len < buflen ? 0 : -1);
}

/* 3.6.26 Floating-point data-processing (2 source) */
static int
a64_simd_handle_fpdp2s(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;

	switch (data->size) {
	case 0:	/* single-precision */
		data->rd.width = A64_REGWIDTH_SIMD_32;
		break;
	case 1: /* double-precision */
		data->rd.width = A64_REGWIDTH_SIMD_64;
		break;
	default:
		return (-1);
	}

	data->rn.width = data->rm.width = data->rd.width;

	len = dis_snprintf(buf, buflen, " %s, %s, %s",
	    a64_reg_name(data->rd), a64_reg_name(data->rn),
	    a64_reg_name(data->rm));

	return (len < buflen ? 0 : -1);
}

/* 3.6.27 Floating-point data-processing (3 source) */
static int
a64_simd_handle_fpdp3s(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	a64_reg_t ra;

	ra.id = bitx32(in, 14, 10);

	switch (data->size) {
	case 0:	/* single-precision */
		data->rd.width = A64_REGWIDTH_SIMD_32;
		break;
	case 1: /* double-precision */
		data->rd.width = A64_REGWIDTH_SIMD_64;
		break;
	default:
		return (-1);
	}

	data->rn.width = data->rm.width = ra.width = data->rd.width;

	len = dis_snprintf(buf, buflen, " %s, %s, %s, %s",
	    a64_reg_name(data->rd), a64_reg_name(data->rn),
	    a64_reg_name(data->rm), a64_reg_name(ra));

	return (len < buflen ? 0 : -1);
}

/* 3.6.28 Floating-point immediate */
static int
a64_simd_handle_fpi(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	uint8_t imm8 = bitx32(in, 20, 13);
	uint64_t imm;

	switch (data->size) {
	case 0:	/* single-precision */
		data->rd.width = A64_REGWIDTH_SIMD_32;
		break;
	case 1: /* double-precision */
		data->rd.width = A64_REGWIDTH_SIMD_64;
		break;
	default:
		return (-1);
	}

	imm = a64_vfp_expand_imm(imm8, data->rd.width);

	len = dis_snprintf(buf, buflen, " %s, #",
	    a64_reg_name(data->rd));
	if (len >= buflen)
		return (-1);
	buf += len;
	buflen -= len;

	len = a64_simd_float(imm, data->rd.width, buf, buflen);

	return (len < buflen ? 0 : -1);
}

static int
a64_simd_handle_fpfxpc(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	uint8_t sf = bitx32(in, 31, 31);
	uint8_t rmode = bitx32(in, 20, 19);
	uint8_t scale = bitx32(in, 15, 10);
	a64_reg_t *ireg, *fpreg;
	uint8_t fbits;

	if (rmode == 0) {
		ireg = &data->rn;
		fpreg = &data->rd;
	} else {
		ireg = &data->rd;
		fpreg = &data->rn;
	}

	fpreg->width = data->size == 0 ? A64_REGWIDTH_SIMD_32 :
	    A64_REGWIDTH_SIMD_64;
	ireg->width = sf == 1 ? A64_REGWIDTH_64 : A64_REGWIDTH_32;
	fbits = 64 - scale;

	len = dis_snprintf(buf, buflen, " %s, %s, #%d",
	    a64_reg_name(data->rd), a64_reg_name(data->rn), fbits);

	return (len < buflen ? 0 : -1);
}

static int
a64_simd_handle_fpic(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	uint8_t sf = bitx32(in, 31, 31);
	uint8_t rmode = bitx32(in, 20, 19);
	uint8_t opcode = bitx32(in, 18, 16);
	a64_reg_t *ireg, *fpreg;

	if ((opcode & 6) == 2 && rmode == 0) {
		ireg = &data->rn;
		fpreg = &data->rd;
	} else {
		ireg = &data->rd;
		fpreg = &data->rn;
	}

	ireg->width = sf == 1 ? A64_REGWIDTH_64 : A64_REGWIDTH_32;
	fpreg->width = data->size == 0 ? A64_REGWIDTH_SIMD_32 :
	    A64_REGWIDTH_SIMD_64;

	len = dis_snprintf(buf, buflen, " %s, %s",
	    a64_reg_name(data->rd), a64_reg_name(data->rn));

	return (len < buflen ? 0 : -1);
}

static int
a64_simd_handle_fpic_fmov(const dis_handle_t *dhp,
    const a64_simd_class_t *class, const a64_simd_opcode_entry_t *op,
    a64_simd_data_t *data, uint32_t in, char *buf, size_t buflen)
{
	size_t len;
	uint8_t sf = bitx32(in, 31, 31);
	uint8_t rmode = bitx32(in, 20, 19);
	uint8_t opcode = bitx32(in, 18, 16);
	a64_reg_t *ireg, *fpreg;

	if (opcode & 1) {
		ireg = &data->rn;
		fpreg = &data->rd;
	} else {
		ireg = &data->rd;
		fpreg = &data->rn;
	}

	ireg->width = sf == 1 ? A64_REGWIDTH_64 : A64_REGWIDTH_32;
	switch (data->size) {
	case 0:
		fpreg->width = A64_REGWIDTH_SIMD_32;
		break;
	case 1:
		fpreg->width = A64_REGWIDTH_SIMD_64;
		break;
	case 2:
		fpreg->width = A64_REGWIDTH_SIMD_V;
		break;
	default:
		return (-1);
	}

	if (rmode == 0) {
		len = dis_snprintf(buf, buflen, " %s, %s",
		    a64_reg_name(data->rd), a64_reg_name(data->rn));
	} else if (opcode & 1) {
		len = dis_snprintf(buf, buflen, " %s.d[1], %s",
		    a64_reg_name(data->rd), a64_reg_name(data->rn));
	} else {
		len = dis_snprintf(buf, buflen, " %s, %s.d[1]",
		    a64_reg_name(data->rd), a64_reg_name(data->rn));
	}

	return (len < buflen ? 0 : -1);
}

#define	F_VAR1	SIMD_F_VAR1
#define	F_VAR2	SIMD_F_VAR2
#define	F_VAR3	SIMD_F_VAR3
#define	F_VAR4	SIMD_F_VAR4
#define	F_VAR5	SIMD_F_VAR5
#define	F_VAR6	SIMD_F_VAR6

/* 3.6.1 AdvSIMD EXT */
static const a64_simd_opcode_entry_t a64_simd_ext[] = {
	{ .name = "ext" },
	{ .name = NULL }
};

/* 3.6.2 AdvSIMD TBL/TBX */
static const a64_simd_opcode_entry_t a64_simd_tbltbx[] = {
	/*                op      */
	{ .name = "tbl", .d1v = 0 },
	{ .name = "tbx", .d1v = 1 },
	{ .name = NULL }
};

/* 3.6.3 AdvSIMD ZIP/UZP/TRN */
static const a64_simd_opcode_entry_t a64_simd_trn[] = {
	/*                 opcode  */
	{ .name = "uzp1", .d1v = 1 },
	{ .name = "trn1", .d1v = 2 },
	{ .name = "zip1", .d1v = 3 },
	{ .name = "uzp2", .d1v = 5 },
	{ .name = "trn2", .d1v = 6 },
	{ .name = "zip2", .d1v = 7 },
	{ .name = NULL }
};

/* 3.6.4 ADVSIMD across lanes */
static const a64_simd_opcode_entry_t a64_simd_acr[] = {
	/*                    U         size[1]    opcode  */
	{ .name = "saddlv",  .d1v = 0, .d2v = DC, .d3v = 3,  .flags = F_VAR2 },
	{ .name = "smaxv",   .d1v = 0, .d2v = DC, .d3v = 10, .flags = F_VAR1 },
	{ .name = "sminv",   .d1v = 0, .d2v = DC, .d3v = 26, .flags = F_VAR1 },
	{ .name = "addv",    .d1v = 0, .d2v = DC, .d3v = 27, .flags = F_VAR1 },
	{ .name = "uaddlv",  .d1v = 1, .d2v = DC, .d3v = 3,  .flags = F_VAR2 },
	{ .name = "umaxv",   .d1v = 1, .d2v = DC, .d3v = 10, .flags = F_VAR1 },
	{ .name = "uminv",   .d1v = 1, .d2v = DC, .d3v = 26, .flags = F_VAR1 },
	{ .name = "fmaxnmv", .d1v = 1, .d2v = 0,  .d3v = 12, .flags = F_VAR3 },
	{ .name = "fmaxv",   .d1v = 1, .d2v = 0,  .d3v = 15, .flags = F_VAR3 },
	{ .name = "fminnmv", .d1v = 1, .d2v = 1,  .d3v = 12, .flags = F_VAR3 },
	{ .name = "fminv",   .d1v = 1, .d2v = 1,  .d3v = 15, .flags = F_VAR3 },
	{ .name = NULL }
};

/* 3.6.5 AdvSIMD copy */
static const a64_simd_opcode_entry_t a64_simd_cpy[] = {
	/*                 Q          op        imm4 */
	{ .name = "dup",  .d1v = DC, .d2v = 0, .d3v = 0,  .flags = F_VAR1 },
	{ .name = "dup",  .d1v = DC, .d2v = 0, .d3v = 1,  .flags = F_VAR2 },
	{ .name = "smov", .d1v = 0,  .d2v = 0, .d3v = 5,  .flags = F_VAR3 },
	{ .name = "mov",  .d1v = 0,  .d2v = 0, .d3v = 7,  .flags = F_VAR3 },
	{ .name = "mov",  .d1v = 1,  .d2v = 0, .d3v = 3,  .flags = F_VAR4 },
	{ .name = "smov", .d1v = 1,  .d2v = 0, .d3v = 5,  .flags = F_VAR3 },
	{ .name = "mov",  .d1v = 1,  .d2v = 0, .d3v = 7,  .flags = F_VAR3 },
	{ .name = "mov",  .d1v = 1,  .d2v = 1, .d3v = DC, .flags = F_VAR5 },
	{ .name = NULL }
};

/* 3.6.6 AdvSIMD modified immediate */
#define	F_MIM		SIMD_F_MIM
#define	F_MIM_S		(SIMD_F_MIM | SIMD_F_S)
#define	F_MIM_H		(SIMD_F_MIM | SIMD_F_H)
#define	F_MIM_B		(SIMD_F_MIM | SIMD_F_B)
#define	F_MIM_D		(SIMD_F_MIM | SIMD_F_D)
#define	F_MIM1_S	(SIMD_F_MIM1 | SIMD_F_S)
static const a64_simd_opcode_entry_t a64_simd_mmi[] = {
	/*                 Q          op        cmode */
	{ .name = "movi", .d1v = DC, .d2v = 0, .d3v = 0,  .flags = F_MIM_S },
	{ .name = "movi", .d1v = DC, .d2v = 0, .d3v = 2,  .flags = F_MIM_S },
	{ .name = "movi", .d1v = DC, .d2v = 0, .d3v = 4,  .flags = F_MIM_S },
	{ .name = "movi", .d1v = DC, .d2v = 0, .d3v = 6,  .flags = F_MIM_S },
	{ .name = "orr",  .d1v = DC, .d2v = 0, .d3v = 1,  .flags = F_MIM_S },
	{ .name = "orr",  .d1v = DC, .d2v = 0, .d3v = 3,  .flags = F_MIM_S },
	{ .name = "orr",  .d1v = DC, .d2v = 0, .d3v = 5,  .flags = F_MIM_S },
	{ .name = "orr",  .d1v = DC, .d2v = 0, .d3v = 7,  .flags = F_MIM_S },
	{ .name = "movi", .d1v = DC, .d2v = 0, .d3v = 8,  .flags = F_MIM_H },
	{ .name = "movi", .d1v = DC, .d2v = 0, .d3v = 10, .flags = F_MIM_H },
	{ .name = "orr",  .d1v = DC, .d2v = 0, .d3v = 9,  .flags = F_MIM_H },
	{ .name = "orr",  .d1v = DC, .d2v = 0, .d3v = 11, .flags = F_MIM_H },
	{ .name = "movi", .d1v = DC, .d2v = 0, .d3v = 12, .flags = F_MIM1_S },
	{ .name = "movi", .d1v = DC, .d2v = 0, .d3v = 13, .flags = F_MIM1_S },
	{ .name = "movi", .d1v = DC, .d2v = 0, .d3v = 14, .flags = F_MIM_B },
	{ .name = "fmov", .d1v = DC, .d2v = 0, .d3v = 15, .flags = F_MIM },
	{ .name = "mvni", .d1v = DC,  .d2v = 1, .d3v = 0,  .flags = F_MIM_S },
	{ .name = "mvni", .d1v = DC, .d2v = 1, .d3v = 2,  .flags = F_MIM_S },
	{ .name = "mvni", .d1v = DC, .d2v = 1, .d3v = 4,  .flags = F_MIM_S },
	{ .name = "mvni", .d1v = DC, .d2v = 1, .d3v = 6,  .flags = F_MIM_S },
	{ .name = "bic",  .d1v = DC, .d2v = 1, .d3v = 1,  .flags = F_MIM_S },
	{ .name = "bic",  .d1v = DC, .d2v = 1, .d3v = 3,  .flags = F_MIM_S },
	{ .name = "bic",  .d1v = DC, .d2v = 1, .d3v = 5,  .flags = F_MIM_S },
	{ .name = "bic",  .d1v = DC, .d2v = 1, .d3v = 7,  .flags = F_MIM_S },
	{ .name = "mvni", .d1v = DC, .d2v = 1, .d3v = 8,  .flags = F_MIM_H },
	{ .name = "mvni", .d1v = DC, .d2v = 1, .d3v = 10, .flags = F_MIM_H },
	{ .name = "bic",  .d1v = DC, .d2v = 1, .d3v = 9,  .flags = F_MIM_H },
	{ .name = "bic",  .d1v = DC, .d2v = 1, .d3v = 11, .flags = F_MIM_H },
	{ .name = "mvni", .d1v = DC, .d2v = 1, .d3v = 12, .flags = F_MIM1_S },
	{ .name = "mvni", .d1v = DC, .d2v = 1, .d3v = 13, .flags = F_MIM1_S },
	{ .name = "movi", .d1v = 0,  .d2v = 1, .d3v = 14, .flags = F_MIM_D },
	{ .name = "movi", .d1v = 1,  .d2v = 1, .d3v = 14, .flags = F_MIM_D },
	{ .name = "fmov", .d1v = 1,  .d2v = 1, .d3v = 15, .flags = F_MIM },
	{ .name = NULL }
};

/* 3.6.7 AdvSIMD scalar copy */
static const a64_simd_opcode_entry_t a64_simd_scpy[] = {
	{ .name = "mov", .flags = SIMD_F_VAR6 },
	{ .name = NULL }
};

/* 3.6.8 AdvSIMD scalar pairwise */
static const a64_simd_opcode_entry_t a64_simd_spr[] = {
	/*                    U         size[1]    opcode */
	{ .name = "addp",    .d1v = 0, .d2v = DC, .d3v = 27 },
	{ .name = "fmaxnmp", .d1v = 1, .d2v = 0,  .d3v = 12 },
	{ .name = "faddp",   .d1v = 1, .d2v = 0,  .d3v = 13 },
	{ .name = "fmaxp",   .d1v = 1, .d2v = 0,  .d3v = 15 },
	{ .name = "fminnmp", .d1v = 1, .d2v = 1,  .d3v = 12 },
	{ .name = "fminp",   .d1v = 1, .d2v = 1,  .d3v = 15 },
	{ .name = NULL }
};

/* 3.6.9 AdvSIMD scalar shift by immediate */
static const a64_simd_opcode_entry_t a64_simd_ssh[] = {
	/*                    U          immh       opcode */
	/* If immh is 0, we must skip the decode */
	{ .name = "SKIP",    .d1v = DC, .d2v = 0,  .d3v = DC,
		.flags = SIMD_F_SKIP },
	{ .name = "sshr",     .d1v = 0,  .d2v = DC, .d3v = 0 },
	{ .name = "ssra",     .d1v = 0,  .d2v = DC, .d3v = 2 },
	{ .name = "srshr",    .d1v = 0,  .d2v = DC, .d3v = 4 },
	{ .name = "srsra",    .d1v = 0,  .d2v = DC, .d3v = 6 },
	{ .name = "shl",      .d1v = 0,  .d2v = DC, .d3v = 10,
		.flags = SIMD_F_VAR1 },
	{ .name = "sqshl",    .d1v = 0,  .d2v = DC, .d3v = 14,
		.flags = SIMD_F_VAR1 },
	{ .name = "sqshrn",   .d1v = 0,  .d2v = DC, .d3v = 18,
		.flags = SIMD_F_VAR2 },
	{ .name = "sqrshrn",  .d1v = 0,  .d2v = DC, .d3v = 19,
		.flags = SIMD_F_VAR2 },
	{ .name = "scvtf",    .d1v = 0,  .d2v = DC, .d3v = 28 },
	{ .name = "fcvtzs",   .d1v = 0,  .d2v = DC, .d3v = 31 },
	{ .name = "ushr",     .d1v = 1,  .d2v = DC, .d3v = 0 },
	{ .name = "usra",     .d1v = 1,  .d2v = DC, .d3v = 2 },
	{ .name = "urshr",    .d1v = 1,  .d2v = DC, .d3v = 4 },
	{ .name = "ursra",    .d1v = 1,  .d2v = DC, .d3v = 6 },
	{ .name = "sri",      .d1v = 1,  .d2v = DC, .d3v = 8 },
	{ .name = "sli",      .d1v = 1,  .d2v = DC, .d3v = 10,
		.flags = SIMD_F_VAR1 },
	{ .name = "sqshlu",   .d1v = 1,  .d2v = DC, .d3v = 12,
		.flags = SIMD_F_VAR1 },
	{ .name = "uqshl",    .d1v = 1,  .d2v = DC, .d3v = 14,
		.flags = SIMD_F_VAR1 },
	{ .name = "sqshrun",  .d1v = 1,  .d2v = DC, .d3v = 16,
		.flags = SIMD_F_VAR2 },
	{ .name = "sqrshrun", .d1v = 1,  .d2v = DC, .d3v = 17,
		.flags = SIMD_F_VAR2 },
	{ .name = "uqshrn",   .d1v = 1,  .d2v = DC, .d3v = 18,
		.flags = SIMD_F_VAR2 },
	{ .name = "ucvtf",    .d1v = 1,  .d2v = DC, .d3v = 28 },
	{ .name = "fcvtzu",   .d1v = 1,  .d2v = DC, .d3v = 31 },
	{ .name = NULL }
};

/* 3.6.10 AdvSIMD scalar three different */
static const a64_simd_opcode_entry_t a64_simd_astd[] = {
	/*                    U         opcode */
	{ .name = "sqdmlal", .d1v = 0, .d2v = 9 },
	{ .name = "sqdmlsl", .d1v = 0, .d2v = 11 },
	{ .name = "sqdmull", .d1v = 0, .d2v = 13 },
	{ .name = NULL }
};

/* 3.6.11 AdvSIMD scalar three same */
static const a64_simd_opcode_entry_t a64_simd_asts[] = {
	/*                     U         size[1]    opcode */
	{ .name = "sqadd",    .d1v = 0, .d2v = DC, .d3v = 1 },
	{ .name = "sqsub",    .d1v = 0, .d2v = DC, .d3v = 5 },
	{ .name = "cmgt",     .d1v = 0, .d2v = DC, .d3v = 6 },
	{ .name = "cmge",     .d1v = 0, .d2v = DC, .d3v = 7 },
	{ .name = "sshl",     .d1v = 0, .d2v = DC, .d3v = 8 },
	{ .name = "sqshl",    .d1v = 0, .d2v = DC, .d3v = 9 },
	{ .name = "srshl",    .d1v = 0, .d2v = DC, .d3v = 10 },
	{ .name = "sqrshl",   .d1v = 0, .d2v = DC, .d3v = 11 },
	{ .name = "add",      .d1v = 0, .d2v = DC, .d3v = 16 },
	{ .name = "cmtst",    .d1v = 0, .d2v = DC, .d3v = 17 },
	{ .name = "sqdmulh",  .d1v = 0, .d2v = DC, .d3v = 22 },
	{ .name = "fmulx",    .d1v = 0, .d2v = 0,  .d3v = 27 },
	{ .name = "fcmeq",    .d1v = 0, .d2v = 0,  .d3v = 28 },
	{ .name = "frecps",   .d1v = 0, .d2v = 0,  .d3v = 31 },
	{ .name = "frsqrts",  .d1v = 0, .d2v = 1,  .d3v = 31 },
	{ .name = "uqadd",    .d1v = 1, .d2v = DC, .d3v = 1 },
	{ .name = "uqsub",    .d1v = 1, .d2v = DC, .d3v = 5 },
	{ .name = "cmhi",     .d1v = 1, .d2v = DC, .d3v = 6 },
	{ .name = "cmhs",     .d1v = 1, .d2v = DC, .d3v = 7 },
	{ .name = "ushl",     .d1v = 1, .d2v = DC, .d3v = 8},
	{ .name = "uqshl",    .d1v = 1, .d2v = DC, .d3v = 9 },
	{ .name = "urshl",    .d1v = 1, .d2v = DC, .d3v = 10 },
	{ .name = "uqrshl",   .d1v = 1, .d2v = DC, .d3v = 11 },
	{ .name = "sub",      .d1v = 1, .d2v = DC, .d3v = 16 },
	{ .name = "cmeq",     .d1v = 1, .d2v = DC, .d3v = 17 },
	{ .name = "sqrdmulh", .d1v = 1, .d2v = DC, .d3v = 22 },
	{ .name = "fcmge",   .d1v = 1, .d2v = 0,   .d3v = 28 },
	{ .name = "facge",   .d1v = 1, .d2v = 0,   .d3v = 29 },
	{ .name = "fabd",    .d1v = 1, .d2v = 1,   .d3v = 26 },
	{ .name = "fcmgt",   .d1v = 1, .d2v = 1,   .d3v = 28 },
	{ .name = "facgt",   .d1v = 1, .d2v = 1,   .d3v = 29 },
	{ .name = NULL }
};

/* 3.6.12 AdvSIMD scalar two-reg misc */
static const a64_simd_opcode_entry_t a64_simd_astrm[] = {
	/*                    U         size[1]    opcode */
	{ .name = "suqadd",  .d1v = 0, .d2v = DC, .d3v = 3 },
	{ .name = "sqabs",   .d1v = 0, .d2v = DC, .d3v = 7 },
	{ .name = "cmgt",    .d1v = 0, .d2v = DC, .d3v = 8,
		.flags = SIMD_F_ZERO },
	{ .name = "cmeq",    .d1v = 0, .d2v = DC, .d3v = 9,
		.flags = SIMD_F_ZERO },
	{ .name = "cmlt",    .d1v = 0, .d2v = DC, .d3v = 10,
		.flags = SIMD_F_ZERO },
	{ .name = "abs",     .d1v = 0, .d2v = DC, .d3v = 11 },
	{ .name = "sqxtn",   .d1v = 0, .d2v = DC, .d3v = 20 },
	{ .name = "fcvtns",  .d1v = 0, .d2v = 0,  .d3v = 26 },
	{ .name = "fcvtms",  .d1v = 0, .d2v = 0,  .d3v = 27 },
	{ .name = "fcvtas",  .d1v = 0, .d2v = 0,  .d3v = 28 },
	{ .name = "scvtf",   .d1v = 0, .d2v = 0,  .d3v = 29 },
	{ .name = "fcmgt",   .d1v = 0, .d2v = 1,  .d3v = 12,
		.flags = SIMD_F_ZERO },
	{ .name = "fcmeq",   .d1v = 0, .d2v = 1,  .d3v = 13,
		.flags = SIMD_F_ZERO },
	{ .name = "fcmlt",   .d1v = 0, .d2v = 1,  .d3v = 14,
		.flags = SIMD_F_ZERO },
	{ .name = "fcvtps",  .d1v = 0, .d2v = 1,  .d3v = 26 },
	{ .name = "fcvtzs",  .d1v = 0, .d2v = 1,  .d3v = 27 },
	{ .name = "frecpe",  .d1v = 0, .d2v = 1,  .d3v = 29 },
	{ .name = "frecpx",  .d1v = 0, .d2v = 1,  .d3v = 31 },
	{ .name = "usqadd",  .d1v = 1, .d2v = DC, .d3v = 3 },
	{ .name = "sqneg",   .d1v = 1, .d2v = DC, .d3v = 7 },
	{ .name = "cmge",    .d1v = 1, .d2v = DC, .d3v = 8,
		.flags = SIMD_F_ZERO },
	{ .name = "cmle",    .d1v = 1, .d2v = DC, .d3v = 9,
		.flags = SIMD_F_ZERO },
	{ .name = "neg",     .d1v = 1, .d2v = DC, .d3v = 11 },
	{ .name = "sqxtun",  .d1v = 1, .d2v = DC, .d3v = 18 },
	{ .name = "uqxtn",   .d1v = 1, .d2v = DC, .d3v = 20 },
	{ .name = "fcvtxn",  .d1v = 1, .d2v = 0,  .d3v = 22 },
	{ .name = "fcvtnu",  .d1v = 1, .d2v = 0,  .d3v = 26 },
	{ .name = "fcvtmu",  .d1v = 1, .d2v = 0,  .d3v = 27 },
	{ .name = "fcvtau",  .d1v = 1, .d2v = 0,  .d3v = 28 },
	{ .name = "ucvtf",   .d1v = 1, .d2v = 0,  .d3v = 29 },
	{ .name = "fcmge",   .d1v = 1, .d2v = 1,  .d3v = 12,
		.flags = SIMD_F_ZERO },
	{ .name = "fcmle",   .d1v = 1, .d2v = 1,  .d3v = 13,
		.flags = SIMD_F_ZERO },
	{ .name = "fcvtpu",  .d1v = 1, .d2v = 1,  .d3v = 26 },
	{ .name = "fcvtzu",  .d1v = 1, .d2v = 1,  .d3v = 27 },
	{ .name = "frsqrte", .d1v = 1, .d2v = 1,  .d3v = 29 },
	{ .name = NULL }
};

/* 3.6.13 AdvSIMD scalar x indexed element */
static const a64_simd_opcode_entry_t a64_simd_sxie[] = {
	/*                     U         size[1]    opcode */
	{ .name = "sqdmlal",  .d1v = 0, .d2v = DC, .d3v = 3 },
	{ .name = "sqdmlsl",  .d1v = 0, .d2v = DC, .d3v = 7 },
	{ .name = "sqdmull",  .d1v = 0, .d2v = DC, .d3v = 11 },
	{ .name = "sqdmulh",  .d1v = 0, .d2v = DC, .d3v = 12 },
	{ .name = "sqrdmulh", .d1v = 0, .d2v = DC, .d3v = 13 },
	{ .name = "fmla",     .d1v = 0, .d2v = 1,  .d3v = 1 },
	{ .name = "fmls",     .d1v = 0, .d2v = 1,  .d3v = 5 },
	{ .name = "fmul",     .d1v = 0, .d2v = 1,  .d3v = 9 },
	{ .name = "fmulx",    .d1v = 1, .d2v = 1,  .d3v = 9 },
	{ .name = NULL }
};

/* 3.6.14 AdvSIMD shift by immediate */
static const a64_simd_opcode_entry_t a64_simd_asbi[] = {
	/*                      U         opcode     Q */
	{ .name = "sshr",      .d1v = 0, .d2v = 0,  .d3v = DC },
	{ .name = "ssra",      .d1v = 0, .d2v = 2,  .d3v = DC },
	{ .name = "srshr",     .d1v = 0, .d2v = 4,  .d3v = DC },
	{ .name = "srsra",     .d1v = 0, .d2v = 6,  .d3v = DC },
	{ .name = "shl",       .d1v = 0, .d2v = 10, .d3v = DC,
		.flags = SIMD_F_VAR1 },
	{ .name = "sqshl",     .d1v = 0, .d2v = 14, .d3v = DC,
		.flags = SIMD_F_VAR1 },
	{ .name = "shrn",      .d1v = 0, .d2v = 16, .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "shrn2",     .d1v = 0, .d2v = 16, .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "rshrn",     .d1v = 0, .d2v = 17, .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "rshrn2",    .d1v = 0, .d2v = 17, .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "sqshrn",    .d1v = 0, .d2v = 18, .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "sqshrn2",   .d1v = 0, .d2v = 18, .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "sqrshrn",   .d1v = 0, .d2v = 19, .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "sqrshrn2",  .d1v = 0, .d2v = 19, .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "sshll",     .d1v = 0, .d2v = 20, .d3v = 0,
		.flags = SIMD_F_VAR3 },
	{ .name = "sshll2",    .d1v = 0, .d2v = 20, .d3v = 1,
		.flags = SIMD_F_VAR3 },
	{ .name = "scvtf",     .d1v = 0, .d2v = 28, .d3v = DC },
	{ .name = "fcvtzs",    .d1v = 0, .d2v = 31, .d3v = DC },
	{ .name = "ushr",      .d1v = 1, .d2v = 0,  .d3v = DC },
	{ .name = "usra",      .d1v = 1, .d2v = 2,  .d3v = DC },
	{ .name = "urshr",     .d1v = 1, .d2v = 4,  .d3v = DC },
	{ .name = "ursra",     .d1v = 1, .d2v = 6,  .d3v = DC },
	{ .name = "sri",       .d1v = 1, .d2v = 8,  .d3v = DC },
	{ .name = "sli",       .d1v = 1, .d2v = 10, .d3v = DC,
		.flags = SIMD_F_VAR1 },
	{ .name = "sqshlu",    .d1v = 1, .d2v = 12, .d3v = DC,
		.flags = SIMD_F_VAR1 },
	{ .name = "uqshl",     .d1v = 1, .d2v = 14, .d3v = DC,
		.flags = SIMD_F_VAR1 },
	{ .name = "sqshrun",   .d1v = 1, .d2v = 16, .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "sqshrun2",  .d1v = 1, .d2v = 16, .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "sqrshrun",  .d1v = 1, .d2v = 17, .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "sqrshrun2", .d1v = 1, .d2v = 17, .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "uqshrn",    .d1v = 1, .d2v = 18, .d3v = DC,
		.flags = SIMD_F_VAR2 },
	{ .name = "uqrshrn",   .d1v = 1, .d2v = 19, .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "uqrshrn2",  .d1v = 1, .d2v = 19, .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "ushll",     .d1v = 1, .d2v = 20, .d3v = 0,
		.flags = SIMD_F_VAR3 },
	{ .name = "ushll2",    .d1v = 1, .d2v = 20, .d3v = 1,
		.flags = SIMD_F_VAR3 },
	{ .name = "ucvtf",     .d1v = 1, .d2v = 28, .d3v = DC },
	{ .name = "fcvtzu",    .d1v = 1, .d2v = 31, .d3v = DC },
	{ .name = NULL }
};

/* 3.6.15 AdvSIMD three different */
static const a64_simd_opcode_entry_t a64_simd_atd[] = {
	/*                     U         opcode     Q */
	{ .name = "saddl",    .d1v = 0, .d2v = 0,  .d3v = 0 },
	{ .name = "saddl2",   .d1v = 0, .d2v = 0,  .d3v = 1 },
	{ .name = "saddw",    .d1v = 0, .d2v = 1,  .d3v = 0,
		.flags = SIMD_F_VAR1 },
	{ .name = "saddw2",   .d1v = 0, .d2v = 1,  .d3v = 1,
		.flags = SIMD_F_VAR1 },
	{ .name = "ssubl",    .d1v = 0, .d2v = 2,  .d3v = 0 },
	{ .name = "ssubl2",   .d1v = 0, .d2v = 2,  .d3v = 1 },
	{ .name = "ssubw",    .d1v = 0, .d2v = 3,  .d3v = 0,
		.flags = SIMD_F_VAR1 },
	{ .name = "ssubw2",   .d1v = 0, .d2v = 3,  .d3v = 1,
		.flags = SIMD_F_VAR1 },
	{ .name = "addhn",    .d1v = 0, .d2v = 4,  .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "addhn2",   .d1v = 0, .d2v = 4,  .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "sabal",    .d1v = 0, .d2v = 5,  .d3v = 0 },
	{ .name = "sabal2",   .d1v = 0, .d2v = 5,  .d3v = 1 },
	{ .name = "subhn",    .d1v = 0, .d2v = 6,  .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "subhn2",   .d1v = 0, .d2v = 6,  .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "sabdl",    .d1v = 0, .d2v = 7,  .d3v = 0 },
	{ .name = "sabdl2",   .d1v = 0, .d2v = 7,  .d3v = 1 },
	{ .name = "smlal",    .d1v = 0, .d2v = 8,  .d3v = 0 },
	{ .name = "smlal2",   .d1v = 0, .d2v = 8,  .d3v = 1 },
	{ .name = "sqdmlal",  .d1v = 0, .d2v = 9,  .d3v = 0 },
	{ .name = "sqdmlal2", .d1v = 0, .d2v = 9,  .d3v = 1 },
	{ .name = "smlsl",    .d1v = 0, .d2v = 10, .d3v = 0 },
	{ .name = "smlsl2",   .d1v = 0, .d2v = 10, .d3v = 1 },
	{ .name = "sqdmlsl",  .d1v = 0, .d2v = 11, .d3v = 0 },
	{ .name = "sqdmlsl2", .d1v = 0, .d2v = 11, .d3v = 1 },
	{ .name = "smull",    .d1v = 0, .d2v = 12, .d3v = 0 },
	{ .name = "smull2",   .d1v = 0, .d2v = 12, .d3v = 1 },
	{ .name = "sqdmull",  .d1v = 0, .d2v = 13, .d3v = 0 },
	{ .name = "sqdmull2", .d1v = 0, .d2v = 13, .d3v = 1 },
	{ .name = "pmull",    .d1v = 0, .d2v = 14, .d3v = 0 },
	{ .name = "pmull2",   .d1v = 0, .d2v = 14, .d3v = 1 },
	{ .name = "uaddl",    .d1v = 1, .d2v = 0,  .d3v = 0 },
	{ .name = "uaddl2",   .d1v = 1, .d2v = 0,  .d3v = 1 },
	{ .name = "uaddw",    .d1v = 1, .d2v = 1,  .d3v = 0,
		.flags = SIMD_F_VAR1 },
	{ .name = "uaddw2",   .d1v = 1, .d2v = 1,  .d3v = 1,
		.flags = SIMD_F_VAR1 },
	{ .name = "usubl",    .d1v = 1, .d2v = 2,  .d3v = 0 },
	{ .name = "usubl2",   .d1v = 1, .d2v = 2,  .d3v = 1 },
	{ .name = "usubw",    .d1v = 1, .d2v = 3,  .d3v = 0,
		.flags = SIMD_F_VAR1 },
	{ .name = "usubw2",   .d1v = 1, .d2v = 3,  .d3v = 1,
		.flags = SIMD_F_VAR1 },
	{ .name = "raddhn",   .d1v = 1, .d2v = 4,  .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "raddhn2",  .d1v = 1, .d2v = 4,  .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "uabal",    .d1v = 1, .d2v = 5,  .d3v = 0 },
	{ .name = "uabal2",   .d1v = 1, .d2v = 5,  .d3v = 1 },
	{ .name = "rsubhn",   .d1v = 1, .d2v = 6,  .d3v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "rsubhn2",  .d1v = 1, .d2v = 6,  .d3v = 1,
		.flags = SIMD_F_VAR2 },
	{ .name = "uabdl",    .d1v = 1, .d2v = 7,  .d3v = 0 },
	{ .name = "uabdl2",   .d1v = 1, .d2v = 7,  .d3v = 1 },
	{ .name = "umlal",    .d1v = 1, .d2v = 8,  .d3v = 0 },
	{ .name = "umlal2",   .d1v = 1, .d2v = 8,  .d3v = 1 },
	{ .name = "umlsl",    .d1v = 1, .d2v = 11, .d3v = 0 },
	{ .name = "umlsl2",   .d1v = 1, .d2v = 11, .d3v = 1 },
	{ .name = "umull",    .d1v = 1, .d2v = 12, .d3v = 0 },
	{ .name = "umull2",   .d1v = 1, .d2v = 12, .d3v = 1 },
	{ .name = NULL }
};

/* 3.6.16 AdvSIMD three same */
static const a64_simd_opcode_entry_t a64_simd_ats[] = {
	/*                     U         size[1]    size[0]    opcode  */
	{ .name = "shadd",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 0 },
	{ .name = "sqadd",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 1 },
	{ .name = "srhadd",   .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 2 },
	{ .name = "shsub",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 4 },
	{ .name = "sqsub",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 5 },
	{ .name = "cmgt",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 6 },
	{ .name = "cmge",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 7 },
	{ .name = "sshl",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 8 },
	{ .name = "sqshl",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 9 },
	{ .name = "srshl",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 10 },
	{ .name = "sqrshl",   .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 11 },
	{ .name = "smax",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 12 },
	{ .name = "smin",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 13 },
	{ .name = "sabd",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 14 },
	{ .name = "saba",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 15 },
	{ .name = "add",      .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 16 },
	{ .name = "cmtst",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 17 },
	{ .name = "mla",      .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 18 },
	{ .name = "mul",      .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 19 },
	{ .name = "smaxp",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 20 },
	{ .name = "sminp",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 21 },
	{ .name = "sqdmulh",  .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 22 },
	{ .name = "addp",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 23 },
	{ .name = "fmaxnm",   .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 24 },
	{ .name = "fmla",     .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 25 },
	{ .name = "fadd",     .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 26 },
	{ .name = "fmulx",    .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 27 },
	{ .name = "fcmeq",    .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 28 },
	{ .name = "fmax",     .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 30 },
	{ .name = "frecps",   .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 31 },
	{ .name = "and",      .d1v = 0, .d2v = 0,  .d3v = 0,  .d4v = 3 },
	{ .name = "bic",      .d1v = 0, .d2v = 0,  .d3v = 1,  .d4v = 3 },
	{ .name = "fminnm",   .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = 24 },
	{ .name = "fmls",     .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = 25 },
	{ .name = "fsub",     .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = 26 },
	{ .name = "fmin",     .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = 30 },
	{ .name = "frsqrts",  .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = 31 },
	{ .name = "orr",      .d1v = 0, .d2v = 1,  .d3v = 0,  .d4v = 3 },
	{ .name = "orn",      .d1v = 0, .d2v = 1,  .d3v = 1,  .d4v = 3 },
	{ .name = "uhadd",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 0 },
	{ .name = "uqadd",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 1 },
	{ .name = "urhadd",   .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 2 },
	{ .name = "uhsub",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 4 },
	{ .name = "uqsub",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 5 },
	{ .name = "cmhi",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 6 },
	{ .name = "cmhs",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 7 },
	{ .name = "ushl",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 8 },
	{ .name = "uqshl",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 9 },
	{ .name = "urshl",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 10 },
	{ .name = "uqrshl",   .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 11 },
	{ .name = "umax",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 12 },
	{ .name = "umin",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 13 },
	{ .name = "uabd",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 14 },
	{ .name = "uaba",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 15 },
	{ .name = "sub",      .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 16 },
	{ .name = "cmeq",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 17 },
	{ .name = "mls",      .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 18 },
	{ .name = "pmul",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 19 },
	{ .name = "umaxp",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 20 },
	{ .name = "uminp",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 21 },
	{ .name = "sqrdmulh", .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 22 },
	{ .name = "fmaxnmp",  .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = 24 },
	{ .name = "faddp",    .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = 26 },
	{ .name = "fmul",     .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = 27 },
	{ .name = "fcmge",    .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = 28 },
	{ .name = "facge",    .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = 29 },
	{ .name = "fmaxp",    .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = 30 },
	{ .name = "fdiv",     .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = 31 },
	{ .name = "eor",      .d1v = 1, .d2v = 0,  .d3v = 0,  .d4v = 3 },
	{ .name = "bsl",      .d1v = 1, .d2v = 0,  .d3v = 1,  .d4v = 3 },
	{ .name = "fminnmp",  .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = 24 },
	{ .name = "fabd",     .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = 26 },
	{ .name = "fcmgt",    .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = 28 },
	{ .name = "facgt",    .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = 29 },
	{ .name = "fminp",    .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = 30 },
	{ .name = "bit",      .d1v = 1, .d2v = 1,  .d3v = 0,  .d4v = 3 },
	{ .name = "bif",      .d1v = 1, .d2v = 1,  .d3v = 1,  .d4v = 3 },
	{ .name = NULL }
};

/* 3.6.17 AdvSIMD two-reg misc */
static const a64_simd_opcode_entry_t a64_simd_atrm[] = {
	/*       opcode       U         size[1]    size[0]    Q */
	{ .name = "rev64",   .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 0 },
	{ .name = "rev16",   .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 1 },
	{ .name = "saddlp",  .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 2, .handler = a64_simd_handle_atrm_4h8b },
	{ .name = "suqadd",  .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 3 },
	{ .name = "cls",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 4 },
	{ .name = "cnt",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 5 },
	{ .name = "sadalp",  .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 6, .handler = a64_simd_handle_atrm_4h8b },
	{ .name = "sqabs",   .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 7 },
	{ .name = "cmgt",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 8, .flags = SIMD_F_ZERO },
	{ .name = "cmeq",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 9, .flags = SIMD_F_ZERO },
	{ .name = "cmlt",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 10, .flags = SIMD_F_ZERO },
	{ .name = "abs",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 11 },
	{ .name = "xtn",     .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 0,
		.d5v = 18, .handler = a64_simd_handle_atrm_8b8h },
	{ .name = "xtn2",    .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 1,
		.d5v = 18, .handler = a64_simd_handle_atrm_8b8h },
	{ .name = "sqxtn",   .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 0,
		.d5v = 20, .handler = a64_simd_handle_atrm_8b8h },
	{ .name = "sqxtn2",  .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 1,
		.d5v = 20, .handler = a64_simd_handle_atrm_8b8h },
	{ .name = "fcvtn",   .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 0,
		.d5v = 22, .handler = a64_simd_handle_atrm_fcvt },
	{ .name = "fcvtn2",  .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 1,
		.d5v = 22, .handler = a64_simd_handle_atrm_fcvt },
	{ .name = "fcvtl",   .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 0,
		.d5v = 23, .handler = a64_simd_handle_atrm_fcvt },
	{ .name = "fcvtl2",  .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = 1,
		.d5v = 23, .handler = a64_simd_handle_atrm_fcvt },
	{ .name = "frintn",  .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 24 },
	{ .name = "frintm",  .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 25 },
	{ .name = "fcvtns",  .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 26 },
	{ .name = "fcvtms",  .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 27 },
	{ .name = "fcvtas",  .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 28 },
	{ .name = "scvtf",   .d1v = 0, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 29 },
	{ .name = "fcmgt",   .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 12, .flags = SIMD_F_ZERO },
	{ .name = "fcmeq",   .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 13, .flags = SIMD_F_ZERO },
	{ .name = "fcmlt",   .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 14, .flags = SIMD_F_ZERO },
	{ .name = "fabs",    .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 15 },
	{ .name = "frintp",  .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 24 },
	{ .name = "frintz",  .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 25 },
	{ .name = "fcvtps",  .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 26 },
	{ .name = "fcvtzs",  .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 27 },
	{ .name = "urecpe",  .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 28 },
	{ .name = "frecpe",  .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 29 },
	{ .name = "rev32",   .d1v = 1, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 0 },
	{ .name = "uaddlp",  .d1v = 1, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 2, .handler = a64_simd_handle_atrm_4h8b },
	{ .name = "usqadd",  .d1v = 1, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 3 },
	{ .name = "clz",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 4 },
	{ .name = "uadalp",  .d1v = 1, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 6, .handler = a64_simd_handle_atrm_4h8b },
	{ .name = "sqneg",   .d1v = 1, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 7 },
	{ .name = "cmge",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 8, .flags = SIMD_F_ZERO },
	{ .name = "cmle",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 9, .flags = SIMD_F_ZERO },
	{ .name = "neg",     .d1v = 1, .d2v = DC, .d3v = DC, .d4v = DC,
		.d5v = 11 },
	{ .name = "sqxtun",  .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 0,
		.d5v = 18 },
	{ .name = "sqxtun2", .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 1,
		.d5v = 18 },
	{ .name = "shll",    .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 0,
		.d5v = 19, .handler = a64_simd_handle_atrm_shll },
	{ .name = "shll2",   .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 1,
		.d5v = 19, .handler = a64_simd_handle_atrm_shll },
	{ .name = "uqxtn",   .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 0,
		.d5v = 20, .handler = a64_simd_handle_atrm_8b8h },
	{ .name = "uqxtn2",  .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 1,
		.d5v = 20, .handler = a64_simd_handle_atrm_8b8h },
	{ .name = "fcvtxn",  .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = 0,
		.d5v = 22, .handler = a64_simd_handle_atrm_fcvt },
	{ .name = "fcvtxn2", .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = 1,
		.d5v = 22, .handler = a64_simd_handle_atrm_fcvt },
	{ .name = "frinta",  .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 24 },
	{ .name = "frintx",  .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 25 },
	{ .name = "fcvtnu",  .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 26 },
	{ .name = "fcvtmu",  .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 27 },
	{ .name = "fcvtau",  .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 28 },
	{ .name = "ucvtf",   .d1v = 1, .d2v = 0,  .d3v = DC, .d4v = DC,
		.d5v = 29 },
	{ .name = "mvn",     .d1v = 1, .d2v = 0,  .d3v = 0,  .d4v = DC,
		.d5v = 5 },
	{ .name = "rbit",    .d1v = 1, .d2v = 0,  .d3v = 1,  .d4v = DC,
		.d5v = 5 },
	{ .name = "fcmge",   .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 12 },
	{ .name = "fcmle",   .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 13 },
	{ .name = "fneg",    .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 15 },
	{ .name = "frinti",  .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 25 },
	{ .name = "fcvtpu",  .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 26 },
	{ .name = "fcvtzu",  .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 27 },
	{ .name = "ursqrte", .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 28 },
	{ .name = "frsqrte", .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 29 },
	{ .name = "fsqrt",   .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = DC,
		.d5v = 31 },
	{ .name = NULL }
};

/* 3.6.18 AdvSIMD vector x indexed element */
static const a64_simd_opcode_entry_t a64_simd_avxie[] = {
	/*                     U         size[1]    Q          opcode */
	{ .name = "smlal",    .d1v = 0, .d2v = DC, .d3v = 0,  .d4v = 2 },
	{ .name = "smlal2",   .d1v = 0, .d2v = DC, .d3v = 1,  .d4v = 2 },
	{ .name = "sqdmlal",  .d1v = 0, .d2v = DC, .d3v = 0,  .d4v = 3 },
	{ .name = "sqdmlal2", .d1v = 0, .d2v = DC, .d3v = 1,  .d4v = 3 },
	{ .name = "smlsl",    .d1v = 0, .d2v = DC, .d3v = 0,  .d4v = 6 },
	{ .name = "smlsl2",   .d1v = 0, .d2v = DC, .d3v = 1,  .d4v = 6 },
	{ .name = "sqdmlsl",  .d1v = 0, .d2v = DC, .d3v = 0,  .d4v = 7 },
	{ .name = "sqdmlsl2", .d1v = 0, .d2v = DC, .d3v = 1,  .d4v = 7 },
	{ .name = "mul",      .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 8,
		.flags = SIMD_F_VAR2 },
	{ .name = "smull",    .d1v = 0, .d2v = DC, .d3v = 0,  .d4v = 10 },
	{ .name = "smull2",   .d1v = 0, .d2v = DC, .d3v = 1,  .d4v = 10 },
	{ .name = "sqdmull",  .d1v = 0, .d2v = DC, .d3v = 0,  .d4v = 11 },
	{ .name = "sqdmull2", .d1v = 0, .d2v = DC, .d3v = 1,  .d4v = 11 },
	{ .name = "sqdmulh",  .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 12 },
	{ .name = "sqrdmulh", .d1v = 0, .d2v = DC, .d3v = DC, .d4v = 12,
		.flags = SIMD_F_VAR2 },
	{ .name = "fmla",     .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = 1,
		.flags = SIMD_F_VAR1 },
	{ .name = "fmls",     .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = 5,
		.flags = SIMD_F_VAR1 },
	{ .name = "fmul",     .d1v = 0, .d2v = 1,  .d3v = DC, .d4v = 9,
		.flags = SIMD_F_VAR1 },
	{ .name = "mla",      .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 0,
		.flags = SIMD_F_VAR2 },
	{ .name = "umlal",    .d1v = 1, .d2v = DC, .d3v = 0,  .d4v = 2 },
	{ .name = "umlal2",   .d1v = 1, .d2v = DC, .d3v = 1,  .d4v = 2 },
	{ .name = "mls",      .d1v = 1, .d2v = DC, .d3v = DC, .d4v = 4,
		.flags = SIMD_F_VAR2 },
	{ .name = "umlsl",    .d1v = 1, .d2v = DC, .d3v = 0,  .d4v = 6 },
	{ .name = "umlsl2",   .d1v = 1, .d2v = DC, .d3v = 1,  .d4v = 6 },
	{ .name = "umull",    .d1v = 1, .d2v = DC, .d3v = 0,  .d4v = 10 },
	{ .name = "umull2",   .d1v = 1, .d2v = DC, .d3v = 1,  .d4v = 10 },
	{ .name = "fmulx",    .d1v = 1, .d2v = 1,  .d3v = DC, .d4v = 9,
		.flags = SIMD_F_VAR1 },
	{ .name = NULL }
};

/* 3.6.19 Crypto AES */
static const a64_simd_opcode_entry_t a64_simd_aes[] = {
	/*                   opcode */
	{ .name = "aese",   .d1v = 4 },
	{ .name = "aesd",   .d1v = 5 },
	{ .name = "aesmc",  .d1v = 6 },
	{ .name = "aesimc", .d1v = 7 },
	{ .name = NULL }
};

/* 3.6.20 Crypto three-reg SHA */
static const a64_simd_opcode_entry_t a64_simd_3rsha[] = {
	/*                      opcode */
	{ .name = "sha1c",     .d1v = 0 },
	{ .name = "sha1p",     .d1v = 1 },
	{ .name = "sha1m",     .d1v = 2 },
	{ .name = "sha1su0",   .d1v = 3, .flags = SIMD_F_VAR2 },
	{ .name = "sha256h",   .d1v = 4, .flags = SIMD_F_VAR1 },
	{ .name = "sha256h2",  .d1v = 5, .flags = SIMD_F_VAR1 },
	{ .name = "sha256su1", .d1v = 6, .flags = SIMD_F_VAR2 },
	{ .name = NULL }
};

/* 3.6.21 Crypto two-reg SHA */
static const a64_simd_opcode_entry_t a64_simd_2rsha[] = {
	/*                      opcode */
	{ .name = "sha1h",     .d1v = 0, .flags = SIMD_F_VAR1 },
	{ .name = "sha1su1",   .d1v = 1 },
	{ .name = "sha256su0", .d1v = 2 },
	{ .name = NULL }
};

/* 3.6.22 Floating-point compare */
static const a64_simd_opcode_entry_t a64_simd_fpc[] = {
	/*                  opcode2[4] */
	{ .name = "fcmp",  .d1v = 0 },
	{ .name = "fcmpe", .d1v = 1 },
	{ .name = NULL }
};

/* 3.6.23 Floating-point conditional compare */
static const a64_simd_opcode_entry_t a64_simd_fpcc[] = {
	/*                   op */
	{ .name = "fccmp",  .d1v = 0 },
	{ .name = "fccmpe", .d1v = 1 },
	{ .name = NULL }
};

/* 3.6.24 Floating-point conditional select */
static const a64_simd_opcode_entry_t a64_simd_fpcs[] = {
	{ .name = "fcsel" },
	{ .name = NULL }
};

/* 3.6.25 Floating-point data-processing (1 source) */
static const a64_simd_opcode_entry_t a64_simd_fpdp1s[] = {
	/*                   type      opcode */
	{ .name = "fmov",   .d1v = 0, .d2v = 0 },
	{ .name = "fabs",   .d1v = 0, .d2v = 1 },
	{ .name = "fneg",   .d1v = 0, .d2v = 2 },
	{ .name = "fsqrt",  .d1v = 0, .d2v = 4 },
	{ .name = "fcvt",   .d1v = 0, .d2v = 5,
		.handler = a64_simd_handle_fpdp1s_fcvt },
	{ .name = "fcvt",   .d1v = 0, .d2v = 7,
		.handler = a64_simd_handle_fpdp1s_fcvt },
	{ .name = "frintn", .d1v = 0, .d2v = 8 },
	{ .name = "frintp", .d1v = 0, .d2v = 9 },
	{ .name = "frintm", .d1v = 0, .d2v = 10 },
	{ .name = "frintz", .d1v = 0, .d2v = 11 },
	{ .name = "frinta", .d1v = 0, .d2v = 12 },
	{ .name = "frintx", .d1v = 0, .d2v = 14 },
	{ .name = "frinti", .d1v = 0, .d2v = 15 },
	{ .name = "fmov",   .d1v = 1, .d2v = 0 },
	{ .name = "fabs",   .d1v = 1, .d2v = 1 },
	{ .name = "fneg",   .d1v = 1, .d2v = 2 },
	{ .name = "fsqrt",  .d1v = 1, .d2v = 3 },
	{ .name = "fcvt",   .d1v = 1, .d2v = 4,
		.handler = a64_simd_handle_fpdp1s_fcvt },
	{ .name = "fcvt",   .d1v = 1, .d2v = 7,
		.handler = a64_simd_handle_fpdp1s_fcvt },
	{ .name = "frintn", .d1v = 1, .d2v = 8 },
	{ .name = "frintp", .d1v = 1, .d2v = 9 },
	{ .name = "frintm", .d1v = 1, .d2v = 10 },
	{ .name = "frintz", .d1v = 1, .d2v = 11 },
	{ .name = "frinta", .d1v = 1, .d2v = 12 },
	{ .name = "frintx", .d1v = 1, .d2v = 14 },
	{ .name = "frinti", .d1v = 1, .d2v = 15 },
	{ .name = "fcvt",   .d1v = 3, .d2v = 4,
		.handler = a64_simd_handle_fpdp1s_fcvt },
	{ .name = "fcvt",   .d1v = 3, .d2v = 5,
		.handler = a64_simd_handle_fpdp1s_fcvt },
	{ .name = NULL }
};

/* 3.6.26 Floating-point data-processing (2 source) */
static const a64_simd_opcode_entry_t a64_simd_fpdp2s[] = {
	/*                   opcode */
	{ .name = "fmul",   .d1v = 0 },
	{ .name = "fdiv",   .d1v = 1 },
	{ .name = "fadd",   .d1v = 2 },
	{ .name = "fsub",   .d1v = 3 },
	{ .name = "fmax",   .d1v = 4 },
	{ .name = "fmin",   .d1v = 5 },
	{ .name = "fmaxnm", .d1v = 6 },
	{ .name = "fminnm", .d1v = 7 },
	{ .name = "fnmul",  .d1v = 8 },
	{ .name = NULL }
};

/* 3.6.27 Floating-point data-processing (3 source) */
static const a64_simd_opcode_entry_t a64_simd_fpdp3s[] = {
	/*                   o1        o0 */
	{ .name = "fmadd",  .d1v = 0, .d2v = 0 },
	{ .name = "fmsub",  .d1v = 0, .d2v = 1 },
	{ .name = "fnmadd", .d1v = 1, .d2v = 0 },
	{ .name = "fnmsub", .d1v = 1, .d2v = 1 },
	{ .name = NULL }
};

/* 3.6.28 Floating-point immediate */
static const a64_simd_opcode_entry_t a64_simd_fpi[] = {
	{ .name = "fmov" },
	{ .name = NULL }
};

/* 3.6.29 Floating-point<->fixed-point conversions */
static const a64_simd_opcode_entry_t a64_simd_fpfxpc[] = {
	/*                   opcode */
	{ .name = "scvtf",  .d1v = 2 },
	{ .name = "ucvtf",  .d1v = 3 },
	{ .name = "fcvtzs", .d1v = 0 },
	{ .name = "fcvtzu", .d1v = 1 },
	{ .name = NULL }
};

/* 3.6.30 Floating-point<->integer conversions */
static const a64_simd_opcode_entry_t a64_simd_fpic[] = {
	/*                   rmode      opcode */
	{ .name = "fcvtns", .d1v = 0,  .d2v = 0 },
	{ .name = "fcvtnu", .d1v = 0,  .d2v = 1 },
	{ .name = "scvtf",  .d1v = 0,  .d2v = 2 },
	{ .name = "ucvtf",  .d1v = 0,  .d2v = 3 },
	{ .name = "fcvtas", .d1v = 0,  .d2v = 4 },
	{ .name = "fcvtau", .d1v = 0,  .d2v = 5 },
	{ .name = "fmov",   .d1v = DC, .d2v = 6,
		.handler = a64_simd_handle_fpic_fmov },
	{ .name = "fmov",   .d1v = DC, .d2v = 7,
		.handler = a64_simd_handle_fpic_fmov },
	{ .name = "fcvtps", .d1v = 1,  .d2v = 0 },
	{ .name = "fcvtpu", .d1v = 1,  .d2v = 1 },
	{ .name = "fcvtms", .d1v = 2,  .d2v = 0 },
	{ .name = "fcvtmu", .d1v = 2,  .d2v = 1 },
	{ .name = "fcvtzs", .d1v = 3,  .d2v = 0 },
	{ .name = "fcvtzu", .d1v = 3,  .d2v = 1 },
	{ .name = NULL }
};

static const a64_simd_class_t a64_simd_classes[] = {
	{	/* 3.6.1 AdvSIMD EXT */
		.mask = BIT(31) | BITS(29, 24) | BIT(21) | BIT(15) | BIT(10),
		.targ = BIT(29) | BITS(27, 25),
		.handler = a64_simd_handle_ext,
		.entries = a64_simd_ext
	}, {	/* 3.6.2 AdvSIMD TBL/TBX */
		.mask =
		    BIT(31) | BITS(29, 24) | BIT(21) | BIT(15) | BITS(11, 10),
		.targ = BITS(27, 25),
		.handler = a64_simd_handle_tbltbx,
		.entries = a64_simd_tbltbx,
		.d1m = BIT(12), .d1s = 12	/* op */
	}, {	/* 3.6.3 AdvSIMD ZIP/UZP/TRN */
		.mask =
		    BIT(31) | BITS(29, 24) | BIT(21) | BIT(15) | BITS(11, 10),
		.targ = BITS(27, 25) | BIT(11),
		.handler = a64_simd_handle_trn,
		.entries = a64_simd_trn,
		.d1m = BITS(14, 12), .d1s = 12	/* opcode */
	}, {	/* 3.6.4 ADVSIMD across lanes */
		.mask = BIT(31) | BITS(28, 24) | BITS(21, 17) | BITS(11, 10),
		.targ = BITS(27, 25) | BITS(21, 20) | BIT(11),
		.handler = a64_simd_handle_acr,
		.entries = a64_simd_acr,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BIT(23), .d2s = 23,	/* size[1] */
		.d3m = BITS(16, 12), .d3s = 12	/* opcode */
	}, {	/* 3.6.5 AdvSIMD copy */
		.mask = BIT(31) | BITS(28, 21) | BIT(15) | BIT(10),
		.targ = BITS(27, 25) | BIT(10),
		.handler = a64_simd_handle_cpy,
		.entries = a64_simd_cpy,
		.d1m = BIT(30), .d1s = 30,	/* Q */
		.d2m = BIT(29), .d2s = 29,	/* op */
		.d3m = BITS(14, 11), .d3s = 11	/* imm4 */
	}, {	/* 3.6.6 AdvSIMD modified immediate */
		.mask = BIT(31) | BITS(28, 19) | BIT(10),
		.targ = BITS(27, 24) | BIT(10),
		.handler = a64_simd_handle_mmi,
		.entries = a64_simd_mmi,
		.d1m = BIT(30), .d1s = 30,	/* Q */
		.d2m = BIT(29), .d2s = 29,	/* op */
		.d3m = BITS(15, 12), .d3s = 12	/* cmode */
	}, {	/* 3.6.7 AdvSIMD scalar copy */
		.mask = BITS(31, 30) | BITS(28, 21) | BIT(15) | BIT(10),
		.targ = BIT(30) | BITS(28, 25) | BIT(10),
		.handler = a64_simd_handle_cpy,
		.entries = a64_simd_scpy
	}, {	/* 3.6.8 AdvSIMD scalar pairwise */
		.mask =
		    BITS(31, 30) | BITS(28, 24) | BITS(21, 17) | BITS(11, 10),
		.targ = BIT(30) | BITS(28, 25) | BITS(21, 20) | BIT(11),
		.handler = a64_simd_handle_spr,
		.entries = a64_simd_spr,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BIT(23), .d2s = 23,	/* size[1] */
		.d3m = BITS(16, 12), .d3s = 12	/* opcode */
	}, {	/* 3.6.9 AdvSIMD scalar shift by immediate */
		.mask = BITS(31, 30) | BITS(28, 23) | BIT(10),
		.targ = BIT(30) | BITS(28, 24) | BIT(10),
		.handler = a64_simd_handle_ssh,
		.entries = a64_simd_ssh,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BITS(22, 19), .d2s = 19,	/* immh */
		.d3m = BITS(15, 11), .d3s = 11	/* opcode */
	}, {	/* 3.6.10 AdvSIMD scalar three different */
		.mask = BITS(31, 30) | BITS(28, 24) | BIT(21) | BITS(11, 10),
		.targ = BIT(30) | BITS(28, 25) | BIT(21),
		.handler = a64_simd_handle_astd,
		.entries = a64_simd_astd,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BITS(15, 12), .d2s = 12	/* opcode */
	}, {	/* 3.6.11 AdvSIMD scalar three same */
		.mask = BITS(31, 30) | BITS(28, 24) | BIT(21) | BIT(10),
		.targ = BIT(30) | BITS(28, 25) | BIT(21) | BIT(10),
		.handler = a64_simd_handle_asts,
		.entries = a64_simd_asts,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BIT(23), .d2s = 23,	/* size[1] */
		.d3m = BITS(15, 11), .d3s = 11	/* opcode */
	}, {	/* 3.6.12 AdvSIMD scalar two-reg misc */
		.mask =
		    BITS(31, 30) | BITS(28, 24) | BITS(21, 17) | BITS(11, 10),
		.targ = BIT(30) | BITS(28, 25) | BIT(21) | BIT(11),
		.handler = a64_simd_handle_astrm,
		.entries = a64_simd_astrm,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BIT(23), .d2s = 23,	/* size[1] */
		.d3m = BITS(16, 12), .d3s = 12	/* opcode */
	}, {	/* 3.6.13 AdvSIMD scalar x indexed element */
		.mask = BITS(31, 30) | BITS(28, 24) | BIT(10),
		.targ = BIT(30) | BITS(28, 24),
		.handler = a64_simd_handle_sxie,
		.entries = a64_simd_sxie,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BIT(23), .d2s = 23,	/* size[1] */
		.d3m = BITS(15, 12), .d3s = 12	/* opcode */
	}, {	/* 3.6.14 AdvSIMD shift by immediate */
		.mask = BIT(31) | BITS(28, 23) | BIT(10),
		.targ = BITS(27, 24) | BIT(10),
		.handler = a64_simd_handle_asbi,
		.entries = a64_simd_asbi,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BITS(15, 11), .d2s = 11,	/* opcode */
		.d3m = BIT(30), .d3s = 30	/* Q */
	}, {	/* 3.6.15 AdvSIMD three different */
		.mask = BIT(31) | BITS(28, 24) | BIT(21) | BITS(11, 10),
		.targ = BITS(27, 25) | BIT(21),
		.handler = a64_simd_handle_atd,
		.entries = a64_simd_atd,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BITS(15, 12), .d2s = 12,	/* opcode */
		.d3m = BIT(30), .d3s = 30	/* Q */
	}, {	/* 3.6.16 AdvSIMD three same */
		.mask = BIT(31) | BITS(28, 24) | BIT(21) | BIT(10),
		.targ = BITS(27, 25) | BIT(21) | BIT(10),
		.handler = a64_simd_handle_ats,
		.entries = a64_simd_ats,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BIT(23), .d2s = 23,	/* size[1] */
		.d3m = BIT(22), .d3s = 22,	/* size[0] */
		.d4m = BITS(15, 11), .d4s = 11	/* opcode */
	}, {	/* 3.6.17 AdvSIMD two-reg misc */
		.mask = BIT(31) | BITS(28, 24) | BITS(21, 17) | BITS(11, 10),
		.targ = BITS(27, 25) | BIT(21) | BIT(11),
		.handler = a64_simd_handle_atrm,
		.entries = a64_simd_atrm,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BIT(23), .d2s = 23,	/* size[1] */
		.d3m = BIT(22), .d3s = 22,	/* size[0] */
		.d4m = BIT(30), .d4s = 30,	/* Q */
		.d5m = BITS(16, 12), .d5s = 12	/* opcode */
	}, {	/* 3.6.18 AdvSIMD vector x indexed element */
		.mask = BIT(31) | BITS(28, 24) | BIT(10),
		.targ = BITS(27, 24),
		.handler = a64_simd_handle_avxie,
		.entries = a64_simd_avxie,
		.d1m = BIT(29), .d1s = 29,	/* U */
		.d2m = BIT(23), .d2s = 23,	/* size[1] */
		.d3m = BIT(30), .d3s = 30,	/* Q */
		.d4m = BITS(15, 12), .d4s = 12	/* opcode */
	}, {	/* 3.6.19 Crypto AES */
		.mask = BITS(31, 24) | BITS(21, 17) | BITS(11, 10),
		.targ = BIT(30) | BITS(27, 25) | BIT(21) | BIT(11),
		.handler = a64_simd_handle_aes,
		.entries = a64_simd_aes,
		.d1m = BITS(16, 12), .d1s = 12	/* opcode */
	}, {	/* 3.6.20 Crypto three-reg SHA */
		.mask = BITS(31, 24) | BIT(21) | BIT(15) | BITS(11, 10),
		.targ = BIT(30) | BITS(28, 25),
		.handler = a64_simd_handle_3rsha,
		.entries = a64_simd_3rsha,
		.d1m = BITS(14, 12), .d1s = 12	/* opcode */
	}, {	/* 3.6.21 Crypto two-reg SHA */
		.mask = BITS(31, 24) | BITS(21, 17) | BITS(11, 10),
		.targ = BIT(30) | BITS(28, 25) | BIT(21) | BIT(19) | BIT(11),
		.handler = a64_simd_handle_2rsha,
		.entries = a64_simd_2rsha,
		.d1m = BITS(16, 12), .d1s = 12	/* opcode */
	}, {	/* 3.6.22 Floating-point compare */
		.mask = BIT(30) | BITS(28, 24) | BIT(21) | BITS(13, 10),
		.targ = BITS(28, 25) | BIT(21) | BIT(13),
		.handler = a64_simd_handle_fpc,
		.entries = a64_simd_fpc,
		.d1m = BIT(4), .d1s = 4		/* opcode2[4], fcmp/fcmpe bit */
	}, {	/* 3.6.23 Floating-point conditional compare */
		.mask = BIT(30) | BITS(28, 24) | BIT(21) | BITS(11, 10),
		.targ = BITS(28, 25) | BIT(21) | BIT(10),
		.handler = a64_simd_handle_fpcc,
		.entries = a64_simd_fpcc,
		.d1m = BIT(4), .d1s = 4		/* op */
	}, {	/* 3.6.24 Floating-point conditional select */
		.mask = BIT(30) | BITS(28, 24) | BIT(21) | BITS(11, 10),
		.targ = BITS(28, 25) | BIT(21) | BITS(11, 10),
		.handler = a64_simd_handle_fpcs,
		.entries = a64_simd_fpcs
	}, {	/* 3.6.25 Floating-point data-processing (1 source) */
		.mask = BIT(30) | BITS(28, 24) | BIT(21) | BITS(14, 10),
		.targ = BITS(28, 25) | BIT(21) | BIT(14),
		.handler = a64_simd_handle_fpdp1s,
		.entries = a64_simd_fpdp1s,
		.d1m = BITS(23, 22), .d1s = 22,	/* type */
		.d2m = BITS(20, 15), .d2s = 15	/* opcode */
	}, {	/* 3.6.26 Floating-point data-processing (2 source) */
		.mask = BIT(30) | BITS(28, 24) | BIT(21) | BITS(11, 10),
		.targ = BITS(28, 25) | BIT(21) | BIT(11),
		.handler = a64_simd_handle_fpdp2s,
		.entries = a64_simd_fpdp2s,
		.d1m = BITS(15, 12), .d1s = 12	/* opcode */
	}, {	/* 3.6.27 Floating-point data-processing (3 source) */
		.mask = BIT(30) | BITS(28, 24),
		.targ = BITS(28, 24),
		.handler = a64_simd_handle_fpdp3s,
		.entries = a64_simd_fpdp3s,
		.d1m = BIT(21), .d1s = 21,	/* o1 */
		.d2m = BIT(15), .d2s = 15	/* o0 */
	}, {	/* 3.6.28 Floating-point immediate */
		.mask = BIT(30) | BITS(28, 24) | BIT(21) | BITS(12, 10),
		.targ = BITS(28, 25) | BIT(21) | BIT(12),
		.handler = a64_simd_handle_fpi,
		.entries = a64_simd_fpi
	}, {	/* 3.6.29 Floating-point<->fixed-point conversions */
		.mask = BIT(30) | BITS(28, 24) | BIT(21),
		.targ = BITS(28, 25),
		.handler = a64_simd_handle_fpfxpc,
		.entries = a64_simd_fpfxpc,
		.d1m = BITS(18, 16), .d1s = 16	/* opcode */
	}, {	/* 3.6.30 Floating-point<->integer conversions */
		.mask = BIT(30) | BITS(28, 24) | BIT(21) | BITS(15, 10),
		.targ = BITS(28, 25) | BIT(21),
		.handler = a64_simd_handle_fpic,
		.entries = a64_simd_fpic,
		.d1m = BITS(20, 19), .d1s = 19,	/* rmode */
		.d2m = BITS(18, 16), .d2s = 16	/* opcode */
	}
};

static int
a64_dis_simd(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen)
{
	const a64_simd_opcode_entry_t *op = NULL;
	const a64_simd_class_t *class = NULL;
	a64_simd_data_t data;
	size_t len;

	for (uint8_t i = 0; i < ARRAY_SIZE(a64_simd_classes); i++) {
		class = &a64_simd_classes[i];

		if ((in & class->mask) == class->targ) {
			op = class->entries;
			break;
		}
	}
	if (op == NULL)
		return (-1);

	for (; op->name != NULL; op++) {
		if (op->d1v != DC &&
		    ((in & class->d1m) >> class->d1s) != op->d1v) {
			continue;
		}
		if (op->d2v != DC &&
		    ((in & class->d2m) >> class->d2s) != op->d2v) {
			continue;
		}
		if (op->d3v != DC &&
		    ((in & class->d3m) >> class->d3s) != op->d3v) {
			continue;
		}
		if (op->d4v != DC &&
		    ((in & class->d4m) >> class->d4s) != op->d4v) {
			continue;
		}
		if (op->d5v != DC &&
		    ((in & class->d5m) >> class->d5s) != op->d5v) {
			continue;
		}
		break;
	}

	if (op->name == NULL || (op->flags & SIMD_F_SKIP))
		return (-1);

	/* Extract commonly-used sections */
	bzero(&data, sizeof (data));
	data.q = bitx32(in, 30, 30);
	data.size = bitx32(in, 23, 22);
	data.rm.id = bitx32(in, 20, 16);
	data.rn.id = bitx32(in, 9, 5);
	data.rd.id = bitx32(in, 4, 0);
	data.rm.width = data.rn.width = data.rd.width = A64_REGWIDTH_SIMD_V;

	/*
	 * Print operand.
	 */
	if ((len = dis_snprintf(buf, buflen, "%s", op->name)) >= buflen)
		return (-1);
	buflen -= len;
	buf += len;

	if (op->handler != NULL)
		return (op->handler)(dhp, class, op, &data, in, buf, buflen);

	if (class->handler == NULL) {
		len = dis_snprintf(buf, buflen, "<TODO>");
		return (len < buflen ? 0 : -1);
	}

	return (class->handler)(dhp, class, op, &data, in, buf, buflen);
}

static int
a64_dis(dis_handle_t *dhp, uint32_t in, char *buf, size_t buflen)
{
	/*
	 * This mask contains three groups we need to individually test for.
	 *
	 * C3.2 Branches, Exception generating, and System instructions
	 */
	if ((in & A64_ENCGRP_BRANCH_MASK) == A64_ENCGRP_BRANCH_TARG) {

		/* C3.2.3 Exception generation */
		if ((in & A64_ENCGRP_EXCPTN_MASK) == A64_ENCGRP_EXCPTN_TARG)
			return (a64_dis_exception(dhp, in, buf, buflen));

		/* C3.2.4 System */
		if ((in & A64_ENCGRP_SYSTEM_MASK) == A64_ENCGRP_SYSTEM_TARG)
			return (a64_dis_system(dhp, in, buf, buflen));

		/* Otherwise it's a branch instruction class */
		return (a64_dis_branch(dhp, in, buf, buflen));
	}

	/*
	 * C3.3 Loads and stores
	 */
	if ((in & A64_ENCGRP_LDSTR_MASK) == A64_ENCGRP_LDSTR_TARG) {

		/* Handle AdvSIMD load/store separately */
		if ((in & A64_ENCGRP_LSSIMD_MASK) == A64_ENCGRP_LSSIMD_TARG)
			return (a64_dis_ldstr_simd(dhp, in, buf, buflen));

		return (a64_dis_ldstr(dhp, in, buf, buflen));
	}

	/*
	 * C3.4 Data processing - immediate
	 * C3.5 Data processing - register
	 */
	if ((in & A64_ENCGRP_DPIMM_MASK) == A64_ENCGRP_DPIMM_TARG ||
	    (in & A64_ENCGRP_DPREG_MASK) == A64_ENCGRP_DPREG_TARG) {
		return (a64_dis_dataproc(dhp, in, buf, buflen));
	}

	/*
	 * C3.6 Data processing - SIMD and floating point
	 */
	if ((in & A64_ENCGRP_DPSIMD_MASK) == A64_ENCGRP_DPSIMD_TARG)
		return (a64_dis_simd(dhp, in, buf, buflen));

	/*
	 * UNALLOCATED
	 */
	return (-1);
}

static int
dis_a64_supports_flags(int flags)
{
	int archflags = flags & DIS_ARCH_MASK;

	return (archflags == DIS_ARM_64);
}

static int
dis_a64_handle_attach(dis_handle_t *dhp)
{
	dis_handle_arm_64_t *dhx;

	if ((dhx = dis_zalloc(sizeof (*dhx))) == NULL) {
		(void) dis_seterrno(E_DIS_NOMEM);
		return (-1);
	}

	dhp->dh_arch_private = dhx;
	return (0);
}

static void
dis_a64_handle_detach(dis_handle_t *dhp)
{
	dis_free(dhp->dh_arch_private, sizeof (dis_handle_arm_64_t));
	dhp->dh_arch_private = NULL;
}

static int
dis_a64_disassemble(dis_handle_t *dhp, uint64_t addr, char *buf, size_t buflen)
{
	uint32_t in;

	buf[0] = '\0';
	dhp->dh_addr = addr;
	if (dhp->dh_read(dhp->dh_data, addr, &in, sizeof (in)) != sizeof (in))
		return (-1);

	/* Translate in case we're on sparc? */
	in = LE_32(in);

	return (a64_dis(dhp, in, buf, buflen));
}

/*
 * This is simple in a non Thumb world. If and when we do enter a world where
 * we support thumb instructions, then this becomes far less than simple.
 */
static uint64_t
dis_a64_previnstr(dis_handle_t *dhp, uint64_t pc, int n)
{
	if (n <= 0)
		return (pc);

	return (pc - n * 4);
}

/*
 * If and when we support thumb, then this value should probably become two.
 * However, it varies based on whether or not a given instruction is in thumb
 * mode.
 */
static int
dis_a64_min_instrlen(dis_handle_t *dhp)
{
	return (4);
}

/*
 * Regardless of thumb, this value does not change.
 */
static int
dis_a64_max_instrlen(dis_handle_t *dhp)
{
	return (4);
}

static int
dis_a64_instrlen(dis_handle_t *dhp, uint64_t pc)
{
	return (4);
}

dis_arch_t dis_arch_arm_64 = {
	dis_a64_supports_flags,
	dis_a64_handle_attach,
	dis_a64_handle_detach,
	dis_a64_disassemble,
	dis_a64_previnstr,
	dis_a64_min_instrlen,
	dis_a64_max_instrlen,
	dis_a64_instrlen
};
