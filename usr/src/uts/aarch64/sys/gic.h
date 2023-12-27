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
 * Copyright 2024 Michael van der Westhuizen
 */

#ifndef _SYS_GIC_H
#define	_SYS_GIC_H

#include <sys/types.h>
#include <sys/cpuvar.h>
#include <sys/stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int gic_init(void);
extern void gic_cpu_init(cpu_t *cp);
extern void gic_send_ipi(cpuset_t cpuset, int irq);
extern void gic_config_irq(uint32_t irq, bool is_edge);

/*
 * For interrupt handling.
 */
extern uint32_t gic_acknowledge(void);
extern uint32_t gic_ack_to_vector(uint32_t ack);
extern void gic_eoi(uint32_t ack);
extern void gic_deactivate(uint32_t ack);
extern int gic_vector_is_special(uint32_t intid);

/*
 * Types and data structure filled by GIC implementation modules
 */
typedef void (*gic_send_ipi_t)(cpuset_t cpuset, int irq);
typedef int (*gic_init_t)(void);
typedef void (*gic_cpu_init_t)(cpu_t *cp);
typedef void (*gic_config_irq_t)(uint32_t irq, bool is_edge);
typedef int (*gic_addspl_t)(int irq, int ipl, int min_ipl, int max_ipl);
typedef int (*gic_delspl_t)(int irq, int ipl, int min_ipl, int max_ipl);
typedef int (*gic_setlvl_t)(int irq);
typedef void (*gic_setlvlx_t)(int ipl);
typedef uint32_t (*gic_acknowledge_t)(void);
typedef uint32_t (*gic_ack_to_vector_t)(uint32_t ack);
typedef void (*gic_eoi_t)(uint32_t ack);
typedef void (*gic_deactivate_t)(uint32_t ack);
typedef int (*gic_vector_is_special_t)(uint32_t intid);

typedef struct gic_ops {
	gic_send_ipi_t		go_send_ipi;
	gic_init_t		go_init;
	gic_cpu_init_t		go_cpu_init;
	gic_config_irq_t	go_config_irq;
	gic_addspl_t		go_addspl;
	gic_delspl_t		go_delspl;
	gic_setlvl_t		go_setlvl;
	gic_setlvlx_t		go_setlvlx;
	gic_acknowledge_t	go_acknowledge;
	gic_ack_to_vector_t	go_ack_to_vector;
	gic_eoi_t		go_eoi;
	gic_deactivate_t	go_deactivate;
	gic_vector_is_special_t	go_vector_is_special;
} gic_ops_t;

extern gic_ops_t gic_ops;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_GIC_H */
