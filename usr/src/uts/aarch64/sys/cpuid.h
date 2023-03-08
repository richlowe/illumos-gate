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

/* Ampere */
#define	MIDR_PART_AMPERE_AMPERE1	0xac3

#define	MIDR_REVISION(midr)	bitx64(midr, 3, 0)

extern void cpuid_implementer(const cpu_t *, char *, size_t);
extern void cpuid_partname(const cpu_t *, char *, size_t);
extern void cpuid_brandstr(const cpu_t *, char *, size_t);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_CPUID_H */
