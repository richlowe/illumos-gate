/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2023 Michael van der Westhuizen
 */

#ifndef _SYS_GIC_H
#define	_SYS_GIC_H

#include <sys/types.h>
#include <sys/cpuvar.h>
#include <sys/stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All names/etc here are derived from:
 *
 * ARM® Generic Interrupt Controller Architecture Specification
 *    GIC architecture version 3.0 and version 4.0
 *
 * (XXXARM: despite us using the memory mapped cpuif right now)
 */

struct gic_dist {
	volatile uint32_t ctlr;
	volatile uint32_t typer;
	volatile uint32_t iidr;
	volatile uint32_t resv0[29];
	/* : 0x80 */
	volatile uint32_t igroupr[32];
	/* : 0x100 */
	volatile uint32_t isenabler[32];
	volatile uint32_t icenabler[32];
	/* : 0x200 */
	volatile uint32_t ispendr[32];
	volatile uint32_t icpendr[32];
	/* : 0x300 */
	volatile uint32_t isactiver[32];
	volatile uint32_t icactiver[32];
	/* : 0x400 */
	volatile uint32_t ipriorityr[256];
	/* : 0x800 */
	volatile uint32_t itargetsr[256];
	/* : 0xc00 */
	volatile uint32_t icfgr[64];
	/* : 0xd00 */
	volatile uint32_t resv1[64];
	/* : 0xe00 */
	volatile uint32_t nsacr[64];
	/* : 0xf00 */
	volatile uint32_t sgir;
	volatile uint32_t resv2[3];
	/* : 0xf10 */
	volatile uint32_t cpendsgir[4];
	/* : 0xf20 */
	volatile uint32_t spendsgir[4];
	/* : 0xf30 */
	volatile uint32_t resv3[52];
};

struct gic_cpuif {
	volatile uint32_t ctlr;
	volatile uint32_t pmr;
	volatile uint32_t bpr;
	volatile uint32_t iar;
	/* : 0x10 */
	volatile uint32_t eoir;
	volatile uint32_t rpr;
	volatile uint32_t hppir;
	volatile uint32_t abpr;
	/* : 0x20 */
	volatile uint32_t aiar;
	volatile uint32_t aeoir;
	volatile uint32_t ahppir;
	/* : 0x2c */
	volatile uint32_t resv0[41];
	/* : 0xd0 */
	volatile uint32_t apr[4];
	/* : 0xe0 */
	volatile uint32_t nsapr[4];
	/* : 0xf0 */
	volatile uint32_t resv1[3];
	volatile uint32_t iidr;
};

/*
 * §2.2 INTIDs
 *
 * Interrupts are partitioned with a certain convention, older GIC interfaces
 * only support up to 1024
 *
 * XXXARM: In GICv2 compat mode we only support 1024.  When we change to
 * modern GIC the max must be changed.
 */
#define	GIC_INTID_MIN		0	/* Min Interrupt */
#define	GIC_INTID_MIN_SGI	0	/* Min Software Generated Interrupt */
#define	GIC_INTID_MAX_SGI	15	/* Max Software Generated Interrupt */
#define	GIC_INTID_MIN_PPI	16	/* Min Private Peripheral Interrupt */
#define	GIC_INTID_MAX_PPI	31	/* Max Private Peripheral Interrupt */
#define	GIC_INTID_MIN_SPI	32	/* Min Shared Peripheral Interrupt */
#define	GIC_INTID_MAX_SPI	1019	/* Max Shared Peripheral Interrupt */
#define	GIC_INTID_MIN_SPECIAL	1020	/* Min "Special" Interrupt */
#define	GIC_INTID_MAX_SPECIAL	1023	/* Max "Special" Interrupt */
#define	GIC_INTID_MAX		1023	/* Max Interrupt (in compat mode) */

#define	GIC_INTID_PERCPU_MIN	0	/* Start of cpu-local range */
#define	GIC_INTID_PERCPU_MAX	31	/* End of cpu-local range */

/*
 * §8.9.24 GICD_TYPER interrupt controller type register
 */

/* [25] 1 of N SPI interrupts supported? */
#define	GICD_TYPER_NO1N_SHIFT	25
#define	GICD_TYPER_NO1N_MASK	(0x1 << GICD_TYPER_NO1N_SHIFT)
#define	GICD_TYPER_NO1N(typer)	((typer & GICD_TYPER_NO1N_MASK) >> \
    GICD_TYPER_NO1N_SHIFT)

/* [24] affinity 3 valid? */
#define	GICD_TYPER_A3V_SHIFT	24
#define	GICD_TYPER_A3V_MASK	(0x1 << GICD_TYPER_A3V_SHIFT)
#define	GICD_TYPER_A3V(typer)	((typer & GICD_TYPER_A3V_MASK) >> \
    GICD_TYPER_A3V_SHIFT)

/* [23:19] number of interrupt ID bits supported - 1 */
#define	GICD_TYPER_IDBITS_SHIFT		19
#define	GICD_TYPER_IDBITS_MASK		(0x1f << GICD_TYPER_IDBITS_SHIFT)
#define	GICD_TYPER_IDBITS(typer)	((typer & GICD_TYPER_IDBITS_MASK) >> \
    GICD_TYPER_IDBITS_SHIFT)

/* [18] supports Direct Virtual LPI injection? */
#define	GICD_TYPER_DVIS_SHIFT	18
#define	GICD_TYPER_DVIS_MASK	(0x1 << GICD_TYPER_DVIS_SHIFT)
#define	GICD_TYPER_DVIS(typer)	((typer & GICD_TYPER_DVIS_MASK) >> \
    GICD_TYPER_DVIS_SHIFT)

/* [17] supports LPIs? */
#define	GICD_TYPER_LPIS_SHIFT	17
#define	GICD_TYPER_LPIS_MASK	(0x1 << GICD_TYPER_LPIS_SHIFT)
#define	GICD_TYPER_LPIS(typer)	((typer & GICD_TYPER_LPIS_MASK) >> \
    GICD_TYPER_LPIS_SHIFT)

/* [16] supports message-based interrupts via distributor writes? */
#define	GICD_TYPER_MBIS_SHIFT	16
#define	GICD_TYPER_MBIS_MASK	(0x1 << GICD_TYPER_MBIS_SHIFT)
#define	GICD_TYPER_MBIS(typer)	((typer & GICD_TYPER_MBIS_MASK) >> \
    GICD_TYPE_MBIS_SHIFT)

/* [10] supports two security states? */
#define	GICD_TYPER_SECURITYEXTN_SHIFT	10
#define	GICD_TYPER_SECURITYEXTN_MASK	(0x1 << GICD_TYPER_SECURITYEXTN_SHIFT)
#define	GICD_TYPER_SECURITYEXTN(typer)	\
	((typer & GICD_TYPER_SECURITYEXTN_MASK) >> \
	GICD_TYPER_SECURITYEXTN_SHIFT)

/* [7:5] number of cpus supported without affinity routing - 1 */
#define	GICD_TYPER_CPUNUMBER_SHIFT	5
#define	GICD_TYPER_CPUNUMBER_MASK	(0x7 << GICD_TYPER_CPUNUMBER_SHIFT)
#define	GICD_TYPER_CPUNUMBER(typer)	\
	((typer & GICD_TYPER_CPUNUMBER_MASK) >> GICD_TYPER_CPUNUMBER_SHIFT)

/* [4:0] maximum SPI INTID, for value N maximum is (32*(N+1))-1 */
#define	GICD_TYPER_ITLINESNUMBER_SHIFT	0
#define	GICD_TYPER_ITLINESNUMBER_MASK	0x1f
#define	GICD_TYPER_ITLINESNUMBER(typer)	\
	((typer & GICD_TYPER_ITLINESNUMBER_MASK) >> \
	GICD_TYPER_ITLINESNUMBER_SHIFT)

/*
 * 8.9.4 GICD_CTLR, Distributor Control Register
 */

/* [31] is a register write still propagating? */
#define	GICD_CTLR_RWP_SHIFT	31
#define	GICD_CTLR_RWP_MASK	(0x1 << GICD_CTLR_RWP_SHIFT)
#define	GICD_CTLR_RWP(ctlr)	((ctlr & GICD_CTLR_RWP_MASK) >> \
    GICD_CTLR_RWP_SHIFT)

/* [7] enable 1 of N wakeup? */
#define	GICD_CTLR_E1NWF_SHIFT	7
#define	GICD_CTLR_E1NWF_MASK	(0x1 << GICD_CTLR_E1NWF_SHIFT)
#define	GICD_CTLR_E1NWF(ctlr)	((ctlr & GICD_CTLR_E1NWF_MASK) >> \
    GICD_CTLR_E1NWF_SHIFT)

/* [6] disable security? */
#define	GICD_CTLR_DS_SHIFT	6
#define	GICD_CTLR_DS_MASK	(0x1 << GICD_CTLR_DS_SHIFT)
#define	GICD_CTLR_DS(ctlr)	((ctlr & GICD_CTLR_DS_MASK) >> \
    GICD_CTLR_DS_SHIFT)

/* [5] enable affinity routing (for non secure state)? */
#define	GICD_CTLR_ARENS_SHIFT	5
#define	GICD_CTLR_ARENS_MASK	(0x1 << GICD_CTLR_ARENS_SHIFT)
#define	GICD_CTLR_ARENS(ctlr)	((ctlr & GICD_CTLR_ARENS_MASK) >> \
    GICD_CTLR_ARENS_SHIFT)

/* [4] enable affinity routing (for secure state)? */
#define	GICD_CTLR_ARES_SHIFT	4
#define	GICD_CTLR_ARES_MASK	(0x1 << GICD_CTLR_ARES_SHIFT)
#define	GICD_CTLR_ARES(ctlr)	((ctlr & GICD_CTLR_ARES_MASK) >> \
    GICD_CTLR_ARES_SHIFT)

/* [2] enable secure group 1 interrupts? */
#define	GICD_CTLR_ENABLEGRP1S_SHIFT	2
#define	GICD_CTLR_ENABLEGRP1S_MASK	(0x1 << GICD_CTLR_ENABLEGRP1S_SHIFT)
#define	GICD_CTLR_ENABLEGRP1S(ctlr)	\
	((ctlr & GICD_CTLR_ENABLEGRP1S_MASK) >> GICD_CTLR_ENABLEGRP1S_SHIFT)

/* [1] enable non-secure group 1 interrupts?  */
#define	GICD_CTLR_ENABLEGRP1NS_SHIFT	1
#define	GICD_CTLR_ENABLEGRP1NS_MASK	(0x1 << GICD_CTLR_ENABLEGRP1NS_SHIFT)
#define	GICD_CTLR_ENABLEGRP1NS(ctlr)	\
	((ctlr & GICD_CTLR_ENABLEGRP1NS_MASK) >> GICD_CTLR_ENABLEGRP1NS_SHIFT)

/* [0] enable group 0 interrupts? */
#define	GICD_CTLR_ENABLEGRP0_SHIFT	0
#define	GICD_CTLR_ENABLEGRP0_MASK	(0x1 << GICD_CTLR_ENABLEGRP0_SHIFT)
#define	GICD_CTLR_ENABLEGRP0(ctlr)	((ctlr & GICD_CTLR_ENABLEGRP0_MASK) >> \
    GICD_CTLR_ENABLEGRP0_SHIFT)

#define	GICD_CTLR_ENABLE_GROUP0	GICD_CTLR_ENABLEGRP0_MASK

/*
 * §8.13.7 GICC_CTLR, CPU Interface Control Register
 *
 * NB: This register changes form depending on the value of GICD_CTLR.DS We
 * always establish this as 0, and these values assume it GICD_CTLR.DS==0
 */

/* [10] non-secure EOI behaviour.  If 1 only does pri drop, not deactivate */
#define	GICC_CTLR_EOIMODENS_SHIFT	10
#define	GICC_CTLR_EOIMODENS_MASK	(0x1 << GICC_CTLR_EOIMODENS_SHIFT)
#define	GICC_CTLR_EOIMODENS(ctlr)	((ctlr & GICC_CTLR_EOIMODENS_MASK) >> \
    GICC_CTLR_EOIMODENS_SHIFT)

/* [9] secure EOI behaviour.  If 1 only does pri drop, not deactivate */
#define	GICC_CTLR_EOIMODES_SHIFT	9
#define	GICC_CTLR_EOIMODES_MASK		(0x1 << GICC_CTLR_EOIMODES_SHIFT)
#define	GICC_CTLR_EOIMODES(ctlr)	((ctlr & GICC_CTLR_EOIMODES_MASK) >> \
    GICC_CTLR_EOIMODES_SHIFT)

/* [8] disable bypass signal for irq group 1? */
#define	GICC_CTLR_IRQBYPDISGRP1_SHIFT	8
#define	GICC_CTLR_IRQBYPDISGRP1_MASK	(0x1 << GICC_CTLR_IRQBYPDISGRP1_SHIFT)
#define	GICC_CTLR_IRQBYPDISGRP1(ctlr)	\
	((ctlr & GICC_CTLR_IRQBYPDISGRP1_MASK) >> GICC_CTLR_IRQBYPDISGRP1_SHIFT)

/* [7] disable bypass signal for fiq group 1? */
#define	GICC_CTLR_FIQBYPDISGRP1_SHIFT	7
#define	GICC_CTLR_FIQBYPDISGRP1_MASK	(0x1 << GICC_CTLR_FIQBYPDISGRP1_SHIFT)
#define	GICC_CTLR_FIQBYPDISGRP1(ctlr)	\
	((ctlr & GICC_CTLR_FIQBYPDISGRP1_MASK) >> GICC_CTLR_FIQBYPDISGRP1_SHIFT)

/* [6] disable bypass signal for irq group 0 */
#define	GICC_CTLR_IRQBYPDISGRP0_SHIFT	6
#define	GICC_CTLR_IRQBYPDISGRP0_MASK	(0x1 << GICC_CTLR_IRQBYPDISGRP0_SHIFT)
#define	GICC_CTLR_IRQBYPDISGRP0(ctlr)	\
	((ctlr & GICC_CTLR_IRQBYPDISGRP0_MASK) >> GICC_CTLR_IRQBYPDISGRP0_SHIFT)

/* [5] disable bypass signal for fiq group 1? */
#define	GICC_CTLR_FIQBYPDISGRP0_SHIFT	5
#define	GICC_CTLR_FIQBYPDISGRP0_MASK	(0x1 << GICC_CTLR_FIQBYPDISGRP0_SHIFT)
#define	GICC_CTLR_FIQBYPDISGRP0(ctlr)	\
	((ctlr & GICC_CTLR_FIQBYPDISGRP0_MASK) >> GICC_CTLR_FIQBYPDISGRP0_SHIFT)

/* [4] GICC_BPR contrlors both group 0 and group 1? */
#define	GICC_CTLR_CBPR_SHIFT	4
#define	GICC_CTLR_CBPR_MASK	(0x1 << GICC_CTLR_CBPR_SHIFT)
#define	GICC_CTLR_CBPR(ctlr)	((ctlr & GICC_CTLR_CBPR_MASK) >> \
    GICC_CTLR_CBPR_SHIFT)

/* [3] group 0 interrupts via FIQ? */
#define	GICC_CTLR_FIQEN_SHIFT	3
#define	GICC_CTLR_FIQEN_MASK	(0x1 << GICC_CTLR_FIQEN_SHIFT)
#define	GICC_CTLR_FIQEN(ctlr)	((ctlr & GICC_CTLR_FIQEN_MASK) >> \
    GICC_CTLR_FIQEN_SHIFT)

/* [1] enable group 1? */
#define	GICC_CTLR_ENABLEGRP1_SHIFT	1
#define	GICC_CTLR_ENABLEGRP1_MASK	(0x1 << GICC_CTLR_ENABLEGRP1_SHIFT)
#define	GICC_CTLR_ENABLEGRP1(ctlr)	((ctlr & GICC_CTLR_ENABLEGRP1_MASK) >> \
    GICC_CTLR_ENABLEGRP1_SHIFT)

/* [0] enable group 1? */
#define	GICC_CTLR_ENABLEGRP0_SHIFT	0
#define	GICC_CTLR_ENABLEGRP0_MASK	(0x1 << GICC_CTLR_ENABLEGRP0_SHIFT)
#define	GICC_CTLR_ENABLEGRP0(ctlr)	((ctlr & GICC_CTLR_ENABLEGRP0_MASK) >> \
    GICC_CTLR_ENABLEGRP0_SHIFT)

#define	GICC_CTLR_ENABLE_GROUP0		GICC_CTLR_ENABLEGRP0_MASK
#define	GICC_CTLR_ENABLE_GROUP1		GICC_CTLR_ENABLEGRP1_MASK

/*
 * §8.13.11 GICC_IAR CPU Interface Interrupt Acknowledge Register
 *
 * Note that this differs when affinity routing is enabled to a mask of 0x3fffff
 * Without affinity routing bits [23:13] are also reserved, and bits [12:10]
 * indicate the source of an SGI or are otherwise reserved.
 *
 * This leaves us with [9:0] as intid.
 *
 * NB: The bits that become reserved _do not necessarily become 0_, we must
 * adjust the masking.
 */
#define	GICC_IAR_INTID_SHIFT	0
#define	GICC_IAR_INTID_MASK	(0x3ff << GICC_IAR_INTID_SHIFT)
#define	GICC_IAR_INTID(iar)	((iar & GICC_IAR_INTID_MASK) >> \
    GICC_IAR_INTID_SHIFT)

/* Map IRQ to GICD_IPRIORITYR<n> */
#define	GICD_IRQ_TO_IPRIORITYR(irq)	((irq) / 4)

/* Map IRQ to GICD_IRPRIORITYR<n>.INTR (the 8 bit field within each register) */
#define	GICD_IRQ_TO_IPRIORITYR_FIELD(irq, val)	(val << (8 * (irq % 4)))

/* Map IRQ to GICD_ITARGETSR<n> */
#define	GICD_IRQ_TO_ITARGETSR(irq)	((irq) / 4)

/* Map IRQ to GICD_ITARGETSR<n>.INTR (the 8 bit field within each register) */
#define	GICD_IRQ_TO_ITARGETSR_FIELD(irq, val)	(val << (8 * (irq % 4)))

/* Map IRQ to GICD_ISENABLER<n> */
#define	GICD_IRQ_TO_ISENABLER(irq)	((irq) / 32)

/* Map IRQ to GICD_ISENABLER<n>.INTR */
#define	GICD_IRQ_TO_ISENABLER_FIELD(irq)	(1 << ((irq) % 32))

/* Map IRQ to GICD_ICFGR<n> */
#define	GICD_IRQ_TO_ICFGR(irq)	((irq) / 16)

/* Map IRQ to GICD_ICFGR<n>.INTR */
#define	GICD_IRQ_TO_ICFGR_FIELD(irq, val)	(val << (((irq) % 16) * 2))

/*
 * §8.9.21 GICD_SGIR, Software Generated Interrupt Register
 */
/*
 * [24:23] determine how SGI is forwarded
 * 00 - forward to all in TARGETS
 * 01 - forward the interrupt to all cpus except this one
 * 10 - forward to the cpu that requested the interrupt
 * 11 - Reserved.
 */
#define	GICD_SGIR_TARGETFILTER_SHIFT	24
#define	GICD_SGIR_TARGETFILTER_MASK	(0x3 << GICD_SGIR_TARGETFILTER_SHIFT)
/*
 * [23:16] target list, set bit for each cpu to receive
 * (XXXARM: GICv2/8 cpu limit)
 */
#define	GICD_SGIR_TARGETS_SHIFT		16
#define	GICD_SGIR_TARGETS_MASK		(0xff << GICD_SGIR_TARGETS_SHIFT)

/* [15] forward to cpu only if group 1 on that cpu */
#define	GICD_SGIR_NSATT_SHIFT		15
#define	GICD_SGIR_NSATT_MASK		(1 << GICD_SGIR_NSATT_SHIFT)

/* [3:0] SGI INTID to send */
#define	GICD_SGIR_INTID_SHIFT		0
#define	GICD_SGIR_INTID_MASK		(0xf << GICD_SGIR_INTID_SHIFT)

#define	GICD_SGIR(filter, targets, nsatt, intid)			\
	(((filter << GICD_SGIR_TARGETFILTER_SHIFT) &			\
	GICD_SGIR_TARGETFILTER_MASK) |					\
	((targets << GICD_SGIR_TARGETS_SHIFT) &				\
	GICD_SGIR_TARGETS_MASK) |					\
	((nsatt << GICD_SGIR_NSATT_SHIFT) & GICD_SGIR_NSATT_MASK) |	\
	((intid << GICD_SGIR_INTID_SHIFT) & GICD_SGIR_INTID_MASK))

/*
 * GIC supports at least 16 levels, which is what we need, so we only use
 * those 16.
 *
 * See §4.8 Interrupt Prioritization
 * for a description of how these priorities are arranged within a byte
 *
 * In short, we count down from 240 (ipl 0) to 0 (ipl 15) in steps of 16.
 */
#define	GIC_IPL_TO_PRI(ipl)	((0xF & ~(ipl)) << 4)

void gic_mask_level_irq(int irq);
void gic_unmask_level_irq(int irq);

void gic_send_ipi(cpuset_t cpuset, int irq);
int gic_init(void);
void gic_init_primary(void);
void gic_init_secondary(int);
void gic_config_irq(uint32_t irq, bool is_edge);
int gic_num_cpus(void);

/*
 * Types and data structure filled by GIC implementation modukles
 */
typedef void (*mask_level_irq_t)(int irq);
typedef void (*unmask_level_irq_t)(int irq);
typedef void (*send_ipi_t)(cpuset_t cpuset, int irq);
typedef void (*init_primary_t)(void);
typedef void (*init_secondary_t)(int id);
typedef void (*config_irq_t)(uint32_t irq, bool is_edge);
typedef int (*num_cpus_t)(void);
typedef int (*addspl_t)(int irq, int ipl, int min_ipl, int max_ipl);
typedef int (*delspl_t)(int irq, int ipl, int min_ipl, int max_ipl);
typedef int (*setlvl_t)(int irq);
typedef void (*setlvlx_t)(int ipl, int irq);

typedef struct gic_ops {
	mask_level_irq_t	mask_level_irq;
	unmask_level_irq_t	unmask_level_irq;
	send_ipi_t		send_ipi;
	init_primary_t		init_primary;
	init_secondary_t	init_secondary;
	config_irq_t		config_irq;
	num_cpus_t		num_cpus;
	addspl_t		addspl;
	delspl_t		delspl;
	setlvl_t		setlvl;
	setlvlx_t		setlvlx;
} gic_ops_t;

extern gic_ops_t gic_ops;

/*
 * Populated when MMIO access to the CPU interface is needed.
 */
extern volatile struct gic_cpuif *gic_cpuif;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_GIC_H */
