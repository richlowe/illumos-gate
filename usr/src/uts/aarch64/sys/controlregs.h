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
 * Copyright 2022 Michal van der Westhuizen
 */

#ifndef _SYS_CONTROLREGS_H
#define	_SYS_CONTROLREGS_H

#if !defined(_ASM)
#include <sys/bitext.h>
#include <sys/types.h>
#include <asm/controlregs.h>

#define	SCTLR_EL1_RES1	(0x30d00800)
#define	SCTLR_EL2_RES1	(0x30C50830)
#define	SCTLR_EL3_RES1	(0x30C50830)
#define	SCTLR_UCI	(1<<26)
#define	SCTLR_EE	(1<<25)
#define	SCTLR_E0E	(1<<24)
#define	SCTLR_WXN	(1<<19)
#define	SCTLR_nTWE	(1<<18)
#define	SCTLR_nTWI	(1<<16)
#define	SCTLR_UCT	(1<<15)
#define	SCTLR_DZE	(1<<14)
#define	SCTLR_I		(1<<12)
#define	SCTLR_UMA	(1<<9)
#define	SCTLR_SED	(1<<8)
#define	SCTLR_ITD	(1<<7)
#define	SCTLR_CP15BEN	(1<<5)
#define	SCTLR_SA0	(1<<4)
#define	SCTLR_SA	(1<<3)
#define	SCTLR_C		(1<<2)
#define	SCTLR_A		(1<<1)
#define	SCTLR_M		(1<<0)

#define	HCR_RW		(1<<31)
#define	CNTHCTL_EL1PCTEN	(1 << 0)
#define	CNTHCTL_EL1PCEN		(1 << 1)

#define	CPTR_EL2_RES1	(0x33ff)


#define	CPUECTLR_SMP	(1<<6)

#define	TCR_AS		(1ul<<36)
#define	TCR_IPS_4G	(0ul<<32)
#define	TCR_IPS_64G	(1ul<<32)
#define	TCR_IPS_1T	(2ul<<32)
#define	TCR_IPS_4T	(3ul<<32)
#define	TCR_IPS_16T	(4ul<<32)
#define	TCR_IPS_256T	(5ul<<32)
#define	TCR_IPS_SHIFT	32
#define	TCR_TG1_16K	(1ul<<30)
#define	TCR_TG1_4K	(2ul<<30)
#define	TCR_TG1_64K	(3ul<<30)
#define	TCR_SH1_NSH	(0ul<<28)
#define	TCR_SH1_OSH	(2ul<<28)
#define	TCR_SH1_ISH	(3ul<<28)
#define	TCR_ORGN1_NC	(0ul<<26)
#define	TCR_ORGN1_WBWA	(1ul<<26)
#define	TCR_ORGN1_WT	(2ul<<26)
#define	TCR_ORGN1_WBNA	(3ul<<26)
#define	TCR_IRGN1_NC	(0ul<<24)
#define	TCR_IRGN1_WBWA	(1ul<<24)
#define	TCR_IRGN1_WT	(2ul<<24)
#define	TCR_IRGN1_WBNA	(3ul<<24)
#define	TCR_EPD1	(1ul<<23)
#define	TCR_A1		(1ul<<22)
#define	TCR_T1SZ_256T	(16ul<<16)
#define	TCR_TG0_4K	(0ul<<14)
#define	TCR_TG0_64K	(1ul<<14)
#define	TCR_TG0_16K	(2ul<<14)
#define	TCR_SH0_NSH	(0ul<<12)
#define	TCR_SH0_OSH	(2ul<<12)
#define	TCR_SH0_ISH	(3ul<<12)
#define	TCR_ORGN0_NC	(0ul<<10)
#define	TCR_ORGN0_WBWA	(1ul<<10)
#define	TCR_ORGN0_WT	(2ul<<10)
#define	TCR_ORGN0_WBNA	(3ul<<10)
#define	TCR_IRGN0_NC	(0ul<<8)
#define	TCR_IRGN0_WBWA	(1ul<<8)
#define	TCR_IRGN0_WT	(2ul<<8)
#define	TCR_IRGN0_WBNA	(3ul<<8)
#define	TCR_EPD0	(1ul<<7)
#define	TCR_T0SZ_256T	(16ul<<0)


#define	TTBR_ASID_SHIFT		48
#define	TTBR_ASID_MASK		(0xFFull<<TTBR_ASID_SHIFT)
#define	TTBR_BADDR48_SHIFT	12
#define	TTBR_BADDR48_MASK	(0xfffffffffull << TTBR_BADDR48_SHIFT)
#define	TTBR_CNP_SHIFT		0
#define	TRBR_CNP_MASK		(0x1ull << TTBR_CNP_SHIFT)


#define	PSR_N		(1u<<31)
#define	PSR_Z		(1u<<30)
#define	PSR_C		(1u<<29)
#define	PSR_V		(1u<<28)
#define	PSR_SS		(1u<<21)
#define	PSR_IL		(1u<<20)
#define	PSR_D		(1u<<9)
#define	PSR_A		(1u<<8)
#define	PSR_I		(1u<<7)
#define	PSR_F		(1u<<6)
#define	PSR_M_MASK	(0xF)
#define	PSR_M_EL3h	(0xD)
#define	PSR_M_EL3t	(0xC)
#define	PSR_M_EL2h	(0x9)
#define	PSR_M_EL2t	(0x8)
#define	PSR_M_EL1h	(0x5)
#define	PSR_M_EL1t	(0x4)
#define	PSR_M_EL0t	(0x0)

#define	PSR_USERINIT	PSR_M_EL0t
#define	PSR_USERMASK	(PSR_N | PSR_Z | PSR_C | PSR_V | PSR_SS)

#define	CPACR_FPEN_MASK	(3<<20)
#define	CPACR_FPEN_EN	(3<<20)
#define	CPACR_FPEN_DIS	(0<<20)

#define	PAR_ATTR_MASK	(0xFF00000000000000ul)
#define	PAR_PA_MASK	(0x0000FFFFFFFFF000ul)
#define	PAR_NS_MASK	(0x0000000000000200ul)
#define	PAR_SH_MASK	(0x0000000000000180ul)
#define	PAR_F		(0x0000000000000001ul)

/*
 * Arm Architecture Reference Manual for A-profile architecture
 *    D17.2.34 CTR_EL0, Cache Type Register
 *    (ARM DDI 0487I.a)
 */
#define	CTR_TMINLINE(ctr)	bitx64(ctr, 37, 32)
#define	CTR_TMINLINE_SIZE(ctr)	(4 << CTR_TMINLINE(ctr))
#define	CTR_DIC(ctr)		bitx64(ctr, 29, 29)
#define	CTR_IDC(ctr)		bitx64(ctr, 28, 28)
#define	CTR_CWG(ctr)		bitx64(ctr, 27, 24)
#define	CTR_ERG(ctr)		bitx64(ctr, 23, 20)
#define	CTR_DMINLINE(ctr)	bitx64(ctr, 19, 16)
#define	CTR_DMINLINE_SIZE(ctr)	(4 << CTR_DMINLINE(ctr))
#define	CTR_L1IP(ctr)		bitx64(ctr, 15, 14)
#define	CTR_IMINLINE(ctr)	bitx64(ctr, 3, 0)
#define	CTR_IMINLINE_SIZE(ctr)	(4 << CTR_IMINLINE(ctr))

#define	MPIDR_AFF0_MASK	(0x00000000000000FFul)
#define	MPIDR_AFF1_MASK	(0x000000000000FF00ul)
#define	MPIDR_AFF2_MASK	(0x0000000000FF0000ul)
#define	MPIDR_AFF3_MASK	(0x000000FF00000000ul)
#define	MPIDR_AFF_MASK	(MPIDR_AFF0_MASK | MPIDR_AFF1_MASK | \
			MPIDR_AFF2_MASK | MPIDR_AFF3_MASK)

#define	MAIR_ATTR_IWB_OWB	(0xFFul)
#define	MAIR_ATTR_IWT_OWT	(0xBBul)
#define	MAIR_ATTR_IWB_ONC	(0x4Ful)
#define	MAIR_ATTR_IWT_ONC	(0x4Bul)
#define	MAIR_ATTR_INC_ONC	(0x44ul)
#define	MAIR_ATTR_nGnRE		(0x04ul)
#define	MAIR_ATTR_nGnRnE	(0x00ul)
#define	MAIR_ATTR_nGRE		(0x08ul)

#define	MDSCR_RXfull		(1ul << 30)
#define	MDSCR_TXfull		(1ul << 29)
#define	MDSCR_RXO		(1ul << 27)
#define	MDSCR_TXU		(1ul << 26)
#define	MDSCR_INTdis_MASK	(3ul << 22)
#define	MDSCR_TDA		(1ul << 21)
#define	MDSCR_MDE		(1ul << 15)
#define	MDSCR_HDE		(1ul << 14)
#define	MDSCR_KDE		(1ul << 13)
#define	MDSCR_TDCC		(1ul << 12)
#define	MDSCR_ERR		(1ul << 6)
#define	MDSCR_SS		(1ul << 0)

/* hypervisor.h in FreeBSD */
#define	CPTR_RES1		(0x000033ff)
#define	ICC_SRE_EL2_EN		(1U << 3)
#define	ICC_SRE_EL2_SRE		(1U << 0)

#define	SCTLR_RES1		0x30d00800	/* Reserved ARMv8.0, write 1 */

#endif	/* !_ASM */
#endif	/* _SYS_CONTROLREGS_H */
