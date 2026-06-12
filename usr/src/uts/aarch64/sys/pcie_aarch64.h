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
 * Copyright 2026 Michael van der Westhuizen
 */

#ifndef _SYS_PCIE_AARCH64_H
#define	_SYS_PCIE_AARCH64_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pcie_aarch64_priv {
	boolean_t	bus_osc;	/* Has _OSC been called */
	boolean_t	bus_osc_hp;	/* Was native HP granted */
	boolean_t	bus_osc_aer;	/* Was AER granted */
} pcie_aarch64_priv_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PCIE_AARCH64_H */
