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

#ifndef _SYS_CPUID_H
#define	_SYS_CPUID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/bitext.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/null.h>
#include <sys/types.h>


/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.100 MIDR_EL1, Main ID Register
 *    (ARM DDI 0487I.a)
 */
#define	MIDR_IMPL(midr)		bitx64(midr, 31, 24)

#define	MIDR_IMPL_SOFTWARE	0x00	/* Reserved for software use */
#define	MIDR_IMPL_ARM		0x41	/* Arm Limited */
#define	MIDR_IMPL_BROADCOM	0x42	/* Broadcom Corporation */
#define	MIDR_IMPL_CAVIUM	0x43	/* Cavium Inc. */
#define	MIDR_IMPL_DEC		0x44	/* Digital Equipment Corporation */
#define	MIDR_IMPL_FUJITSU	0x46	/* Fujitsu Ltd. */
#define	MIDR_IMPL_INFINEON	0x49	/* Infineon Technologies AG */
#define	MIDR_IMPL_MOTOROLA	0x4D	/* Motorola/Freescale Semiconductor. */
#define	MIDR_IMPL_NVIDIA	0x4E	/* NVIDIA Corporation */
#define	MIDR_IMPL_AMC		0x50	/* Applied Micro Circuits Corporation */
#define	MIDR_IMPL_QUALCOMM	0x51	/* Qualcomm Inc. */
#define	MIDR_IMPL_MARVELL	0x56	/* Marvell International Ltd. */
#define	MIDR_IMPL_APPLE		0x61	/* Apple Computer */
#define	MIDR_IMPL_INTEL		0x69	/* Intel Corporation */
#define	MIDR_IMPL_AMPERE	0xC0	/* Ampere Computing */

#define	MIDR_PART(midr)		bitx64(midr, 15, 4)

/* ARM */
#define	MIDR_PART_ARM_FOUNDATION	0xd00
#define	MIDR_PART_ARM_CORTEX_A34	0xd02
#define	MIDR_PART_ARM_CORTEX_A53	0xd03
#define	MIDR_PART_ARM_CORTEX_A35	0xd04
#define	MIDR_PART_ARM_CORTEX_A55	0xd05
#define	MIDR_PART_ARM_CORTEX_A65	0xd06
#define	MIDR_PART_ARM_CORTEX_A57	0xd07
#define	MIDR_PART_ARM_CORTEX_A72	0xd08
#define	MIDR_PART_ARM_CORTEX_A73	0xd09
#define	MIDR_PART_ARM_CORTEX_A75	0xd0a
#define	MIDR_PART_ARM_CORTEX_A76	0xd0b
#define	MIDR_PART_ARM_NEOVERSE_N1	0xd0c
#define	MIDR_PART_ARM_CORTEX_A77	0xd0d
#define	MIDR_PART_ARM_CORTEX_A76AE	0xd0e
#define	MIDR_PART_ARM_AEM_V8		0xd0f
#define	MIDR_PART_ARM_NEOVERSE_V1	0xd40
#define	MIDR_PART_ARM_CORTEX_A78	0xd41
#define	MIDR_PART_ARM_CORTEX_A65AE	0xd43
#define	MIDR_PART_ARM_CORTEX_X1		0xd44
#define	MIDR_PART_ARM_CORTEX_A510	0xd46
#define	MIDR_PART_ARM_CORTEX_A710	0xd47
#define	MIDR_PART_ARM_CORTEX_X2		0xd48
#define	MIDR_PART_ARM_NEOVERSE_N2	0xd49
#define	MIDR_PART_ARM_NEOVERSE_E1	0xd4a
#define	MIDR_PART_ARM_CORTEX_A78C	0xd4b
#define	MIDR_PART_ARM_CORTEX_X1C	0xd4c
#define	MIDR_PART_ARM_CORTEX_A715	0xd4d
#define	MIDR_PART_ARM_CORTEX_X3		0xd4e
#define	MIDR_PART_ARM_NEOVERSE_V2	0xd4f
#define	MIDR_PART_ARM_CORTEX_A520	0xd80
#define	MIDR_PART_ARM_CORTEX_A720	0xd81
#define	MIDR_PART_ARM_CORTEX_X4		0xd82
#define	MIDR_PART_ARM_NEOVERSE_V3	0xd84
#define	MIDR_PART_ARM_CORTEX_X925	0xd85
#define	MIDR_PART_ARM_CORTEX_A725	0xd87
#define	MIDR_PART_ARM_NEOVERSE_N3	0xd8e

/* Cavium */
#define	MIDR_PART_CAVIUM_THUNDERX	0x0a1
#define	MIDR_PART_CAVIUM_THUNDERX_81XX	0x0a2
#define	MIDR_PART_CAVIUM_THUNDERX_83XX	0x0a3
#define	MIDR_PART_CAVIUM_THUNDERX2	0x0af

#define	MIDR_PART_CAVIUM_OCTX2_98XX	0x0b1
#define	MIDR_PART_CAVIUM_OCTX2_96XX	0x0b2
#define	MIDR_PART_CAVIUM_OCTX2_95XX	0x0b3
#define	MIDR_PART_CAVIUM_OCTX2_95XXN	0x0b4
#define	MIDR_PART_CAVIUM_OCTX2_95XXMM	0x0b5
#define	MIDR_PART_CAVIUM_OCTX2_95XXO	0x0b6

/* AMC */
#define	MIDR_PART_AMC_POTENZA	0x000

/* Qualcomm */
#define	MIDR_PART_QUALCOMM_KRYO			0x200
#define	MIDR_PART_QUALCOMM_KRYO2xx_GOLD		0x800
#define	MIDR_PART_QUALCOMM_KRYO2xx_SILVER	0x801
#define	MIDR_PART_QUALCOMM_KRYO3xx_SILVER	0x803
#define	MIDR_PART_QUALCOMM_KRYO4xx_GOLD		0x804
#define	MIDR_PART_QUALCOMM_KRYO4xx_SILVER	0x805
#define	MIDR_PART_QUALCOMM_FALKOR		0xc00

/* Broadcom */
#define	MIDR_PART_BROADCOM_BRAHMA_B53	0x100
#define	MIDR_PART_BROADCOM_VULCAN	0x516

/* Nvidia */
#define	MIDR_PART_NVIDIA_DENVER	0x003
#define	MIDR_PART_NVIDIA_CARMEL	0x004

/* Fujitsu */
#define	MIDR_PART_FUJITSU_A64FX	0x001

/* Apple */
#define	MIDR_PART_APPLE_M1_ICESTORM		0x022
#define	MIDR_PART_APPLE_M1_FIRESTORM		0x023
#define	MIDR_PART_APPLE_M1_ICESTORM_PRO		0x024
#define	MIDR_PART_APPLE_M1_FIRESTORM_PRO	0x025
#define	MIDR_PART_APPLE_M1_ICESTORM_MAX		0x028
#define	MIDR_PART_APPLE_M1_FIRESTORM_MAX	0x029
#define	MIDR_PART_APPLE_M2_BLIZZARD		0x032
#define	MIDR_PART_APPLE_M2_AVALANCHE		0x033
#define	MIDR_PART_APPLE_M2_BLIZZARD_PRO		0x034
#define	MIDR_PART_APPLE_M2_AVALANCHE_PRO	0x035
#define	MIDR_PART_APPLE_M2_BLIZZARD_MAX		0x038
#define	MIDR_PART_APPLE_M2_AVALANCHE_MAX	0x039
#define	MIDR_PART_APPLE_M3_SAWTOOTH		0x048
#define	MIDR_PART_APPLE_M3_EVEREST		0x049

/* Ampere */
#define	MIDR_PART_AMPERE_AMPERE1	0xac3

/* Software */
#define	MIDR_PART_SOFTWARE_QEMUMAX	0x051 /* "Q" */

#define	MIDR_REVISION(midr)	bitx64(midr, 3, 0)


/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.59 ID_AA64DFR0_EL1, AArch64 Debug Feature Register 0
 *    (ARM DDI 0487I.a)
 */
#define	DFR0_HPMN0_LOW	60
#define	DFR0_HPMN0_HIGH	63
#define	DFR0_HPMN0(dfr)		bitx64(dfr, 63, 60)

#define	DFR0_EXTTRCBUFF_LOW	56
#define	DFR0_EXTTRCBUFF_HIGH	59
#define	DFR0_EXTTRCBUFF(dfr)	bitx64(dfr, 59, 56)

#define	DFR0_BRBE_LOW	52
#define	DFR0_BRBE_HIGH	55
#define	DFR0_BRBE(dfr)		bitx64(dfr, 55, 52)

#define	DFR0_FEAT_BRBE_BRBE	0x1
#define	DFR0_FEAT_BRBE_BRBEv1p1	0x2

#define	DFR0_MTPMU_LOW	48
#define	DFR0_MTPMU_HIGH	51
#define	DFR0_MTPMU(dfr)	bitx64(dfr, 51, 48)

/*
 * If MTPMU is missing, the specific behaviour of PMUv3 differs depending on
 * which value of missing it is.
 */
#define	DFR0_FEAT_MTPMU_MISSING		0x0
#define	DFR0_FEAT_MTPMU_MTPMU		0x1
#define	DFR0_FEAT_MTPMU_ALSO_MISSING	0xf

#define	DFR0_TRACEBUFFER_LOW	44
#define	DFR0_TRACEBUFFER_HIGH	47
#define	DFR0_TRACEBUFFER(dfr)	bitx64(dfr, 47, 44)

#define	DFR0_TRACEFILT_LOW	40
#define	DFR0_TRACEFILT_HIGH	43
#define	DFR0_TRACEFILT(dfr)	bitx64(dfr, 43, 40)

#define	DFR0_DOUBLELOCK_LOW	36
#define	DFR0_DOUBLELOCK_HIGH	39
#define	DFR0_DOUBLELOCK(dfr)	bitx64(dfr, 39, 36)

#define	DFR0_PMSVER_LOW		32
#define	DFR0_PMSVER_HIGH	35
#define	DFR0_PMSVER(dfr)	bitx64(dfr, 35, 32)

#define	DFR0_FEAT_PMSVER_SPE		0x1
#define	DFR0_FEAT_PMSVER_SPEv1p1	0x2
#define	DFR0_FEAT_PMSVER_SPEv1p2	0x3
#define	DFR0_FEAT_PMSVER_SPEv1p3	0x4
#define	DFR0_FEAT_PMSVER_SPEv1p4	0x5

#define	DFR0_CTX_CMPS_LOW	28
#define	DFR0_CTX_CMPS_HIGH	31
#define	DFR0_CTX_CMPS(dfr)	bitx64(dfr, 31, 28)

#define	DFR0_SEBEP_LOW	24
#define	DFR0_SEBEP_HIGH	27
#define	DFR0_SEBEP(dfr)	bitx64(dfr, 27, 24)

#define	DFR0_WRPS_LOW	20
#define	DFR0_WRPS_HIGH	23
#define	DFR0_WRPS(dfr)	bitx64(dfr, 23, 20)

#define	DFR0_PMSS_LOW	16
#define	DFR0_PMSS_HIGH	19
#define	DFR0_PMSS(dfr)	bitx64(dfr, 19, 16)

#define	DFR0_BRPS_LOW	12
#define	DFR0_BRPS_HIGH	15
#define	DFR0_BRPS(dfr)	bitx64(dfr, 15, 12)

#define	DFR0_PMUVER_LOW		8
#define	DFR0_PMUVER_HIGH	11
#define	DFR0_PMUVER(dfr)	bitx64(dfr, 11, 8)

#define	DFR0_FEAT_PMUVER_PMUv3		0x1
#define	DFR0_FEAT_PMUVER_PMUv3p1	0x4
#define	DFR0_FEAT_PMUVER_PMUv3p4	0x5
#define	DFR0_FEAT_PMUVER_PMUv3p5	0x6
#define	DFR0_FEAT_PMUVER_PMUv3p7	0x7
#define	DFR0_FEAT_PMUVER_PMUv3p8	0x8
#define	DFR0_FEAT_PMUVER_PMUv3p9	0x9
#define	DFR0_FEAT_PMUVER_IMPLDEF	0xff

#define	DFR0_TRACEVER_LOW	4
#define	DFR0_TRACEVER_HIGH	7
#define	DFR0_TRACEVER(dfr)	bitx64(dfr, 7, 4)

#define	DFR0_DEBUGVER_LOW	0
#define	DFR0_DEBUGVER_HIGH	3
#define	DFR0_DEBUGVER(dfr)	bitx64(dfr, 3, 0)

#define	DFR0_FEAT_DEBUGVER_DEBUGv8	0x6
#define	DFR0_FEAT_DEBUGVER_VHE		0x7
#define	DFR0_FEAT_DEBUGVER_DEBUGv8p2	0x8
#define	DFR0_FEAT_DEBUGVER_DEBUGv8p4	0x9
#define	DFR0_FEAT_DEBUGVER_DEBUGv8p8	0xa
#define	DFR0_FEAT_DEBUGVER_DEBUGv8p9	0xb

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.60 ID_AA64DFR1_EL1, AArch64 Debug Feature Register 1
 *    (ARM DDI 0487I.a)
 */
#define	DFR1_ABL_CMPS(dfr)	bitx64(dfr, 63, 56)

#define	DFR1_EBEP_LOW	48
#define	DFR1_EBEP_HIGH	51
#define	DFR1_EBEP(dfr)	bitx64(dfr, 51, 48)

#define	DFR1_ITE_LOW	44
#define	DFR1_ITE_HIGH	47
#define	DFR1_ITE(dfr)	bitx64(dfr, 47, 44)

#define	DFR1_ABLE_LOW	40
#define	DFR1_ABLE_HIGH	43
#define	DFR1_ABLE(dfr)	bitx64(dfr, 43, 40)

#define	DFR1_PMICNTR_LOW	36
#define	DFR1_PMICNTR_HIGH	39
#define	DFR1_PMICNTR(dfr)	bitx64(dfr, 39, 36)

#define	DFR1_SPMU_LOW	32
#define	DFR1_SPMU_HIGH	35
#define	DFR1_SPMU(dfr)	bitx64(dfr, 35, 32)

#define	DFR1_CTX_CMPS(dfr)	bitx64(dfr, 31, 24)
#define	DFR1_WRPS(dfr)		bitx64(dfr, 23, 16)
#define	DFR1_BRPS(dfr)		bitx64(dfr, 15, 8)
#define	DFR1_SYSPMUID(dfr)	bitx64(dfr, 7, 0)

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.61 ID_AA64ISAR0_EL1, AArch64 Instruction Set Attribute Register 0
 *    (ARM DDI 0487I.a)
 */
#define	ISAR0_RNDR_LOW		60
#define	ISAR0_RNDR_HIGH		63
#define	ISAR0_RNDR(isar)	bitx64(isar, 63, 60)

#define	ISAR0_TLB_LOW	56
#define	ISAR0_TLB_HIGH	59
#define	ISAR0_TLB(isar)	bitx64(isar, 59, 56)

#define	ISAR0_FEAT_TLB_TLBIOS		0x1
#define	ISAR0_FEAT_TLB_TLBIOS_AND_RANGE	0x2

#define	ISAR0_TS_LOW	52
#define	ISAR0_TS_HIGH	55
#define	ISAR0_TS(isar)	bitx64(isar, 55, 52)

#define	ISAR0_FEAT_TS_FLAGM	0x1
#define	ISAR0_FEAT_TS_FLAGM2	0x2

#define	ISAR0_FHM_LOW	48
#define	ISAR0_FHM_HIGH	51
#define	ISAR0_FHM(isar)	bitx64(isar, 51, 48)

#define	ISAR0_DP_LOW	44
#define	ISAR0_DP_HIGH	47
#define	ISAR0_DP(isar)	bitx64(isar, 47, 44)

#define	ISAR0_SM4_LOW	40
#define	ISAR0_SM4_HIGH	43
#define	ISAR0_SM4(isar) bitx64(isar, 43, 40)

#define	ISAR0_SM3_LOW	36
#define	ISAR0_SM3_HIGH	39
#define	ISAR0_SM3(isar) bitx64(isar, 39, 36)

#define	ISAR0_SHA3_LOW		32
#define	ISAR0_SHA3_HIGH		35
#define	ISAR0_SHA3(isar)	bitx64(isar, 35, 32)

#define	ISAR0_RDM_LOW	28
#define	ISAR0_RDM_HIGH	31
#define	ISAR0_RDM(isar)	bitx64(isar, 31, 28)

#define	ISAR0_TME_LOW	24
#define	ISAR0_TME_HIGH	27
#define	ISAR0_TME(isar)	bitx64(isar, 27, 24)

#define	ISAR0_ATOMIC_LOW	20
#define	ISAR0_ATOMIC_HIGH	23
#define	ISAR0_ATOMIC(isar)	bitx64(isar, 23, 20)

#define	ISAR0_FEAT_ATOMIC_LSE		0x1
#define	ISAR0_FEAT_ATOMIC_LSE128	0x2

#define	ISAR0_CRC32_LOW		16
#define	ISAR0_CRC32_HIGH	19
#define	ISAR0_CRC32(isar)	bitx64(isar, 19, 16)

#define	ISAR0_SHA2_LOW		12
#define	ISAR0_SHA2_HIGH		15
#define	ISAR0_SHA2(isar)	bitx64(isar, 15, 12)

#define	ISAR0_FEAT_SHA2_SHA256	0x1
#define	ISAR0_FEAT_SHA2_SHA512	0x2

#define	ISAR0_SHA1_LOW		8
#define	ISAR0_SHA1_HIGH		11
#define	ISAR0_SHA1(isar)	bitx64(isar, 11, 8)

#define	ISAR0_AES_LOW		4
#define	ISAR0_AES_HIGH		7
#define	ISAR0_AES(isar)		bitx64(isar, 7, 4)

#define	ISAR0_FEAT_AES_AES	0x1
#define	ISAR0_FEAT_AES_PMULL	0x2

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.62 ID_AA64ISAR1_EL1, AArch64 Instruction Set Attribute Register 1
 *    (ARM DDI 0487I.a)
 */
#define	ISAR1_LS64_LOW		60
#define	ISAR1_LS64_HIGH		63
#define	ISAR1_LS64(isar)	bitx64(isar, 63, 60)

#define	ISAR1_FEAT_LS64		0x1
#define	ISAR1_FEAT_LS64_V	0x2
#define	ISAR1_FEAT_LS64_ACCDATA	0x3

#define	ISAR1_XS_LOW	56
#define	ISAR1_XS_HIGH	59
#define	ISAR1_XS(isar)	bitx64(isar, 59, 56)

#define	ISAR1_I8MM_LOW		52
#define	ISAR1_I8MM_HIGH		55
#define	ISAR1_I8MM(isar)	bitx64(isar, 59, 56)

#define	ISAR1_DGH_LOW	48
#define	ISAR1_DGH_HIGH	51
#define	ISAR1_DGH(isar)	bitx64(isar, 51, 48)

#define	ISAR1_BF16_LOW		44
#define	ISAR1_BF16_HIGH		47
#define	ISAR1_BF16(isar)	bitx64(isar, 47, 44)

#define	ISAR1_FEAT_BF16_BF16	0x1
#define	ISAR1_FEAT_BF16_EBF16	0x2

#define	ISAR1_SPECRES_LOW	40
#define	ISAR1_SPECRES_HIGH	43
#define	ISAR1_SPECRES(isar)	bitx64(isar, 43, 40)

#define	ISAR1_FEAT_SPECRES_SPECRES	0x1
#define	ISAR1_FEAT_SPECRES_SPECRES2	0x2

#define	ISAR1_SB_LOW	36
#define	ISAR1_SB_HIGH	39
#define	ISAR1_SB(isar)	bitx64(isar, 39, 36)

#define	ISAR1_FRINTTS_LOW	32
#define	ISAR1_FRINTTS_HIGH	35
#define	ISAR1_FRINTTS(isar)	bitx64(isar, 35, 32)

#define	ISAR1_GPI_LOW	28
#define	ISAR1_GPI_HIGH	31
#define	ISAR1_GPI(isar)	bitx64(isar, 31, 28)

#define	ISAR1_GPA_LOW	24
#define	ISAR1_GPA_HIGH	27
#define	ISAR1_GPA(isar)	bitx64(isar, 27, 24)

#define	ISAR1_LRCPC_LOW		20
#define	ISAR1_LRCPC_HIGH	23
#define	ISAR1_LRCPC(isar)	bitx64(isar, 23, 20)

#define	ISAR1_FEAT_LRCPC_LRCPC	0x1
#define	ISAR1_FEAT_LRCPC_LRCPC2	0x2
#define	ISAR1_FEAT_LRCPC_LRCPC3	0x3

#define	ISAR1_FCMA_LOW		16
#define	ISAR1_FCMA_HIGH		19
#define	ISAR1_FCMA(isar)	bitx64(isar, 19, 16)

#define	ISAR1_JSCVT_LOW		12
#define	ISAR1_JSCVT_HIGH	15
#define	ISAR1_JSCVT(isar)	bitx64(isar, 15, 12)

#define	ISAR1_API_LOW	8
#define	ISAR1_API_HIGH	11
#define	ISAR1_API(isar)	bitx64(isar, 11, 8)

#define	ISAR1_FEAT_API_PAuth		0x1
#define	ISAR1_FEAT_API_EPAC		0x2
#define	ISAR1_FEAT_API_PAuth2		0x3
#define	ISAR1_FEAT_API_FPAC		0x4
#define	ISAR1_FEAT_API_FPACCOMBINE	0x5

#define	ISAR1_APA_LOW	4
#define	ISAR1_APA_HIGH	7
#define	ISAR1_APA(isar)	bitx64(isar, 7, 4)

#define	ISAR1_FEAT_APA_PAuth		0x1
#define	ISAR1_FEAT_APA_EPAC		0x2
#define	ISAR1_FEAT_APA_PAuth2		0x3
#define	ISAR1_FEAT_APA_FPAC		0x4
#define	ISAR1_FEAT_APA_FPACCOMBINE	0x5

#define	ISAR1_DPB_LOW	0
#define	ISAR1_DPB_HIGH	3
#define	ISAR1_DPB(isar)	bitx64(isar, 3, 0)

#define	ISAR1_FEAT_DPB_DPB	0x1
#define	ISAR1_FEAT_DPB_DPB2	0x2

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.63 ID_AA64ISAR2_EL1, AArch64 Instruction Set Attribute Register 2
 *    (ARM DDI 0487I.a)
 */
#define	ISAR2_CSSC_LOW		52
#define	ISAR2_CSSC_HIGH		55
#define	ISAR2_CSSC(isar)	bitx64(isar, 55, 52)

#define	ISAR2_RPRFM_LOW		48
#define	ISAR2_RPRFM_HIGH	51
#define	ISAR2_RPRFM(isar)	bitx64(isar, 51, 48)

#define	ISAR2_PRFMSLC_LOW	40
#define	ISAR2_PRFMSLC_HIGH	43
#define	ISAR2_PRFMSLC(isar)	bitx64(isar, 43, 40)

#define	ISAR2_SYSINSTR_128_LOW		36
#define	ISAR2_SYSINSTR_128_HIGH		39
#define	ISAR2_SYSINSTR_128(isar)	bitx64(isar, 39, 36)

#define	ISAR2_SYSREG_128_LOW	32
#define	ISAR2_SYSREG_128_HIGH	35
#define	ISAR2_SYSREG_128(isar)	bitx64(isar, 35, 32)

#define	ISAR2_CLRBHB_LOW	28
#define	ISAR2_CLRBHB_HIGH	31
#define	ISAR2_CLRBHB(isar)	bitx64(isar, 31, 28)

#define	ISAR2_PAC_FRAC_LOW	24
#define	ISAR2_PAC_FRAC_HIGH	27
#define	ISAR2_PAC_FRAC(isar)	bitx64(isar, 27, 24)

#define	ISAR2_BC_LOW	20
#define	ISAR2_BC_HIGH	23
#define	ISAR2_BC(isar)	bitx64(isar, 23, 20)

#define	ISAR2_MOPS_LOW		16
#define	ISAR2_MOPS_HIGH		19
#define	ISAR2_MOPS(isar)	bitx64(isar, 19, 16)

#define	ISAR2_APA3_LOW		12
#define	ISAR2_APA3_HIGH		15
#define	ISAR2_APA3(isar)	bitx64(isar, 15, 12)

#define	ISAR2_FEAT_APA3_PAUTH		0x1
#define	ISAR2_FEAT_APA3_EPAC		0x2
#define	ISAR2_FEAT_APA3_PAuth2		0x3
#define	ISAR2_FEAT_APA3_FPAC		0x4
#define	ISAR2_FEAT_APA3_FPACCOMBINE	0x5

#define	ISAR2_GPA3_LOW		8
#define	ISAR2_GPA3_HIGH		11
#define	ISAR2_GPA3(isar)	bitx64(isar, 11, 8)

#define	ISAR2_RPRES_LOW		4
#define	ISAR2_RPRES_HIGH	7
#define	ISAR2_RPRES(isar)	bitx64(isar, 7, 4)

#define	ISAR2_WFXT_LOW		0
#define	ISAR2_WFXT_HIGH		3
#define	ISAR2_WFXT(isar)	bitx64(isar, 3, 0)

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.64 ID_AA64MMFR0_EL1, AArch64 Memory Model Feature Register 0
 *    (ARM DDI 0487I.a)
 */
#define	MMFR0_ECV_LOW	60
#define	MMFR0_ECV_HIGH	63
#define	MMFR0_ECV(mmfr)	bitx64(mmfr, 63, 60)

#define	MMFR0_FEAT_ECV_ECV	0x1

#define	MMFR0_FGT_LOW	56
#define	MMFR0_FGT_HIGH	59
#define	MMFR0_FGT(mmfr)	bitx64(mmfr, 59, 56)

#define	MMFR0_FEAT_FGT_FGT	0x1
#define	MMFR0_FEAT_FGT_FGT2	0x2

#define	MMFR0_EXS_LOW	44
#define	MMFR0_EXS_HIGH	47
#define	MMFR0_EXS(mmfr)	bitx64(mmfr, 47, 44)

#define	MMFR0_TGRAN4_2_LOW	40
#define	MMFR0_TGRAN4_2_HIGH	43
#define	MMFR0_TGRAN4_2(mmfr)	bitx64(mmfr, 43, 40)

#define	MMFR0_FEAT_TGRAN4_2_LPA2	0x3

#define	MMFR0_TGRAN64_2_LOW	36
#define	MMFR0_TGRAN64_2_HIGH	39
#define	MMFR0_TGRAN64_2(mmfr)	bitx64(mmfr, 39, 36)

#define	MMFR0_TGRAN16_2_LOW	32
#define	MMFR0_TGRAN16_2_HIGH	35
#define	MMFR0_TGRAN16_2(mmfr)	bitx64(mmfr, 35, 32)

#define	MMFR0_FEAT_TGRAN16_2_LPA2	0x3

#define	MMFR0_TGRAN4_LOW	28
#define	MMFR0_TGRAN4_HIGH	31
#define	MMFR0_TGRAN4(mmfr)	bitx64(mmfr, 31, 28)

#define	MMFR0_FEAT_TGRAN4_LPA2	0x1

#define	MMFR0_TGRAN64_LOW	24
#define	MMFR0_TGRAN64_HIGH	27
#define	MMFR0_TGRAN64(mmfr)	bitx64(mmfr, 27, 24)

#define	MMFR0_TGRAN16_LOW	20
#define	MMFR0_TGRAN16_HIGH	23
#define	MMFR0_TGRAN16(mmfr)	bitx64(mmfr, 23, 20)

#define	MMFR0_FEAT_TGRAN16_LPA2	0x2

#define	MMFR0_BIGENDEL0_LOW	16
#define	MMFR0_BIGENDEL0_HIGH	19
#define	MMFR0_BIGENDEL0(mmfr)	bitx64(mmfr, 19, 16)

#define	MMFR0_SNSMEM_LOW	12
#define	MMFR0_SNSMEM_HIGH	15
#define	MMFR0_SNSMEM(mmfr)	bitx64(mmfr, 15, 12)

#define	MMFR0_BIGEND_LOW	8
#define	MMFR0_BIGEND_HIGH	11
#define	MMFR0_BIGEND(mmfr)	bitx64(mmfr, 11, 8)

#define	MMFR0_ASIDBITS_LOW	4
#define	MMFR0_ASIDBITS_HIGH	7
#define	MMFR0_ASIDBITS(mmfr)	bitx64(mmfr, 7, 4)

#define	MMFR0_PARANGE_LOW	0
#define	MMFR0_PARANGE_HIGH	3
#define	MMFR0_PARANGE(mmfr)	(enum mmfr0_parange)bitx64(mmfr, 3, 0)

enum mmfr0_parange {
	MMFR0_PARANGE_4G	= 0x0,
	MMFR0_PARANGE_64G	= 0x1,
	MMFR0_PARANGE_1T	= 0x2,
	MMFR0_PARANGE_4T	= 0x3,
	MMFR0_PARANGE_16T	= 0x4,
	MMFR0_PARANGE_256T	= 0x5,
	MMFR0_PARANGE_4P	= 0x6,
	MMFR0_PARANGE_64P	= 0x7,
};

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.65 ID_AA64MMFR1_EL1, AArch64 Memory Model Feature Register 1
 *    (ARM DDI 0487I.a)
 */
#define	MMFR1_ECBHB_LOW		60
#define	MMFR1_ECBHB_HIGH	63
#define	MMFR1_ECBHB(mmfr)	bitx64(mmfr, 63, 60)

#define	MMFR1_CMOW_LOW		56
#define	MMFR1_CMOW_HIGH		59
#define	MMFR1_CMOW(mmfr)	bitx64(mmfr, 59, 56)

#define	MMFR1_TIDCP1_LOW	52
#define	MMFR1_TIDCP1_HIGH	55
#define	MMFR1_TIDCP1(mmfr)	bitx64(mmfr, 55, 52)

#define	MMFR1_NTLBPA_LOW	48
#define	MMFR1_NTLBPA_HIGH	51
#define	MMFR1_NTLBPA(mmfr)	bitx64(mmfr, 51, 48)

#define	MMFR1_AFP_LOW	44
#define	MMFR1_AFP_HIGH	47
#define	MMFR1_AFP(mmfr)	bitx64(mmfr, 47, 44)

#define	MMFR1_HCX_LOW	40
#define	MMFR1_HCX_HIGH	43
#define	MMFR1_HCX(mmfr)	bitx64(mmfr, 43, 40)

#define	MMFR1_ETS_LOW	36
#define	MMFR1_ETS_HIGH	39
#define	MMFR1_ETS(mmfr)	bitx64(mmfr, 39, 36)

#define	MMFR1_TWED_LOW		32
#define	MMFR1_TWED_HIGH		35
#define	MMFR1_TWED(mmfr)	bitx64(mmfr, 35, 32)

#define	MMFR1_XNX_LOW	28
#define	MMFR1_XNX_HIGH	31
#define	MMFR1_XNX(mmfr)	bitx64(mmfr, 31, 28)

#define	MMFR1_SPECSEI_LOW	24
#define	MMFR1_SPECSEI_HIGH	27
#define	MMFR1_SPECSEI(mmfr)	bitx64(mmfr, 27, 24)

#define	MMFR1_PAN_LOW	20
#define	MMFR1_PAN_HIGH	23
#define	MMFR1_PAN(mmfr)	bitx64(mmfr, 23, 20)

#define	MMFR1_FEAT_PAN_PAN	0x1
#define	MMFR1_FEAT_PAN_PAN2	0x2
#define	MMFR1_FEAT_PAN_PAN3	0x3

#define	MMFR1_LO_LOW	16
#define	MMFR1_LO_HIGH	19
#define	MMFR1_LO(mmfr)	bitx64(mmfr, 19, 16)

#define	MMFR1_HPDS_LOW		12
#define	MMFR1_HPDS_HIGH		15
#define	MMFR1_HPDS(mmfr)	bitx64(mmfr, 15, 12)

#define	MMFR1_FEAT_HPDS_HPDS	0x1
#define	MMFR1_FEAT_HPDS_HPDS2	0x2

#define	MMFR1_VH_LOW	8
#define	MMFR1_VH_HIGH	11
#define	MMFR1_VH(mmfr)	bitx64(mmfr, 11, 8)

#define	MMFR1_VMIDBITS_LOW	4
#define	MMFR1_VMIDBITS_HIGH	7
#define	MMFR1_VMIDBITS(mmfr)	bitx64(mmfr, 7, 4)

#define	MMFR1_FEAT_VMIDBITS_8	0x0
#define	MMFR1_FEAT_VMIDBITS_16	0x1

#define	MMFR1_HAFDBS_LOW	0
#define	MMFR1_HAFDBS_HIGH	3
#define	MMFR1_HAFDBS(mmfr)	bitx64(mmfr, 3, 0)

#define	MMFR1_FEAT_HAFDBS_HAFDBS	0x1
#define	MMFR1_FEAT_HAFDBS_HAFDBS_DIRTY	0x2
#define	MMFR1_FEAT_HAFDBS_HAFT		0x3

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.66 ID_AA64MMFR2_EL1, AArch64 Memory Model Feature Register 2
 *    (ARM DDI 0487I.a)
 */
#define	MMFR2_E0PD_LOW		60
#define	MMFR2_E0PD_HIGH		63
#define	MMFR2_E0PD(mmfr)	bitx64(mmfr, 63, 60)

#define	MMFR2_EVT_LOW	56
#define	MMFR2_EVT_HIGH	59
#define	MMFR2_EVT(mmfr)	bitx64(mmfr, 59, 56)

#define	MMFR2_BBM_LOW	52
#define	MMFR2_BBM_HIGH	55
#define	MMFR2_BBM(mmfr)	bitx64(mmfr, 55, 52)

#define	MMFR2_TTL_LOW	48
#define	MMFR2_TTL_HIGH	51
#define	MMFR2_TTL(mmfr)	bitx64(mmfr, 51, 48)

#define	MMFR2_FWB_LOW	40
#define	MMFR2_FWB_HIGH	43
#define	MMFR2_FWB(mmfr)	bitx64(mmfr, 43, 40)

#define	MMFR2_IDS_LOW	36
#define	MMFR2_IDS_HIGH	39
#define	MMFR2_IDS(mmfr)	bitx64(mmfr, 39, 36)

#define	MMFR2_AT_LOW	32
#define	MMFR2_AT_HIGH	35
#define	MMFR2_AT(mmfr)	bitx64(mmfr, 35, 32)

#define	MMFR2_ST_LOW	28
#define	MMFR2_ST_HIGH	31
#define	MMFR2_ST(mmfr)	bitx64(mmfr, 31, 28)

#define	MMFR2_NV_LOW	24
#define	MMFR2_NV_HIGH	27
#define	MMFR2_NV(mmfr)	bitx64(mmfr, 27, 24)

#define	MMFR2_FEAT_NV_NV	0x1
#define	MMFR2_FEAT_NV_NV2	0x2

#define	MMFR2_CCIDX_LOW		20
#define	MMFR2_CCIDX_HIGH	23
#define	MMFR2_CCIDX(mmfr)	bitx64(mmfr, 23, 20)

#define	MMFR2_VARANGE_LOW	16
#define	MMFR2_VARANGE_HIGH	19
#define	MMFR2_VARANGE(mmfr)	bitx64(mmfr, 19, 16)

#define	MMFR2_IESB_LOW		12
#define	MMFR2_IESB_HIGH		15
#define	MMFR2_IESB(mmfr)	bitx64(mmfr, 15, 12)

#define	MMFR2_LSM_LOW	8
#define	MMFR2_LSM_HIGH	11
#define	MMFR2_LSM(mmfr)	bitx64(mmfr, 11, 8)

#define	MMFR2_UAO_LOW	4
#define	MMFR2_UAO_HIGH	7
#define	MMFR2_UAO(mmfr)	bitx64(mmfr, 7, 4)

#define	MMFR2_CNP_LOW	0
#define	MMFR2_CNP_HIGH	3
#define	MMFR2_CNP(mmfr)	bitx64(mmfr, 3, 0)

/*
 * Information currently from:
 *  Arm® Architecture Registers
 *     for A-profile architecture
 *     ID_AA64MMFR3_EL1, AArch64 Memory Model Feature Register 3
 *  pp. 1327 (not yet in the architecture reference)
 */
#define	MMFR3_SPEC_FPACC_LOW	60
#define	MMFR3_SPEC_FPACC_HIGH	63
#define	MMFR3_SPEC_FPACC(mmfr)	bitx64(mmfr, 63, 60)

#define	MMFR3_ADERR_LOW		56
#define	MMFR3_ADERR_HIGH	59
#define	MMFR3_ADERR(mmfr)	bitx64(mmfr, 59, 56)

#define	MMFR3_FEAT_ADERR_ADERR 0x2

#define	MMFR3_SDERR_LOW		52
#define	MMFR3_SDERR_HIGH	55
#define	MMFR3_SDERR(mmfr)	bitx64(mmfr, 55, 52)

#define	MMFR3_FEAT_SDERR_ADERR 0x2

#define	MMFR3_ANERR_LOW		44
#define	MMFR3_ANERR_HIGH	47
#define	MMFR3_ANERR(mmfr)	bitx64(mmfr, 47, 44)

#define	MMFR3_FEAT_ANERR_ANERR	0x2

#define	MMFR3_SNERR_LOW		40
#define	MMFR3_SNERR_HIGH	43
#define	MMFR3_SNERR(mmfr)	bitx64(mmfr, 43, 40)

#define	MMFR3_FEAT_SNERR_ANERR	0x2

#define	MMFR3_D128_2_LOW	36
#define	MMFR3_D128_2_HIGH	39
#define	MMFR3_D128_2(mmfr)	bitx64(mmfr, 39, 36)

#define	MMFR3_D128_LOW		32
#define	MMFR3_D128_HIGH		35
#define	MMFR3_D128(mmfr)	bitx64(mmfr, 35, 32)

#define	MMFR3_MEC_LOW	28
#define	MMFR3_MEC_HIGH	31
#define	MMFR3_MEC(mmfr)	bitx64(mmfr, 31, 28)

#define	MMFR3_AIE_LOW	24
#define	MMFR3_AIE_HIGH	27
#define	MMFR3_AIE(mmfr)	bitx64(mmfr, 27, 24)

#define	MMFR3_S2POE_LOW		20
#define	MMFR3_S2POE_HIGH	23
#define	MMFR3_S2POE(mmfr)	bitx64(mmfr, 23, 20)

#define	MMFR3_S1POE_LOW		16
#define	MMFR3_S1POE_HIGH	19
#define	MMFR3_S1POE(mmfr)	bitx64(mmfr, 19, 16)

#define	MMFR3_S2PIE_LOW		12
#define	MMFR3_S2PIE_HIGH	15
#define	MMFR3_S2PIE(mmfr)	bitx64(mmfr, 15, 12)

#define	MMFR3_S1PIE_LOW		8
#define	MMFR3_S1PIE_HIGH	11
#define	MMFR3_S1PIE(mmfr)	bitx64(mmfr, 11, 8)

#define	MMFR3_SCTLRX_LOW	4
#define	MMFR3_SCTLRX_HIGH	7
#define	MMFR3_SCTLRX(mmfr)	bitx64(mmfr, 7, 4)

#define	MMFR3_TCRX_LOW		0
#define	MMFR3_TCRX_HIGH		3
#define	MMFR3_TCRX(mmfr)	bitx64(mmfr, 3, 0)

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.67 ID_AA64PFR0_EL1, AArch64 Processor Feature Register 0
 *    (ARM DDI 0487I.a)
 */
#define	PFR0_CSV3_LOW	60
#define	PFR0_CSV3_HIGH	63
#define	PFR0_CSV3(pfr)	bitx64(pfr, 63, 60)

#define	PFR0_CSV2_LOW	56
#define	PFR0_CSV2_HIGH	59
#define	PFR0_CSV2(pfr)	bitx64(pfr, 59, 56)

#define	PFR0_FEAT_CSV2		0x01
#define	PFR0_FEAT_CSV2_2	0x02
#define	PFR0_FEAT_CSV2_3	0x03

#define	PFR0_RME_LOW	52
#define	PFR0_RME_HIGH	55
#define	PFR0_RME(pfr)	bitx64(pfr, 55, 52)

#define	PFR0_DIT_LOW	48
#define	PFR0_DIT_HIGH	51
#define	PFR0_DIT(pfr)	bitx64(pfr, 51, 48)

#define	PFR0_AMU_LOW	44
#define	PFR0_AMU_HIGH	47
#define	PFR0_AMU(pfr)	bitx64(pfr, 47, 44)

#define	PFR0_FEAT_AMUv1		0x1
#define	PFR0_FEAT_AMUv1p1	0x2

#define	PFR0_MPAM_LOW	40
#define	PFR0_MPAM_HIGH	43
#define	PFR0_MPAM(pfr)	bitx64(pfr, 43, 40)

#define	PFR0_FEAT_MPAM_0dotX	0x0
#define	PFR0_FEAT_MPAM_1dotX	0x1

#define	PFR0_SEL2_LOW	36
#define	PFR0_SEL2_HIGH	39
#define	PFR0_SEL2(pfr)	bitx64(pfr, 39, 36)

#define	PFR0_SVE_LOW	32
#define	PFR0_SVE_HIGH	35
#define	PFR0_SVE(pfr)	bitx64(pfr, 35, 32)

#define	PFR0_RAS_LOW	28
#define	PFR0_RAS_HIGH	31
#define	PFR0_RAS(pfr)	bitx64(pfr, 31, 28)

#define	PFR0_FEAT_RAS_RASv1		0x1
#define	PFR0_FEAT_RAS_RASv1p1		0x2
#define	PFR0_FEAT_RAS_RASv2		0x3

#define	PFR0_GIC_LOW	24
#define	PFR0_GIC_HIGH	27
#define	PFR0_GIC(pfr)	bitx64(pfr, 27, 24)

#define	PFR0_FEAT_GIC		0x1
#define	PFR0_FEAT_GICv4p1	0x3

#define	PFR0_ADVSIMD_LOW	20
#define	PFR0_ADVSIMD_HIGH	23
#define	PFR0_ADVSIMD(pfr)	bitx64(pfr, 23, 20)

#define	PFR0_FEAT_ADVSIMD_PRESENT	0x0
#define	PFR0_FEAT_ADVSIMD_FP16		0x1
#define	PFR0_FEAT_ADVSIMD_MISSING	0xf

#define	PFR0_FP_LOW	16
#define	PFR0_FP_HIGH	19
#define	PFR0_FP(pfr)	bitx64(pfr, 19, 16)

#define	PFR0_FEAT_FP_PRESENT	0x0
#define	PFR0_FEAT_FP_FP16	0x1
#define	PFR0_FEAT_FP_MISSING	0xf

#define	PFR0_EL3_LOW	12
#define	PFR0_EL3_HIGH	15
#define	PFR0_EL3(pfr)	bitx64(pfr, 15, 12)

#define	PFR0_FEAT_EL3		0x1
#define	PFR0_FEAT_EL3_AARCH32	0x2

#define	PFR0_EL2_LOW	8
#define	PFR0_EL2_HIGH	11
#define	PFR0_EL2(pfr)	bitx64(pfr, 11, 8)

#define	PFR0_FEAT_EL2		0x1
#define	PFR0_FEAT_EL2_AARCH32	0x2

#define	PFR0_EL1_LOW	4
#define	PFR0_EL1_HIGH	7
#define	PFR0_EL1(pfr)	bitx64(pfr, 7, 4)

#define	PFR0_FEAT_EL1		0x1
#define	PFR0_FEAT_EL1_AARCH32	0x2

#define	PFR0_EL0_LOW	0
#define	PFR0_EL0_HIGH	3
#define	PFR0_EL0(pfr)	bitx64(pfr, 3, 0)

#define	PFR0_FEAT_EL0		0x1
#define	PFR0_FEAT_EL0_AARCH32	0x2

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.68 ID_AA64PFR1_EL1, AArch64 Processor Feature Register 1
 *    (ARM DDI 0487I.a)
 */
#define	PFR1_PFAR_LOW	60
#define	PFR1_PFAR_HIGH	63
#define	PFR1_PFAR(pfr)	bitx64(pfr, 63, 60)

#define	PFR1_DF2_LOW	56
#define	PFR1_DF2_HIGH	59
#define	PFR1_DF2(pfr)	bitx64(pfr, 59, 56)

#define	PFR1_MTEX_LOW	52
#define	PFR1_MTEX_HIGH	55
#define	PFR1_MTEX(pfr)	bitx64(pfr, 55, 52)

#define	PFR1_THE_LOW	48
#define	PFR1_THE_HIGH	51
#define	PFR1_THE(pfr)	bitx64(pfr, 51, 48)

#define	PFR1_GCS_LOW	44
#define	PFR1_GCS_HIGH	47
#define	PFR1_GCS(pfr)	bitx64(pfr, 47, 44)

#define	PFR1_MTE_FRAC_LOW	40
#define	PFR1_MTE_FRAC_HIGH	43
#define	PFR1_MTE_FRAC(pfr)	bitx64(pfr, 43, 40)

#define	PFR1_NMI_LOW	36
#define	PFR1_NMI_HIGH	39
#define	PFR1_NMI(pfr)	bitx64(pfr, 39, 36)

#define	PFR1_CSV2_FRAC_LOW	32
#define	PFR1_CSV2_FRAC_HIGH	35
#define	PFR1_CSV2_FRAC(pfr)	bitx64(pfr, 35, 32)

#define	PFR1_FEAT_CSV2_1p1	0x1
#define	PFR1_FEAT_CSV2_1p2	0x2

#define	PFR1_RNDR_TRAP_LOW	28
#define	PFR1_RNDR_TRAP_HIGH	31
#define	PFR1_RNDR_TRAP(pfr)	bitx64(pfr, 31, 28)

#define	PFR1_SME_LOW	24
#define	PFR1_SME_HIGH	27
#define	PFR1_SME(pfr)	bitx64(pfr, 27, 24)

#define	PFR1_FEAT_SME_SME	0x1
#define	PFR1_FEAT_SME_SME2	0x2

#define	PFR1_MPAM_FRAC_LOW	16
#define	PFR1_MPAM_FRAC_HIGH	19
#define	PFR1_MPAM_FRAC(pfr)	bitx64(pfr, 19, 16)

#define	PFR1_FEAT_MPAM_MINOR_0	0x0
#define	PFR1_FEAT_MPAM_MINOR_1	0x1

#define	PFR1_RAS_FRAC_LOW	12
#define	PFR1_RAS_FRAC_HIGH	15
#define	PFR1_RAS_FRAC(pfr)	bitx64(pfr, 15, 12)

#define	PFR1_FEAT_RAS_MINOR_0	0x0
#define	PFR1_FEAT_RAS_MINOR_1	0x1

#define	PFR1_MTE_LOW	8
#define	PFR1_MTE_HIGH	11
#define	PFR1_MTE(pfr)	bitx64(pfr, 11, 8)

#define	PFR1_FEAT_MTE_MTE	0x1
#define	PFR1_FEAT_MTE_MTE2	0x2
#define	PFR1_FEAT_MTE_MTE3	0x3

#define	PFR1_SSBS_LOW	4
#define	PFR1_SSBS_HIGH	7
#define	PFR1_SSBS(pfr)	bitx64(pfr, 7, 4)

#define	PFR1_FEAT_SSBS_SSBS	0x1
#define	PFR1_FEAT_SSBS_SSBS2	0x2

#define	PFR1_BT_LOW	0
#define	PFR1_BT_HIGH	3
#define	PFR1_BT(pfr)	bitx64(pfr, 3, 0)

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *  D17.2.69 ID_AA64SMFR0_EL1, SME Feature ID register 0
 *  pp. D17-6013
 */
#define	SMFR0_FA64_LOW		63
#define	SMFR0_FA64_HIGH		63
#define	SMFR0_FA64(smfr)	bitx64(smfr, 63, 63)

#define	SMFR0_SMEVER_LOW	56
#define	SMFR0_SMEVER_HIGH	59
#define	SMFR0_SMEVER(smfr)	bitx64(smfr, 59, 56)

#define	SMFR0_FEAT_SMEVER_SME		0x0
#define	SMFR0_FEAT_SMEVER_SME2		0x1
#define	SMFR0_FEAT_SMEVER_SME2p1	0x2

#define	SMFR0_I16I64_LOW	52
#define	SMFR0_I16I64_HIGH	55
#define	SMFR0_I16I64(smfr)	bitx64(smfr, 55, 52)

#define	SMFR0_F64F64_LOW	48
#define	SMFR0_F64F64_HIGH	51
#define	SMFR0_F64F64(smfr)	bitx64(smfr, 51, 48)

#define	SMFR0_I16I32_LOW	44
#define	SMFR0_I16I32_HIGH	47
#define	SMFR0_I16I32(smfr)	bitx64(smfr, 47, 44)

#define	SMFR0_B16B16_LOW	43
#define	SMFR0_B16B16_HIGH	43
#define	SMFR0_B16B16(smfr)	bitx64(smfr, 43, 43)

#define	SMFR0_F16F16_LOW	42
#define	SMFR0_F16F16_HIGH	42
#define	SMFR0_F16F16(smfr)	bitx64(smfr, 42, 42)

#define	SMFR0_I8I32_LOW	36
#define	SMFR0_I8I32_HIGH	39
#define	SMFR0_I8I32(smfr)	bitx64(smfr, 39, 36)

#define	SMFR0_F16F32_LOW	35
#define	SMFR0_F16F32_HIGH	35
#define	SMFR0_F16F32(smfr)	bitx64(smfr, 35, 35)

#define	SMFR0_B16F32_LOW	34
#define	SMFR0_B16F32_HIGH	34
#define	SMFR0_B16F32(smfr)	bitx64(smfr, 34, 34)

#define	SMFR0_F32F32_LOW	32
#define	SMFR0_F32F32_HIGH	32
#define	SMFR0_F32F32(smfr)	bitx64(smfr, 32, 32)

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.70 ID_AA64ZFR0_EL1, SVE Feature ID register 0
 *    (ARM DDI 0487I.a)
 */
#define	ZFR0_F64MM_LOW	56
#define	ZFR0_F64MM_HIGH	59
#define	ZFR0_F64MM(zfr)	bitx64(zfr, 59, 56)

#define	ZFR0_F32MM_LOW	52
#define	ZFR0_F32MM_HIGH	55
#define	ZFR0_F32MM(zfr)	bitx64(zfr, 55, 52)

#define	ZFR0_I8MM_LOW	44
#define	ZFR0_I8MM_HIGH	47
#define	ZFR0_I8MM(zfr)	bitx64(zfr, 47, 44)

#define	ZFR0_SM4_LOW	40
#define	ZFR0_SM4_HIGH	43
#define	ZFR0_SM4(zfr)	bitx64(zfr, 43, 40)

#define	ZFR0_SHA3_LOW	32
#define	ZFR0_SHA3_HIGH	35
#define	ZFR0_SHA3(zfr)	bitx64(zfr, 35, 32)

#define	ZFR0_B16B16_LOW		24
#define	ZFR0_B16B16_HIGH	27
#define	ZFR0_B16B16(zfr)	bitx64(zfr, 27, 24)

#define	ZFR0_BF16_LOW	20
#define	ZFR0_BF16_HIGH	23
#define	ZFR0_BF16(zfr)	bitx64(zfr, 23, 20)

#define	ZFR0_FEAT_BF16_BF16	0x1
#define	ZFR0_FEAT_BF16_EBF16	0x2

#define	ZFR0_BITPERM_LOW	16
#define	ZFR0_BITPERM_HIGH	19
#define	ZFR0_BITPERM(zfr)	bitx64(zfr, 19, 16)

#define	ZFR0_AES_LOW	4
#define	ZFR0_AES_HIGH	7
#define	ZFR0_AES(zfr)	bitx64(zfr, 7, 4)

#define	ZFR0_FEAT_AES_AES	0x1
#define	ZFR0_FEAT_AES_PMULL128	0x2

#define	ZFR0_SVEVER_LOW	0
#define	ZFR0_SVEVER_HIGH	3
#define	ZFR0_SVEVER(zfr)	bitx64(zfr, 3, 0)

#define	ZFR0_FEAT_SVEVER_SVE	0x0
#define	ZFR0_FEAT_SVEVER_SVE2	0x1
#define	ZFR0_FEAT_SVEVER_SVE2p1	0x2

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.5.12 PMMIR_EL1, Performance Monitors Machine Identification Register
 *    pp. D17-6787
 *
 * XXXARM: Should really be with PMU stuff, but we don't have any yet.
 */
#define	PMMIR_THWIDTH_LOW	20
#define	PMMIR_THWIDTH_HIGH	23
#define	PMMIR_THWIDTH(pmmir)	bitx64(pmmir, 23, 20)

#define	PMMIR_BUS_WIDTH_LOW	16
#define	PMMIR_BUS_WIDTH_HIGH	19
#define	PMMIR_BUS_WIDTH(pmmir)	bitx64(pmmir, 19, 16)

#define	PMMIR_BUS_SLOTS_LOW	8
#define	PMMIR_BUS_SLOTS_HIGH	11
#define	PMMIR_BUS_SLOTS(pmmir)	bitx64(pmmir, 11, 8)

#define	PMMIR_SLOTS_LOW		0
#define	PMMIR_SLOTS_HIGH	3
#define	PMMIR_SLOTS(pmmir)	bitx64(pmmir, 3, 0)

extern void cpuid_implementer(const cpu_t *, char *, size_t);
extern void cpuid_partname(const cpu_t *, char *, size_t);
extern void cpuid_brandstr(const cpu_t *, char *, size_t);

extern void cpuid_gather_arm_features(void *);
extern void cpuid_features_to_hwcap(void *, uint_t *, uint_t *, uint_t *);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_CPUID_H */
