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
 * Copyright 2025 <contributor>
 */

#ifndef _MMU_H
#define	_MMU_H

/*
 * Describe the purpose of the file here.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <vm/hat_pte.h>

extern struct hat_mmu_info mmu;

extern void init_mmu(void);
extern int ttbr_dcmd(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int pte_dcmd(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int ptable_dcmd(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int vatopfn_dcmd(uintptr_t, uint_t, int, const mdb_arg_t *);

#ifdef __cplusplus
}
#endif

#endif /* _MMU_H */
