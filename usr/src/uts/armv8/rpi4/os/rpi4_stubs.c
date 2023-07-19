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
 * Copyright 2021 Hayashi Naoyuki
 */

#include <sys/types.h>
#include <sys/machclock.h>
#include <sys/platform.h>
#include <sys/modctl.h>
#include <sys/platmod.h>
#include <sys/promif.h>
#include <sys/errno.h>
#include <sys/byteorder.h>
#include <sys/gpio.h>

int plat_clk_enable(const char *name)
{
	return -1;
}

int plat_clk_disable(const char *name)
{
	return -1;
}

int plat_clk_get_rate(const char *name)
{
	return -1;
}

int plat_clk_set_rate(const char *name, int rate)
{
	return -1;
}

int plat_hwclock_enable(struct prom_hwclock *clk)
{
	return -1;
}

int plat_hwclock_disable(struct prom_hwclock *clk)
{
	return -1;
}

int plat_hwclock_is_enabled(struct prom_hwclock *clk, boolean_t *enabled)
{
	return -1;
}

int plat_pinmux_set(pnode_t node)
{
	return -1;
}

int plat_hwreset_assert(struct prom_hwreset *rst)
{
	return -1;
}

int plat_hwreset_deassert(struct prom_hwreset *rst)
{
	return -1;
}

int plat_hwreset_is_asserted(struct prom_hwreset *rst, boolean_t *asserted)
{
	return -1;
}

int plat_gpio_direction_output(struct gpio_ctrl *gpio, int value)
{
	return -1;
}

int plat_gpio_direction_input(struct gpio_ctrl *gpio)
{
	return -1;
}


int plat_gpio_set_pullup(struct gpio_ctrl *gpio, int value)
{
	return -1;
}
