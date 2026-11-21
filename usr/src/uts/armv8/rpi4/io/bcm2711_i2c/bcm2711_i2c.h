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
 * Copyright 2026 OmniOS Community Edition (OmniOSce) Association.
 */

#ifndef _BCM2711_I2C_H
#define	_BCM2711_I2C_H

/*
 * Broadcom Serial Controller (BSC) register definitions.
 *
 * The BSC is the I2C controller found in the BCM2835 family of SoCs, which
 * includes the BCM2711. These definitions come from the BSC chapter of the
 * Broadcom BCM2835 and BCM2711 ARM Peripherals documents. All registers are
 * 32 bits wide.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Control register -- C.
 */
#define	BSC_R_C			0x00
#define	BSC_C_I2CEN		(1U << 15)
#define	BSC_C_INTR		(1U << 10)
#define	BSC_C_INTT		(1U << 9)
#define	BSC_C_INTD		(1U << 8)
#define	BSC_C_ST		(1U << 7)
#define	BSC_C_CLEAR		(3U << 4)
#define	BSC_C_READ		(1U << 0)
#define	BSC_C_INT_ALL		(BSC_C_INTR | BSC_C_INTT | BSC_C_INTD)

/*
 * Status register -- S. The CLKT, ERR and DONE bits are cleared by writing
 * a one to them. All other bits are read-only.
 */
#define	BSC_R_S			0x04
#define	BSC_S_CLKT		(1U << 9)
#define	BSC_S_ERR		(1U << 8)
#define	BSC_S_RXF		(1U << 7)
#define	BSC_S_TXE		(1U << 6)
#define	BSC_S_RXD		(1U << 5)
#define	BSC_S_TXD		(1U << 4)
#define	BSC_S_RXR		(1U << 3)
#define	BSC_S_TXW		(1U << 2)
#define	BSC_S_DONE		(1U << 1)
#define	BSC_S_TA		(1U << 0)
#define	BSC_S_CLEAR		(BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE)

/*
 * Data length register -- DLEN. A 16-bit count of the bytes remaining in the
 * transfer.
 */
#define	BSC_R_DLEN		0x08
#define	BSC_DLEN_MAX		0xffff

/*
 * Slave address register -- A. Holds a 7-bit target address.
 */
#define	BSC_R_A			0x0c

/*
 * Data FIFO register -- FIFO. The FIFO is 16 bytes deep.
 */
#define	BSC_R_FIFO		0x10

/*
 * Clock divider register -- DIV. SCL is the core clock divided by CDIV. The
 * hardware always rounds CDIV down to an even number and treats zero as
 * 32768.
 */
#define	BSC_R_DIV		0x14
#define	BSC_DIV_MIN		2
#define	BSC_DIV_MAX		0xfffe

/*
 * Data delay register -- DEL. FEDL and REDL are the number of core clock
 * cycles to wait after the falling and rising edges of SCL before sampling
 * or changing SDA. Each must be less than CDIV / 2.
 */
#define	BSC_R_DEL		0x18
#define	BSC_DEL(fedl, redl)	(((uint32_t)(fedl) << 16) | (redl))
#define	BSC_DEL_DEFAULT		0x30

/*
 * Clock stretch timeout register -- CLKT. The number of SCL cycles to wait
 * for a target that is stretching the clock before setting S.CLKT.
 */
#define	BSC_R_CLKT		0x1c

#ifdef __cplusplus
}
#endif

#endif /* _BCM2711_I2C_H */
