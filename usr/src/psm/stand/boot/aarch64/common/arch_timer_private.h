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

/*
 * Copyright 2023 Brian McKenzie
 */

/*
 * Private prototypes and definitions for the generic timer interface.
 */

#ifndef __COMMON_ARCH_TIMER_PRIVATE_H
#define	__COMMON_ARCH_TIMER_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/arch_timer.h>

extern void init_arch_timer(arch_timer_t type);

#ifdef __cplusplus
}
#endif

#endif /* __COMMON_ARCH_TIMER_PRIVATE_H */
