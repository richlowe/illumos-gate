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

/*
 * Boot-time SMCCC initialization.
 *
 * This shim runs in the boot context (compiled into both BOOT_DRIVER_OBJS
 * and CORE_OBJS).  It calls smccc_init which, depending on the link
 * context, resolves to either the dboot or kernel implementation.
 *
 * Must be called before boot_psci_init so that the SMCCC transport
 * layer is established before PSCI attempts to use it.
 */

#include <sys/types.h>
#include <sys/null.h>
#include <sys/bootinfo.h>
#include <sys/smccc.h>
#include <sys/linker_set.h>
#include <asm/controlregs.h>

extern void boot_uart_putchar(int c);

void
boot_smccc_init(struct xboot_info *xbp)
{
	static const char no_smccc[] = "boot: SMCCC init failed";
	int rv;

	if (xbp == NULL) {
		return;
	}

	rv = smccc_init(xbp);
	if (rv != 0) {
		const char *p = no_smccc;
		while (*p) {
			boot_uart_putchar(*p++);
		}
		boot_uart_putchar('\n');

		/*
		 * Fatal: without the SMCCC transport we cannot call
		 * PSCI and the system cannot boot.
		 */
		for (;;) {
			/*
			 * Mask all exceptions: Debug, SError, IRQ and FIQ.
			 */
			__asm__ __volatile__(
			    "msr DAIFSet, #" __XSTRING(DAIF_SETCLEAR_ALL)
			    ::: "memory"
			);
			__asm__ volatile("dsb sy":::"memory");
			__asm__ volatile("isb sy":::"memory");
			__asm__ volatile("wfi":::"memory");
		}
		/* UNREACHABLE */
	}
}
