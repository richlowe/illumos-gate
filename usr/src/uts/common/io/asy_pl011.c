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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved					*/

/*
 * Copyright (c) 1992, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2012 Milan Jurik. All rights reserved.
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2023 Oxide Computer Company
 * Copyright 2026 Michael van der Westhuizen
 */

/*
 * ARM PL011 / SBSA Generic UART hardware operations for the asy(4D) serial
 * driver.
 *
 * This file implements the asy_hw_ops_t interface for the ARM PL011 UART
 * (as described in the ARM PrimeCell UART PL011 Technical Reference Manual)
 * and its SBSA (Server Base System Architecture) subset.
 *
 * The PL011 ops translate between the common ASY_*_* semantic bit definitions
 * used by the shared asy.c driver and the PL011's own register layout.
 *
 * SBSA subset: when ASY2_SBSA is set, baud rate, line control, and FIFO
 * level configuration are no-ops (firmware owns these) and clock-frequency
 * is not required.  ASY2_SBSA always implies ASY2_NOCLK.
 *
 * When the UART is a real PL011 but no clock-frequency property is available
 * (common on ACPI platforms where firmware has already configured the baud
 * rate), ASY2_NOCLK is set without ASY2_SBSA.  This suppresses baud rate
 * changes while still allowing LCR and FIFO configuration.
 *
 * Three-tier capability model:
 *   SBSA (ASY2_SBSA | ASY2_NOCLK) - firmware owns baud, LCR, FIFO
 *   PL011 without clock (ASY2_NOCLK) - can set LCR/FIFO, not baud
 *   PL011 with clock (neither flag)  - fully functional
 */

#if defined(__aarch64__)

#include <sys/param.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stream.h>
#include <sys/termio.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/consdev.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/note.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/asy.h>

/*
 * PL011 register offsets (byte offsets, 32-bit wide registers).
 */
#define	PL011_DR	0x000	/* Data Register */
#define	PL011_RSR	0x004	/* Receive Status / Error Clear (ECR) */
#define	PL011_FR	0x018	/* Flag Register */
#define	PL011_ILPR	0x020	/* IrDA Low-Power Counter */
#define	PL011_IBRD	0x024	/* Integer Baud Rate Divisor */
#define	PL011_FBRD	0x028	/* Fractional Baud Rate Divisor */
#define	PL011_LCR_H	0x02C	/* Line Control Register */
#define	PL011_CR	0x030	/* Control Register */
#define	PL011_IFLS	0x034	/* Interrupt FIFO Level Select */
#define	PL011_IMSC	0x038	/* Interrupt Mask Set/Clear */
#define	PL011_RIS	0x03C	/* Raw Interrupt Status */
#define	PL011_MIS	0x040	/* Masked Interrupt Status */
#define	PL011_ICR	0x044	/* Interrupt Clear Register */

/*
 * Data Register bits.
 */
#define	PL011_DR_OE		(1 << 11)	/* Overrun error */
#define	PL011_DR_BE		(1 << 10)	/* Break error */
#define	PL011_DR_PE		(1 << 9)	/* Parity error */
#define	PL011_DR_FE		(1 << 8)	/* Framing error */
#define	PL011_DR_DATA		0xFF		/* Received data mask */

/*
 * Receive Status Register / Error Clear Register bits.
 */
#define	PL011_RSR_OE		(1 << 3)
#define	PL011_RSR_BE		(1 << 2)
#define	PL011_RSR_PE		(1 << 1)
#define	PL011_RSR_FE		(1 << 0)

/*
 * Flag Register bits.
 */
#define	PL011_FR_RI		(1 << 8)	/* Ring indicator */
#define	PL011_FR_TXFE		(1 << 7)	/* TX FIFO empty */
#define	PL011_FR_RXFF		(1 << 6)	/* RX FIFO full */
#define	PL011_FR_TXFF		(1 << 5)	/* TX FIFO full */
#define	PL011_FR_RXFE		(1 << 4)	/* RX FIFO empty */
#define	PL011_FR_BUSY		(1 << 3)	/* UART busy transmitting */
#define	PL011_FR_DCD		(1 << 2)	/* Data carrier detect */
#define	PL011_FR_DSR		(1 << 1)	/* Data set ready */
#define	PL011_FR_CTS		(1 << 0)	/* Clear to send */

/*
 * Line Control Register (LCR_H) bits.
 */
#define	PL011_LCR_H_SPS		(1 << 7)	/* Stick parity select */
#define	PL011_LCR_H_WLEN_MASK	(3 << 5)	/* Word length mask */
#define	PL011_LCR_H_WLEN_8	(3 << 5)	/* 8 data bits */
#define	PL011_LCR_H_WLEN_7	(2 << 5)	/* 7 data bits */
#define	PL011_LCR_H_WLEN_6	(1 << 5)	/* 6 data bits */
#define	PL011_LCR_H_WLEN_5	(0 << 5)	/* 5 data bits */
#define	PL011_LCR_H_FEN		(1 << 4)	/* FIFO enable */
#define	PL011_LCR_H_STP2	(1 << 3)	/* Two stop bits select */
#define	PL011_LCR_H_EPS		(1 << 2)	/* Even parity select */
#define	PL011_LCR_H_PEN		(1 << 1)	/* Parity enable */
#define	PL011_LCR_H_BRK		(1 << 0)	/* Send break */

/*
 * Control Register (CR) bits.
 */
#define	PL011_CR_CTSEN		(1 << 15)	/* CTS hardware flow control */
#define	PL011_CR_RTSEN		(1 << 14)	/* RTS hardware flow control */
#define	PL011_CR_OUT2		(1 << 13)	/* Complement of nUARTOut2 */
#define	PL011_CR_OUT1		(1 << 12)	/* Complement of nUARTOut1 */
#define	PL011_CR_RTS		(1 << 11)	/* Request to send */
#define	PL011_CR_DTR		(1 << 10)	/* Data transmit ready */
#define	PL011_CR_RXE		(1 << 9)	/* Receive enable */
#define	PL011_CR_TXE		(1 << 8)	/* Transmit enable */
#define	PL011_CR_LBE		(1 << 7)	/* Loopback enable */
#define	PL011_CR_UARTEN		(1 << 0)	/* UART enable */

/*
 * Interrupt FIFO Level Select (IFLS) bits.
 */
#define	PL011_IFLS_RX_SHIFT	3
#define	PL011_IFLS_TX_SHIFT	0
#define	PL011_IFLS_1_8		0
#define	PL011_IFLS_1_4		1
#define	PL011_IFLS_1_2		2
#define	PL011_IFLS_3_4		3
#define	PL011_IFLS_7_8		4

/*
 * Interrupt bits (shared layout for IMSC, RIS, MIS, ICR).
 */
#define	PL011_INT_OE		(1 << 10)	/* Overrun error */
#define	PL011_INT_BE		(1 << 9)	/* Break error */
#define	PL011_INT_PE		(1 << 8)	/* Parity error */
#define	PL011_INT_FE		(1 << 7)	/* Framing error */
#define	PL011_INT_RT		(1 << 6)	/* Receive timeout */
#define	PL011_INT_TX		(1 << 5)	/* Transmit */
#define	PL011_INT_RX		(1 << 4)	/* Receive */
#define	PL011_INT_DSR		(1 << 3)	/* DSR modem status */
#define	PL011_INT_DCD		(1 << 2)	/* DCD modem status */
#define	PL011_INT_CTS		(1 << 1)	/* CTS modem status */
#define	PL011_INT_RI		(1 << 0)	/* RI modem status */
#define	PL011_INT_ALL		0x7FF
#define	PL011_INT_ERRORS	\
	(PL011_INT_OE | PL011_INT_BE | PL011_INT_PE | PL011_INT_FE)
#define	PL011_INT_MODEM		\
	(PL011_INT_DSR | PL011_INT_DCD | PL011_INT_CTS | PL011_INT_RI)

/*
 * Baud rate table - maps termios Bxxx index to actual baud rate value.
 * Used to calculate PL011 IBRD/FBRD divisors from the UART clock frequency.
 */
static const int pl011_baud_rates[] = {
	[B0] = 0,
	[B50] = 50,
	[B75] = 75,
	[B110] = 110,
	[B134] = 134,
	[B150] = 150,
	[B200] = 200,
	[B300] = 300,
	[B600] = 600,
	[B1200] = 1200,
	[B1800] = 1800,
	[B2400] = 2400,
	[B4800] = 4800,
	[B9600] = 9600,
	[B19200] = 19200,
	[B38400] = 38400,
	[B57600] = 57600,
	[B76800] = 76800,
	[B115200] = 115200,
	[B153600] = 153600,
	[B230400] = 230400,
	[B307200] = 307200,
	[B460800] = 460800,
	[B921600] = 921600,
};

/*
 * Forward declarations.
 */
static uint_t	asy_pl011_intr(caddr_t, caddr_t);
static int	asy_pl011_identify(dev_info_t *, struct asycom *);
static char	*asy_pl011_hw_name(struct asycom *);
static void	asy_pl011_set_baud(struct asycom *, int);
static boolean_t asy_pl011_baudok(struct asycom *);
static void	asy_pl011_wait_baud(struct asycom *);
static void	asy_pl011_set_lcr(struct asycom *, uint8_t);
static void	asy_pl011_set_break(struct asycom *, boolean_t);
static uint8_t	asy_pl011_get_mcr(struct asycom *);
static void	asy_pl011_set_mcr(struct asycom *, uint8_t);
static void	asy_pl011_mcr_set(struct asycom *, uint8_t);
static void	asy_pl011_mcr_clr(struct asycom *, uint8_t);
static uint8_t	asy_pl011_get_msr(struct asycom *);
static uint8_t	asy_pl011_get_lsr(struct asycom *);
static void	asy_pl011_enable_intr(struct asycom *, uint8_t);
static void	asy_pl011_disable_intr(struct asycom *, uint8_t);
static void	asy_pl011_fifo_setup(struct asycom *, uint8_t);
static uint8_t	asy_pl011_get_rx(struct asycom *);
static void	asy_pl011_put_tx(struct asycom *, uint8_t);
static void	asy_pl011_flush_status(struct asycom *);
static void	asy_pl011_rx_drain(struct asycom *);
static void	asy_pl011_polledio_putchar(struct asycom *, uchar_t);
static int	asy_pl011_polledio_getchar(struct asycom *);
static boolean_t asy_pl011_polledio_ischar(struct asycom *);

#pragma weak	plat_clk_get_rate
#pragma weak	plat_clk_get_rate_by_name

extern int	plat_clk_get_rate(dev_info_t *, uint_t);
extern int	plat_clk_get_rate_by_name(dev_info_t *, const char *);

/*
 * Register access helpers
 */

static uint32_t
pl011_reg_read(const struct asycom *asy, uint16_t off)
{
	return (ddi_get32(asy->asy_iohandle,
	    (uint32_t *)(asy->asy_ioaddr + off)));
}

static void
pl011_reg_write(const struct asycom *asy, uint16_t off, uint32_t val)
{
	ddi_put32(asy->asy_iohandle,
	    (uint32_t *)(asy->asy_ioaddr + off), val);
}

/*
 * ASY_IER_* <-> PL011 IMSC translation
 */

static uint16_t
pl011_ier_to_imsc(uint8_t ier)
{
	uint16_t imsc = 0;

	if (ier & ASY_IER_RIEN) {
		imsc |= PL011_INT_RX | PL011_INT_RT;
	}
	if (ier & ASY_IER_TIEN) {
		imsc |= PL011_INT_TX;
	}
	if (ier & ASY_IER_SIEN) {
		imsc |= PL011_INT_ERRORS;
	}
	if (ier & ASY_IER_MIEN) {
		imsc |= PL011_INT_MODEM;
	}

	return (imsc);
}

/*
 * Chip identification
 */

static char *
asy_pl011_hw_name(struct asycom *asy)
{
	if (asy->asy_flags2 & ASY2_SBSA) {
		return ("SBSA Generic UART");
	}

	return ("PL011");
}

/*
 * Snap a firmware-reported baud rate to the nearest standard termios
 * baud rate from pl011_baud_rates[].  If the value already matches a
 * table entry exactly, return it unchanged.  Otherwise return the
 * closest entry, defaulting to 115200 if the table is empty or the
 * value is zero.
 */
static uint32_t
asy_pl011_snap_baud(uint32_t fw_baud)
{
	uint32_t best = 115200;
	uint32_t best_delta = UINT32_MAX;

	for (uint_t i = 1; i < ARRAY_SIZE(pl011_baud_rates); i++) {
		uint32_t rate = (uint32_t)pl011_baud_rates[i];
		uint32_t delta;

		if (rate == 0) {
			continue;
		}

		if (rate == fw_baud) {
			return (fw_baud);
		}

		delta = (fw_baud > rate) ? (fw_baud - rate) : (rate - fw_baud);
		if (delta < best_delta) {
			best_delta = delta;
			best = rate;
		}
	}

	return (best);
}

static int
asy_pl011_identify(dev_info_t *devi, struct asycom *asy)
{
	uint32_t cr;
	uint32_t lcrh;

	/*
	 * For PL011/SBSA, identification is straightforward - we already
	 * know the hardware variant from the compatible string match in
	 * attach.  This function sets up FIFO parameters, reads the clock
	 * frequency, and enables the UART.
	 */
	asy->asy_hwtype = ASY_PL011;
	asy->asy_use_fifo = ASY_FCR_FIFO_EN;
	asy->asy_fifo_buf = 16;
	asy->asy_fifor = 0;

	/*
	 * Discover the UART clock frequency from device properties.
	 * SBSA UARTs don't need this -- firmware owns the baud rate
	 * and ASY2_NOCLK was already set alongside ASY2_SBSA in attach.
	 *
	 * For real PL011 UARTs, if the clock-frequency is not available
	 * (common on ACPI platforms such as Ampere Altra where the
	 * firmware has already configured the baud rate), we set
	 * ASY2_NOCLK to suppress baud rate changes while still allowing
	 * LCR and FIFO configuration.
	 */
	if (!(asy->asy_flags2 & ASY2_SBSA)) {
		int err;
		int clk_freq = 0;

		if (&plat_clk_get_rate_by_name) {
			if ((err = plat_clk_get_rate_by_name(
			    devi, "uartclk")) > 0) {
				clk_freq = err;
			}
		}

		if (clk_freq == 0 && &plat_clk_get_rate) {
			if ((err = plat_clk_get_rate(devi, 0)) > 0) {
				clk_freq = err;
			}
		}

		if (clk_freq == 0) {
			clk_freq = ddi_prop_get_int(DDI_DEV_T_ANY, devi,
			    DDI_PROP_DONTPASS, "clock-frequency", 0);
		}

		if (clk_freq <= 0) {
			asyerror(asy, CE_NOTE,
			    "!PL011: no clock-frequency property; "
			    "baud rate changes will be suppressed");
			asy->asy_flags2 |= ASY2_NOCLK;
		} else {
			asy->asy_clk_freq = (uint32_t)clk_freq;
		}
	}

	/* SBSA always implies NOCLK */
	ASSERT(!(asy->asy_flags2 & ASY2_SBSA) ||
	    (asy->asy_flags2 & ASY2_NOCLK));

	/*
	 * When the baud rate is firmware-owned (ASY2_NOCLK), discover
	 * the fixed baud rate so we can report it truthfully and reject
	 * attempts to change it.
	 *
	 * The "arm,sbsa-uart" bindings specify a u32 "current-speed" property
	 * on the device node.  ACPI systems are expected to synthesize this
	 * property from the SPCR baud rate.
	 *
	 * Default to 115200 if the property is absent or zero.
	 *
	 * The firmware value must correspond to a standard termios baud
	 * rate so that baudok can match it.  If the reported value is
	 * non-standard, snap to the nearest supported rate.
	 */
	if (asy->asy_flags2 & ASY2_NOCLK) {
		int fw_baud;

		fw_baud = ddi_prop_get_int(DDI_DEV_T_ANY, devi,
		    DDI_PROP_DONTPASS, "current-speed", 0);
		if (fw_baud <= 0) {
			fw_baud = 115200;
			dev_err(devi, CE_CONT, "?%s: current-speed is not set, "
			    "using %d\n", asy_pl011_hw_name(asy), fw_baud);
		}
		asy->asy_fw_baud = asy_pl011_snap_baud((uint32_t)fw_baud);

		if (asy->asy_fw_baud != (uint32_t)fw_baud) {
			dev_err(devi, CE_WARN, "%s: current-speed %d "
			    "is not a standard baud rate, using %u",
			    asy_pl011_hw_name(asy), fw_baud,
			    asy->asy_fw_baud);
		}

		ASY_DPRINTF(asy, ASY_DEBUG_CHIP,
		    "%s: firmware baud rate %u",
		    asy_pl011_hw_name(asy), asy->asy_fw_baud);
	}

	/* Disable all interrupts during setup */
	pl011_reg_write(asy, PL011_IMSC, 0);

	/* Clear all pending interrupts */
	pl011_reg_write(asy, PL011_ICR, PL011_INT_ALL);

	/* Clear sticky error bits */
	pl011_reg_write(asy, PL011_RSR, 0);

	/* Enable FIFO */
	lcrh = pl011_reg_read(asy, PL011_LCR_H);
	lcrh |= PL011_LCR_H_FEN;
	pl011_reg_write(asy, PL011_LCR_H, lcrh);

	/* Set FIFO interrupt trigger levels to 1/2 full */
	pl011_reg_write(asy, PL011_IFLS,
	    (PL011_IFLS_1_2 << PL011_IFLS_RX_SHIFT) |
	    (PL011_IFLS_1_2 << PL011_IFLS_TX_SHIFT));

	/* Enable UART: UARTEN + TXE + RXE */
	cr = pl011_reg_read(asy, PL011_CR);
	cr |= PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE;
	pl011_reg_write(asy, PL011_CR, cr);

	ASY_DPRINTF(asy, ASY_DEBUG_CHIP,
	    "%s identified, clock=%u Hz",
	    asy_pl011_hw_name(asy), asy->asy_clk_freq);

	/*
	 * RX drain guard.  The PL011 FIFO is at most 32 deep (r1p5);
	 * 256 provides generous margin for bytes arriving mid-drain
	 * without being so large that a wedged UART holds the CPU forever.
	 */
	asy->asy_rx_drain_limit = 256;

	return (DDI_SUCCESS);
}

/*
 * Baud rate
 */

/*
 * Set baud rate using the PL011 IBRD/FBRD integer/fractional divisor pair.
 *
 *   BRD = UART_CLK / (16 × baud_rate)
 *   IBRD = integer part of BRD
 *   FBRD = round(fractional part of BRD × 64)
 *
 * To avoid floating point in the kernel we compute:
 *   IBRD = clk / (16 * baud)
 *   remainder = clk % (16 * baud)
 *   FBRD = (remainder * 4 + baud / 2) / baud   [with rounding]
 */
static void
asy_pl011_set_baud(struct asycom *asy, int baudrate)
{
	uint32_t baud, ibrd, fbrd, remainder, divisor;
	uint32_t cr, lcrh;
	int tries;

	/* No clock frequency available -- cannot compute divisors */
	if (asy->asy_flags2 & ASY2_NOCLK) {
		return;
	}

	ASSERT(asy->asy_clk_freq != 0);

	if (baudrate == 0) {
		return;
	}

	if (baudrate < 0 ||
	    baudrate >= (int)(sizeof (pl011_baud_rates) /
	    sizeof (pl011_baud_rates[0]))) {
		return;
	}

	baud = pl011_baud_rates[baudrate];
	if (baud == 0) {
		return;
	}

	/*
	 * The PL011 TRM (DDI0183G §3.3.8, UARTCR) requires the UART to be
	 * disabled before control registers are reprogrammed:
	 *
	 *   1. Disable the UART.
	 *   2. Wait for the end of transmission or reception of the
	 *      current character.
	 *   3. Flush the transmit FIFO by clearing FEN in UARTLCR_H.
	 *   4. Reprogram the control registers.
	 *   5. Enable the UART.
	 *
	 * We wait for the transmitter to go fully idle (FR.TXFE && !FR.BUSY)
	 * before disabling, rather than after, to avoid losing any data
	 * still in the TX FIFO or shift register.  The FIFO flush step is
	 * therefore unnecessary -- the TX FIFO is confirmed empty.
	 *
	 * Both asy_excl and asy_excl_hi are held by our caller
	 * (asy_program), which locks out the interrupt handler and TX
	 * softint path -- no new data can enter the FIFO while we drain
	 * and reprogram.
	 */
	ASSERT(mutex_owned(&asy->asy_excl));
	ASSERT(mutex_owned(&asy->asy_excl_hi));

	/* Wait for TX FIFO empty and shift register idle. */
	for (tries = 20000; tries > 0; tries--) {
		uint32_t fr = pl011_reg_read(asy, PL011_FR);
		if ((fr & PL011_FR_TXFE) && !(fr & PL011_FR_BUSY)) {
			break;
		}
		drv_usecwait(10);
	}

	/* Save CR and disable UART, preserving all other CR bits. */
	cr = pl011_reg_read(asy, PL011_CR);
	pl011_reg_write(asy, PL011_CR, cr & ~PL011_CR_UARTEN);

	/* Save LCR_H -- includes framing set by aho_set_lcr(). */
	lcrh = pl011_reg_read(asy, PL011_LCR_H);

	/* Reprogram baud rate divisors. */
	divisor = 16 * baud;
	ibrd = asy->asy_clk_freq / divisor;
	remainder = asy->asy_clk_freq % divisor;
	fbrd = (remainder * 4 + baud / 2) / baud;
	if (fbrd > 63) {
		fbrd = 0;
		ibrd++;
	}

	pl011_reg_write(asy, PL011_IBRD, ibrd);
	pl011_reg_write(asy, PL011_FBRD, fbrd);

	/*
	 * Write LCR_H to latch the new divisor into the baud rate generator.
	 * This also commits any framing changes from aho_set_lcr().
	 */
	pl011_reg_write(asy, PL011_LCR_H, lcrh);

	/* Restore CR, re-enabling UART with original flags intact. */
	pl011_reg_write(asy, PL011_CR, cr);

	ASY_DPRINTF(asy, ASY_DEBUG_CHIP,
	    "set baud %u: IBRD=%u FBRD=%u (clk=%u)",
	    baud, ibrd, fbrd, asy->asy_clk_freq);
}

static boolean_t
asy_pl011_baudok(struct asycom *asy)
{
	struct asyncline *async = asy->asy_priv;
	ASSERT3P(async, !=, NULL);

	/*
	 * If no clock frequency is available (SBSA or PL011 on ACPI
	 * without clock-frequency property), the baud rate is fixed by
	 * firmware.  Only accept requests that match the firmware-
	 * configured baud rate; reject everything else so the termios
	 * state never lies about what the hardware is actually doing.
	 */
	if (asy->asy_flags2 & ASY2_NOCLK) {
		int bidx;
		uint32_t requested;

		bidx = BAUDINDEX(async->async_ttycommon.t_cflag);
		if (bidx <= 0 ||
		    bidx >= (int)ARRAY_SIZE(pl011_baud_rates)) {
			return (B_FALSE);
		}

		requested = pl011_baud_rates[bidx];
		return (requested == asy->asy_fw_baud);
	}

	/* PL011 with clock can generate any standard baud rate */
	return (B_TRUE);
}

static void
asy_pl011_wait_baud(struct asycom *asy)
{
	int tries;

	ASSERT(mutex_owned(&asy->asy_excl));
	ASSERT(mutex_owned(&asy->asy_excl_hi));

	/*
	 * PL011 baud rate takes effect immediately, but wait for any
	 * in-flight transmission to complete.  At 50 baud (lowest rate)
	 * one character is ~200ms; 20000 iterations at 10µs = 200ms.
	 */
	for (tries = 20000; tries > 0; tries--) {
		if (!(pl011_reg_read(asy, PL011_FR) & PL011_FR_BUSY)) {
			break;
		}

		mutex_exit(&asy->asy_excl_hi);
		mutex_exit(&asy->asy_excl);
		drv_usecwait(10);
		mutex_enter(&asy->asy_excl);
		mutex_enter(&asy->asy_excl_hi);
	}
}

/*
 * Line control
 */

/*
 * Translate ASY_LCR_* semantic bits to PL011 LCR_H register value.
 * Preserves FEN (FIFO enable) and BRK (break) bits.
 *
 * Note: this writes LCR_H while the UART may still be enabled.  All
 * callers (asy_program, attach) immediately follow with aho_set_baud(),
 * which performs the full TRM-mandated disable-reprogram-enable sequence
 * and re-commits LCR_H with the UART disabled.  The intervening window
 * is safe because asy_excl_hi is held, locking out interrupt-driven I/O.
 */
static void
asy_pl011_set_lcr(struct asycom *asy, uint8_t lcr)
{
	uint32_t lcrh;

	/* SBSA: firmware owns line control */
	if (asy->asy_flags2 & ASY2_SBSA) {
		return;
	}

	lcrh = pl011_reg_read(asy, PL011_LCR_H);

	/* Clear word length, stop, parity bits; preserve FEN and BRK */
	lcrh &= (PL011_LCR_H_FEN | PL011_LCR_H_BRK);

	/* Word length */
	switch (lcr & (ASY_LCR_WLS0 | ASY_LCR_WLS1)) {
	case ASY_LCR_BITS5:
		lcrh |= PL011_LCR_H_WLEN_5;
		break;
	case ASY_LCR_BITS6:
		lcrh |= PL011_LCR_H_WLEN_6;
		break;
	case ASY_LCR_BITS7:
		lcrh |= PL011_LCR_H_WLEN_7;
		break;
	case ASY_LCR_BITS8:
		lcrh |= PL011_LCR_H_WLEN_8;
		break;
	}

	/* Stop bits */
	if (lcr & ASY_LCR_STB) {
		lcrh |= PL011_LCR_H_STP2;
	}

	/* Parity */
	if (lcr & ASY_LCR_PEN) {
		lcrh |= PL011_LCR_H_PEN;
	}
	if (lcr & ASY_LCR_EPS) {
		lcrh |= PL011_LCR_H_EPS;
	}

	pl011_reg_write(asy, PL011_LCR_H, lcrh);
}

/*
 * Break control
 */

static void
asy_pl011_set_break(struct asycom *asy, boolean_t on)
{
	uint32_t lcrh;

	lcrh = pl011_reg_read(asy, PL011_LCR_H);
	if (on) {
		lcrh |= PL011_LCR_H_BRK;
	} else {
		lcrh &= ~PL011_LCR_H_BRK;
	}
	pl011_reg_write(asy, PL011_LCR_H, lcrh);
}

/*
 * Modem control (ASY_MCR_* <-> PL011 CR)
 */

static uint8_t
asy_pl011_get_mcr(struct asycom *asy)
{
	uint32_t cr = pl011_reg_read(asy, PL011_CR);
	uint8_t mcr = 0;

	if (cr & PL011_CR_DTR) {
		mcr |= ASY_MCR_DTR;
	}
	if (cr & PL011_CR_RTS) {
		mcr |= ASY_MCR_RTS;
	}
	if (cr & PL011_CR_OUT1) {
		mcr |= ASY_MCR_OUT1;
	}
	if (cr & PL011_CR_OUT2) {
		mcr |= ASY_MCR_OUT2;
	}
	if (cr & PL011_CR_LBE) {
		mcr |= ASY_MCR_LOOP;
	}

	return (mcr);
}

static void
asy_pl011_set_mcr(struct asycom *asy, uint8_t mcr)
{
	uint32_t cr = pl011_reg_read(asy, PL011_CR);

	/* Preserve UART enable, TX/RX enable, and HW flow control bits */
	cr &= (PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE |
	    PL011_CR_CTSEN | PL011_CR_RTSEN);

	if (mcr & ASY_MCR_DTR) {
		cr |= PL011_CR_DTR;
	}
	if (mcr & ASY_MCR_RTS) {
		cr |= PL011_CR_RTS;
	}
	if (mcr & ASY_MCR_OUT1) {
		cr |= PL011_CR_OUT1;
	}
	if (mcr & ASY_MCR_OUT2) {
		cr |= PL011_CR_OUT2;
	}
	if (mcr & ASY_MCR_LOOP) {
		cr |= PL011_CR_LBE;
	}

	pl011_reg_write(asy, PL011_CR, cr);
}

static void
asy_pl011_mcr_set(struct asycom *asy, uint8_t bits)
{
	uint8_t mcr = asy_pl011_get_mcr(asy);

	asy_pl011_set_mcr(asy, mcr | bits);
}

static void
asy_pl011_mcr_clr(struct asycom *asy, uint8_t bits)
{
	uint8_t mcr = asy_pl011_get_mcr(asy);

	asy_pl011_set_mcr(asy, mcr & ~bits);
}

/*
 * Modem status (PL011 FR -> ASY_MSR_*)
 */

/*
 * Read modem status and synthesize delta bits.
 *
 * The 16550 provides hardware delta bits (DCTS/DDSR/TERI/DDCD) in the MSR.
 * The PL011 does not - modem line state is read from the FR (flag register)
 * and delta bits must be synthesized by comparing against the cached value
 * in asy->asy_msr.
 */
static uint8_t
asy_pl011_get_msr(struct asycom *asy)
{
	uint32_t fr = pl011_reg_read(asy, PL011_FR);
	uint8_t msr = 0;
	uint8_t old_msr = asy->asy_msr;

	/* Current modem line state */
	if (fr & PL011_FR_CTS) {
		msr |= ASY_MSR_CTS;
	}
	if (fr & PL011_FR_DSR) {
		msr |= ASY_MSR_DSR;
	}
	if (fr & PL011_FR_RI) {
		msr |= ASY_MSR_RI;
	}
	if (fr & PL011_FR_DCD) {
		msr |= ASY_MSR_DCD;
	}

	/* Synthesize delta bits from cached state */
	if ((msr & ASY_MSR_CTS) != (old_msr & ASY_MSR_CTS)) {
		msr |= ASY_MSR_DCTS;
	}
	if ((msr & ASY_MSR_DSR) != (old_msr & ASY_MSR_DSR)) {
		msr |= ASY_MSR_DDSR;
	}
	if ((msr & ASY_MSR_DCD) != (old_msr & ASY_MSR_DCD)) {
		msr |= ASY_MSR_DDCD;
	}
	/* RI trailing edge: was asserted, now deasserted */
	if ((old_msr & ASY_MSR_RI) && !(msr & ASY_MSR_RI)) {
		msr |= ASY_MSR_TERI;
	}

	return (msr);
}

/*
 * Line status (PL011 FR + RSR -> ASY_LSR_*)
 */

static uint8_t
asy_pl011_get_lsr(struct asycom *asy)
{
	uint32_t fr = pl011_reg_read(asy, PL011_FR);
	uint32_t rsr;
	uint8_t lsr = 0;

	/* TX FIFO empty -> THRE; transmitter fully idle (TSR empty) -> TEMT */
	if (fr & PL011_FR_TXFE) {
		lsr |= ASY_LSR_THRE;
		/*
		 * TEMT means the holding register AND the shift register are
		 * empty.  FR.TXFE only tells us the FIFO drained; the last
		 * character may still be clocking out on the wire.  Gate TEMT
		 * on !BUSY so drain-on-close and suspend (which wait on TEMT)
		 * do not truncate the final character.
		 */
		if (!(fr & PL011_FR_BUSY))
			lsr |= ASY_LSR_TEMT;
	} else if (!(fr & PL011_FR_TXFF)) {
		/* TX FIFO not full -> THRE (space available) */
		lsr |= ASY_LSR_THRE;
	}

	/* RX FIFO not empty -> DR (data ready) */
	if (!(fr & PL011_FR_RXFE)) {
		lsr |= ASY_LSR_DR;
	}

	/*
	 * Error bits from RSR.  The RSR is sticky - it accumulates errors
	 * until explicitly cleared by writing to ECR (same address).
	 * Read and clear so error reporting works per-poll-cycle.
	 */
	rsr = pl011_reg_read(asy, PL011_RSR);
	if (rsr & PL011_RSR_OE) {
		lsr |= ASY_LSR_OE;
	}
	if (rsr & PL011_RSR_PE) {
		lsr |= ASY_LSR_PE;
	}
	if (rsr & PL011_RSR_FE) {
		lsr |= ASY_LSR_FE;
	}
	if (rsr & PL011_RSR_BE) {
		lsr |= ASY_LSR_BI;
	}
	if (rsr != 0) {
		pl011_reg_write(asy, PL011_RSR, 0);	/* ECR: clear errors */
	}

	return (lsr);
}

/*
 * Interrupt enable/disable
 */

static void
asy_pl011_enable_intr(struct asycom *asy, uint8_t ier)
{
	uint32_t imsc = pl011_reg_read(asy, PL011_IMSC);

	imsc |= pl011_ier_to_imsc(ier);
	pl011_reg_write(asy, PL011_IMSC, imsc);
}

static void
asy_pl011_disable_intr(struct asycom *asy, uint8_t ier)
{
	uint32_t imsc = pl011_reg_read(asy, PL011_IMSC);

	imsc &= ~pl011_ier_to_imsc(ier);
	pl011_reg_write(asy, PL011_IMSC, imsc);
}

/*
 * FIFO setup/flush
 */

/*
 * PL011 FIFO flush.
 *
 * Unlike the 16550 which has explicit flush bits in FCR, the PL011 has a
 * single shared FIFO pair that can only be flushed by toggling LCR_H.FEN
 * (clearing FEN destroys the contents of both FIFOs simultaneously).
 *
 * To avoid collateral damage when only one direction is being flushed, we
 * use three strategies depending on the requested flush flags:
 *
 *   RX-only (ASY_FCR_RHR_FL):  Drain the RX FIFO by reading DR until
 *       FR.RXFE without toggling FEN.  TX FIFO is preserved.
 *
 *   TX-only (ASY_FCR_THR_FL):  First consume any pending RX data into
 *       the ring buffer via async_rxint() so it is not lost, then toggle
 *       FEN to flush both hardware FIFOs.  The RX data has already been
 *       delivered through the normal receive path (flow control, error
 *       handling, soft interrupt scheduling).
 *
 *   Both (ASY_FCR_RHR_FL | ASY_FCR_THR_FL):  Toggle FEN.  Both sides
 *       are being discarded intentionally.
 *
 * All callers hold asy_excl_hi (or are in quiesce context where no
 * concurrency exists), satisfying the locking requirements of both the
 * register accesses and async_rxint().
 */
static void
asy_pl011_fifo_setup(struct asycom *asy, uint8_t flush)
{
	uint32_t lcrh;

	if (asy->asy_flags2 & ASY2_SBSA) {
		return;
	}

	if ((flush & ASY_FCR_RHR_FL) && !(flush & ASY_FCR_THR_FL)) {
		/*
		 * RX-only flush: drain the receive FIFO by reading DR.
		 * No FEN toggle - the TX FIFO is preserved.
		 */
		asy_pl011_rx_drain(asy);
		return;
	}

	if ((flush & ASY_FCR_THR_FL) && !(flush & ASY_FCR_RHR_FL)) {
		/*
		 * TX-only flush: consume any pending RX data into the ring
		 * buffer before the FEN toggle destroys it.  async_rxint()
		 * processes the data through the normal receive path -
		 * XON/XOFF, error handling, PARMRK, abort detection, and
		 * soft interrupt scheduling all apply.
		 */
		async_rxint(asy, asy_pl011_get_lsr(asy));
	}

	/*
	 * Both flags, or TX-only (RX already consumed above): toggle FEN
	 * to flush the hardware FIFOs, then re-enable if appropriate.
	 */
	lcrh = pl011_reg_read(asy, PL011_LCR_H);
	pl011_reg_write(asy, PL011_LCR_H, lcrh & ~PL011_LCR_H_FEN);

	if (asy->asy_use_fifo == ASY_FCR_FIFO_EN) {
		pl011_reg_write(asy, PL011_IFLS,
		    (PL011_IFLS_1_2 << PL011_IFLS_RX_SHIFT) |
		    (PL011_IFLS_1_2 << PL011_IFLS_TX_SHIFT));
		pl011_reg_write(asy, PL011_LCR_H, lcrh | PL011_LCR_H_FEN);
	}
}

/*
 * Receive/transmit data
 */

static uint8_t
asy_pl011_get_rx(struct asycom *asy)
{
	uint32_t dr = pl011_reg_read(asy, PL011_DR);

	/*
	 * Per-character error bits are in DR[11:8].  These are also
	 * accumulated in RSR for bulk error reporting via get_lsr().
	 * Return only the data byte.
	 */
	return ((uint8_t)(dr & PL011_DR_DATA));
}

static void
asy_pl011_put_tx(struct asycom *asy, uint8_t c)
{
	pl011_reg_write(asy, PL011_DR, (uint32_t)c);
}

/*
 * Status flush
 */

static void
asy_pl011_flush_status(struct asycom *asy)
{
	/* Clear all pending interrupts */
	pl011_reg_write(asy, PL011_ICR, PL011_INT_ALL);

	/* Clear sticky error bits in RSR/ECR */
	pl011_reg_write(asy, PL011_RSR, 0);
}

/*
 * Interrupt service routine
 */

static uint_t
asy_pl011_intr(caddr_t argasy, caddr_t argunused __unused)
{
	struct asycom *asy = (struct asycom *)argasy;
	struct asyncline *async;
	int ret_status = DDI_INTR_UNCLAIMED;
	uint32_t mis;

	mutex_enter(&asy->asy_excl_hi);
	async = asy->asy_priv;

	if (async == NULL ||
	    (async->async_flags & (ASYNC_ISOPEN | ASYNC_WOPEN)) == 0) {
		mis = pl011_reg_read(asy, PL011_MIS);

		ASY_DPRINTF(asy, ASY_DEBUG_INTR,
		    "not open, MIS=0x%x", mis);

		if (mis != 0) {
			/* Clear all pending interrupts and drain RX */
			pl011_reg_write(asy, PL011_ICR, mis);
			asy_pl011_rx_drain(asy);
			asy->asy_msr = asy_pl011_get_msr(asy);
			ret_status = DDI_INTR_CLAIMED;
		}
		mutex_exit(&asy->asy_excl_hi);
		return (ret_status);
	}

	/*
	 * Before this flag was set, interrupts were disabled.  We may still
	 * get here if asy_pl011_intr() waited on the mutex.
	 */
	if (asy->asy_flags & ASY_DDI_SUSPENDED) {
		mutex_exit(&asy->asy_excl_hi);
		return (DDI_INTR_CLAIMED);
	}

	mis = pl011_reg_read(asy, PL011_MIS);
	if (mis == 0) {
		mutex_exit(&asy->asy_excl_hi);
		return (DDI_INTR_UNCLAIMED);
	}

	ret_status = DDI_INTR_CLAIMED;

	/* Clear the interrupts we're about to handle */
	pl011_reg_write(asy, PL011_ICR, mis);

	/* Receive interrupt, receive timeout, or receive errors */
	if (mis & (PL011_INT_RX | PL011_INT_RT | PL011_INT_ERRORS)) {
		uint8_t lsr = asy_pl011_get_lsr(asy);
		async_rxint(asy, lsr);
	}

	/* Transmit interrupt */
	if (mis & PL011_INT_TX) {
		async_txint(asy);
	}

	/* Modem status change */
	if (mis & PL011_INT_MODEM) {
		async_msint(asy);
	}

	/*
	 * Opportunistic TX refill: if we have pending data, let
	 * async_txint() attempt to fill.  asy_pl011_tx_fill() handles
	 * the full-FIFO case internally via FR.TXFF.
	 */
	if ((async->async_flags & ASYNC_BUSY) && async->async_ocnt > 0) {
		async_txint(asy);
	}

	mutex_exit(&asy->asy_excl_hi);
	return (ret_status);
}

/*
 * Polled console I/O
 */

static void
asy_pl011_polledio_putchar(struct asycom *asy, uchar_t c)
{
	if (c == '\n') {
		asy_pl011_polledio_putchar(asy, '\r');
	}

	/* Wait for TX FIFO space */
	while (pl011_reg_read(asy, PL011_FR) & PL011_FR_TXFF) {
		;
	}

	pl011_reg_write(asy, PL011_DR, (uint32_t)c);
}

static int
asy_pl011_polledio_getchar(struct asycom *asy)
{
	/* Wait for data available */
	while (pl011_reg_read(asy, PL011_FR) & PL011_FR_RXFE) {
		;
	}
	return ((int)(pl011_reg_read(asy, PL011_DR) & PL011_DR_DATA));
}

static boolean_t
asy_pl011_polledio_ischar(struct asycom *asy)
{
	return (!(pl011_reg_read(asy, PL011_FR) & PL011_FR_RXFE));
}

/*
 * Fill the TX FIFO from buf, writing until the pending data is exhausted
 * or FR.TXFF indicates the FIFO is full -- whichever comes first.
 *
 * Unlike the 16550, the PL011 exposes a real-time TX-full predicate
 * (FR.TXFF) so we never need to count FIFO depth.  Ground truth replaces
 * arithmetic.  No asy_max_tx_fifo policy cap is applied: when TXFF is
 * the stop condition, a static cap is vestigial.
 *
 * The `reserve` parameter (holding one FIFO slot for a software flow-
 * control character) is not implemented here.  On the 16550 the reserve
 * matters because THRE fires only when the FIFO is completely empty, so
 * filling every slot defers the XON/XOFF byte by an entire drain cycle.
 * On the PL011 the TX interrupt fires at the half-watermark transition,
 * providing fresh FIFO space well before drain completes.
 */
static uint_t
asy_pl011_tx_fill(struct asycom *asy, uchar_t *buf, uint_t ocnt,
    uint_t reserve __unused, boolean_t *space_left)
{
	uint_t n = 0;

	while (n < ocnt &&
	    !(pl011_reg_read(asy, PL011_FR) & PL011_FR_TXFF)) {
		asy->asy_hw->aho_put_tx(asy, buf[n]);
		n++;
	}

	/* FR.TXFF is real-time ground truth for the asysetsoft gate */
	*space_left = !(pl011_reg_read(asy, PL011_FR) & PL011_FR_TXFF);

	return (n);
}

/*
 * Initial TX fill for async_start: fill the transmitter FIFO for a
 * new message.
 *
 * On the PL011, FR.TXFF provides real-time ground truth, so the start
 * path fills until full -- identical to the interrupt refill path.
 * Factors through asy_pl011_tx_fill with a throwaway space_left.
 */
static uint_t
asy_pl011_tx_start(struct asycom *asy, uchar_t *buf, uint_t ocnt,
    uint_t reserve)
{
	boolean_t space_left;

	return (asy_pl011_tx_fill(asy, buf, ocnt, reserve, &space_left));
}

/*
 * Drain the RX FIFO by reading DR until FR.RXFE reports empty, with a
 * runaway guard.  Unlike the 16550, the PL011 FIFO depth may not match
 * asy_fifo_buf (which reflects the programmed watermark, not the
 * hardware capacity), and on a generic SBSA UART the depth is
 * unreadable.  FR.RXFE is ground truth and is in the BSA mandated
 * register subset.
 */
static void
asy_pl011_rx_drain(struct asycom *asy)
{
	uint_t limit = asy->asy_rx_drain_limit;
	uint_t n = 0;

	while (!(pl011_reg_read(asy, PL011_FR) & PL011_FR_RXFE)) {
		(void) pl011_reg_read(asy, PL011_DR);
		if (++n >= limit) {
			ASY_DPRINTF(asy, ASY_DEBUG_INIT,
			    "RX drain guard tripped after %u reads", n);
			break;
		}
	}
}

/*
 * Hardware operations table
 */

const asy_hw_ops_t asy_pl011_ops = {
	.aho_intr		= asy_pl011_intr,
	.aho_identify		= asy_pl011_identify,
	.aho_hw_name		= asy_pl011_hw_name,
	.aho_set_baud		= asy_pl011_set_baud,
	.aho_baudok		= asy_pl011_baudok,
	.aho_wait_baud		= asy_pl011_wait_baud,
	.aho_set_lcr		= asy_pl011_set_lcr,
	.aho_set_break		= asy_pl011_set_break,
	.aho_get_mcr		= asy_pl011_get_mcr,
	.aho_set_mcr		= asy_pl011_set_mcr,
	.aho_mcr_set		= asy_pl011_mcr_set,
	.aho_mcr_clr		= asy_pl011_mcr_clr,
	.aho_get_msr		= asy_pl011_get_msr,
	.aho_get_lsr		= asy_pl011_get_lsr,
	.aho_enable_intr	= asy_pl011_enable_intr,
	.aho_disable_intr	= asy_pl011_disable_intr,
	.aho_fifo_setup		= asy_pl011_fifo_setup,
	.aho_get_rx		= asy_pl011_get_rx,
	.aho_put_tx		= asy_pl011_put_tx,
	.aho_tx_fill		= asy_pl011_tx_fill,
	.aho_tx_start		= asy_pl011_tx_start,
	.aho_rx_drain		= asy_pl011_rx_drain,
	.aho_flush_status	= asy_pl011_flush_status,
	.aho_polledio_putchar	= asy_pl011_polledio_putchar,
	.aho_polledio_getchar	= asy_pl011_polledio_getchar,
	.aho_polledio_ischar	= asy_pl011_polledio_ischar,
};

#endif /* __aarch64__ */
