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
 * Copyright 2026 Oxide Computer Company
 */

/*
 * 16550-family UART hardware operations for the asy(4D) serial driver.
 *
 * This file contains all register access, chip identification, baud rate
 * programming, interrupt handling, and polled console I/O for the
 * 8250/16450/16550A/16650/16750/16950 family of UARTs.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stream.h>
#include <sys/termio.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/note.h>
#include <sys/pci.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/consdev.h>
#include <sys/asy.h>


/* Forward declarations */
static void	asy_put(const struct asycom *, asy_reg_t, uint8_t);
static uint8_t	asy_get(const struct asycom *, asy_reg_t);
static void	asy_set(const struct asycom *, asy_reg_t, uint8_t);
static void	asy_clr(const struct asycom *, asy_reg_t, uint8_t);
static void	asy_put_reg(const struct asycom *, asy_reg_t, uint8_t);
static uint8_t	asy_get_reg(const struct asycom *, asy_reg_t);
static void	asy_put_ext(const struct asycom *, asy_reg_t, uint8_t);
static uint8_t	asy_get_ext(const struct asycom *, asy_reg_t);
static void	asy_put_idx(const struct asycom *, asy_reg_t, uint8_t);
static uint8_t	asy_get_idx(const struct asycom *, asy_reg_t);
static void	asy_put_add(const struct asycom *, asy_reg_t, uint8_t);
static uint8_t	asy_get_add(const struct asycom *, asy_reg_t);

static void	asy_enable_interrupts(struct asycom *, uint8_t);
static void	asy_disable_interrupts(struct asycom *, uint8_t);
static void	asy_set_baudrate(struct asycom *, int);
static void	asy_wait_baudrate(struct asycom *);
static void	asy_reset_fifo(struct asycom *, uchar_t);
static boolean_t	asy_baudok(struct asycom *);
static char	*asy_hw_name(struct asycom *);


/*
 * Baud rate table. Indexed by #defines found in sys/termios.h
 *
 * The default crystal frequency is 1.8432 MHz. The 8250A used a fixed /16
 * prescaler and a 16bit divisor, split in two registers (DLH and DLL).
 *
 * The 16950 adds TCR and CKS registers. The TCR can be used to set the
 * prescaler from /4 to /16. The CKS can be used, among other things, to
 * select a isochronous 1x mode, effectively disabling the prescaler.
 * This would theoretically allow a baud rate of 1843200 driven directly
 * by the default crystal frequency, although the highest termios.h-defined
 * baud rate we can support is half of that, 921600 baud.
 */
#define	UNSUPPORTED	0x00, 0x00, 0x00
static struct {
	uint8_t asy_dlh;
	uint8_t asy_dll;
	uint8_t asy_tcr;
} asy_baud_tab[] = {
	[B0] =		{ UNSUPPORTED },	/* 0 baud */
	[B50] =		{ 0x09, 0x00, 0x00 },	/* 50 baud */
	[B75] =		{ 0x06, 0x00, 0x00 },	/* 75 baud */
	[B110] =	{ 0x04, 0x17, 0x00 },	/* 110 baud (0.026% error) */
	[B134] =	{ 0x03, 0x59, 0x00 },	/* 134 baud (0.058% error) */
	[B150] =	{ 0x03, 0x00, 0x00 },	/* 150 baud */
	[B200] =	{ 0x02, 0x40, 0x00 },	/* 200 baud */
	[B300] =	{ 0x01, 0x80, 0x00 },	/* 300 baud */
	[B600] =	{ 0x00, 0xc0, 0x00 },	/* 600 baud */
	[B1200] =	{ 0x00, 0x60, 0x00 },	/* 1200 baud */
	[B1800] =	{ 0x00, 0x40, 0x00 },	/* 1800 baud */
	[B2400] =	{ 0x00, 0x30, 0x00 },	/* 2400 baud */
	[B4800] =	{ 0x00, 0x18, 0x00 },	/* 4800 baud */
	[B9600] =	{ 0x00, 0x0c, 0x00 },	/* 9600 baud */
	[B19200] =	{ 0x00, 0x06, 0x00 },	/* 19200 baud */
	[B38400] =	{ 0x00, 0x03, 0x00 },	/* 38400 baud */
	[B57600] =	{ 0x00, 0x02, 0x00 },	/* 57600 baud */
	[B76800] =	{ 0x00, 0x06, 0x04 },	/* 76800 baud (16950) */
	[B115200] =	{ 0x00, 0x01, 0x00 },	/* 115200 baud */
	[B153600] =	{ 0x00, 0x03, 0x04 },	/* 153600 baud (16950) */
	[B230400] =	{ 0x00, 0x02, 0x04 },	/* 230400 baud (16950) */
	[B307200] =	{ 0x00, 0x01, 0x06 },	/* 307200 baud (16950) */
	[B460800] =	{ 0x00, 0x01, 0x04 },	/* 460800 baud (16950) */
	[B921600] =	{ 0x00, 0x02, 0x01 },	/* 921600 baud (16950) */
	[B1000000] =	{ UNSUPPORTED },	/* 1000000 baud */
	[B1152000] =	{ UNSUPPORTED },	/* 1152000 baud */
	[B1500000] =	{ UNSUPPORTED },	/* 1500000 baud */
	[B2000000] =	{ UNSUPPORTED },	/* 2000000 baud */
	[B2500000] =	{ UNSUPPORTED },	/* 2500000 baud */
	[B3000000] =	{ UNSUPPORTED },	/* 3000000 baud */
	[B3500000] =	{ UNSUPPORTED },	/* 3500000 baud */
	[B4000000] =	{ UNSUPPORTED },	/* 4000000 baud */
};

/*
 * Register table. For each logical register, we define the minimum hwtype, the
 * register offset, and function pointers for reading and writing the register.
 * A NULL pointer indicates the register cannot be read from or written to,

 * respectively.
 */
static struct {
	int asy_min_hwtype;
	int8_t asy_reg_off;
	uint8_t (*asy_get_reg)(const struct asycom *, asy_reg_t);
	void (*asy_put_reg)(const struct asycom *, asy_reg_t, uint8_t);
} asy_reg_table[] = {
	[ASY_ILLEGAL] = { 0, -1, NULL, NULL },
	/* 8250 / 16450 / 16550 registers */
	[ASY_THR] =   { ASY_8250A,  0, NULL,	    asy_put_reg },
	[ASY_RHR] =   { ASY_8250A,  0, asy_get_reg, NULL },
	[ASY_IER] =   { ASY_8250A,  1, asy_get_reg, asy_put_reg },
	[ASY_FCR] =   { ASY_16550,  2, NULL,	    asy_put_reg },
	[ASY_ISR] =   { ASY_8250A,  2, asy_get_reg, NULL },
	[ASY_LCR] =   { ASY_8250A,  3, asy_get_reg, asy_put_reg },
	[ASY_MCR] =   { ASY_8250A,  4, asy_get_reg, asy_put_reg },
	[ASY_LSR] =   { ASY_8250A,  5, asy_get_reg, NULL },
	[ASY_MSR] =   { ASY_8250A,  6, asy_get_reg, NULL },
	[ASY_SPR] =   { ASY_8250A,  7, asy_get_reg, asy_put_reg },
	[ASY_DLL] =   { ASY_8250A,  0, asy_get_reg, asy_put_reg },
	[ASY_DLH] =   { ASY_8250A,  1, asy_get_reg, asy_put_reg },
	/* 16750 extended register */
	[ASY_EFR] =   { ASY_16750,  2, asy_get_ext, asy_put_ext },
	/* 16650 extended registers */
	[ASY_XON1] =  { ASY_16650,  4, asy_get_ext, asy_put_ext },
	[ASY_XON2] =  { ASY_16650,  5, asy_get_ext, asy_put_ext },
	[ASY_XOFF1] = { ASY_16650,  6, asy_get_ext, asy_put_ext },
	[ASY_XOFF2] = { ASY_16650,  7, asy_get_ext, asy_put_ext },
	/* 16950 additional registers */
	[ASY_ASR] =   { ASY_16950,  1, asy_get_add, asy_put_add },
	[ASY_RFL] =   { ASY_16950,  3, asy_get_add, NULL },
	[ASY_TFL] =   { ASY_16950,  4, asy_get_add, NULL },
	[ASY_ICR] =   { ASY_16950,  5, asy_get_reg, asy_put_reg },
	/* 16950 indexed registers */
	[ASY_ACR] =   { ASY_16950,  0, asy_get_idx, asy_put_idx },
	[ASY_CPR] =   { ASY_16950,  1, asy_get_idx, asy_put_idx },
	[ASY_TCR] =   { ASY_16950,  2, asy_get_idx, asy_put_idx },
	[ASY_CKS] =   { ASY_16950,  3, asy_get_idx, asy_put_idx },
	[ASY_TTL] =   { ASY_16950,  4, asy_get_idx, asy_put_idx },
	[ASY_RTL] =   { ASY_16950,  5, asy_get_idx, asy_put_idx },
	[ASY_FCL] =   { ASY_16950,  6, asy_get_idx, asy_put_idx },
	[ASY_FCH] =   { ASY_16950,  7, asy_get_idx, asy_put_idx },
	[ASY_ID1] =   { ASY_16950,  8, asy_get_idx, NULL },
	[ASY_ID2] =   { ASY_16950,  9, asy_get_idx, NULL },
	[ASY_ID3] =   { ASY_16950, 10, asy_get_idx, NULL },
	[ASY_REV] =   { ASY_16950, 11, asy_get_idx, NULL },
	[ASY_CSR] =   { ASY_16950, 12, NULL,	    asy_put_idx },
	[ASY_NMR] =   { ASY_16950, 13, asy_get_idx, asy_put_idx },
};



static void
asy_put_idx(const struct asycom *asy, asy_reg_t reg, uint8_t val)
{
	ASSERT(asy->asy_hwtype >= ASY_16950);

	ASSERT(reg >= ASY_ACR);
	ASSERT(reg <= ASY_NREG);

	/*
	 * The last value written to LCR must not have been the magic value for
	 * EFR access. Every time the driver writes that magic value to access
	 * EFR, XON1, XON2, XOFF1, and XOFF2, the driver restores the original
	 * value of LCR, so we should be good here.
	 *
	 * I'd prefer to ASSERT this, but I'm not sure it's worth the hassle.
	 */

	/* Write indexed register offset to SPR. */
	asy_put(asy, ASY_SPR, asy_reg_table[reg].asy_reg_off);

	/* Write value to ICR. */
	asy_put(asy, ASY_ICR, val);
}

static uint8_t
asy_get_idx(const struct asycom *asy, asy_reg_t reg)
{
	uint8_t val;

	ASSERT(asy->asy_hwtype >= ASY_16950);

	ASSERT(reg >= ASY_ACR);
	ASSERT(reg <= ASY_NREG);

	/* Enable access to ICR in ACR. */
	asy_put(asy, ASY_ACR, ASY_ACR_ICR | asy->asy_acr);

	/* Write indexed register offset to SPR. */
	asy_put(asy, ASY_SPR, asy_reg_table[reg].asy_reg_off);

	/* Read value from ICR. */
	val = asy_get(asy, ASY_ICR);

	/* Restore ACR. */
	asy_put(asy, ASY_ACR, asy->asy_acr);

	return (val);
}

static void
asy_put_add(const struct asycom *asy, asy_reg_t reg, uint8_t val)
{
	ASSERT(asy->asy_hwtype >= ASY_16950);

	/* Only ASR is writable, RFL and TFL are read-only. */
	ASSERT(reg == ASY_ASR);

	/*
	 * Only ASR[0] (Transmitter Disabled) and ASR[1] (Remote Transmitter
	 * Disabled) are writable.
	 */
	ASSERT((val & ~(ASY_ASR_TD | ASY_ASR_RTD)) == 0);

	/* Enable access to ASR in ACR. */
	asy_put(asy, ASY_ACR, ASY_ACR_ASR | asy->asy_acr);

	/* Write value to ASR. */
	asy_put_reg(asy, reg, val);

	/* Restore ACR. */
	asy_put(asy, ASY_ACR, asy->asy_acr);
}

static uint8_t
asy_get_add(const struct asycom *asy, asy_reg_t reg)
{
	uint8_t val;

	ASSERT(asy->asy_hwtype >= ASY_16950);

	ASSERT(reg >= ASY_ASR);
	ASSERT(reg <= ASY_TFL);

	/*
	 * The last value written to LCR must not have been the magic value for
	 * EFR access. Every time the driver writes that magic value to access
	 * EFR, XON1, XON2, XOFF1, and XOFF2, the driver restores the original
	 * value of LCR, so we should be good here.
	 *
	 * I'd prefer to ASSERT this, but I'm not sure it's worth the hassle.
	 */

	/* Enable access to ASR in ACR. */
	asy_put(asy, ASY_ACR, ASY_ACR_ASR | asy->asy_acr);

	/* Read value from register. */
	val = asy_get_reg(asy, reg);

	/* Restore ACR. */
	asy_put(asy, ASY_ACR, 0 | asy->asy_acr);

	return (val);
}

static void
asy_put_ext(const struct asycom *asy, asy_reg_t reg, uint8_t val)
{
	uint8_t lcr;

	/*
	 * On the 16750, EFR can be accessed when LCR[7]=1 (DLAB).
	 * Only two bits are assigned for auto RTS/CTS, which we don't support
	 * yet.
	 *
	 * So insist we have a 16650 or up.
	 */
	ASSERT(asy->asy_hwtype >= ASY_16650);

	ASSERT(reg >= ASY_EFR);
	ASSERT(reg <= ASY_XOFF2);

	/* Save LCR contents. */
	lcr = asy_get(asy, ASY_LCR);

	/* Enable extended register access. */
	asy_put(asy, ASY_LCR, ASY_LCR_EFRACCESS);

	/* Write extended register */
	asy_put_reg(asy, reg, val);

	/* Restore previous LCR contents, disabling extended register access. */
	asy_put(asy, ASY_LCR, lcr);
}

static uint8_t
asy_get_ext(const struct asycom *asy, asy_reg_t reg)
{
	uint8_t lcr, val;

	/*
	 * On the 16750, EFR can be accessed when LCR[7]=1 (DLAB).
	 * Only two bits are assigned for auto RTS/CTS, which we don't support
	 * yet.
	 *
	 * So insist we have a 16650 or up.
	 */
	ASSERT(asy->asy_hwtype >= ASY_16650);

	ASSERT(reg >= ASY_EFR);
	ASSERT(reg <= ASY_XOFF2);

	/* Save LCR contents. */
	lcr = asy_get(asy, ASY_LCR);

	/* Enable extended register access. */
	asy_put(asy, ASY_LCR, ASY_LCR_EFRACCESS);

	/* Read extended register */
	val = asy_get_reg(asy, reg);

	/* Restore previous LCR contents, disabling extended register access. */
	asy_put(asy, ASY_LCR, lcr);

	return (val);
}

static void
asy_put_reg(const struct asycom *asy, asy_reg_t reg, uint8_t val)
{
	ASSERT(asy->asy_hwtype >= asy_reg_table[reg].asy_min_hwtype);

	ddi_put8(asy->asy_iohandle,
	    asy->asy_ioaddr + asy_reg_table[reg].asy_reg_off, val);
}

static uint8_t
asy_get_reg(const struct asycom *asy, asy_reg_t reg)
{
	ASSERT(asy->asy_hwtype >= asy_reg_table[reg].asy_min_hwtype);

	return (ddi_get8(asy->asy_iohandle,
	    asy->asy_ioaddr + asy_reg_table[reg].asy_reg_off));
}

static void
asy_put(const struct asycom *asy, asy_reg_t reg, uint8_t val)
{
	ASSERT(mutex_owned(&asy->asy_excl_hi));

	ASSERT(reg > ASY_ILLEGAL);
	ASSERT(reg < ASY_NREG);

	ASSERT(asy->asy_hwtype >= asy_reg_table[reg].asy_min_hwtype);
	ASSERT(asy_reg_table[reg].asy_put_reg != NULL);

	asy_reg_table[reg].asy_put_reg(asy, reg, val);
}

static uint8_t
asy_get(const struct asycom *asy, asy_reg_t reg)
{
	uint8_t val;

	ASSERT(mutex_owned(&asy->asy_excl_hi));

	ASSERT(reg > ASY_ILLEGAL);
	ASSERT(reg < ASY_NREG);

	ASSERT(asy->asy_hwtype >= asy_reg_table[reg].asy_min_hwtype);
	ASSERT(asy_reg_table[reg].asy_get_reg != NULL);

	val = asy_reg_table[reg].asy_get_reg(asy, reg);

	return (val);
}

static void
asy_set(const struct asycom *asy, asy_reg_t reg, uint8_t bits)
{
	uint8_t val = asy_get(asy, reg);

	asy_put(asy, reg, val | bits);
}

static void
asy_clr(const struct asycom *asy, asy_reg_t reg, uint8_t bits)
{
	uint8_t val = asy_get(asy, reg);

	asy_put(asy, reg, val & ~bits);
}

static void
asy_enable_interrupts(struct asycom *asy, uint8_t intr)
{
	/* Don't touch any IER bits we don't support. */
	intr &= ASY_IER_ALL;

	asy_set(asy, ASY_IER, intr);
}

static void
asy_disable_interrupts(struct asycom *asy, uint8_t intr)
{
	/* Don't touch any IER bits we don't support. */
	intr &= ASY_IER_ALL;

	asy_clr(asy, ASY_IER, intr);
}

static void
asy_set_baudrate(struct asycom *asy, int baudrate)
{
	uint8_t tcr;

	if (baudrate == 0)
		return;

	if (baudrate >= ARRAY_SIZE(asy_baud_tab))
		return;

	tcr = asy_baud_tab[baudrate].asy_tcr;

	if (tcr != 0 && asy->asy_hwtype < ASY_16950)
		return;

	if (asy->asy_hwtype >= ASY_16950) {
		if (tcr == 0x01) {
			/* Isochronous 1x mode is selected in CKS, not TCR. */
			asy_put(asy, ASY_CKS,
			    ASY_CKS_RCLK_1X | ASY_CKS_TCLK_1X);
			asy_put(asy, ASY_TCR, 0);
		} else {
			/* Reset CKS in case it was set to 1x mode. */
			asy_put(asy, ASY_CKS, 0);

			ASSERT(tcr == 0x00 || tcr >= 0x04 || tcr <= 0x0f);
			asy_put(asy, ASY_TCR, tcr);
		}
		ASY_DPRINTF(asy, ASY_DEBUG_IOCTL,
		    "setting baudrate %d, CKS 0x%02x, TCR 0x%02x",
		    baudrate, asy_get(asy, ASY_CKS), asy_get(asy, ASY_TCR));
	}

	ASY_DPRINTF(asy, ASY_DEBUG_IOCTL,
	    "setting baudrate %d, divisor 0x%02x%02x",
	    baudrate, asy_baud_tab[baudrate].asy_dlh,
	    asy_baud_tab[baudrate].asy_dll);

	asy_set(asy, ASY_LCR, ASY_LCR_DLAB);

	asy_put(asy, ASY_DLL, asy_baud_tab[baudrate].asy_dll);
	asy_put(asy, ASY_DLH, asy_baud_tab[baudrate].asy_dlh);

	asy_clr(asy, ASY_LCR, ASY_LCR_DLAB);
}

/*
 * Loop until the TSR is empty.
 *
 * The wait period is clock / (baud * 16) * 16 * 2.
 */
static void
asy_wait_baudrate(struct asycom *asy)
{
	struct asyncline *async = asy->asy_priv;
	int rate = BAUDINDEX(async->async_ttycommon.t_cflag);
	clock_t usec =
	    ((((clock_t)asy_baud_tab[rate].asy_dlh) << 8) |
	    ((clock_t)asy_baud_tab[rate].asy_dll)) * 16 * 2;

	ASSERT(mutex_owned(&asy->asy_excl));
	ASSERT(mutex_owned(&asy->asy_excl_hi));

	while ((asy_get(asy, ASY_LSR) & ASY_LSR_TEMT) == 0) {
		mutex_exit(&asy->asy_excl_hi);
		mutex_exit(&asy->asy_excl);
		drv_usecwait(usec);
		mutex_enter(&asy->asy_excl);
		mutex_enter(&asy->asy_excl_hi);
	}
}

static char *
asy_hw_name(struct asycom *asy)
{
	switch (asy->asy_hwtype) {
	case ASY_8250A:
		return ("8250A/16450");
	case ASY_16550:
		return ("16550");
	case ASY_16550A:
		return ("16550A");
	case ASY_16650:
		return ("16650");
	case ASY_16750:
		return ("16750");
	case ASY_16950:
		return ("16950");
	}

	ASY_DPRINTF(asy, ASY_DEBUG_INIT, "unknown asy_hwtype: %d",
	    asy->asy_hwtype);
	return ("?");
}

static boolean_t
asy_is_devid(struct asycom *asy, char *venprop, char *devprop,
    int venid, int devid)
{
	if (ddi_prop_get_int(DDI_DEV_T_ANY, asy->asy_dip, DDI_PROP_DONTPASS,
	    venprop, 0) != venid) {
		return (B_FALSE);
	}

	if (ddi_prop_get_int(DDI_DEV_T_ANY, asy->asy_dip, DDI_PROP_DONTPASS,
	    devprop, 0) != devid) {
		return (B_FALSE);
	}

	return (B_FALSE);
}

static void
asy_check_loopback(struct asycom *asy)
{
	if (asy_get_bus_type(asy->asy_dip) != ASY_BUS_PCI)
		return;

	/* Check if this is a Agere/Lucent Venus PCI modem chipset. */
	if (asy_is_devid(asy, "vendor-id", "device-id", 0x11c1, 0x0480) ||
	    asy_is_devid(asy, "subsystem-vendor-id", "subsystem-id", 0x11c1,
	    0x0480))
		asy->asy_flags2 |= ASY2_NO_LOOPBACK;
}

static int
asy_identify_chip(dev_info_t *devi, struct asycom *asy)
{
	int isr, lsr, mcr, spr;
	dev_t dev;
	uint_t hwtype;

	/*
	 * Initially, we'll assume we have the highest supported chip model
	 * until we find out what we actually have.
	 */
	asy->asy_hwtype = ASY_MAXCHIP;

	/*
	 * First, see if we can even do the loopback check, which may not work
	 * on certain hardware.
	 */
	asy_check_loopback(asy);

	if (asy_scr_test) {
		/* Check that the scratch register works. */

		/* write to scratch register */
		asy_put(asy, ASY_SPR, ASY_SPR_TEST);
		/* make sure that pattern doesn't just linger on the bus */
		asy_put(asy, ASY_FCR, 0x00);
		/* read data back from scratch register */
		spr = asy_get(asy, ASY_SPR);
		if (spr != ASY_SPR_TEST) {
			/*
			 * Scratch register not working.
			 * Probably not an async chip.
			 * 8250 and 8250B don't have scratch registers,
			 * but only worked in ancient PC XT's anyway.
			 */
			ASY_DPRINTF(asy, ASY_DEBUG_INIT, "UART @ %p "
			    "scratch register: expected 0x5a, got 0x%02x",
			    (void *)asy->asy_ioaddr, spr);
			return (DDI_FAILURE);
		}
	}
	/*
	 * Use 16550 fifo reset sequence specified in NS application
	 * note. Disable fifos until chip is initialized.
	 */
	asy_put(asy, ASY_FCR, 0x00);				 /* disable */
	asy_put(asy, ASY_FCR, ASY_FCR_FIFO_EN);			 /* enable */
	asy_put(asy, ASY_FCR, ASY_FCR_FIFO_EN | ASY_FCR_RHR_FL); /* reset */
	if (asymaxchip >= ASY_16650 && asy_scr_test) {
		/*
		 * Reset 16650 enhanced regs also, in case we have one of these
		 */
		asy_put(asy, ASY_EFR, 0);
	}

	/*
	 * See what sort of FIFO we have.
	 * Try enabling it and see what chip makes of this.
	 */

	asy->asy_fifor = 0;
	if (asymaxchip >= ASY_16550A)
		asy->asy_fifor |=
		    ASY_FCR_FIFO_EN | ASY_FCR_DMA | (asy_trig_level & 0xff);

	/*
	 * On the 16750, FCR[5] enables the 64 byte FIFO. FCR[5] can only be set
	 * while LCR[7] = 1 (DLAB), which is taken care of by asy_reset_fifo().
	 */
	if (asymaxchip >= ASY_16750)
		asy->asy_fifor |= ASY_FCR_FIFO64;

	asy_reset_fifo(asy, ASY_FCR_THR_FL | ASY_FCR_RHR_FL);

	mcr = asy_get(asy, ASY_MCR);
	isr = asy_get(asy, ASY_ISR);

	/*
	 * Note we get 0xff if chip didn't return us anything,
	 * e.g. if there's no chip there.
	 */
	if (isr == 0xff) {
		asyerror(asy, CE_WARN, "UART @ %p interrupt register: got 0xff",
		    (void *)asy->asy_ioaddr);
		return (DDI_FAILURE);
	}

	ASY_DPRINTF(asy, ASY_DEBUG_CHIP,
	    "probe fifo FIFOR=0x%02x ISR=0x%02x MCR=0x%02x",
	    asy->asy_fifor | ASY_FCR_THR_FL | ASY_FCR_RHR_FL, isr, mcr);

	/*
	 * Detect the chip type by comparing ISR[7,6] and ISR[5].
	 *
	 * When the FIFOs are enabled by setting FCR[0], ISR[7,6] read as 1.
	 * Additionally on a 16750, the 64 byte FIFOs are enabled by setting
	 * FCR[5], and ISR[5] will read as 1, too.
	 *
	 * We will check later whether we have a 16650, which requires EFR[4]=1
	 * to enable its deeper FIFOs and extra features. It does not use FCR[5]
	 * and ISR[5] to enable deeper FIFOs like the 16750 does.
	 */
	switch (isr & (ASY_ISR_FIFOEN | ASY_ISR_FIFO64)) {
	case 0x40:				/* 16550 with broken FIFOs */
		hwtype = ASY_16550;
		asy->asy_fifor = 0;
		break;

	case ASY_ISR_FIFOEN:			/* 16550A with working FIFOs */
		hwtype = ASY_16550A;
		asy->asy_fifo_buf = 16;
		asy->asy_use_fifo = ASY_FCR_FIFO_EN;
		asy->asy_fifor &= ~ASY_FCR_FIFO64;
		break;

	case ASY_ISR_FIFOEN | ASY_ISR_FIFO64:	/* 16750 with 64byte FIFOs */
		hwtype = ASY_16750;
		asy->asy_fifo_buf = 64;
		asy->asy_use_fifo = ASY_FCR_FIFO_EN;
		break;

	default:				/* 8250A/16450 without FIFOs */
		hwtype = ASY_8250A;
		asy->asy_fifor = 0;
	}

	if (hwtype > asymaxchip) {
		asyerror(asy, CE_WARN, "UART @ %p "
		    "unexpected probe result: "
		    "FCR=0x%02x ISR=0x%02x MCR=0x%02x",
		    (void *)asy->asy_ioaddr,
		    asy->asy_fifor | ASY_FCR_THR_FL | ASY_FCR_RHR_FL, isr, mcr);
		return (DDI_FAILURE);
	}

	/*
	 * Now reset the FIFO operation appropriate for the chip type.
	 * Note we must call asy_reset_fifo() before any possible
	 * downgrade of the asy->asy_hwtype, or it may not disable
	 * the more advanced features we specifically want downgraded.
	 */
	asy_reset_fifo(asy, 0);

	/*
	 * Check for Exar/Startech ST16C650 or newer, which will still look like
	 * a 16550A until we enable its enhanced mode.
	 */
	if (hwtype >= ASY_16550A && asymaxchip >= ASY_16650 &&
	    asy_scr_test) {
		/*
		 * Write the XOFF2 register, which shadows SPR on the 16650.
		 * On other chips, SPR will be overwritten.
		 */
		asy_put(asy, ASY_XOFF2, 0);

		/* read back scratch register */
		spr = asy_get(asy, ASY_SPR);

		if (spr == ASY_SPR_TEST) {
			/* looks like we have an ST16650 -- enable it */
			hwtype = ASY_16650;
			asy_put(asy, ASY_EFR, ASY_EFR_ENH_EN);

			/*
			 * Some 16650-compatible chips are also compatible with
			 * the 16750 and have deeper FIFOs, which we may have
			 * detected above. Don't downgrade the FIFO size.
			 */
			if (asy->asy_fifo_buf < 32)
				asy->asy_fifo_buf = 32;

			/*
			 * Use a 24 byte transmit FIFO trigger only if were
			 * allowed to use >16 transmit FIFO depth by the
			 * global tunable.
			 */
			if (asy_max_tx_fifo >= asy->asy_fifo_buf)
				asy->asy_fifor |= ASY_FCR_THR_TRIG_24;
			asy_reset_fifo(asy, 0);
		}
	}

	/*
	 * If we think we got a 16650, we may actually have a 16950, so check
	 * for that.
	 */
	if (hwtype >= ASY_16650 && asymaxchip >= ASY_16950) {
		uint8_t ier, asr;

		/*
		 * First, clear IER and read it back. That should be a no-op as
		 * either asyattach() or asy_resume() disabled all interrupts
		 * before we were called.
		 */
		asy_put(asy, ASY_IER, 0);
		ier = asy_get(asy, ASY_IER);
		if (ier != 0) {
			dev_err(asy->asy_dip, CE_WARN, "!%s: UART @ %p "
			    "interrupt enable register: got 0x%02x", __func__,
			    (void *)asy->asy_ioaddr, ier);
			return (DDI_FAILURE);
		}

		/*
		 * Next, try to read ASR, which shares the register offset with
		 * IER. ASR can only be read if the ASR enable bit is set in
		 * ACR, which itself is an indexed registers. This is taken care
		 * of by asy_get().
		 *
		 * There are a few bits in ASR which should be 1 at this point,
		 * definitely the TX idle bit (ASR[7]) and also the FIFO size
		 * bit (ASR[6]) since we've done everything we can to enable any
		 * deeper FIFO support.
		 *
		 * Thus if we read back ASR as 0, we failed to read it, and this
		 * isn't the chip we're looking for.
		 */
		asr = asy_get(asy, ASY_ASR);

		if (asr != ier) {
			hwtype = ASY_16950;

			if ((asr & ASY_ASR_FIFOSZ) != 0)
				asy->asy_fifo_buf = 128;
			else
				asy->asy_fifo_buf = 16;

			asy_reset_fifo(asy, 0);

			/*
			 * Enable 16950 specific trigger level registers. Set
			 * DTR pin to be compatible to 16450, 16550, and 16750.
			 */
			asy->asy_acr = ASY_ACR_TRIG | ASY_ACR_DTR_NORM;
			asy_put(asy, ASY_ACR, asy->asy_acr);

			/* Set half the FIFO size as receive trigger level. */
			asy_put(asy, ASY_RTL, asy->asy_fifo_buf/2);

			/*
			 * Set the transmit trigger level to 1.
			 *
			 * While one would expect that any transmit trigger
			 * level would work (the 16550 uses a hardwired level
			 * of 16), in my tests with a 16950 compatible chip
			 * (MosChip 9912) I would never see a TX interrupt
			 * on any transmit trigger level > 1.
			 */
			asy_put(asy, ASY_TTL, 1);

			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "ASR 0x%02x", asr);
			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "RFL 0x%02x",
			    asy_get(asy, ASY_RFL));
			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "TFL 0x%02x",
			    asy_get(asy, ASY_TFL));

			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "ACR 0x%02x",
			    asy_get(asy, ASY_ACR));
			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "CPR 0x%02x",
			    asy_get(asy, ASY_CPR));
			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "TCR 0x%02x",
			    asy_get(asy, ASY_TCR));
			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "CKS 0x%02x",
			    asy_get(asy, ASY_CKS));
			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "TTL 0x%02x",
			    asy_get(asy, ASY_TTL));
			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "RTL 0x%02x",
			    asy_get(asy, ASY_RTL));
			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "FCL 0x%02x",
			    asy_get(asy, ASY_FCL));
			ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "FCH 0x%02x",
			    asy_get(asy, ASY_FCH));

			ASY_DPRINTF(asy, ASY_DEBUG_CHIP,
			    "Chip ID: %02x%02x%02x,%02x",
			    asy_get(asy, ASY_ID1), asy_get(asy, ASY_ID2),
			    asy_get(asy, ASY_ID3), asy_get(asy, ASY_REV));

		}
	}

	asy->asy_hwtype = hwtype;

	/*
	 * If we think we might have a FIFO larger than 16 characters,
	 * measure FIFO size and check it against expected.
	 */
	if (asy_fifo_test > 0 &&
	    !(asy->asy_flags2 & ASY2_NO_LOOPBACK) &&
	    (asy->asy_fifo_buf > 16 ||
	    (asy_fifo_test > 1 && asy->asy_use_fifo == ASY_FCR_FIFO_EN) ||
	    ASY_DEBUG(asy, ASY_DEBUG_CHIP))) {
		int i;

		/* Set baud rate to 57600 (fairly arbitrary choice) */
		asy_set_baudrate(asy, B57600);
		/* Set 8 bits, 1 stop bit */
		asy_put(asy, ASY_LCR, ASY_LCR_STOP1 | ASY_LCR_BITS8);
		/* Set loopback mode */
		asy_put(asy, ASY_MCR, ASY_MCR_LOOPBACK);

		/* Overfill fifo */
		for (i = 0; i < asy->asy_fifo_buf * 2; i++) {
			asy_put(asy, ASY_THR, i);
		}
		/*
		 * Now there's an interesting question here about which
		 * FIFO we're testing the size of, RX or TX. We just
		 * filled the TX FIFO much faster than it can empty,
		 * although it is possible one or two characters may
		 * have gone from it to the TX shift register.
		 * We wait for enough time for all the characters to
		 * move into the RX FIFO and any excess characters to
		 * have been lost, and then read all the RX FIFO. So
		 * the answer we finally get will be the size which is
		 * the MIN(RX FIFO,(TX FIFO + 1 or 2)). The critical
		 * one is actually the TX FIFO, because if we overfill
		 * it in normal operation, the excess characters are
		 * lost with no warning.
		 */
		/*
		 * Wait for characters to move into RX FIFO.
		 * In theory, 200 * asy->asy_fifo_buf * 2 should be
		 * enough. However, in practice it isn't always, so we
		 * increase to 400 so some slow 16550A's finish, and we
		 * increase to 3 so we spot more characters coming back
		 * than we sent, in case that should ever happen.
		 */
		delay(drv_usectohz(400 * asy->asy_fifo_buf * 3));

		/* Now see how many characters we can read back */
		for (i = 0; i < asy->asy_fifo_buf * 3; i++) {
			lsr = asy_get(asy, ASY_LSR);
			if (!(lsr & ASY_LSR_DR))
				break;	/* FIFO emptied */
			(void) asy_get(asy, ASY_RHR); /* lose another */
		}

		ASY_DPRINTF(asy, ASY_DEBUG_CHIP,
		    "FIFO size: expected=%d, measured=%d",
		    asy->asy_fifo_buf, i);

		hwtype = asy->asy_hwtype;
		if (i < asy->asy_fifo_buf) {
			/*
			 * FIFO is somewhat smaller than we anticipated.
			 * If we have 16 characters usable, then this
			 * UART will probably work well enough in
			 * 16550A mode. If less than 16 characters,
			 * then we'd better not use it at all.
			 * UARTs with busted FIFOs do crop up.
			 */
			if (i >= 16 && asy->asy_fifo_buf >= 16) {
				/* fall back to a 16550A */
				hwtype = ASY_16550A;
				asy->asy_fifo_buf = 16;
				asy->asy_fifor &=
				    ~(ASY_FCR_THR_TR0 | ASY_FCR_THR_TR1);
			} else {
				/* fall back to no FIFO at all */
				hwtype = ASY_16550;
				asy->asy_fifo_buf = 1;
				asy->asy_use_fifo = ASY_FCR_FIFO_OFF;
				asy->asy_fifor = 0;
			}
		} else if (i > asy->asy_fifo_buf) {
			/*
			 * The FIFO is larger than expected. Use it if it is
			 * a power of 2.
			 */
			if (ISP2(i))
				asy->asy_fifo_buf = i;
		}

		/*
		 * We will need to reprogram the FIFO if we changed
		 * our mind about how to drive it above, and in any
		 * case, it would be a good idea to flush any garbage
		 * out incase the loopback test left anything behind.
		 * Again as earlier above, we must call asy_reset_fifo()
		 * before any possible downgrade of asy->asy_hwtype.
		 */
		if (asy->asy_hwtype >= ASY_16650 && hwtype < ASY_16650) {
			/* Disable 16650 enhanced mode */
			asy_put(asy, ASY_EFR, 0);
		}
		asy_reset_fifo(asy, ASY_FCR_THR_FL | ASY_FCR_RHR_FL);
		asy->asy_hwtype = hwtype;

		/* Clear loopback mode and restore DTR/RTS */
		asy_put(asy, ASY_MCR, mcr);
	}

	ASY_DPRINTF(asy, ASY_DEBUG_CHIP, "%s @ %p",
	    asy_hw_name(asy), (void *)asy->asy_ioaddr);

	/* Make UART type visible in device tree for prtconf, etc */
	dev = makedevice(DDI_MAJOR_T_UNKNOWN, asy->asy_unit);
	(void) ddi_prop_update_string(dev, devi, "uart", asy_hw_name(asy));

	if (asy->asy_hwtype == ASY_16550)	/* for broken 16550's, */
		asy->asy_hwtype = ASY_8250A;	/* drive them as 8250A */

	asy->asy_rx_drain_limit = asy->asy_fifo_buf * 2;

	return (DDI_SUCCESS);
}

/* asy_reset_fifo -- flush fifos and [re]program fifo control register */
static void
asy_reset_fifo(struct asycom *asy, uchar_t flush)
{
	ASSERT(mutex_owned(&asy->asy_excl_hi));

	/* On a 16750, we have to set DLAB in order to set ASY_FCR_FIFO64. */
	if (asy->asy_hwtype >= ASY_16750)
		asy_set(asy, ASY_LCR, ASY_LCR_DLAB);

	asy_put(asy, ASY_FCR, asy->asy_fifor | flush);

	/* Clear DLAB */
	if (asy->asy_hwtype >= ASY_16750)
		asy_clr(asy, ASY_LCR, ASY_LCR_DLAB);
}

static boolean_t
asy_baudok(struct asycom *asy)
{
	struct asyncline *async = asy->asy_priv;
	int baudrate;


	baudrate = BAUDINDEX(async->async_ttycommon.t_cflag);

	if (baudrate >= ARRAY_SIZE(asy_baud_tab))
		return (0);

	return (baudrate == 0 ||
	    asy_baud_tab[baudrate].asy_dll != 0 ||
	    asy_baud_tab[baudrate].asy_dlh != 0);
}

/*
 * asyintr() is the High Level Interrupt Handler.
 *
 * There are four different interrupt types indexed by ISR register values:
 *		0: modem
 *		1: Tx holding register is empty, ready for next char
 *		2: Rx register now holds a char to be picked up
 *		3: error or break on line
 * This routine checks the Bit 0 (interrupt-not-pending) to determine if
 * the interrupt is from this port.
 */
static uint_t
asy_16550_intr(caddr_t argasy, caddr_t argunused __unused)
{
	struct asycom		*asy = (struct asycom *)argasy;
	struct asyncline	*async;
	int			ret_status = DDI_INTR_UNCLAIMED;

	mutex_enter(&asy->asy_excl_hi);
	async = asy->asy_priv;
	if (async == NULL ||
	    (async->async_flags & (ASYNC_ISOPEN|ASYNC_WOPEN)) == 0) {
		const uint8_t intr_id = asy_get(asy, ASY_ISR);

		ASY_DPRINTF(asy, ASY_DEBUG_INTR,
		    "not open async=%p flags=0x%x interrupt_id=0x%x",
		    async, async == NULL ? 0 : async->async_flags, intr_id);

		if ((intr_id & ASY_ISR_NOINTR) == 0) {
			/*
			 * reset the device by:
			 *	reading line status
			 *	reading any data from data status register
			 *	reading modem status
			 */
			(void) asy_get(asy, ASY_LSR);
			(void) asy_get(asy, ASY_RHR);
			asy->asy_msr = asy_get(asy, ASY_MSR);
			ret_status = DDI_INTR_CLAIMED;
		}
		mutex_exit(&asy->asy_excl_hi);
		return (ret_status);
	}

	/* By this point we're sure this is for us. */
	ret_status = DDI_INTR_CLAIMED;

	/*
	 * Before this flag was set, interrupts were disabled. We may still get
	 * here if asy_16550_intr() waited on the mutex.
	 */
	if (asy->asy_flags & ASY_DDI_SUSPENDED) {
		mutex_exit(&asy->asy_excl_hi);
		return (ret_status);
	}

	/*
	 * We will loop until the interrupt line is pulled low. asy
	 * interrupt is edge triggered.
	 */
	for (;;) {
		const uint8_t intr_id = asy_get(asy, ASY_ISR);
		/*
		 * Reading LSR will clear any error bits (ASY_LSR_ERRORS) which
		 * are set which is why the value is passed through to
		 * async_rxint() and not re-read there. In the unexpected event
		 * that we've ended up here without a pending interrupt, the
		 * ASY_ISR_NOINTR case, it should do no harm to have cleared
		 * the error bits, and it means we can get some additional
		 * information in the debug message if it's enabled.
		 */
		const uint8_t lsr = asy_get(asy, ASY_LSR);

		ASY_DPRINTF(asy, ASY_DEBUG_INTR,
		    "interrupt_id=0x%x LSR=0x%x",
		    intr_id, lsr);

		if (intr_id & ASY_ISR_NOINTR)
			break;

		switch (intr_id & ASY_ISR_MASK) {
		case ASY_ISR_ID_RLST:
		case ASY_ISR_ID_RDA:
		case ASY_ISR_ID_TMO:
			/* receiver interrupt or receiver errors */
			async_rxint(asy, lsr);
			break;

		case ASY_ISR_ID_THRE:
			/*
			 * The transmit-ready interrupt implies an empty
			 * transmit-hold register (or FIFO).  Check that it is
			 * present before attempting to transmit more data.
			 */
			if ((lsr & ASY_LSR_THRE) == 0) {
				/*
				 * Taking a THRE interrupt only to find THRE
				 * absent would be a surprise, except for a
				 * racing asyputchar(), which ignores the
				 * excl_hi mutex when writing to the device.
				 */
				continue;
			}
			async_txint(asy);
			/*
			 * Unlike the other interrupts which fall through to
			 * attempting to fill the output register/FIFO, THRE
			 * has no need having just done so.
			 */
			continue;

		case ASY_ISR_ID_MST:
			/* modem status interrupt */
			async_msint(asy);
			break;
		}

		/* Refill the output FIFO if it has gone empty */
		if ((lsr & ASY_LSR_THRE) && (async->async_flags & ASYNC_BUSY) &&
		    async->async_ocnt > 0)
			async_txint(asy);
	}

	mutex_exit(&asy->asy_excl_hi);
	return (ret_status);
}

/*
 * debugger/console support routines.
 */

/*
 * put a character out
 * Do not use interrupts.  If char is LF, put out CR, LF.
 */
static void
asy_16550_polledio_putchar(struct asycom *asy, uchar_t c)
{
	if (c == '\n')
		asy_16550_polledio_putchar(asy, '\r');

	while ((asy_get_reg(asy, ASY_LSR) & ASY_LSR_THRE) == 0) {
		/* wait for xmit to finish */
		drv_usecwait(10);
	}

	/* put the character out */
	asy_put_reg(asy, ASY_THR, c);
}

/*
 * See if there's a character available. If no character is
 * available, return 0. Run in polled mode, no interrupts.
 */
static boolean_t
asy_16550_polledio_ischar(struct asycom *asy)
{
	return ((asy_get_reg(asy, ASY_LSR) & ASY_LSR_DR) != 0);
}

/*
 * Get a character. Run in polled mode, no interrupts.
 */
static int
asy_16550_polledio_getchar(struct asycom *asy)
{
	while (!asy_16550_polledio_ischar(asy))
		drv_usecwait(10);
	return (asy_get_reg(asy, ASY_RHR));
}


/*
 * Thin wrapper functions for the hardware operations table.
 * These translate between the generic ops interface and the 16550
 * register access primitives.
 */

static void
asy_16550_set_lcr(struct asycom *asy, uint8_t lcr)
{
	asy_clr(asy, ASY_LCR, ASY_LCR_WLS0 | ASY_LCR_WLS1 |
	    ASY_LCR_STB | ASY_LCR_PEN | ASY_LCR_EPS);
	asy_set(asy, ASY_LCR, lcr);
}

static void
asy_16550_set_break(struct asycom *asy, boolean_t on)
{
	if (on) {
		asy_set(asy, ASY_LCR, ASY_LCR_SETBRK);
	} else {
		asy_clr(asy, ASY_LCR, ASY_LCR_SETBRK);
	}
}

static uint8_t
asy_16550_get_mcr(struct asycom *asy)
{
	return (asy_get(asy, ASY_MCR));
}

static void
asy_16550_set_mcr(struct asycom *asy, uint8_t val)
{
	asy_put(asy, ASY_MCR, val);
}

static void
asy_16550_mcr_set(struct asycom *asy, uint8_t bits)
{
	asy_set(asy, ASY_MCR, bits);
}

static void
asy_16550_mcr_clr(struct asycom *asy, uint8_t bits)
{
	asy_clr(asy, ASY_MCR, bits);
}

static uint8_t
asy_16550_get_msr(struct asycom *asy)
{
	return (asy_get(asy, ASY_MSR));
}

static uint8_t
asy_16550_get_lsr(struct asycom *asy)
{
	return (asy_get(asy, ASY_LSR));
}

static uint8_t
asy_16550_get_rx(struct asycom *asy)
{
	return (asy_get(asy, ASY_RHR));
}

static void
asy_16550_put_tx(struct asycom *asy, uint8_t c)
{
	asy_put(asy, ASY_THR, c);
}

static void
asy_16550_flush_status(struct asycom *asy)
{
	(void) asy_get(asy, ASY_ISR);
	(void) asy_get(asy, ASY_LSR);
}

/*
 * Fill the TX FIFO with up to ocnt bytes from buf.
 *
 * On the 16550 the TX (THRE) interrupt fires only when the XMIT FIFO is
 * completely empty, so the budget is the full FIFO depth (capped by the
 * asy_max_tx_fifo policy tunable).  No per-byte hardware check is needed
 * or available -- the 16550 exposes "empty" but not "not full".
 *
 * The fill arithmetic and asysetsoft gate are bit-exact reproductions of
 * the original inline loop in async_txint().  The old
 * while (fifo_len-- > 0 && ocnt-- > 0) double-decremented on the failing
 * test, so asysetsoft only fired when budget exceeded the bytes written
 * by at least 2.  We preserve that here.
 */
static uint_t
asy_16550_tx_fill(struct asycom *asy, uchar_t *buf, uint_t ocnt,
    uint_t reserve, boolean_t *space_left)
{
	uint_t budget = asy->asy_fifo_buf;
	uint_t n;

	if (budget > (uint_t)asy_max_tx_fifo)
		budget = (uint_t)asy_max_tx_fifo;
	if (reserve != 0 && budget > 0)
		budget--;

	n = (ocnt < budget) ? ocnt : budget;
	for (uint_t i = 0; i < n; i++) {
		asy->asy_hw->aho_put_tx(asy, buf[i]);
	}

	/*
	 * Preserve the original asysetsoft gate: asysetsoft fired only
	 * when the remaining budget after the fill was at least 2.
	 */
	*space_left = (budget >= n + 2);

	return (n);
}

/*
 * Initial TX fill for async_start: prime the transmitter with the first
 * bytes of a new message.
 *
 * On the 16550, THRE means "FIFO empty" -- it clears as soon as the first
 * byte lands.  The per-byte THRE check therefore produces a single-byte
 * prime pump; the TX interrupt chain (async_txint -> aho_tx_fill) handles
 * the remaining bulk fill.  This is the original async_start behavior,
 * preserved byte-exact.
 */
static uint_t
asy_16550_tx_start(struct asycom *asy, uchar_t *buf, uint_t ocnt,
    uint_t reserve)
{
	int fifo_len = 1;
	uint_t n = 0;

	if (asy->asy_use_fifo == ASY_FCR_FIFO_EN) {
		fifo_len = asy->asy_fifo_buf;
		if (fifo_len > asy_max_tx_fifo)
			fifo_len = asy_max_tx_fifo;
	}
	fifo_len -= reserve;

	while (--fifo_len >= 0 && n < ocnt) {
		if (!(asy_16550_get_lsr(asy) & ASY_LSR_THRE))
			break;
		asy_16550_put_tx(asy, buf[n]);
		n++;
	}
	return (n);
}

/*
 * Drain the RX FIFO (or holding register) by reading and discarding
 * all pending data.  With a FIFO, read asy_fifo_buf times (the known
 * hardware depth); without, read the single holding register once.
 * Byte-exact reproduction of the old inline loop in asy_program().
 */
static void
asy_16550_rx_drain(struct asycom *asy)
{
	if (asy->asy_use_fifo == ASY_FCR_FIFO_EN) {
		for (uint_t i = asy->asy_fifo_buf; i > 0; i--) {
			(void) asy->asy_hw->aho_get_rx(asy);
		}
	} else {
		(void) asy->asy_hw->aho_get_rx(asy);
	}
}

/*
 * 16550-family hardware operations table.
 */
const asy_hw_ops_t asy_16550_ops = {
	.aho_intr		= asy_16550_intr,
	.aho_identify		= asy_identify_chip,
	.aho_hw_name		= asy_hw_name,
	.aho_set_baud		= asy_set_baudrate,
	.aho_baudok		= asy_baudok,
	.aho_wait_baud		= asy_wait_baudrate,
	.aho_set_lcr		= asy_16550_set_lcr,
	.aho_set_break		= asy_16550_set_break,
	.aho_get_mcr		= asy_16550_get_mcr,
	.aho_set_mcr		= asy_16550_set_mcr,
	.aho_mcr_set		= asy_16550_mcr_set,
	.aho_mcr_clr		= asy_16550_mcr_clr,
	.aho_get_msr		= asy_16550_get_msr,
	.aho_get_lsr		= asy_16550_get_lsr,
	.aho_enable_intr	= asy_enable_interrupts,
	.aho_disable_intr	= asy_disable_interrupts,
	.aho_fifo_setup		= asy_reset_fifo,
	.aho_get_rx		= asy_16550_get_rx,
	.aho_put_tx		= asy_16550_put_tx,
	.aho_tx_fill		= asy_16550_tx_fill,
	.aho_tx_start		= asy_16550_tx_start,
	.aho_rx_drain		= asy_16550_rx_drain,
	.aho_flush_status	= asy_16550_flush_status,
	.aho_polledio_putchar	= asy_16550_polledio_putchar,
	.aho_polledio_getchar	= asy_16550_polledio_getchar,
	.aho_polledio_ischar	= asy_16550_polledio_ischar,
};
