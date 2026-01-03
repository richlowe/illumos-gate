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
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2025 Michael van der Westhuizen
 * Copyright 2026 Richard Lowe
 */
#ifndef _KBOOT_MMU_H
#define	_KBOOT_MMU_H

/* Early MMU mapping interfaces */

#ifdef __cplusplus
extern "C" {
#endif

extern void kbm_map(uintptr_t, paddr_t, uint_t);
extern void kbm_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _KBOOT_MMU_H */
