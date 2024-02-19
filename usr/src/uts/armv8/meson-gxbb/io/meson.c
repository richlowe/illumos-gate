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
#include <sys/bootsvcs.h>
#include <sys/psci.h>

#define	UART_ADDR	(UART_PHYS + SEGKPM_BASE)
#define	UART_WFIFO	(*(volatile uint32_t *)(UART_ADDR + 0x00))
#define	UART_RFIFO	(*(volatile uint32_t *)(UART_ADDR + 0x04))
#define	UART_CONTROL	(*(volatile uint32_t *)(UART_ADDR + 0x08))
#define	UART_STATUS	(*(volatile uint32_t *)(UART_ADDR + 0x0C))
#define	UART_MISC	(*(volatile uint32_t *)(UART_ADDR + 0x10))
#define	UART_REG5	(*(volatile uint32_t *)(UART_ADDR + 0x14))

#define	UART_CONTROL_CLR_ERR	(1u << 24)
#define	UART_CONTROL_RST_RX	(1u << 23)
#define	UART_CONTROL_RST_TX	(1u << 22)
#define	UART_CONTROL_ENB_RX	(1u << 13)
#define	UART_CONTROL_ENB_TX	(1u << 12)

#define	UART_STATUS_RECV_BUSY	(1u << 26)
#define	UART_STATUS_XMIT_BUSY	(1u << 25)
#define	UART_STATUS_TFIFO_EMPTY	(1u << 22)
#define	UART_STATUS_TFIFO_FULL	(1u << 21)
#define	UART_STATUS_RFIFO_EMPTY	(1u << 20)
#define	UART_STATUS_RFIFO_FULL	(1u << 19)
#define	UART_STATUS_ERR_WFIFO	(1u << 18)
#define	UART_STATUS_ERR_FRAME	(1u << 17)
#define	UART_STATUS_ERR_PARITY	(1u << 16)


static void
yield()
{
	__asm__ volatile("yield":::"memory");
}

static int
_getchar()
{
	while (UART_STATUS & UART_STATUS_RFIFO_EMPTY)
		yield();
	return (UART_RFIFO);
}

static void
_putchar(int c)
{
	while (UART_STATUS & UART_STATUS_TFIFO_FULL) {}
	UART_WFIFO = c;
	if (c == '\n')
		_putchar('\r');
	while (!(UART_STATUS & UART_STATUS_TFIFO_EMPTY)) {}
}

static int
_ischar()
{
	return (!(UART_STATUS & UART_STATUS_RFIFO_EMPTY));
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
	tod_module_name = "todmeson";
}

#define	HHI_SYS_CPU_CLK_CNTL0	(*(volatile uint32_t *)	\
	(0xc883c000 + (0x67 << 2) + SEGKPM_BASE))
#define	HHI_SYS_PLL_CNTL	(*(volatile uint32_t *)	\
	(0xc883c000 + (0xc0 << 2) + SEGKPM_BASE))

union hhi_sys_cpu_clk_cntl0 {
	uint32_t dw;
	struct {
		uint32_t	mux		:	2;
		uint32_t			:	30;
	};
};

union hhi_sys_pll_cntl {
	uint32_t dw;
	struct {
		uint32_t	m		:	9;
		uint32_t	n		:	5;
		uint32_t			:	2;
		uint32_t	od		:	2;
		uint32_t			:	14;
	};
};

uint64_t
plat_get_cpu_clock(int cpu_no)
{
	uint32_t clk = 24 * 1000000;

	union hhi_sys_pll_cntl pll_cntl;
	pll_cntl.dw = HHI_SYS_PLL_CNTL;

	union hhi_sys_cpu_clk_cntl0 sys_cpu_clk_cntl0;
	sys_cpu_clk_cntl0.dw = HHI_SYS_CPU_CLK_CNTL0;

	clk *= pll_cntl.m / pll_cntl.n;
	clk >>= pll_cntl.od;
	clk /= (sys_cpu_clk_cntl0.mux + 1);

	return (clk);
}
