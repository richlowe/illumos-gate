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

#include <stdbool.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/promif.h>
#include <sys/salib.h>
#include <sys/byteorder.h>
#include <sys/sysmacros.h>
#include <sys/controlregs.h>
#include <sys/arch_timer.h>
#include <sys/dditypes.h>
#include <sys/devops.h>
#include <sys/sdcard/sda.h>
#include <sys/platform.h>
#include <sys/platmod.h>
#include <sys/gpio.h>
#include <util/sscanf.h>
#include <sys/mmcreg.h>

#include "prom_dev.h"
#include "boot_plat.h"
#include "mmc.h"

#define BUFFER_SIZE 0x20000

struct mmc_sc {
	uint64_t base;
	uint32_t ocr;
	uint32_t ocr_avail;
	uint32_t vdd;
	uint32_t csd[4];
	uint32_t cid[4];
	uint32_t scr[2];
	uint32_t func_status[16];
	uint32_t rca;
	uint32_t f_min;
	uint32_t f_max;
	char *buffer;
	uint64_t buffer_bus_address;
	int clock;
	bool ddr;
	bool in_tuning;
};

static struct mmc_sc *mmc_dev[3];

static void
usecwait(int usec)
{
	arch_timer_udelay(usec);
}

static uint64_t
get_usec()
{
	return (arch_timer_count() / (arch_timer_freq() / 1000000));
}

static void
cache_flush(void *addr, size_t len)
{
	for (uintptr_t v = P2ALIGN((uintptr_t)addr, DCACHE_LINE);
	     v < (uintptr_t)addr + len; v += DCACHE_LINE) {
		flush_data_cache(v);
	}
	dsb(sy);
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

static uint32_t
mmc_wait_intr(struct mmc_sc *sc, uint32_t mask, uint64_t usec)
{
	uint64_t timeout = get_usec() + usec;
	union sd_emmc_status _status = { mask };
	_status.error = 0x1fff;
	uint32_t val;
	for (;;) {
		bool timeout_occurred = (get_usec() > timeout);
		val = (mmc_reg_read(sc, SD_EMMC_STATUS) & _status.dw);
		if (val != 0) {
			mmc_reg_write(sc, SD_EMMC_STATUS, val);
			break;
		}
		if (timeout_occurred)
			break;
	}

	_status.dw = val;
	if (_status.error || val == 0) {
		if (!sc->in_tuning) {
			prom_printf(
			    "%s:%d val %08x\n", __func__, __LINE__, val);
		}
		mmc_stop(sc);
		val = 0;
	}
	return val;
}

static int
mmc_wait_state_idle(
    struct mmc_sc *sc, uint32_t mask, uint32_t val, uint64_t usec)
{
	uint64_t timeout = get_usec() + usec;
	for (;;) {
		bool timeout_occurred = (get_usec() > timeout);
		if ((mmc_reg_read(sc, SD_EMMC_STATUS) & mask) == val)
			return 0;
		if (timeout_occurred)
			break;
		usecwait(200);
	}

	prom_printf("%s:%d %08x\n", __func__, __LINE__,
	    mmc_reg_read(sc, SD_EMMC_STATUS));
	mmc_stop(sc);

	return -1;
}

static int
mmc_start_cmd(struct mmc_sc *sc, struct sda_cmd *cmd)
{
	union sd_emmc_status status_mask = { 0 };
	status_mask.CMD_i = 1;
	if ((cmd->sc_flags & (SDA_CMDF_READ | SDA_CMDF_WRITE)) &&
	    cmd->sc_kvaddr)
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
		data = (uint32_t)((cmd->sc_kvaddr - sc->buffer) +
		    sc->buffer_bus_address);
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
	sc->in_tuning = true;

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

	for (int i = 0; i < max_delay; i++) {
		mmc_reg_write(sc, SD_EMMC_ADJUST, _adjust.dw);

		struct sda_cmd cmd = {
			.sc_index = 19,
			.sc_rtype = R1,

			.sc_nblks = 1,
			.sc_blksz = sizeof(tuning_blk_pattern),
			.sc_flags = SDA_CMDF_READ,
			.sc_kvaddr = sc->buffer,
		};
		if (mmc_send_cmd(sc, &cmd) == 0) {
			cache_flush(sc->buffer, sizeof(tuning_blk_pattern));
			if (memcmp(tuning_blk_pattern, sc->buffer,
				sizeof(tuning_blk_pattern)) == 0) {
				sc->in_tuning = false;
				return 0;
			}
		}
		usecwait(1000);

		_adjust.Adj_delay = (_adjust.Adj_delay + 1) % max_delay;
	}

	_adjust.dw = mmc_reg_read(sc, SD_EMMC_ADJUST);
	_adjust.Adj_enable = 0;
	_adjust.Adj_delay = 0;
	mmc_reg_write(sc, SD_EMMC_ADJUST, _adjust.dw);
	sc->in_tuning = false;

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
		.sc_kvaddr = sc->buffer,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;

	cache_flush(sc->buffer, sizeof(sc->scr));

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
		.sc_kvaddr = sc->buffer,
	};
	if (mmc_send_cmd(sc, &cmd) != 0)
		return -1;

	cache_flush(sc->buffer, sizeof(sc->func_status));

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

	union sd_emmc_cfg _cfg = { mmc_reg_read(sc, SD_EMMC_CFG) };
	switch (width) {
	case 1:
		_cfg.Cfg_bus_width = 0;
		break;
	case 2:
		_cfg.Cfg_bus_width = 3;
		break;
	case 4:
		_cfg.Cfg_bus_width = 1;
		break;
	case 8:
		_cfg.Cfg_bus_width = 2;
		break;
	}
	mmc_reg_write(sc, SD_EMMC_CFG, _cfg.dw);

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

static bool
mmc_has_cap(const char *caps, int len, const char *cap)
{
	while (len > 2) {
		if (strcmp(caps, cap) == 0)
			return true;
		len -= (strlen(caps) + 1);
		caps += (strlen(caps) + 1);
	}
	return false;
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
	sc->ddr = false;
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
		sc->ddr = true;
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
mmc_open(const char *name)
{
	pnode_t node;
	pnode_t slot_node;
	int fd;
	int len;
	int i;

	for (fd = 0; fd < sizeof(mmc_dev) / sizeof(mmc_dev[0]); fd++) {
		if (mmc_dev[fd] == NULL)
			break;
	}

	if (fd == sizeof(mmc_dev) / sizeof(mmc_dev[0]))
		return -1;

	struct mmc_sc *sc = kmem_alloc(sizeof(struct mmc_sc), 0);
	memset(sc, 0, sizeof(struct mmc_sc));

	node = prom_finddevice(name);
	if (node <= 0)
		return -1;

	if (!prom_is_compatible(node, "amlogic,aml_sd_emmc"))
		return -1;

	len = prom_getproplen(node, "status");
	if (len <= 0)
		return -1;
	char *status = __builtin_alloca(len);
	prom_getprop(node, "status", (caddr_t)status);
	if (strcmp(status, "ok") != 0 && strcmp(status, "okay") != 0)
		return -1;

	if (prom_get_reg_address(node, 0, &sc->base) != 0)
		return -1;

	slot_node = prom_childnode(node);
	if (slot_node < 0)
		return -1;

	sc->buffer = malloc(BUFFER_SIZE + 2 * DCACHE_LINE);
	cache_flush(sc->buffer, BUFFER_SIZE + 2 * DCACHE_LINE);
	sc->buffer = (char *)roundup((uintptr_t)sc->buffer, DCACHE_LINE);

	int power_level = prom_get_prop_int(slot_node, "power_level", 0);
	struct gpio_ctrl gpio_cd;
	struct gpio_ctrl gpio_ro;
	struct gpio_ctrl gpio_power;
	struct gpio_ctrl gpio_volsw;
	bool has_gpio_cd = (init_mmc_gpio(slot_node, "gpio_cd", &gpio_cd) == 0);
	bool has_gpio_ro = (init_mmc_gpio(slot_node, "gpio_ro", &gpio_ro) == 0);
	bool has_gpio_power =
	    (init_mmc_gpio(slot_node, "gpio_power", &gpio_power) == 0);
	bool has_gpio_volsw =
	    (init_mmc_gpio(slot_node, "gpio_volsw", &gpio_volsw) == 0);

	write_s1e1r(P2ALIGN((uintptr_t)sc->buffer, MMU_PAGESIZE));
	isb();

	uint64_t par = read_par_el1();
	if (par & PAR_F)
		return -1;
	uint64_t buffer_phys =
	    ((par & PAR_PA_MASK) | (((uintptr_t)sc->buffer) & MMU_PAGEOFFSET));
	if (prom_get_bus_address(node, buffer_phys, &sc->buffer_bus_address) <
	    0)
		return -1;

	sc->f_min = prom_get_prop_int(slot_node, "f_min", 400000);
	sc->f_max = prom_get_prop_int(slot_node, "f_max", 50000000);
	sc->ocr_avail = prom_get_prop_int(slot_node, "ocr_avail",
	    OCR_33_34V | OCR_32_33V | OCR_31_32V | OCR_18_19V);

	if (mmc_pinmux(node, "sd_all_pins") < 0)
		if (mmc_pinmux(node, "emmc_all_pins") < 0)
			return -1;

	if (has_gpio_power) {
		if (plat_gpio_direction_output(&gpio_power, !power_level) < 0)
			return -1;
	}

	if (has_gpio_volsw) {
		// 3.3v
		if (plat_gpio_direction_output(&gpio_volsw, 0) < 0)
			return -1;
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
			return -1;
		usecwait(200);
	}

	if (has_gpio_cd) {
		if (plat_gpio_direction_input(&gpio_cd) < 0)
			return -1;
		if (plat_gpio_get(&gpio_cd))
			return -1;
	}

	if (has_gpio_ro) {
		if (plat_gpio_direction_input(&gpio_ro) < 0)
			return -1;
		if (plat_gpio_set_pullup(&gpio_ro, 1) < 0)
			return -1;
	}

	mmc_set_sd_clock(sc, 1);

	if (mmc_go_idle_state(sc) != 0)
		return -1;

	if (mmc_send_if_cond(sc) != 0)
		return -1;

	for (i = 0; i < 1000; i++) {
		if (mmc_sd_send_ocr(sc) != 0)
			return -1;

		if (sc->ocr & OCR_POWER_UP)
			break;

		usecwait(1000);
	}
	if (i >= 1000)
		return -1;

	if ((sc->ocr & OCR_CCS) && (sc->ocr & OCR_S18A) && has_gpio_volsw) {
		if (mmc_voltage_switch(sc, &gpio_volsw) != 0)
			return -1;
	}

	if (mmc_all_send_cid(sc) != 0)
		return -1;

	if (mmc_send_relative_addr(sc) != 0)
		return -1;

	if (mmc_send_csd(sc) != 0)
		return -1;

	if (mmc_select_card(sc) != 0)
		return -1;

	for (i = 0; i < 3; i++) {
		if (mmc_send_scr(sc) == 0)
			break;
	}
	if (i >= 3)
		return -1;

	if (mmc_swtch_func(sc, 0) != 0)
		return -1;

	// 4bit
	if (mmc_extract_bits(sc->scr, 51, 4) & (1 << 2)) {
		if (mmc_set_bus_width(sc, 4) < 0)
			return -1;
	}

	if (mmc_set_blocklen(sc, DEV_BSIZE) != 0)
		return -1;

	len = prom_getproplen(slot_node, "caps");
	if (len <= 0)
		return -1;
	char *caps = __builtin_alloca(len);
	prom_getprop(slot_node, "caps", (caddr_t)caps);

	bool sup_sdr104 = mmc_has_cap(caps, len, "MMC_CAP_UHS_SDR104");
	bool sup_ddr50 = mmc_has_cap(caps, len, "MMC_CAP_UHS_DDR50");
	bool sup_sdr50 = mmc_has_cap(caps, len, "MMC_CAP_UHS_SDR50");
	bool sup_sdr25 = mmc_has_cap(caps, len, "MMC_CAP_UHS_SDR25");
	bool sup_hispeed = mmc_has_cap(caps, len, "MMC_CAP_SD_HIGHSPEED");

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
		return -1;

	mmc_dev[fd] = sc;

	return fd;
}

static ssize_t
mmc_read(int dev, caddr_t buf, size_t buf_len, uint_t startblk)
{
	size_t read_size = buf_len;
	struct mmc_sc *sc = mmc_dev[dev];

	while (read_size > 0) {
		size_t nblks = MIN(read_size, BUFFER_SIZE) / DEV_BSIZE;

		struct sda_cmd cmd = {
			.sc_index =
			    (nblks == 1 ? CMD_READ_SINGLE : CMD_READ_MULTI),
			.sc_rtype = R1,
			.sc_nblks = nblks,
			.sc_blksz = DEV_BSIZE,
			.sc_flags = SDA_CMDF_READ,
			.sc_kvaddr = sc->buffer,
		};
		if (sc->ocr & OCR_CCS) {
			cmd.sc_argument = startblk;
		} else {
			cmd.sc_argument = startblk * DEV_BSIZE;
		}
		if (mmc_send_cmd(sc, &cmd) < 0)
			return -1;

		cache_flush(sc->buffer, nblks * DEV_BSIZE);
		memcpy(buf, sc->buffer, nblks * DEV_BSIZE);

		buf += nblks * DEV_BSIZE;
		startblk += nblks;
		read_size -= nblks * DEV_BSIZE;
	}

	return buf_len;
}

static int
mmc_match(const char *path)
{
	const char *cmp;

	cmp = "/soc/sd@";
	if (strncmp(path, cmp, strlen(cmp)) == 0)
		return 1;

	cmp = "/soc/emmc@";
	if (strncmp(path, cmp, strlen(cmp)) == 0)
		return 1;

	return 0;
}

static struct prom_dev mmc_prom_dev = {
	.match = mmc_match,
	.open = mmc_open,
	.read = mmc_read,
};

void
init_mmc(void)
{
	prom_register(&mmc_prom_dev);
}
