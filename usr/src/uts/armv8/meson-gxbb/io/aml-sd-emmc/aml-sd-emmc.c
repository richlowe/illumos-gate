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
 * Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2017 Hayashi Naoyuki
 */

#include <sys/promif.h>
#include <sys/byteorder.h>
#include <sys/debug.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/ddi_impldefs.h>
#include <sys/sysmacros.h>
#include <sys/platmod.h>
#include <sys/gpio.h>
#include <sys/mmcreg.h>
#include <sys/sdcard/sda.h>
#include <sys/callo.h>
#include <sys/ddi_subrdefs.h>
#include "aml-sd-emmc.h"

#define MMC_BUFFER_SIZE 0x10000
#define MMC_REQUESTS_MAX 0x20

struct mmc_request {
	list_node_t node;
	struct mmc_sc *sc;
	bd_xfer_t *xfer;
};

static void
usecwait(int usec)
{
	drv_usecwait(usec);
}

static int
mmc_pinmux(pnode_t node, const char *pinname)
{
	int len;
	int pinctrl_index = prom_get_prop_index(node, "pinctrl-names", pinname);
	if (pinctrl_index < 0)
		return -1;
	char buf[80];
	sprintf(buf, "pinctrl-%d", pinctrl_index);

	len = prom_getproplen(node, buf);
	if (len != sizeof(uint32_t))
		return -1;
	uint32_t pinctrl;
	prom_getprop(node, buf, (caddr_t)&pinctrl);
	pnode_t pinctrl_node;
	pinctrl_node = prom_findnode_by_phandle(ntohl(pinctrl));
	if (pinctrl_node < 0)
		return -1;

	return plat_pinmux_set(pinctrl_node);
}

static int
init_mmc_gpio(pnode_t node, const char *name, struct gpio_ctrl *gpio)
{
	uint32_t gpio_buf[3];
	int len = prom_getproplen(node, name);
	if (len != sizeof(gpio_buf))
		return -1;
	prom_getprop(node, name, (caddr_t)&gpio_buf);
	gpio->node = prom_findnode_by_phandle(ntohl(gpio_buf[0]));
	gpio->pin = ntohl(gpio_buf[1]);
	gpio->flags = ntohl(gpio_buf[2]);

	return 0;
}

static uint32_t
mmc_extract_bits(uint32_t *bits, int hi, int len)
{
	uint32_t val = 0;

	for (int i = hi; i >= (hi - len + 1); i--) {
		val = (val << 1) | (((bits[i / 32]) >> (i % 32)) & 1);
	}

	return val;
}

static void
mmc_reg_write(struct mmc_sc *sc, size_t offset, uint32_t val)
{
	*(volatile uint32_t *)(sc->base + offset) = val;
}

static uint32_t
mmc_reg_read(struct mmc_sc *sc, size_t offset)
{
	uint32_t val = *(volatile uint32_t *)(sc->base + offset);
	return val;
}

static void
mmc_stop(struct mmc_sc *sc)
{
	union sd_emmc_start _start = { mmc_reg_read(sc, SD_EMMC_START) };
	_start.Desc_busy = 0;
	mmc_reg_write(sc, SD_EMMC_START, _start.dw);

	for (;;) {
		union sd_emmc_status _status = { mmc_reg_read(
		    sc, SD_EMMC_STATUS) };
		if (_status.Desc_Busy == 0 && _status.Core_Busy == 0)
			break;
	}
}

static int mmc_set_mode(struct mmc_sc *sc, int mode);
static void mmc_reset_tune(struct mmc_sc *sc);
static int mmc_retune(struct mmc_sc *sc);

static uint32_t
mmc_wait_intr(struct mmc_sc *sc, uint32_t mask, uint64_t usec)
{
	union sd_emmc_status _status;
	uint32_t val;
	{
		mutex_enter(&sc->intrlock);

		hrtime_t timeout = gethrtime() + USEC2NSEC(usec);
		_status.dw = mask;
		_status.error = 0x1fff;
		boolean_t timeout_occurred = B_FALSE;
		for (;;) {
			val = (sc->interrupted & _status.dw);
			if (val != 0 || timeout_occurred)
				break;

			if (cv_timedwait_hires(&sc->waitcv, &sc->intrlock,
				timeout, USEC2NSEC(1),
				CALLOUT_FLAG_ABSOLUTE) < 0)
				timeout_occurred = B_TRUE;
		}

		sc->interrupted &= ~val;

		mutex_exit(&sc->intrlock);
	}

	_status.dw = val;
	if (_status.error || val == 0) {
		if (!sc->in_tuning) {
			cmn_err(CE_WARN, "mmc_wait_intr() error %08x", val);
		}
		mmc_stop(sc);
		val = 0;

		{
			mutex_enter(&sc->intrlock);

			union sd_emmc_status status_clear = { 0 };
			status_clear.error = 0x1fff;
			status_clear.End_of_Chain = 1;
			status_clear.Resp_status = 1;
			status_clear.IRQ_sdio = 1;
			uint32_t val = mmc_reg_read(sc, SD_EMMC_STATUS);
			mmc_reg_write(
			    sc, SD_EMMC_STATUS, val & status_clear.dw);

			sc->interrupted = val & ~status_clear.dw;

			mutex_exit(&sc->intrlock);
		}

		if (sc->tuning_enable) {
			if (mmc_retune(sc) != 0)
				mmc_set_mode(sc, 1); // HiSpeed mode
		}
	}
	return val;
}

static int
mmc_wait_state_idle(
    struct mmc_sc *sc, uint32_t mask, uint32_t val, uint64_t usec)
{
	hrtime_t timeout = gethrtime() + USEC2NSEC(usec);
	for (;;) {
		boolean_t timeout_occurred = (gethrtime() > timeout);
		if ((mmc_reg_read(sc, SD_EMMC_STATUS) & mask) == val)
			return 0;
		if (timeout_occurred)
			break;
		usecwait(200);
	}

	cmn_err(CE_WARN, "mmc_wait_state_idle() timeout");
	mmc_stop(sc);

	return -1;
}

static int
mmc_start_cmd(struct mmc_sc *sc, struct sda_cmd *cmd)
{
	union sd_emmc_status status_mask = { 0 };
	status_mask.CMD_i = 1;
	if ((cmd->sc_flags & (SDA_CMDF_READ | SDA_CMDF_WRITE)) && cmd->sc_ndmac)
		status_mask.DAT_i = 0xf;
	else if (cmd->sc_rtype & Rb)
		status_mask.DAT_i = 0xf;

	if (mmc_wait_state_idle(sc, status_mask.dw, status_mask.dw, 1000000) !=
	    0)
		return -1;

	union sd_emmc_cmd_cfg cmd_cfg = { 0 };

	cmd_cfg.Cmd_index = cmd->sc_index;
	switch (cmd->sc_rtype) {
	case R0:
		cmd_cfg.No_resp = 1;
		break;
	case R1b:
		cmd_cfg.R1b = 1;
		break;
	case R2:
		cmd_cfg.Resp_128 = 1;
		break;
	case R3:
	case R4:
		cmd_cfg.Resp_nocrc = 1;
		break;
	default:
		break;
	}

	uint32_t data = 0;
	if (cmd->sc_flags & (SDA_CMDF_READ | SDA_CMDF_WRITE)) {
		cmd_cfg.Data_io = 1;
		cmd_cfg.Data_wr = !!(cmd->sc_flags & SDA_CMDF_WRITE);
		if (cmd->sc_nblks > 1) {
			cmd_cfg.Block_mode = 1;
			cmd_cfg.Length = cmd->sc_nblks;
		} else {
			cmd_cfg.Block_mode = 0;
			cmd_cfg.Length = cmd->sc_blksz;
		}
		ASSERT(cmd->sc_ndmac == 1);
		data = (uint32_t)cmd->sc_dmac.dmac_laddress;
	}

	cmd_cfg.End_of_chain = 1;
	cmd_cfg.Owner = 1;

	mmc_reg_write(sc, SD_EMMC_CMD_CFG, cmd_cfg.dw);
	mmc_reg_write(sc, SD_EMMC_CMD_DAT, data);
	mmc_reg_write(sc, SD_EMMC_CMD_ARG, cmd->sc_argument); // start

	return 0;
}

static int
mmc_wait_cmd_done(struct mmc_sc *sc, struct sda_cmd *cmd)
{
	union sd_emmc_status cmd_done = { 0 };
	cmd_done.End_of_Chain = 1;

	if (mmc_wait_intr(sc, cmd_done.dw, 1000000) == 0)
		return -1;

	switch (cmd->sc_rtype) {
	case R0:
		break;
	case R2:
		cmd->sc_response[0] = mmc_reg_read(sc, SD_EMMC_CMD_RSP0);
		cmd->sc_response[1] = mmc_reg_read(sc, SD_EMMC_CMD_RSP1);
		cmd->sc_response[2] = mmc_reg_read(sc, SD_EMMC_CMD_RSP2);
		cmd->sc_response[3] = mmc_reg_read(sc, SD_EMMC_CMD_RSP3);
		break;
	default:
		cmd->sc_response[0] = mmc_reg_read(sc, SD_EMMC_CMD_RSP0);
		break;
	}

	return 0;
}

static void
mmc_set_sd_clock(struct mmc_sc *sc, int enable)
{
	union sd_emmc_cfg _cfg = { mmc_reg_read(sc, SD_EMMC_CFG) };
	_cfg.Cfg_stop_clk = (enable ? 0 : 1);
	mmc_reg_write(sc, SD_EMMC_CFG, _cfg.dw);
}

static uint32_t sd_emmc_clocks[] = {
	24000000,
	1000000000,
};

static void
mmc_set_clock(struct mmc_sc *sc, int clock)
{
	union sd_emmc_clock _clock = { mmc_reg_read(sc, SD_EMMC_CLOCK) };
	union sd_emmc_cfg _cfg = { mmc_reg_read(sc, SD_EMMC_CFG) };
	int stop_clk = _cfg.Cfg_stop_clk;

	mmc_set_sd_clock(sc, 0);

	_cfg.dw = mmc_reg_read(sc, SD_EMMC_CFG);
	_cfg.Cfg_ddr = (sc->ddr ? 1 : 0);
	mmc_reg_write(sc, SD_EMMC_CFG, _cfg.dw);

	int clk_src = ((clock > 12000000) ? 1 : 0);
	_clock.Cfg_src = clk_src;
	_clock.Cfg_div = sd_emmc_clocks[clk_src] / clock;
	mmc_reg_write(sc, SD_EMMC_CLOCK, _clock.dw);
	sc->clock = clock;

	mmc_set_sd_clock(sc, stop_clk == 0);
}

static int
mmc_set_voltage(
    struct mmc_sc *sc, struct gpio_ctrl *gpio_volsw, uint32_t voltage)
{
	if (voltage != 3300000 && voltage != 1800000)
		return -1;

	union sd_emmc_cfg _cfg = { mmc_reg_read(sc, SD_EMMC_CFG) };
	_cfg.Cfg_stop_clk = 1;
	mmc_reg_write(sc, SD_EMMC_CFG, _cfg.dw);
	usecwait(5000);

	if (plat_gpio_direction_output(gpio_volsw, voltage == 1800000) < 0)
		return -1;
	usecwait(5000);

	_cfg.Cfg_stop_clk = 0;
	mmc_reg_write(sc, SD_EMMC_CFG, _cfg.dw);
	usecwait(5000);

	return 0;
}

// clang-format off
static const uint8_t tuning_blk_pattern[] = {
    0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
    0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
    0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
    0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
    0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
    0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
    0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
    0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};
// clang-format on

static int mmc_send_cmd(struct mmc_sc *sc, struct sda_cmd *cmd);
static int
mmc_retune(struct mmc_sc *sc)
{
	sc->tuning_enable = B_FALSE;
	sc->in_tuning = B_TRUE;

	union sd_emmc_adjust _adjust = { mmc_reg_read(sc, SD_EMMC_ADJUST) };
	_adjust.Adj_enable = 0;
	_adjust.Adj_delay = 0;
	mmc_reg_write(sc, SD_EMMC_ADJUST, _adjust.dw);

	_adjust.dw = mmc_reg_read(sc, SD_EMMC_ADJUST);
	_adjust.Adj_enable = 1;
	mmc_reg_write(sc, SD_EMMC_ADJUST, _adjust.dw);

	union sd_emmc_clock _clock = { mmc_reg_read(sc, SD_EMMC_CLOCK) };
	int clk_src =
	    (_clock.Cfg_src == 0 ? sd_emmc_clocks[0] : sd_emmc_clocks[1]);
	int max_delay = (clk_src + sc->clock - 1) / sc->clock;
	usecwait(1000);

	int ok_hi = -1;
	int ok_lo = max_delay;
	for (int i = 0; i < max_delay; i++) {
		_adjust.Adj_delay = i;
		_adjust.Adj_enable = 0;
		mmc_reg_write(sc, SD_EMMC_ADJUST, _adjust.dw);
		usecwait(1000);
		_adjust.dw = mmc_reg_read(sc, SD_EMMC_ADJUST);
		_adjust.Adj_enable = 1;
		mmc_reg_write(sc, SD_EMMC_ADJUST, _adjust.dw);
		usecwait(1000);

		struct sda_cmd cmd = {
			.sc_index = 19,
			.sc_rtype = R1,

			.sc_nblks = 1,
			.sc_blksz = sizeof(tuning_blk_pattern),
			.sc_flags = SDA_CMDF_READ,
			.sc_dmah = sc->buf_dmah,
			.sc_ndmac = 1,
			.sc_dmac = sc->buf_dmac,
		};
		if (mmc_send_cmd(sc, &cmd) == 0) {
			ddi_dma_sync(sc->buf_dmah, 0,
			    sizeof(tuning_blk_pattern), DDI_DMA_SYNC_FORKERNEL);
			if (memcmp(tuning_blk_pattern, sc->buffer,
				sizeof(tuning_blk_pattern)) == 0) {
				if (i > ok_hi)
					ok_hi = i;
				if (i < ok_lo)
					ok_lo = i;
			}
		}
	}
	if (ok_hi >= 0) {
		_adjust.Adj_delay = (ok_hi + ok_lo) / 2;
		mmc_reg_write(sc, SD_EMMC_ADJUST, _adjust.dw);
		usecwait(1000);
		sc->tuning_enable = B_TRUE;
		sc->in_tuning = B_FALSE;
		return 0;
	}

	_adjust.dw = mmc_reg_read(sc, SD_EMMC_ADJUST);
	_adjust.Adj_enable = 0;
	_adjust.Adj_delay = 0;
	mmc_reg_write(sc, SD_EMMC_ADJUST, _adjust.dw);
	sc->in_tuning = B_FALSE;

	return -1;
}

static int mmc_stop_transmission(struct mmc_sc *);

static int
mmc_send_cmd(struct mmc_sc *sc, struct sda_cmd *cmd)
{
	if (mmc_start_cmd(sc, cmd) != 0)
		goto err_exit;

	if (mmc_wait_cmd_done(sc, cmd) != 0)
		goto err_exit;

	if (cmd->sc_index == CMD_STOP_TRANSMIT)
		mmc_stop(sc);

	if (cmd->sc_index == CMD_READ_MULTI ||
	    cmd->sc_index == CMD_WRITE_MULTI) {
		mmc_stop_transmission(sc);
	}

	return 0;
err_exit:
	if (cmd->sc_index == CMD_READ_MULTI ||
	    cmd->sc_index == CMD_WRITE_MULTI) {
		mmc_stop_transmission(sc);
	}
	return -1;
}

static int
mmc_go_idle_state(struct mmc_sc *sc)
{
	struct sda_cmd cmd = {
		.sc_index = CMD_GO_IDLE,
		.sc_rtype = R0,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;
	return 0;
}

static int
mmc_send_if_cond(struct mmc_sc *sc)
{
	struct sda_cmd cmd = {
		.sc_index = CMD_SEND_IF_COND,
		.sc_rtype = R7,
		.sc_argument = ((!!(sc->ocr_avail & OCR_HI_MASK)) << 8) | 0xaa,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;
	if ((cmd.sc_response[0] & 0xff) != 0xaa)
		return -1;
	return 0;
}

static int
mmc_sd_send_ocr(struct mmc_sc *sc)
{
	struct sda_cmd acmd = {
		.sc_index = CMD_APP_CMD,
		.sc_rtype = R1,
	};
	if (mmc_send_cmd(sc, &acmd) != 0)
		return -1;

	uint32_t ocr = (sc->ocr_avail & OCR_HI_MASK) | OCR_CCS;

	ocr |= OCR_S18R;

	struct sda_cmd cmd = {
		.sc_index = ACMD_SD_SEND_OCR,
		.sc_rtype = R3,
		.sc_argument = ocr,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;
	if (cmd.sc_response[0] & OCR_POWER_UP)
		sc->ocr = cmd.sc_response[0];

	return 0;
}

static int
mmc_voltage_switch(struct mmc_sc *sc, struct gpio_ctrl *gpio_volsw)
{
	struct sda_cmd cmd = {
		.sc_index = CMD_VOLTAGE_SWITCH,
		.sc_rtype = R1,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;

	usecwait(1000);

	if (mmc_set_voltage(sc, gpio_volsw, 1800000) < 0)
		return -1;

	sc->vdd = 1800000;

	return 0;
}

static int
mmc_all_send_cid(struct mmc_sc *sc)
{
	struct sda_cmd cmd = {
		.sc_index = CMD_BCAST_CID,
		.sc_rtype = R2,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;

	memcpy(sc->cid, cmd.sc_response, sizeof(sc->cid));

	return 0;
}

static int
mmc_send_relative_addr(struct mmc_sc *sc)
{
	struct sda_cmd cmd = {
		.sc_index = CMD_SEND_RCA,
		.sc_rtype = R6,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;

	sc->rca = (cmd.sc_response[0] >> 16) & 0xffff;

	return 0;
}

static int
mmc_send_csd(struct mmc_sc *sc)
{
	struct sda_cmd cmd = {
		.sc_index = CMD_SEND_CSD,
		.sc_rtype = R2,
		.sc_argument = sc->rca << 16,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;

	memcpy(sc->csd, cmd.sc_response, sizeof(sc->csd));

	return 0;
}

static int
mmc_select_card(struct mmc_sc *sc)
{
	struct sda_cmd cmd = {
		.sc_index = CMD_SELECT_CARD,
		.sc_rtype = R1,
		.sc_argument = sc->rca << 16,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;

	return 0;
}

static int
mmc_send_scr(struct mmc_sc *sc)
{
	struct sda_cmd acmd = {
		.sc_index = CMD_APP_CMD,
		.sc_rtype = R1,
		.sc_argument = sc->rca << 16,
	};
	if (mmc_send_cmd(sc, &acmd) != 0)
		return -1;

	struct sda_cmd cmd = {
		.sc_index = ACMD_SEND_SCR,
		.sc_rtype = R1,

		.sc_nblks = 1,
		.sc_blksz = 8,
		.sc_flags = SDA_CMDF_READ,
		.sc_dmah = sc->buf_dmah,
		.sc_ndmac = 1,
		.sc_dmac = sc->buf_dmac,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;

	ddi_dma_sync(sc->buf_dmah, 0, sizeof(sc->scr), DDI_DMA_SYNC_FORKERNEL);

	for (int i = 0; i < ARRAY_SIZE(sc->scr); i++)
		sc->scr[i] = ntohl(*(uint32_t *)(sc->buffer +
		    sizeof(sc->scr) * (ARRAY_SIZE(sc->scr) - 1 - i)));

	return 0;
}

static int
mmc_swtch_func(struct mmc_sc *sc, uint32_t argument)
{
	struct sda_cmd cmd = {
		.sc_index = CMD_SWITCH_FUNC,
		.sc_rtype = R1,
		.sc_argument = argument,

		.sc_nblks = 1,
		.sc_blksz = 64,
		.sc_flags = SDA_CMDF_READ,
		.sc_dmah = sc->buf_dmah,
		.sc_ndmac = 1,
		.sc_dmac = sc->buf_dmac,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;

	ddi_dma_sync(
	    sc->buf_dmah, 0, sizeof(sc->func_status), DDI_DMA_SYNC_FORKERNEL);

	for (int i = 0; i < ARRAY_SIZE(sc->func_status); i++)
		sc->func_status[i] = ntohl(*(uint32_t *)(sc->buffer +
		    sizeof(sc->func_status[0]) *
			(ARRAY_SIZE(sc->func_status) - 1 - i)));

	return 0;
}

static int
mmc_stop_transmission(struct mmc_sc *sc)
{
	struct sda_cmd cmd = {
		.sc_index = CMD_STOP_TRANSMIT,
		.sc_rtype = R1b,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;
	return 0;
}

static int
mmc_set_bus_width(struct mmc_sc *sc, int width)
{
	ASSERT(width == 1 || width == 4);
	struct sda_cmd acmd = {
		.sc_index = CMD_APP_CMD,
		.sc_rtype = R1,
		.sc_argument = sc->rca << 16,
	};
	if (mmc_send_cmd(sc, &acmd) < 0)
		return -1;

	struct sda_cmd cmd = {
		.sc_index = ACMD_SET_BUS_WIDTH,
		.sc_rtype = R1,
		.sc_argument = (width == 1 ? 0 : 2),
	};
	if (mmc_send_cmd(sc, &cmd) < 0)
		return -1;

	union sd_emmc_cfg _sd_emmc_cfg = { mmc_reg_read(sc, SD_EMMC_CFG) };
	switch (width) {
	case 1:
		_sd_emmc_cfg.Cfg_bus_width = 0;
		break;
	case 2:
		_sd_emmc_cfg.Cfg_bus_width = 3;
		break;
	case 4:
		_sd_emmc_cfg.Cfg_bus_width = 1;
		break;
	case 8:
		_sd_emmc_cfg.Cfg_bus_width = 2;
		break;
	}
	mmc_reg_write(sc, SD_EMMC_CFG, _sd_emmc_cfg.dw);

	return 0;
}

static int
mmc_set_blocklen(struct mmc_sc *sc, int len)
{
	ASSERT(len == DEV_BSIZE);
	struct sda_cmd cmd = {
		.sc_index = CMD_SET_BLOCKLEN,
		.sc_rtype = R1,

		.sc_argument = len,
	};
	if (mmc_send_cmd(sc, &cmd) < 0)
		return -1;

	return 0;
}

static boolean_t
mmc_has_cap(const char *caps, int len, const char *cap)
{
	while (len > 2) {
		if (strcmp(caps, cap) == 0)
			return (B_TRUE);
		len -= (strlen(caps) + 1);
		caps += (strlen(caps) + 1);
	}
	return B_FALSE;
}

static int
mmc_set_mode(struct mmc_sc *sc, int mode)
{
	uint32_t argument = (1u << 31) | 0xffffff;
	argument &= ~(0xf << ((1 - 1) * 4));
	argument |= (mode << ((1 - 1) * 4));
	if (mmc_swtch_func(sc, argument) != 0)
		return -1;
	int freq = sc->clock;
	sc->ddr = B_FALSE;
	switch (mmc_extract_bits(sc->func_status, 379, 4)) {
	case 0:
		freq = 25000000;
		break;
	case 1:
		freq = 50000000;
		break;
	case 2:
		freq = 100000000;
		break;
	case 3:
		freq = 208000000;
		break;
	case 4:
		freq = 50000000;
		sc->ddr = (B_TRUE);
		break;
	}
	freq = MIN(freq, sc->f_max);
	mmc_set_clock(sc, freq);

	switch (mmc_extract_bits(sc->func_status, 379, 4)) {
	case 2:
	case 3:
	case 4:
		if (mmc_retune(sc) != 0)
			return mmc_set_mode(sc, 1); // HiSpeed mode
		break;
	default:
		break;
	}

	return 0;
}

static int
mmc_init(struct mmc_sc *sc)
{
	int i, len;
	pnode_t node = ddi_get_nodeid(sc->dip);

	int slot_node = prom_childnode(node);
	if (slot_node < 0)
		return DDI_FAILURE;

	int power_level = prom_get_prop_int(slot_node, "power_level", 0);
	struct gpio_ctrl gpio_cd;
	struct gpio_ctrl gpio_ro;
	struct gpio_ctrl gpio_power;
	struct gpio_ctrl gpio_volsw;
	boolean_t has_gpio_cd = (init_mmc_gpio(slot_node, "gpio_cd", &gpio_cd) == 0);
	boolean_t has_gpio_ro = (init_mmc_gpio(slot_node, "gpio_ro", &gpio_ro) == 0);
	boolean_t has_gpio_power =
	    (init_mmc_gpio(slot_node, "gpio_power", &gpio_power) == 0);
	boolean_t has_gpio_volsw =
	    (init_mmc_gpio(slot_node, "gpio_volsw", &gpio_volsw) == 0);

	sc->f_min = prom_get_prop_int(slot_node, "f_min", 400000);
	sc->f_max = prom_get_prop_int(slot_node, "f_max", 50000000);
	sc->ocr_avail = prom_get_prop_int(slot_node, "ocr_avail",
	    OCR_33_34V | OCR_32_33V | OCR_31_32V | OCR_18_19V);

	if (mmc_pinmux(node, "sd_all_pins") < 0)
		if (mmc_pinmux(node, "emmc_all_pins") < 0)
			return DDI_FAILURE;

	if (has_gpio_power) {
		if (plat_gpio_direction_output(&gpio_power, !power_level) < 0)
			return DDI_FAILURE;
	}

	if (has_gpio_volsw) {
		// 3.3v
		if (plat_gpio_direction_output(&gpio_volsw, 0) < 0)
			return DDI_FAILURE;
	}

	// init
	mmc_reg_write(sc, SD_EMMC_ADJUST, 0);
	mmc_reg_write(sc, SD_EMMC_CFG, 0);
	mmc_reg_write(sc, SD_EMMC_IRQ_EN, 0);
	mmc_reg_write(sc, SD_EMMC_STATUS, 0xffffffff);

	union sd_emmc_cfg _cfg = { 0 };
	_cfg.Cfg_bus_width = 0;
	_cfg.Cfg_bl_len = 9; // 512 (1 << 9)
	_cfg.Cfg_resp_timeout = 0x7;
	_cfg.Cfg_rc_cc = 4;
	_cfg.Cfg_stop_clk = 1;
	_cfg.Cfg_err_abor = 1;
	mmc_reg_write(sc, SD_EMMC_CFG, _cfg.dw);
	usecwait(200);

	union sd_emmc_clock _clock = { 0 };
	_clock.Cfg_always_on = 1;
	_clock.Cfg_co_phase = 2;
	_clock.Cfg_div = 0x3f;
	mmc_reg_write(sc, SD_EMMC_CLOCK, _clock.dw);
	usecwait(200);
	mmc_set_clock(sc, sc->f_min);

	// power on
	if (has_gpio_power) {
		if (plat_gpio_direction_output(&gpio_power, power_level) < 0)
			return DDI_FAILURE;
		usecwait(200);
	}

	if (has_gpio_cd) {
		if (plat_gpio_direction_input(&gpio_cd) < 0)
			return DDI_FAILURE;
		if (plat_gpio_get(&gpio_cd))
			return DDI_FAILURE;
	}

	if (has_gpio_ro) {
		if (plat_gpio_direction_input(&gpio_ro) < 0)
			return DDI_FAILURE;
		if (plat_gpio_set_pullup(&gpio_ro, 1) < 0)
			return DDI_FAILURE;
	}

	mmc_set_sd_clock(sc, 1);

	union sd_emmc_irq_en _irq_en = { 0 };
	_irq_en.error = 0x1fff;
	_irq_en.En_End_of_Chain = 1;
	mmc_reg_write(sc, SD_EMMC_IRQ_EN, _irq_en.dw);

	if (mmc_go_idle_state(sc) != 0)
		return DDI_FAILURE;

	if (mmc_send_if_cond(sc) != 0)
		return DDI_FAILURE;

	for (i = 0; i < 1000; i++) {
		if (mmc_sd_send_ocr(sc) != 0)
			return DDI_FAILURE;

		if (sc->ocr & OCR_POWER_UP)
			break;

		usecwait(1000);
	}
	if (i >= 1000)
		return DDI_FAILURE;

	if ((sc->ocr & OCR_CCS) && (sc->ocr & OCR_S18A) && has_gpio_volsw) {
		if (mmc_voltage_switch(sc, &gpio_volsw) != 0)
			return DDI_FAILURE;
	}

	if (mmc_all_send_cid(sc) != 0)
		return DDI_FAILURE;

	if (mmc_send_relative_addr(sc) != 0)
		return DDI_FAILURE;

	if (mmc_send_csd(sc) != 0)
		return DDI_FAILURE;

	if (mmc_select_card(sc) != 0)
		return DDI_FAILURE;

	for (i = 0; i < 3; i++) {
		if (mmc_send_scr(sc) == 0)
			break;
	}
	if (i >= 3)
		return DDI_FAILURE;

	if (mmc_swtch_func(sc, 0) != 0)
		return DDI_FAILURE;

	// 4bit
	if (mmc_extract_bits(sc->scr, 51, 4) & (1 << 2)) {
		if (mmc_set_bus_width(sc, 4) < 0)
			return DDI_FAILURE;
	}

	if (mmc_set_blocklen(sc, DEV_BSIZE) != 0)
		return DDI_FAILURE;

	len = prom_getproplen(slot_node, "caps");
	if (len <= 0)
		return DDI_FAILURE;
	char *caps = __builtin_alloca(len);
	prom_getprop(slot_node, "caps", (caddr_t)caps);

	boolean_t sup_sdr104 = mmc_has_cap(caps, len, "MMC_CAP_UHS_SDR104");
	boolean_t sup_ddr50 = mmc_has_cap(caps, len, "MMC_CAP_UHS_DDR50");
	boolean_t sup_sdr50 = mmc_has_cap(caps, len, "MMC_CAP_UHS_SDR50");
	boolean_t sup_sdr25 = mmc_has_cap(caps, len, "MMC_CAP_UHS_SDR25");
	boolean_t sup_hispeed = mmc_has_cap(caps, len, "MMC_CAP_SD_HIGHSPEED");

	int mode = 0;
	// group 1
	if (sup_sdr104 &&
	    (mmc_extract_bits(sc->func_status, 415, 16) & (1u << 3))) {
		mode = 3;
	} else if (sup_ddr50 &&
	    (mmc_extract_bits(sc->func_status, 415, 16) & (1u << 4))) {
		mode = 4;
	} else if (sup_sdr50 &&
	    (mmc_extract_bits(sc->func_status, 415, 16) & (1u << 2))) {
		mode = 2;
	} else if ((sup_hispeed || sup_sdr25) &&
	    (mmc_extract_bits(sc->func_status, 415, 16) & (1u << 1))) {
		mode = 1;
	}
	if (mmc_set_mode(sc, mode) != 0)
		return DDI_FAILURE;

	return DDI_SUCCESS;
}

static void
mmc_read_block(void *arg)
{
	struct mmc_sc *sc = ((struct mmc_request *)arg)->sc;
	bd_xfer_t *xfer = ((struct mmc_request *)arg)->xfer;

	ASSERT(xfer->x_dmah == 0);
	ASSERT(xfer->x_kaddr != NULL);
	ASSERT(xfer->x_nblks * DEV_BSIZE <= MMC_BUFFER_SIZE);

	boolean_t detach;
	mutex_enter(&sc->lock);
	detach = sc->detach;
	mutex_exit(&sc->lock);

	int r = EIO;
	if (!detach) {
		struct sda_cmd cmd = {
			.sc_index = (xfer->x_nblks == 1 ? CMD_READ_SINGLE
							: CMD_READ_MULTI),
			.sc_rtype = R1,
			.sc_nblks = xfer->x_nblks,
			.sc_blksz = DEV_BSIZE,
			.sc_flags = SDA_CMDF_READ,

			.sc_dmah = sc->buf_dmah,
			.sc_ndmac = 1,
			.sc_dmac = sc->buf_dmac,
		};
		if (sc->ocr & OCR_CCS) {
			cmd.sc_argument = xfer->x_blkno;
		} else {
			cmd.sc_argument = xfer->x_blkno * DEV_BSIZE;
		}

		if (mmc_send_cmd(sc, &cmd) == 0) {
			ddi_dma_sync(sc->buf_dmah, 0, xfer->x_nblks * DEV_BSIZE,
			    DDI_DMA_SYNC_FORKERNEL);
			memcpy(xfer->x_kaddr, sc->buffer,
			    xfer->x_nblks * DEV_BSIZE);
			r = 0;
		}
	}

	mutex_enter(&sc->lock);
	list_insert_head(&sc->free_request, arg);
	mutex_exit(&sc->lock);

	bd_xfer_done(xfer, r);
}

static void
mmc_write_block(void *arg)
{
	struct mmc_sc *sc = ((struct mmc_request *)arg)->sc;
	bd_xfer_t *xfer = ((struct mmc_request *)arg)->xfer;

	ASSERT(xfer->x_dmah == 0);
	ASSERT(xfer->x_kaddr != NULL);
	ASSERT(xfer->x_nblks * DEV_BSIZE <= MMC_BUFFER_SIZE);

	boolean_t detach;
	mutex_enter(&sc->lock);
	detach = sc->detach;
	mutex_exit(&sc->lock);

	int r = EIO;
	if (!detach) {
		memcpy(sc->buffer, xfer->x_kaddr, xfer->x_nblks * DEV_BSIZE);
		ddi_dma_sync(sc->buf_dmah, 0, xfer->x_nblks * DEV_BSIZE,
		    DDI_DMA_SYNC_FORDEV);

		struct sda_cmd cmd = {
			.sc_index = (xfer->x_nblks == 1 ? CMD_WRITE_SINGLE
							: CMD_WRITE_MULTI),
			.sc_rtype = R1,
			.sc_nblks = xfer->x_nblks,
			.sc_blksz = DEV_BSIZE,
			.sc_flags = SDA_CMDF_WRITE,

			.sc_dmah = sc->buf_dmah,
			.sc_ndmac = 1,
			.sc_dmac = sc->buf_dmac,
		};
		if (sc->ocr & OCR_CCS) {
			cmd.sc_argument = xfer->x_blkno;
		} else {
			cmd.sc_argument = xfer->x_blkno * DEV_BSIZE;
		}

		if (mmc_send_cmd(sc, &cmd) == 0)
			r = 0;
	}

	mutex_enter(&sc->lock);
	list_insert_head(&sc->free_request, arg);
	mutex_exit(&sc->lock);

	bd_xfer_done(xfer, r);
}

static int
mmc_bd_read(void *arg, bd_xfer_t *xfer)
{
	if (xfer->x_flags & BD_XFER_POLL)
		return (EIO);

	int r = 0;
	struct mmc_request *req = NULL;
	struct mmc_sc *sc = arg;
	mutex_enter(&sc->lock);
	if (!sc->detach) {
		req = list_head(&sc->free_request);
		if (req != NULL) {
			list_remove(&sc->free_request, req);
			req->sc = sc;
			req->xfer = xfer;
		} else {
			r = ENOMEM;
		}
	} else {
		r = ENXIO;
	}
	mutex_exit(&sc->lock);
	if (req) {
		if (ddi_taskq_dispatch(sc->tq, mmc_read_block, req,
			DDI_SLEEP) != DDI_SUCCESS) {
			mutex_enter(&sc->lock);
			list_insert_head(&sc->free_request, req);
			mutex_exit(&sc->lock);
			r = EIO;
		}
	}
	return r;
}

static int
mmc_bd_write(void *arg, bd_xfer_t *xfer)
{
	if (xfer->x_flags & BD_XFER_POLL)
		return (EIO);

	int r = 0;
	struct mmc_request *req = NULL;
	struct mmc_sc *sc = arg;
	mutex_enter(&sc->lock);
	if (!sc->detach) {
		req = list_head(&sc->free_request);
		if (req != NULL) {
			list_remove(&sc->free_request, req);
			req->sc = sc;
			req->xfer = xfer;
		} else {
			r = ENOMEM;
		}
	} else {
		r = ENXIO;
	}
	mutex_exit(&sc->lock);
	if (req) {
		if (ddi_taskq_dispatch(sc->tq, mmc_write_block, req,
			DDI_SLEEP) != DDI_SUCCESS) {
			mutex_enter(&sc->lock);
			list_insert_head(&sc->free_request, req);
			mutex_exit(&sc->lock);
			r = EIO;
		}
	}
	return r;
}

static void
mmc_bd_driveinfo(void *arg, bd_drive_t *drive)
{
	struct mmc_sc *sc = arg;
	drive->d_qsize = 4;
	drive->d_removable = B_FALSE;
	drive->d_hotpluggable = B_FALSE;
	drive->d_target = 0;
	drive->d_lun = 0;
	drive->d_maxxfer = MMC_BUFFER_SIZE;

	switch (mmc_extract_bits(sc->cid, 127, 8)) {
	case 0x01:
		drive->d_vendor = "Panasonic";
		break;
	case 0x02:
		drive->d_vendor = "Toshiba";
		break;
	case 0x03:
		drive->d_vendor = "SanDisk";
		break;
	case 0x1b:
		drive->d_vendor = "Samsung";
		break;
	case 0x1d:
		drive->d_vendor = "AData";
		break;
	case 0x27:
		drive->d_vendor = "Phison";
		break;
	case 0x28:
		drive->d_vendor = "Lexar";
		break;
	case 0x31:
		drive->d_vendor = "Silicon Power";
		break;
	case 0x41:
		drive->d_vendor = "Kingston";
		break;
	case 0x74:
		drive->d_vendor = "Transcend";
		break;
	default:
		drive->d_vendor = "unknown";
		break;
	}
	drive->d_vendor_len = strlen(drive->d_vendor);

	drive->d_product = kmem_zalloc(6, KM_SLEEP);
	drive->d_product[4] = mmc_extract_bits(sc->cid, 71, 8);
	drive->d_product[3] = mmc_extract_bits(sc->cid, 79, 8);
	drive->d_product[2] = mmc_extract_bits(sc->cid, 87, 8);
	drive->d_product[1] = mmc_extract_bits(sc->cid, 95, 8);
	drive->d_product[0] = mmc_extract_bits(sc->cid, 103, 8);
	drive->d_product_len = strlen(drive->d_product);

	uint32_t serial = mmc_extract_bits(sc->cid, 55, 32);
	drive->d_serial = kmem_zalloc(9, KM_SLEEP);
	sprintf(drive->d_serial, "%08x", serial);
	drive->d_serial_len = 8;
	uint32_t rev = mmc_extract_bits(sc->cid, 63, 8);
	drive->d_revision = kmem_zalloc(6, KM_SLEEP);
	sprintf(drive->d_revision, "%d.%d", rev >> 4, rev & 0xF);
	drive->d_revision_len = strlen(drive->d_revision);
}

static int
mmc_bd_mediainfo(void *arg, bd_media_t *media)
{
	struct mmc_sc *sc = arg;
	uint64_t size;
	if (mmc_extract_bits(sc->csd, 127, 2) == 0) {
		uint64_t csz = mmc_extract_bits(sc->csd, 73, 12);
		uint32_t cmult = mmc_extract_bits(sc->csd, 49, 3);
		uint32_t read_bl_len = mmc_extract_bits(sc->csd, 83, 4);
		size = ((csz + 1) * (1 << (cmult + 2))) * read_bl_len;
	} else if (mmc_extract_bits(sc->csd, 127, 2) == 1) {
		uint64_t csz = mmc_extract_bits(sc->csd, 69, 22);
		size = (csz + 1) * (512 * 1024);
	} else {
		return -1;
	}

	media->m_nblks = size / 512;
	media->m_blksize = 512;
	media->m_readonly = B_FALSE;
	media->m_solidstate = B_TRUE;
	return (0);
}

static bd_ops_t mmc_bd_ops = {
	BD_OPS_VERSION_0,
	mmc_bd_driveinfo,
	mmc_bd_mediainfo,
	NULL, /* devid_init */
	NULL, /* sync_cache */
	mmc_bd_read,
	mmc_bd_write,
};

static void
mmc_destroy(struct mmc_sc *sc)
{
	if (sc->bdh) {
		bd_free_handle(sc->bdh);
	}
	if (sc->tq) {
		ddi_taskq_destroy(sc->tq);
	}

	for (;;) {
		struct mmc_request *req = list_head(&sc->free_request);
		if (req == NULL)
			break;
		list_remove(&sc->free_request, req);
		kmem_free(req, sizeof(struct mmc_request));
	}
	if (sc->buf_dmah) {
		ddi_dma_unbind_handle(sc->buf_dmah);
		ddi_dma_mem_free(&sc->buf_acch);
		ddi_dma_free_handle(&sc->buf_dmah);
	}
	if (sc->ihandle) {
		ddi_intr_disable(sc->ihandle);
		ddi_intr_remove_handler(sc->ihandle);
		ddi_intr_free(sc->ihandle);
	}
	if (sc->handle)
		ddi_regs_map_free(&sc->handle);

	list_destroy(&sc->free_request);
	cv_destroy(&sc->waitcv);
	mutex_destroy(&sc->intrlock);
	mutex_destroy(&sc->lock);
	kmem_free(sc, sizeof(*sc));
}

static int
mmc_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		break;
	default:
		return (DDI_FAILURE);
	}
	struct mmc_sc *sc = ddi_get_driver_private(dip);

	mutex_enter(&sc->lock);
	sc->detach = (B_TRUE);
	mutex_exit(&sc->lock);
	ddi_taskq_wait(sc->tq);

	bd_detach_handle(sc->bdh);

	ddi_set_driver_private(sc->dip, NULL);
	mmc_destroy(sc);

	return DDI_SUCCESS;
}

static int
mmc_quiesce(dev_info_t *dip)
{
	cmn_err(CE_WARN, "%s%d: mmc_quiesce is not implemented",
	    ddi_driver_name(dip), ddi_get_instance(dip));
	return DDI_FAILURE;
}

static int
mmc_probe(dev_info_t *dip)
{
	int len;
	char buf[80];
	pnode_t node = ddi_get_nodeid(dip);
	if (node < 0)
		return (DDI_PROBE_FAILURE);

	len = prom_getproplen(node, "status");
	if (len <= 0 || len >= sizeof(buf))
		return (DDI_PROBE_FAILURE);

	prom_getprop(node, "status", (caddr_t)buf);
	if (strcmp(buf, "ok") != 0 && (strcmp(buf, "okay") != 0))
		return (DDI_PROBE_FAILURE);

	return (DDI_PROBE_SUCCESS);
}

static uint_t
mmc_intr(caddr_t arg1, caddr_t arg2)
{
	struct mmc_sc *sc = (struct mmc_sc *)arg1;
	uint_t status = DDI_INTR_UNCLAIMED;

	mutex_enter(&sc->intrlock);

	union sd_emmc_status status_mask = { 0 };
	status_mask.error = 0x1fff;
	status_mask.End_of_Chain = 1;
	status_mask.Resp_status = 1;
	status_mask.IRQ_sdio = 1;
	uint32_t interrupt =
	    (mmc_reg_read(sc, SD_EMMC_STATUS) & status_mask.dw);
	if (interrupt) {
		mmc_reg_write(sc, SD_EMMC_STATUS, interrupt);
		status = DDI_INTR_CLAIMED;
		sc->interrupted |= interrupt;
		cv_signal(&sc->waitcv);
	}

	mutex_exit(&sc->intrlock);

	return status;
}

static ddi_device_acc_attr_t reg_acc_attr = {
	DDI_DEVICE_ATTR_V0,   /* devacc_attr_version */
	DDI_STRUCTURE_LE_ACC, /* devacc_attr_endian_flags */
	DDI_STRICTORDER_ACC,  /* devacc_attr_dataorder */
	DDI_DEFAULT_ACC,      /* devacc_attr_access */
};

static ddi_device_acc_attr_t buf_acc_attr = {
	DDI_DEVICE_ATTR_V0,      /* devacc_attr_version */
	DDI_NEVERSWAP_ACC,       /* devacc_attr_endian_flags */
	DDI_STORECACHING_OK_ACC, /* devacc_attr_dataorder */
	DDI_DEFAULT_ACC,         /* devacc_attr_access */
};

static ddi_dma_attr_t dma_attr = {
	DMA_ATTR_V0,           /* dma_attr_version	*/
	0x0000000000000000ull, /* dma_attr_addr_lo	*/
	0x00000000FFFFFFFFull, /* dma_attr_addr_hi	*/
	0x00000000FFFFFFFFull, /* dma_attr_count_max	*/
	0x0000000000000001ull, /* dma_attr_align	*/
	0x00000FFF,            /* dma_attr_burstsizes	*/
	0x00000001,            /* dma_attr_minxfer	*/
	0x0000000000010000ull, /* dma_attr_maxxfer	*/
	0x00000000FFFFFFFFull, /* dma_attr_seg		*/
	1,                     /* dma_attr_sgllen	*/
	0x00000001,            /* dma_attr_granular	*/
	DDI_DMA_FLAGERR        /* dma_attr_flags	*/
};

static int
mmc_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;
	default:
		return (DDI_FAILURE);
	}

	struct mmc_sc *sc = kmem_zalloc(sizeof(struct mmc_sc), KM_SLEEP);
	ddi_set_driver_private(dip, sc);

	sc->dip = dip;

	mutex_init(&sc->lock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&sc->intrlock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&sc->waitcv, NULL, CV_DRIVER, NULL);
	list_create(&sc->free_request, sizeof(struct mmc_request),
	    offsetof(struct mmc_request, node));

	int rv;
	rv = ddi_regs_map_setup(
	    sc->dip, 0, (caddr_t *)&sc->base, 0, 0, &reg_acc_attr, &sc->handle);
	if (rv != DDI_SUCCESS) {
		cmn_err(CE_WARN, "ddi_regs_map_setup failed (%d)!", rv);
		sc->handle = 0;
		goto err_exit;
	}

	rv = ddi_dma_alloc_handle(
	    dip, &dma_attr, DDI_DMA_SLEEP, NULL, &sc->buf_dmah);
	if (rv != DDI_SUCCESS) {
		cmn_err(CE_WARN, "ddi_dma_alloc_handle failed (%d)!", rv);
		sc->buf_dmah = 0;
		goto err_exit;
	}

	size_t real_length;
	rv = ddi_dma_mem_alloc(sc->buf_dmah, MMC_BUFFER_SIZE, &buf_acc_attr,
	    DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, NULL, &sc->buffer, &real_length,
	    &sc->buf_acch);
	if (rv != DDI_SUCCESS) {
		cmn_err(CE_WARN, "ddi_dma_mem_alloc failed (%d)!", rv);
		ddi_dma_free_handle(&sc->buf_dmah);
		sc->buf_dmah = 0;
		goto err_exit;
	}
	uint_t ndmac;
	rv = ddi_dma_addr_bind_handle(sc->buf_dmah, NULL, sc->buffer,
	    real_length, DDI_DMA_RDWR | DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, NULL,
	    &sc->buf_dmac, &ndmac);
	if ((rv != DDI_DMA_MAPPED) || (ndmac != 1)) {
		cmn_err(CE_WARN, "ddi_dma_addr_bind_handle failed (%d, %u)!",
		    rv, ndmac);
		ddi_dma_mem_free(&sc->buf_acch);
		ddi_dma_free_handle(&sc->buf_dmah);
		sc->buf_dmah = 0;
		goto err_exit;
	}

	for (int i = 0; i < MMC_REQUESTS_MAX; i++) {
		void *req = kmem_alloc(sizeof(struct mmc_request), KM_NOSLEEP);
		if (req == NULL) {
			cmn_err(CE_WARN, "kmem_alloc failed for mmc_request");
			goto err_exit;
		}
		list_insert_head(&sc->free_request, req);
	}

	int actual;
	rv = ddi_intr_alloc(sc->dip, &sc->ihandle, DDI_INTR_TYPE_FIXED, 0, 1,
	    &actual, DDI_INTR_ALLOC_STRICT);
	if (rv != DDI_SUCCESS) {
		cmn_err(CE_WARN, "ddi_intr_alloc failed (%d)!", rv);
		sc->ihandle = 0;
		goto err_exit;
	}

	rv = ddi_intr_add_handler(sc->ihandle, mmc_intr, sc, NULL);
	if (rv != DDI_SUCCESS) {
		cmn_err(CE_WARN, "ddi_intr_add_handler failed (%d)!", rv);
		ddi_intr_free(sc->ihandle);
		sc->ihandle = 0;
		goto err_exit;
	}

	rv = ddi_intr_enable(sc->ihandle);
	if (rv != DDI_SUCCESS) {
		cmn_err(CE_WARN, "Failed enabling interrupts");
		ddi_intr_remove_handler(sc->ihandle);
		ddi_intr_free(sc->ihandle);
		sc->ihandle = 0;
		goto err_exit;
	}

	rv = mmc_init(sc);
	if (rv != DDI_SUCCESS) {
		cmn_err(CE_WARN, "mmc_init failed!");
		goto err_exit;
	}

	sc->tq = ddi_taskq_create(sc->dip, "taskq", 1, TASKQ_DEFAULTPRI, 0);
	if (sc->tq == NULL) {
		cmn_err(CE_WARN, "ddi_taskq_create failed!");
		goto err_exit;
	}

	sc->bdh = bd_alloc_handle(sc, &mmc_bd_ops, NULL, KM_SLEEP);
	if (sc->bdh == NULL) {
		cmn_err(CE_WARN, "bd_alloc_handle failed!");
		goto err_exit;
	}

	rv = bd_attach_handle(sc->dip, sc->bdh);
	if (rv != DDI_SUCCESS) {
		cmn_err(CE_WARN, "bd_attach_handle failed!");
		goto err_exit;
	}

	return DDI_SUCCESS;
err_exit:
	mmc_destroy(sc);
	return (DDI_FAILURE);
}

static struct dev_ops mmc_dev_ops = {
	DEVO_REV,    /* devo_rev */
	0,           /* devo_refcnt */
	ddi_no_info, /* devo_getinfo */
	nulldev,     /* devo_identify */
	mmc_probe,   /* devo_probe */
	mmc_attach,  /* devo_attach */
	mmc_detach,  /* devo_detach */
	nodev,       /* devo_reset */
	NULL,        /* devo_cb_ops */
	NULL,        /* devo_bus_ops */
	NULL,        /* devo_power */
	mmc_quiesce, /* devo_quiesce */
};

static struct modldrv mmc_modldrv = {
	&mod_driverops,     /* drv_modops */
	"Raspberry Pi MMC", /* drv_linkinfo */
	&mmc_dev_ops        /* drv_dev_ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,              /* ml_rev */
	{ &mmc_modldrv, NULL } /* ml_linkage */
};

int
_init(void)
{
	int i;

	bd_mod_init(&mmc_dev_ops);
	if ((i = mod_install(&modlinkage)) != 0) {
		bd_mod_fini(&mmc_dev_ops);
	}
	return (i);
}

int
_fini(void)
{
	int i;

	if ((i = mod_remove(&modlinkage)) == 0) {
		bd_mod_fini(&mmc_dev_ops);
	}
	return (i);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
