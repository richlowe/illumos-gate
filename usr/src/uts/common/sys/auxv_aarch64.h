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

/* Copyright 2023 Richard Lowe */

#ifndef _SYS_AUXV_AARCH64_H
#define	_SYS_AUXV_AARCH64_H

/*
 * Flags used in AT_SUNW_HWCAP to describe processor features
 *
 * Note that if a given bit is set; the implication is that the kernel
 * provides all the underlying architectural support for the correct
 * functioning of the extended instruction(s).
 *
 * The SysV ABI alpha talks about this in passing in terms of what Linux does,
 * and GNU IFUNCs, neither of which we support.  FreeBSD keeps in line with
 * Linux, for convenience, but for us this is inconvenient as Linux use the
 * top 32bits of HWCAP2 which we cannot. As such we have our own HWCAP space
 * and use our own naming scheme as we have historically.
 *
 * XXXARM: This is user-visible primarily via getauxval(3C) which we do not
 * yet have, and does not seem like it will be unduly difficult to port the
 * small amount of 3rd party software which uses it even if we were to add it.
 * I hope this doesn't come back to bite us.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These are in order of their first mention in:
 * Arm Architecture Reference Manual for A-profile architecture (DDI 0487I.a)
 *     Armv8-A Architecture Extensions
 */
#define	AV_AARCH64_FP		(1 << 0) /* FEAT_FP */
#define	AV_AARCH64_ADVSIMD	(1 << 1) /* FEAT_AdvSIMD */
#define	AV_AARCH64_SVE		(1 << 2) /* FEAT_SVE */
#define	AV_AARCH64_CRC32	(1 << 3) /* FEAT_CRC32 */
#define	AV_AARCH64_SB		(1 << 4) /* FEAT_SB */
#define	AV_AARCH64_SSBS		(1 << 5) /* FEAT_SSBS */
#define	AV_AARCH64_DGH		(1 << 6) /* FEAT_DGH */

/* A2.3 The Armv8 Cryptographic Extension */
#define	AV_AARCH64_AES		(1 << 7) /* FEAT_AES */
#define	AV_AARCH64_PMULL	(1 << 8) /* FEAT_PMULL */
#define	AV_AARCH64_SHA1		(1 << 9) /* FEAT_SHA1 */
#define	AV_AARCH64_SHA256	(1 << 10) /* FEAT_SHA256 */

/* A2.3.1 Armv8.2 extensions to the Cryptographic Extension */
#define	AV_AARCH64_SHA512	(1 << 11) /* FEAT_SHA512 */
#define	AV_AARCH64_SHA3		(1 << 12) /* FEAT_SHA3 */
#define	AV_AARCH64_SM3		(1 << 13) /* FEAT_SM3 */
#define	AV_AARCH64_SM4		(1 << 14) /* FEAT_SM4 */

/* A2.4.1 Architectural features added by Armv8.1 */
/*
 * XXXARM: LSE2 doesn't add to, but changes the atomicity of these.  I think
 * Linux only flags iff LSE2 (as _ATOMICS)
 */
#define	AV_AARCH64_LSE		(1 << 15) /* FEAT_LSE */
#define	AV_AARCH64_RDM		(1 << 16) /* FEAT_RDM */

/* A2.5.1 Architectural features added by Armv8.2 */

/*
 * Linux hwcap's have one tag each for FP and AdvSIMD, but this must be
 * implemented for whichever of those (or both) is implemented, so they're
 * actually orthogonal.
 */
#define	AV_AARCH64_FP16		(1 << 17) /* FEAT_FP16 */

#define	AV_AARCH64_DOTPROD	(1 << 18) /* FEAT_DotProd */
#define	AV_AARCH64_FHM		(1 << 19) /* FEAT_FHM */
#define	AV_AARCH64_DCPOP	(1 << 20) /* FEAT_DPB */
#define	AV_AARCH64_F32MM	(1 << 21) /* FEAT_F32MM */
#define	AV_AARCH64_F64MM	(1 << 22) /* FEAT_F64MM */
#define	AV_AARCH64_DCPODP	(1 << 23) /* FEAT_DPB2 */
#define	AV_AARCH64_BF16		(1 << 24) /* FEAT_BF16 */
#define	AV_AARCH64_I8MM		(1 << 25) /* FEAT_I8MM */

/* A2.6.1 Architectural features added by Armv8.3 */
#define	AV_AARCH64_FCMA		(1 << 26) /* FEAT_FCMA */
#define	AV_AARCH64_JSCVT	(1 << 27) /* FEAT_JSCVT */
#define	AV_AARCH64_LRCPC	(1 << 28) /* FEAT_LRCPC */

#define	AV_AARCH64_PACA		(1 << 29) /* FEAT_PAuth */
#define	AV_AARCH64_PACG		(1 << 30) /* FEAT_PACIMP */

/* A2.6.3 Features added to the Armv8.3 extension in later releases */

/* A2.7.1 Architectural features added by Armv8.4 */
#define	AV_AARCH64_DIT		(1 << 31) /* FEAT_DIT */
#define	AV_AARCH64_2_FLAGM	(1 << 0)  /* FEAT_FlagM */
#define	AV_AARCH64_2_ILRCPC	(1 << 1)  /* FEAT_LRCPC2 */
#define	AV_AARCH64_2_LSE2	(1 << 2)  /* FEAT_LSE2 */

/* A2.8.1 Architectural features added by Armv8.5 */
#define	AV_AARCH64_2_FLAGM2	(1 << 3) /* FEAT_FlagM2 */
#define	AV_AARCH64_2_FRINTTS	(1 << 4) /* FEAT_FRINTTS */
#define	AV_AARCH64_2_BTI	(1 << 5) /* FEAT_BTI */
#define	AV_AARCH64_2_RNG	(1 << 6) /* FEAT_RNG */

#define	AV_AARCH64_2_MTE	(1 << 7) /* FEAT_MTE */
#define	AV_AARCH64_2_MTE3	(1 << 8) /* FEAT_MTE3 */

/* A2.9.1 Architectural features added by Armv8.6 */
#define	AV_AARCH64_2_ECV	(1 << 9) /* FEAT_ECV */

/* A2.10.1 Architectural features added by Armv8.7 */
#define	AV_AARCH64_2_AFP	(1 << 10) /* FEAT_AFP */
#define	AV_AARCH64_2_RPRES	(1 << 11) /* FEAT_RPRES */

#define	AV_AARCH64_2_LD64B	(1 << 12) /* FEAT_LS64 */
#define	AV_AARCH64_2_ST64BV	(1 << 13) /* FEAT_LS64_V */
#define	AV_AARCH64_2_ST64BV0	(1 << 14) /* FEAT_LS64_ACCDATA */

#define	AV_AARCH64_2_WFXT	(1 << 15) /* FEAT_WFxT */

/* A2.11.1 Architectural features added by Armv8.8 */
#define	AV_AARCH64_2_MOPS	(1 << 16) /* FEAT_MOPS */
#define	AV_AARCH64_2_HBC	(1 << 17) /* FEAT_HBC */
/* XXXARM: Necessary? */
#define	AV_AARCH64_2_CMOW	(1 << 18) /* FEAT_CMOW */

/* A3.1 Armv9-A architecture extensions */
#define	AV_AARCH64_2_SVE2		(1 << 19) /* FEAT_SVE2 */
#define	AV_AARCH64_2_SVE2_AES		(1 << 20) /* FEAT_SVE_AES */
#define	AV_AARCH64_2_SVE2_BITPERM	(1 << 21) /* FEAT_SVE_BITPERM */
#define	AV_AARCH64_2_SVE2_PMULL128	(1 << 22) /* FEAT_SVE_PMULL128 */
#define	AV_AARCH64_2_SVE2_SHA3		(1 << 23) /* FEAT_SVE_SHA3 */
#define	AV_AARCH64_2_SVE2_SM4		(1 << 24) /* FEAT_SVE_SM4 */
#define	AV_AARCH64_2_TME		(1 << 25) /* FEAT_TME */
#define	AV_AARCH64_2_SME		(1 << 26) /* FEAT_SME */
#define	AV_AARCH64_2_SME_FA64		(1 << 27) /* FEAT_SME_FA64 */
#define	AV_AARCH64_2_EBF16		(1 << 28) /* FEAT_EBF16 */
#define	AV_AARCH64_2_SME_F64F64		(1 << 29) /* FEAT_SME_F64F64 */
#define	AV_AARCH64_2_SME_I16I64		(1 << 30) /* FEAT_SME_I16I64 */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_AUXV_AARCH64_H */
