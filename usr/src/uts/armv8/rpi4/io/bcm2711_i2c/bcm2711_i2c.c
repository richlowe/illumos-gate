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

/*
 * Broadcom Serial Controller (BSC) I2C driver for the BCM2711
 *
 * The BSC is a simple I2C controller. Software programs the target address
 * and a byte count, starts the transfer, and then feeds or drains a 16-byte
 * FIFO until the controller reports that the transfer is done. Each transfer
 * is in one direction only and the controller generates the START, address
 * and STOP conditions itself.
 *
 * The controller has no explicit means of issuing a repeated START between a
 * write and a following read, which is needed for most register reads. It
 * will, however, issue one if a new read transfer is started while a write
 * is still active. To do this reliably, we start the write with an empty
 * FIFO. The controller sends the START and address and then stalls, holding
 * SCL low, waiting for data. Once the transfer active (TA) bit is set, we
 * program the read length and start the read, and only then fill the FIFO
 * with the write data. When the write completes the controller issues a
 * repeated START and performs the read.
 *
 * The controller supports 7-bit addresses only (10-bit addressing would
 * need a special sequence that we do not implement). As a plain I2C
 * controller, the framework translates SMBus requests into I2C requests on
 * our behalf.
 *
 * The SCL frequency is derived from the core (VPU) clock using a single
 * divider. We obtain the rate of that clock from the firmware when setting
 * the bus speed. If the firmware changes the core clock afterwards, the bus
 * speed will scale with it.
 *
 * When an interrupt is available, we use it to wait for the controller
 * rather than spinning. The controller can interrupt when its FIFO needs
 * reading (INTR), when it needs more data to write (INTT) and when the
 * transfer is done (INTD). We enable these per request, and just the ones
 * that the request needs. The BSC instances on the BCM2711 share a single
 * interrupt, so the handler only claims it while one of our requests is in
 * progress and an enabled condition is pending. The interrupt is level
 * triggered, so the handler disables all of the conditions once the request
 * is complete. It is not clear whether every error raises an interrupt, so
 * while waiting we also check the status register periodically.
 *
 * Requests that ask to be polled, and all requests when no interrupt is
 * available, are completed by polling the status register.
 *
 * The SDA and SCL pins must be switched to their I2C function. There is no
 * pin control framework, so we ask the platform module to apply the
 * "default" pin configuration from our device tree node.
 */

#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sysmacros.h>
#include <sys/stdbool.h>
#include <sys/platmod.h>

#include <sys/i2c/controller.h>
#include "bcm2711_i2c.h"

/*
 * The default bus speed, if the device tree does not specify one.
 */
#define	BCM2711_I2C_DEF_FREQ	100000

/*
 * How often, in microseconds, to check the status register while waiting for
 * an interrupt.
 */
#define	BCM2711_I2C_RECHECK_US	10000

typedef enum {
	BCM2711_I2C_INIT_REGS		= 1 << 0,
	BCM2711_I2C_INIT_INTR_ALLOC	= 1 << 1,
	BCM2711_I2C_INIT_INTR_HDL	= 1 << 2,
	BCM2711_I2C_INIT_SYNC		= 1 << 3,
	BCM2711_I2C_INIT_CTRL		= 1 << 4,
	BCM2711_I2C_INIT_INTR_EN	= 1 << 5,
	BCM2711_I2C_INIT_I2C		= 1 << 6
} bcm2711_i2c_init_t;

typedef struct {
	dev_info_t *bi_dip;
	bcm2711_i2c_init_t bi_init;
	/*
	 * Register related data
	 */
	caddr_t bi_base;
	off_t bi_regsize;
	ddi_acc_handle_t bi_regs;
	/*
	 * Interrupt data. bi_nintrs is zero if no interrupt is available.
	 */
	int bi_nintrs;
	ddi_intr_handle_t bi_intr_hdl;
	uint_t bi_intr_pri;
	/*
	 * Bus speed
	 */
	i2c_speed_t bi_speed;
	/*
	 * Request and framework synchronisation. bi_req_ctl holds the value of
	 * the control register for the current transfer, without the ST and
	 * CLEAR bits.
	 */
	kmutex_t bi_mutex;
	kcondvar_t bi_cv;
	i2c_ctrl_hdl_t *bi_hdl;
	i2c_req_t *bi_req;
	uint16_t bi_req_woff;
	uint16_t bi_req_roff;
	bool bi_req_done;
	bool bi_req_poll;
	bool bi_req_wsent;
	uint32_t bi_req_ctl;
	i2c_ctrl_error_t bi_req_err;
	/*
	 * Statistics that could reasonably become kstats in the future.
	 * bi_stat_recheck counts requests that were found to be complete by a
	 * periodic check rather than by the interrupt handler.
	 */
	uint64_t bi_stat_intr;
	uint64_t bi_stat_intr_unclaimed;
	uint64_t bi_stat_recheck;
} bcm2711_i2c_t;

static uint32_t
bcm2711_i2c_read32(bcm2711_i2c_t *bi, uint32_t regno)
{
	ASSERT3U(regno, <, bi->bi_regsize);

	return (ddi_get32(bi->bi_regs, (uint32_t *)(bi->bi_base + regno)));
}

static void
bcm2711_i2c_write32(bcm2711_i2c_t *bi, uint32_t regno, uint32_t val)
{
	ASSERT3U(regno, <, bi->bi_regsize);

	ddi_put32(bi->bi_regs, (uint32_t *)(bi->bi_base + regno), val);
}

static uint32_t
bcm2711_i2c_speed_hz(i2c_speed_t speed)
{
	switch (speed) {
	case I2C_SPEED_STD:
		return (100000);
	case I2C_SPEED_FAST:
		return (400000);
	default:
		panic("programmer error: unsupported bus speed 0x%x", speed);
	}
}

/*
 * Program the clock divider for the requested bus speed. The divider must be
 * even, so we round it up to the next even number, which errs towards a
 * slower bus.
 */
static bool
bcm2711_i2c_set_speed(bcm2711_i2c_t *bi, i2c_speed_t speed)
{
	int clk = -1;

	if (&plat_clk_get_rate != NULL)
		clk = plat_clk_get_rate(bi->bi_dip, 0);
	if (clk <= 0) {
		dev_err(bi->bi_dip, CE_WARN, "failed to get core clock rate");
		return (false);
	}

	uint32_t div = howmany((uint32_t)clk, bcm2711_i2c_speed_hz(speed));
	div = P2ROUNDUP(div, 2);
	if (div < BSC_DIV_MIN || div > BSC_DIV_MAX) {
		dev_err(bi->bi_dip, CE_WARN,
		    "cannot derive %u Hz bus clock from %d Hz core clock",
		    bcm2711_i2c_speed_hz(speed), clk);
		return (false);
	}

	uint32_t del = MIN(BSC_DEL_DEFAULT, div / 2 - 1);

	bcm2711_i2c_write32(bi, BSC_R_DIV, div);
	bcm2711_i2c_write32(bi, BSC_R_DEL, BSC_DEL(del, del));
	bi->bi_speed = speed;

	return (true);
}

static i2c_errno_t
bcm2711_i2c_prop_info(void *arg __unused, i2c_prop_t prop,
    i2c_prop_info_t *info)
{
	switch (prop) {
	case I2C_PROP_BUS_SPEED:
		i2c_prop_info_set_def_u32(info, I2C_SPEED_STD);
		i2c_prop_info_set_pos_bit32(info,
		    I2C_SPEED_STD | I2C_SPEED_FAST);
		i2c_prop_info_set_perm(info, I2C_PROP_PERM_RW);
		break;
	case I2C_PROP_MAX_READ:
	case I2C_PROP_MAX_WRITE:
		i2c_prop_info_set_perm(info, I2C_PROP_PERM_RO);
		break;
	default:
		return (I2C_PROP_E_UNSUP);
	}

	return (I2C_CORE_E_OK);
}

static i2c_errno_t
bcm2711_i2c_prop_get(void *arg, i2c_prop_t prop, void *buf, size_t buflen)
{
	bcm2711_i2c_t *bi = arg;
	uint32_t val;

	switch (prop) {
	case I2C_PROP_BUS_SPEED:
		mutex_enter(&bi->bi_mutex);
		val = bi->bi_speed;
		mutex_exit(&bi->bi_mutex);
		break;
	case I2C_PROP_MAX_READ:
	case I2C_PROP_MAX_WRITE:
		val = I2C_REQ_MAX;
		break;
	default:
		return (I2C_PROP_E_UNSUP);
	}

	VERIFY3U(buflen, >=, sizeof (val));
	bcopy(&val, buf, sizeof (val));
	return (I2C_CORE_E_OK);
}

static i2c_errno_t
bcm2711_i2c_prop_set(void *arg, i2c_prop_t prop, const void *buf,
    size_t buflen)
{
	bcm2711_i2c_t *bi = arg;
	uint32_t val;

	if (prop != I2C_PROP_BUS_SPEED)
		return (I2C_PROP_E_UNSUP);

	VERIFY3U(buflen, ==, sizeof (val));
	bcopy(buf, &val, sizeof (val));
	if (val != I2C_SPEED_STD && val != I2C_SPEED_FAST)
		return (I2C_PROP_E_BAD_VAL);

	mutex_enter(&bi->bi_mutex);
	bool ok = bcm2711_i2c_set_speed(bi, val);
	mutex_exit(&bi->bi_mutex);

	return (ok ? I2C_CORE_E_OK : I2C_PROP_E_BAD_VAL);
}

/*
 * Stop any transfer in progress and return the controller to an idle state
 * with an empty FIFO and no pending status. Clearing I2CEN aborts the current
 * transfer.
 */
static void
bcm2711_i2c_reset(bcm2711_i2c_t *bi)
{
	bcm2711_i2c_write32(bi, BSC_R_C, BSC_C_CLEAR);
	bcm2711_i2c_write32(bi, BSC_R_S, BSC_S_CLEAR);
	bcm2711_i2c_write32(bi, BSC_R_C, BSC_C_I2CEN);
}

static bool
bcm2711_i2c_bus_avail(bcm2711_i2c_t *bi)
{
	const uint32_t count = i2c_ctrl_timeout_count(bi->bi_hdl,
	    I2C_CTRL_TO_BUS_ACT);
	const uint32_t wait = i2c_ctrl_timeout_delay_us(bi->bi_hdl,
	    I2C_CTRL_TO_BUS_ACT);

	for (uint32_t i = 0; i < count; i++) {
		if ((bcm2711_i2c_read32(bi, BSC_R_S) & BSC_S_TA) == 0)
			return (true);

		delay(drv_usectohz(wait));
	}

	dev_err(bi->bi_dip, CE_WARN,
	    "controller timed out waiting for transfer to complete");
	return (false);
}

static void
bcm2711_i2c_io_error(bcm2711_i2c_t *bi, i2c_ctrl_error_t err)
{
	i2c_ctrl_io_error(&bi->bi_req->ir_error, I2C_CORE_E_CONTROLLER, err);
	bi->bi_req_done = true;
}

static void
bcm2711_i2c_fill_fifo(bcm2711_i2c_t *bi)
{
	const i2c_req_t *req = bi->bi_req;

	while (bi->bi_req_woff < req->ir_wlen &&
	    (bcm2711_i2c_read32(bi, BSC_R_S) & BSC_S_TXD) != 0) {
		bcm2711_i2c_write32(bi, BSC_R_FIFO,
		    req->ir_wdata[bi->bi_req_woff]);
		bi->bi_req_woff++;
	}
}

static void
bcm2711_i2c_drain_fifo(bcm2711_i2c_t *bi)
{
	i2c_req_t *req = bi->bi_req;

	while (bi->bi_req_roff < req->ir_rlen &&
	    (bcm2711_i2c_read32(bi, BSC_R_S) & BSC_S_RXD) != 0) {
		req->ir_rdata[bi->bi_req_roff] =
		    bcm2711_i2c_read32(bi, BSC_R_FIFO) & 0xff;
		bi->bi_req_roff++;
	}
}

/*
 * Advance the state of the current request based on a snapshot of the status
 * register. The controller cannot tell us whether a NACK was for the address
 * or for data, so all are reported as I2C_CTRL_E_NACK.
 *
 * When the controller reports an error it still has to finish the transfer
 * by sending a STOP. Resetting the controller before then aborts the transfer
 * and leaves the bus mid-transaction, which can confuse the next target that
 * is addressed. We therefore record the error and only complete the request
 * once the transfer is no longer active.
 */
static void
bcm2711_i2c_io(bcm2711_i2c_t *bi, uint32_t status)
{
	const i2c_req_t *req = bi->bi_req;

	ASSERT(MUTEX_HELD(&bi->bi_mutex));
	ASSERT3P(req, !=, NULL);

	if (bi->bi_req_err == I2C_CTRL_E_OK) {
		if ((status & BSC_S_CLKT) != 0)
			bi->bi_req_err = I2C_CTRL_E_SMBUS_CLOCK_LOW;
		else if ((status & BSC_S_ERR) != 0)
			bi->bi_req_err = I2C_CTRL_E_NACK;
	}

	if (bi->bi_req_err != I2C_CTRL_E_OK) {
		if ((status & BSC_S_TA) == 0)
			bcm2711_i2c_io_error(bi, bi->bi_req_err);
		return;
	}

	/*
	 * The controller has a single FIFO for both directions. While data that
	 * we are writing is still in it, RXD is set, so we must not drain the
	 * FIFO until all of the write data has left it.
	 */
	bcm2711_i2c_fill_fifo(bi);
	if (!bi->bi_req_wsent && bi->bi_req_woff == req->ir_wlen &&
	    (bcm2711_i2c_read32(bi, BSC_R_S) & BSC_S_TXE) != 0) {
		bi->bi_req_wsent = true;
	}
	if (bi->bi_req_wsent)
		bcm2711_i2c_drain_fifo(bi);

	/*
	 * DONE may be set alongside the last of the received data, which is
	 * why we only look at it after draining the FIFO.
	 */
	if ((status & BSC_S_DONE) == 0)
		return;

	if (bi->bi_req_woff != req->ir_wlen ||
	    bi->bi_req_roff != req->ir_rlen) {
		dev_err(bi->bi_dip, CE_WARN, "!transfer completed after "
		    "%u/%u bytes written and %u/%u bytes read",
		    bi->bi_req_woff, req->ir_wlen,
		    bi->bi_req_roff, req->ir_rlen);
		bcm2711_i2c_io_error(bi, I2C_CTRL_E_INTERNAL);
		return;
	}

	i2c_ctrl_io_success(&bi->bi_req->ir_error);
	bi->bi_req_done = true;
}

/*
 * Wait for the transfer that has just been started to become active. This is
 * used to set up a repeated START, as described in the theory statement. We
 * return the status register so that the caller can tell whether the
 * transfer became active or failed.
 */
static uint32_t
bcm2711_i2c_wait_active(bcm2711_i2c_t *bi, hrtime_t deadline, uint32_t spin)
{
	uint32_t status;

	for (;;) {
		status = bcm2711_i2c_read32(bi, BSC_R_S);
		if ((status & (BSC_S_TA | BSC_S_CLEAR)) != 0 ||
		    gethrtime() >= deadline) {
			return (status);
		}
		drv_usecwait(spin);
	}
}

/*
 * Start a transfer. The control value is recorded so that the interrupt
 * conditions can be changed while the transfer is in progress.
 */
static void
bcm2711_i2c_start(bcm2711_i2c_t *bi, uint32_t ctl)
{
	bi->bi_req_ctl = ctl;
	bcm2711_i2c_write32(bi, BSC_R_C, ctl | BSC_C_ST);
}

/*
 * The interrupt conditions to enable for a transfer.
 */
static uint32_t
bcm2711_i2c_intr_ctl(const bcm2711_i2c_t *bi, bool read)
{
	if (bi->bi_req_poll)
		return (0);

	if (read)
		return (BSC_C_INTR | BSC_C_INTD);

	return (bi->bi_req_woff < bi->bi_req->ir_wlen ?
	    BSC_C_INTT | BSC_C_INTD : BSC_C_INTD);
}

/*
 * Once all of the data to be written is in the FIFO, the controller would
 * keep interrupting for more, so stop it. When the request is complete stop
 * all interrupts, as the line is level triggered and the status remains
 * until the controller is reset.
 */
static void
bcm2711_i2c_intr_update(bcm2711_i2c_t *bi)
{
	uint32_t ctl = bi->bi_req_ctl;

	if (bi->bi_req_done)
		ctl &= ~BSC_C_INT_ALL;
	else if (bi->bi_req_woff == bi->bi_req->ir_wlen)
		ctl &= ~BSC_C_INTT;

	if (ctl != bi->bi_req_ctl) {
		bi->bi_req_ctl = ctl;
		bcm2711_i2c_write32(bi, BSC_R_C, ctl);
	}
}

static bool
bcm2711_i2c_intr_pending(const bcm2711_i2c_t *bi, uint32_t status)
{
	const uint32_t ctl = bi->bi_req_ctl;

	if ((ctl & BSC_C_INT_ALL) == 0)
		return (false);

	return (((ctl & BSC_C_INTR) != 0 && (status & BSC_S_RXR) != 0) ||
	    ((ctl & BSC_C_INTT) != 0 && (status & BSC_S_TXW) != 0) ||
	    ((ctl & BSC_C_INTD) != 0 && (status & BSC_S_DONE) != 0) ||
	    (status & (BSC_S_ERR | BSC_S_CLKT)) != 0);
}

static uint_t
bcm2711_i2c_intr(caddr_t arg1, caddr_t arg2 __unused)
{
	bcm2711_i2c_t *bi = (bcm2711_i2c_t *)arg1;
	uint32_t status;

	mutex_enter(&bi->bi_mutex);
	if (bi->bi_req == NULL || bi->bi_req_poll)
		goto unclaimed;

	status = bcm2711_i2c_read32(bi, BSC_R_S);
	if (!bcm2711_i2c_intr_pending(bi, status))
		goto unclaimed;

	bi->bi_stat_intr++;
	if (!bi->bi_req_done)
		bcm2711_i2c_io(bi, status);
	bcm2711_i2c_intr_update(bi);
	if (bi->bi_req_done)
		cv_broadcast(&bi->bi_cv);
	mutex_exit(&bi->bi_mutex);

	return (DDI_INTR_CLAIMED);

unclaimed:
	bi->bi_stat_intr_unclaimed++;
	mutex_exit(&bi->bi_mutex);
	return (DDI_INTR_UNCLAIMED);
}

/*
 * Wait for the current request to complete or for the deadline to pass.
 */
static void
bcm2711_i2c_wait(bcm2711_i2c_t *bi, hrtime_t deadline, uint32_t spin)
{
	ASSERT(MUTEX_HELD(&bi->bi_mutex));

	if (bi->bi_req_poll) {
		while (!bi->bi_req_done && gethrtime() < deadline) {
			bcm2711_i2c_io(bi, bcm2711_i2c_read32(bi, BSC_R_S));
			if (!bi->bi_req_done)
				drv_usecwait(spin);
		}
		return;
	}

	while (!bi->bi_req_done) {
		hrtime_t now = gethrtime();

		if (now >= deadline)
			break;

		clock_t ticks = drv_usectohz(MIN(NSEC2USEC(deadline - now),
		    BCM2711_I2C_RECHECK_US));
		(void) cv_reltimedwait(&bi->bi_cv, &bi->bi_mutex, ticks,
		    TR_CLOCK_TICK);

		if (!bi->bi_req_done) {
			bcm2711_i2c_io(bi, bcm2711_i2c_read32(bi, BSC_R_S));
			if (bi->bi_req_done)
				bi->bi_stat_recheck++;
		}
	}
}

static void
bcm2711_i2c_io_i2c(void *arg, uint32_t port, i2c_req_t *req)
{
	bcm2711_i2c_t *bi = arg;

	ASSERT3U(port, ==, 0);

	if (req->ir_addr.ia_type != I2C_ADDR_7BIT ||
	    (req->ir_wlen == 0 && req->ir_rlen == 0)) {
		i2c_ctrl_io_error(&req->ir_error, I2C_CORE_E_CONTROLLER,
		    I2C_CTRL_E_UNSUP_CMD);
		return;
	}

	mutex_enter(&bi->bi_mutex);
	if (!bcm2711_i2c_bus_avail(bi)) {
		bcm2711_i2c_reset(bi);
		mutex_exit(&bi->bi_mutex);
		i2c_ctrl_io_error(&req->ir_error, I2C_CORE_E_CONTROLLER,
		    I2C_CTRL_E_BUS_BUSY);
		return;
	}

	ASSERT3P(bi->bi_req, ==, NULL);
	bi->bi_req = req;
	bi->bi_req_woff = 0;
	bi->bi_req_roff = 0;
	bi->bi_req_done = false;
	bi->bi_req_err = I2C_CTRL_E_OK;
	bi->bi_req_ctl = 0;
	bi->bi_req_poll = bi->bi_nintrs == 0 ||
	    (req->ir_flags & I2C_IO_REQ_F_POLL) != 0;
	bi->bi_req_wsent = req->ir_wlen == 0;

	const uint32_t to = i2c_ctrl_timeout_delay_us(bi->bi_hdl,
	    I2C_CTRL_TO_IO);
	const uint32_t spin = i2c_ctrl_timeout_delay_us(bi->bi_hdl,
	    I2C_CTRL_TO_POLL_CTRL);
	const hrtime_t deadline = gethrtime() + USEC2NSEC(to);

	/*
	 * Start from an empty FIFO with no stale status.
	 */
	bcm2711_i2c_write32(bi, BSC_R_C, BSC_C_I2CEN | BSC_C_CLEAR);
	bcm2711_i2c_write32(bi, BSC_R_S, BSC_S_CLEAR);
	bcm2711_i2c_write32(bi, BSC_R_A, req->ir_addr.ia_addr);

	if (req->ir_wlen > 0 && req->ir_rlen > 0) {
		bcm2711_i2c_write32(bi, BSC_R_DLEN, req->ir_wlen);
		bcm2711_i2c_start(bi, BSC_C_I2CEN);

		uint32_t status = bcm2711_i2c_wait_active(bi, deadline, spin);
		if ((status & BSC_S_TA) == 0 ||
		    (status & (BSC_S_ERR | BSC_S_CLKT)) != 0) {
			/*
			 * The write did not become active or has already
			 * failed, most likely because the target did not
			 * acknowledge its address. Let the common code report
			 * the error once the transfer has finished.
			 */
			bcm2711_i2c_io(bi, status);
			goto wait;
		}

		bcm2711_i2c_write32(bi, BSC_R_DLEN, req->ir_rlen);
		bcm2711_i2c_start(bi, BSC_C_I2CEN | BSC_C_READ |
		    bcm2711_i2c_intr_ctl(bi, true));

		/*
		 * The write data goes into the FIFO now that the read has been
		 * started. We do this here rather than from the interrupt
		 * handler, and wait for the data to leave the FIFO, so that the
		 * handler only has to deal with the read.
		 */
		while (!bi->bi_req_wsent && !bi->bi_req_done &&
		    gethrtime() < deadline) {
			bcm2711_i2c_io(bi, bcm2711_i2c_read32(bi, BSC_R_S));
			if (!bi->bi_req_wsent && !bi->bi_req_done)
				drv_usecwait(spin);
		}
	} else if (req->ir_wlen > 0) {
		bcm2711_i2c_write32(bi, BSC_R_DLEN, req->ir_wlen);
		bcm2711_i2c_fill_fifo(bi);
		bcm2711_i2c_start(bi, BSC_C_I2CEN | bcm2711_i2c_intr_ctl(bi,
		    false));
	} else {
		bcm2711_i2c_write32(bi, BSC_R_DLEN, req->ir_rlen);
		bcm2711_i2c_start(bi, BSC_C_I2CEN | BSC_C_READ |
		    bcm2711_i2c_intr_ctl(bi, true));
	}

wait:
	bcm2711_i2c_wait(bi, deadline, spin);

	/*
	 * DTrace is not available on every platform that this runs on, so
	 * record the state of the controller when a request times out. DLEN
	 * holds the number of bytes left in the current transfer. If the
	 * controller had already reported an error, but the transfer never
	 * finished, report that error rather than the timeout.
	 */
	if (!bi->bi_req_done) {
		dev_err(bi->bi_dip, CE_WARN, "!request to address 0x%x timed "
		    "out with status 0x%x and DLEN %u after %u/%u bytes "
		    "written and %u/%u bytes read", req->ir_addr.ia_addr,
		    bcm2711_i2c_read32(bi, BSC_R_S),
		    bcm2711_i2c_read32(bi, BSC_R_DLEN),
		    bi->bi_req_woff, req->ir_wlen,
		    bi->bi_req_roff, req->ir_rlen);
		bcm2711_i2c_io_error(bi, bi->bi_req_err != I2C_CTRL_E_OK ?
		    bi->bi_req_err : I2C_CTRL_E_REQ_TO);
	}

	/*
	 * Whatever the outcome, leave the controller idle for the next
	 * request. After an error or a timeout this also aborts any transfer
	 * that is still in progress.
	 */
	bcm2711_i2c_reset(bi);

	bi->bi_req = NULL;
	bi->bi_req_woff = 0;
	bi->bi_req_roff = 0;
	bi->bi_req_done = false;
	bi->bi_req_err = I2C_CTRL_E_OK;
	bi->bi_req_ctl = 0;
	mutex_exit(&bi->bi_mutex);
}

static const i2c_ctrl_ops_t bcm2711_i2c_ctrl_ops = {
	.i2c_port_name_f = i2c_ctrl_port_name_portno,
	.i2c_io_i2c_f = bcm2711_i2c_io_i2c,
	.i2c_prop_info_f = bcm2711_i2c_prop_info,
	.i2c_prop_get_f = bcm2711_i2c_prop_get,
	.i2c_prop_set_f = bcm2711_i2c_prop_set
};

static bool
bcm2711_i2c_regs_setup(bcm2711_i2c_t *bi)
{
	int ret;
	ddi_device_acc_attr_t attr;

	bzero(&attr, sizeof (attr));
	attr.devacc_attr_version = DDI_DEVICE_ATTR_V1;
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
	attr.devacc_attr_access = DDI_DEFAULT_ACC;

	if (ddi_dev_regsize(bi->bi_dip, 0, &bi->bi_regsize) != DDI_SUCCESS) {
		dev_err(bi->bi_dip, CE_WARN, "failed to get regs[0] size");
		return (false);
	}

	if (bi->bi_regsize <= BSC_R_CLKT) {
		dev_err(bi->bi_dip, CE_WARN, "regs[0] is too small: 0x%lx",
		    bi->bi_regsize);
		return (false);
	}

	ret = ddi_regs_map_setup(bi->bi_dip, 0, &bi->bi_base, 0,
	    bi->bi_regsize, &attr, &bi->bi_regs);
	if (ret != DDI_SUCCESS) {
		dev_err(bi->bi_dip, CE_WARN, "failed to map regs[0]: %d", ret);
		return (false);
	}

	bi->bi_init |= BCM2711_I2C_INIT_REGS;
	return (true);
}

/*
 * Set up our interrupt, if we have one. Without an interrupt we poll for
 * completion, so its absence is not fatal. Failing to set up an interrupt
 * that does exist is.
 */
static bool
bcm2711_i2c_intr_setup(bcm2711_i2c_t *bi)
{
	int types, count, ret;

	bi->bi_nintrs = 0;
	bi->bi_intr_pri = 0;

	if (ddi_intr_get_supported_types(bi->bi_dip, &types) != DDI_SUCCESS ||
	    (types & DDI_INTR_TYPE_FIXED) == 0 ||
	    ddi_intr_get_nintrs(bi->bi_dip, DDI_INTR_TYPE_FIXED, &count) !=
	    DDI_SUCCESS || count == 0) {
		dev_err(bi->bi_dip, CE_WARN,
		    "!no interrupt available, polling for completion");
		return (true);
	}

	ret = ddi_intr_alloc(bi->bi_dip, &bi->bi_intr_hdl, DDI_INTR_TYPE_FIXED,
	    0, 1, &bi->bi_nintrs, DDI_INTR_ALLOC_STRICT);
	if (ret != DDI_SUCCESS) {
		dev_err(bi->bi_dip, CE_WARN,
		    "failed to allocate interrupt: %d", ret);
		bi->bi_nintrs = 0;
		return (false);
	}
	bi->bi_init |= BCM2711_I2C_INIT_INTR_ALLOC;

	ret = ddi_intr_add_handler(bi->bi_intr_hdl, bcm2711_i2c_intr, bi,
	    NULL);
	if (ret != DDI_SUCCESS) {
		dev_err(bi->bi_dip, CE_WARN,
		    "failed to add interrupt handler: %d", ret);
		return (false);
	}
	bi->bi_init |= BCM2711_I2C_INIT_INTR_HDL;

	ret = ddi_intr_get_pri(bi->bi_intr_hdl, &bi->bi_intr_pri);
	if (ret != DDI_SUCCESS) {
		dev_err(bi->bi_dip, CE_WARN,
		    "failed to get interrupt priority: %d", ret);
		return (false);
	}

	return (true);
}

static bool
bcm2711_i2c_enable_intr(bcm2711_i2c_t *bi)
{
	if (bi->bi_nintrs == 0)
		return (true);

	int ret = ddi_intr_enable(bi->bi_intr_hdl);
	if (ret != DDI_SUCCESS) {
		dev_err(bi->bi_dip, CE_WARN,
		    "failed to enable interrupt: %d", ret);
		return (false);
	}

	bi->bi_init |= BCM2711_I2C_INIT_INTR_EN;
	return (true);
}

/*
 * Switch our pins to their I2C function. Failure here is not fatal as the
 * firmware or the boot loader may already have done this.
 */
static void
bcm2711_i2c_pins_setup(bcm2711_i2c_t *bi)
{
	if (&plat_pinctrl_select == NULL)
		return;

	int ret = plat_pinctrl_select(bi->bi_dip, "default");
	if (ret != 0) {
		dev_err(bi->bi_dip, CE_WARN,
		    "!failed to apply default pin configuration: %d", ret);
	}
}

/*
 * Bring the controller to a known state, with all of its interrupt
 * conditions disabled. The initial bus speed comes from the device tree.
 */
static bool
bcm2711_i2c_ctrl_init(bcm2711_i2c_t *bi)
{
	i2c_speed_t speed;

	bcm2711_i2c_reset(bi);
	bi->bi_init |= BCM2711_I2C_INIT_CTRL;

	int freq = ddi_prop_get_int(DDI_DEV_T_ANY, bi->bi_dip,
	    DDI_PROP_DONTPASS, "clock-frequency", BCM2711_I2C_DEF_FREQ);
	switch (freq) {
	case 100000:
		speed = I2C_SPEED_STD;
		break;
	case 400000:
		speed = I2C_SPEED_FAST;
		break;
	default:
		dev_err(bi->bi_dip, CE_WARN,
		    "!unsupported clock-frequency %d, using %u",
		    freq, BCM2711_I2C_DEF_FREQ);
		speed = I2C_SPEED_STD;
		break;
	}

	mutex_enter(&bi->bi_mutex);
	bool ret = bcm2711_i2c_set_speed(bi, speed);
	mutex_exit(&bi->bi_mutex);

	return (ret);
}

static bool
bcm2711_i2c_register(bcm2711_i2c_t *bi)
{
	i2c_ctrl_reg_error_t ret;
	i2c_ctrl_register_t *reg;

	ret = i2c_ctrl_register_alloc(I2C_CTRL_PROVIDER, &reg);
	if (ret != I2C_CTRL_REG_E_OK) {
		dev_err(bi->bi_dip, CE_WARN, "failed to allocate i2c "
		    "controller registration structure: 0x%x", ret);
		return (false);
	}

	reg->ic_type = I2C_CTRL_TYPE_I2C;
	reg->ic_nports = 1;
	reg->ic_dip = bi->bi_dip;
	reg->ic_drv = bi;
	reg->ic_ops = &bcm2711_i2c_ctrl_ops;

	ret = i2c_ctrl_register(reg, &bi->bi_hdl);
	i2c_ctrl_register_free(reg);
	if (ret != I2C_CTRL_REG_E_OK) {
		dev_err(bi->bi_dip, CE_WARN,
		    "failed to register with i2c framework: 0x%x", ret);
		return (false);
	}

	bi->bi_init |= BCM2711_I2C_INIT_I2C;
	return (true);
}

static void
bcm2711_i2c_cleanup(bcm2711_i2c_t *bi)
{
	int ret;

	if ((bi->bi_init & BCM2711_I2C_INIT_INTR_EN) != 0) {
		if ((ret = ddi_intr_disable(bi->bi_intr_hdl)) != DDI_SUCCESS) {
			dev_err(bi->bi_dip, CE_WARN,
			    "failed to disable interrupt: %d", ret);
		}
		bi->bi_init &= ~BCM2711_I2C_INIT_INTR_EN;
	}

	if ((bi->bi_init & BCM2711_I2C_INIT_CTRL) != 0) {
		bcm2711_i2c_write32(bi, BSC_R_C, BSC_C_CLEAR);
		bcm2711_i2c_write32(bi, BSC_R_S, BSC_S_CLEAR);
		bi->bi_init &= ~BCM2711_I2C_INIT_CTRL;
	}

	if ((bi->bi_init & BCM2711_I2C_INIT_INTR_HDL) != 0) {
		if ((ret = ddi_intr_remove_handler(bi->bi_intr_hdl)) !=
		    DDI_SUCCESS) {
			dev_err(bi->bi_dip, CE_WARN,
			    "failed to remove interrupt handler: %d", ret);
		}
		bi->bi_init &= ~BCM2711_I2C_INIT_INTR_HDL;
	}

	if ((bi->bi_init & BCM2711_I2C_INIT_INTR_ALLOC) != 0) {
		if ((ret = ddi_intr_free(bi->bi_intr_hdl)) != DDI_SUCCESS) {
			dev_err(bi->bi_dip, CE_WARN,
			    "failed to free interrupt: %d", ret);
		}
		bi->bi_init &= ~BCM2711_I2C_INIT_INTR_ALLOC;
	}

	if ((bi->bi_init & BCM2711_I2C_INIT_SYNC) != 0) {
		cv_destroy(&bi->bi_cv);
		mutex_destroy(&bi->bi_mutex);
		bi->bi_init &= ~BCM2711_I2C_INIT_SYNC;
	}

	if ((bi->bi_init & BCM2711_I2C_INIT_REGS) != 0) {
		ddi_regs_map_free(&bi->bi_regs);
		bi->bi_base = NULL;
		bi->bi_regsize = 0;
		bi->bi_init &= ~BCM2711_I2C_INIT_REGS;
	}

	ASSERT0(bi->bi_init);
	ddi_set_driver_private(bi->bi_dip, NULL);
	kmem_free(bi, sizeof (bcm2711_i2c_t));
}

static int
bcm2711_i2c_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	bcm2711_i2c_t *bi;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
	default:
		return (DDI_FAILURE);
	}

	bi = kmem_zalloc(sizeof (bcm2711_i2c_t), KM_SLEEP);
	bi->bi_dip = dip;
	ddi_set_driver_private(dip, bi);

	if (!bcm2711_i2c_regs_setup(bi))
		goto cleanup;

	if (!bcm2711_i2c_intr_setup(bi))
		goto cleanup;

	mutex_init(&bi->bi_mutex, NULL, MUTEX_DRIVER,
	    DDI_INTR_PRI(bi->bi_intr_pri));
	cv_init(&bi->bi_cv, NULL, CV_DRIVER, NULL);
	bi->bi_init |= BCM2711_I2C_INIT_SYNC;

	bcm2711_i2c_pins_setup(bi);

	if (!bcm2711_i2c_ctrl_init(bi))
		goto cleanup;

	if (!bcm2711_i2c_enable_intr(bi))
		goto cleanup;

	if (!bcm2711_i2c_register(bi))
		goto cleanup;

	return (DDI_SUCCESS);

cleanup:
	bcm2711_i2c_cleanup(bi);
	return (DDI_FAILURE);
}

static int
bcm2711_i2c_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	bcm2711_i2c_t *bi;

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
	default:
		return (DDI_FAILURE);
	}

	bi = ddi_get_driver_private(dip);
	if (bi == NULL) {
		dev_err(dip, CE_WARN,
		    "asked to detach, but missing private data");
		return (DDI_FAILURE);
	}

	VERIFY3P(bi->bi_dip, ==, dip);
	i2c_ctrl_reg_error_t ret = i2c_ctrl_unregister(bi->bi_hdl);
	if (ret != I2C_CTRL_REG_E_OK) {
		dev_err(dip, CE_WARN,
		    "failed to unregister from i2c framework: 0x%x", ret);
		return (DDI_FAILURE);
	}
	bi->bi_init &= ~BCM2711_I2C_INIT_I2C;
	bcm2711_i2c_cleanup(bi);

	return (DDI_SUCCESS);
}

/*
 * Disable the controller, aborting any transfer in progress.
 */
static int
bcm2711_i2c_quiesce(dev_info_t *dip)
{
	bcm2711_i2c_t *bi = ddi_get_driver_private(dip);

	if (bi == NULL)
		return (DDI_FAILURE);

	bcm2711_i2c_write32(bi, BSC_R_C, BSC_C_CLEAR);
	return (DDI_SUCCESS);
}

static struct dev_ops bcm2711_i2c_dev_ops = {
	.devo_rev = DEVO_REV,
	.devo_refcnt = 0,
	.devo_identify = nulldev,
	.devo_probe = nulldev,
	.devo_attach = bcm2711_i2c_attach,
	.devo_detach = bcm2711_i2c_detach,
	.devo_reset = nodev,
	.devo_quiesce = bcm2711_i2c_quiesce,
};

static struct modldrv bcm2711_i2c_modldrv = {
	.drv_modops = &mod_driverops,
	.drv_linkinfo = "BCM2711 BSC I2C Controller",
	.drv_dev_ops = &bcm2711_i2c_dev_ops
};

static struct modlinkage bcm2711_i2c_modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &bcm2711_i2c_modldrv, NULL }
};

int
_init(void)
{
	int ret;

	i2c_ctrl_mod_init(&bcm2711_i2c_dev_ops);
	if ((ret = mod_install(&bcm2711_i2c_modlinkage)) != 0)
		i2c_ctrl_mod_fini(&bcm2711_i2c_dev_ops);

	return (ret);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&bcm2711_i2c_modlinkage, modinfop));
}

int
_fini(void)
{
	int ret;

	if ((ret = mod_remove(&bcm2711_i2c_modlinkage)) == 0)
		i2c_ctrl_mod_fini(&bcm2711_i2c_dev_ops);

	return (ret);
}
