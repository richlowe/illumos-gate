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
 */

#include <sys/types.h>
#include <sys/machclock.h>
#include <sys/platform.h>
#include <sys/modctl.h>
#include <sys/platmod.h>
#include <sys/promif.h>
#include <sys/errno.h>
#include <sys/byteorder.h>
#include <sys/cmn_err.h>
#include <sys/bootsvcs.h>
#include <sys/psci.h>

#define	UART_ADDR	(UART_PHYS + SEGKPM_BASE)

#define	UARTDR		(*(volatile uint32_t *)(UART_ADDR + 0x00))
#define	UARTFR		(*(volatile uint32_t *)(UART_ADDR + 0x18))

#define	UARTFR_TXFE	(1 << 7)
#define	UARTFR_TXFF	(1 << 5)
#define	UARTFR_RXFE	(1 << 4)

static void
yield()
{
	__asm__ volatile("yield":::"memory");
}

static int
_getchar()
{
	while (UARTFR & UARTFR_RXFE)
		yield();
	return (UARTDR & 0xFF);
}

static void
_putchar(int c)
{
	while (UARTFR & UARTFR_TXFF) {}
	UARTDR = c;
	if (c == '\n')
		_putchar('\r');
	while (!(UARTFR & UARTFR_TXFE)) {}
}

static int
_ischar()
{
	return (!(UARTFR & UARTFR_RXFE));
}

void __NORETURN
_reset(bool poff)
{
	if (poff)
		psci_system_off();
	else
		psci_system_reset();
	for (;;) {
		__asm__ volatile("wfe":::"memory");
	}
}

static struct boot_syscalls _sysp =
{
	.bsvc_getchar = _getchar,
	.bsvc_putchar = _putchar,
	.bsvc_ischar = _ischar,
	.bsvc_reset = _reset,
};
struct boot_syscalls *sysp = &_sysp;

void
set_platform_defaults(void)
{
	tod_module_name = "todpl031";
}

uint64_t
plat_get_cpu_clock(int cpu_no)
{
	char name[80];
	sprintf(name, "/cpus/cpu@%d", cpu_no);
	pnode_t node = prom_finddevice(name);
	if (node > 0) {
		uint_t clock;
		if (prom_getproplen(node, "clock-frequency") ==
		    sizeof (uint_t)) {
			prom_getprop(node, "clock-frequency", (caddr_t)&clock);
			return (ntohl(clock));
		}
	}
	return (1000 * 1000 * 1000);
}
