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
#include <sys/gpio.h>

static int
get_reg_addr(pnode_t node, int index, uint64_t *reg)
{
	uint64_t addr;
	if (prom_get_reg(node, index, &addr) != 0)
		return -1;

	pnode_t parent = prom_parentnode(node);
	while (parent > 0) {
		if (prom_is_compatible(parent, "simple-bus")) {
			int len = prom_getproplen(parent, "ranges");
			if (len > 0) {
				int address_cells = prom_get_prop_int(parent, "#address-cells", 2);
				int size_cells = prom_get_prop_int(parent, "#size-cells", 2);
				int parent_address_cells  = prom_get_prop_int(prom_parentnode(parent), "#address-cells", 2);

				if ((len % CELLS_1275_TO_BYTES(address_cells +
				    parent_address_cells + size_cells)) == 0) {
					uint32_t *ranges = __builtin_alloca(len);
					prom_getprop(parent, "ranges", (caddr_t)ranges);
					int ranges_cells = (address_cells + parent_address_cells + size_cells);

					for (int i = 0; i < len / CELLS_1275_TO_BYTES(ranges_cells); i++) {
						uint64_t base = 0;
						uint64_t target = 0;
						uint64_t size = 0;
						for (int j = 0; j < address_cells; j++) {
							base <<= 32;
							base += htonl(ranges[ranges_cells * i + j]);
						}
						for (int j = 0; j < parent_address_cells; j++) {
							target <<= 32;
							target += htonl(ranges[ranges_cells * i + address_cells + j]);
						}
						for (int j = 0; j < size_cells; j++) {
							size <<= 32;
							size += htonl(ranges[ranges_cells * i + address_cells + parent_address_cells + j]);
						}

						if (base <= addr && addr <= base + size - 1) {
							addr = (addr - base) + target;
							break;
						}
					}
				}
			}
		}
		parent = prom_parentnode(parent);
	}
	*reg = addr;
	return 0;
}

enum gxbb_gpio_ao {
	GPIOAO_0, GPIOAO_1, GPIOAO_2, GPIOAO_3, GPIOAO_4, GPIOAO_5, GPIOAO_6,
	GPIOAO_7, GPIOAO_8, GPIOAO_9, GPIOAO_10, GPIOAO_11, GPIOAO_12, GPIOAO_13,
};

enum gxbb_gpio_ee {
	GPIOZ_0, GPIOZ_1, GPIOZ_2, GPIOZ_3, GPIOZ_4, GPIOZ_5, GPIOZ_6, GPIOZ_7,
	GPIOZ_8, GPIOZ_9, GPIOZ_10, GPIOZ_11, GPIOZ_12, GPIOZ_13, GPIOZ_14, GPIOZ_15,
	GPIOH_0, GPIOH_1, GPIOH_2, GPIOH_3,
	BOOT_0, BOOT_1, BOOT_2, BOOT_3, BOOT_4, BOOT_5, BOOT_6, BOOT_7, BOOT_8, BOOT_9,
	BOOT_10, BOOT_11, BOOT_12, BOOT_13, BOOT_14, BOOT_15, BOOT_16, BOOT_17,
	CARD_0, CARD_1, CARD_2, CARD_3, CARD_4, CARD_5, CARD_6,
	GPIODV_0, GPIODV_1, GPIODV_2, GPIODV_3, GPIODV_4, GPIODV_5, GPIODV_6, GPIODV_7,
	GPIODV_8, GPIODV_9, GPIODV_10, GPIODV_11, GPIODV_12, GPIODV_13, GPIODV_14, GPIODV_15,
	GPIODV_16, GPIODV_17, GPIODV_18, GPIODV_19, GPIODV_20, GPIODV_21, GPIODV_22, GPIODV_23,
	GPIODV_24, GPIODV_25, GPIODV_26, GPIODV_27, GPIODV_28, GPIODV_29,
	GPIOY_0, GPIOY_1, GPIOY_2, GPIOY_3, GPIOY_4, GPIOY_5, GPIOY_6, GPIOY_7, GPIOY_8,
	GPIOY_9, GPIOY_10, GPIOY_11, GPIOY_12, GPIOY_13, GPIOY_14, GPIOY_15, GPIOY_16,
	GPIOX_0, GPIOX_1, GPIOX_2, GPIOX_3, GPIOX_4, GPIOX_5, GPIOX_6, GPIOX_7, GPIOX_8,
	GPIOX_9, GPIOX_10, GPIOX_11, GPIOX_12, GPIOX_13, GPIOX_14, GPIOX_15, GPIOX_16,
	GPIOX_17, GPIOX_18, GPIOX_19, GPIOX_20, GPIOX_21, GPIOX_22,
	GPIOCLK_0, GPIOCLK_1, GPIOCLK_2, GPIOCLK_3,
	GPIO_TEST_N,
};

// reg, bit
// mux, pull, pull-enable, gpio

struct gpio_pin {
	int reg;
	int bit;
};

enum gpio_op {
	GPIO_OUTPUT_ENABLE,
	GPIO_OUTPUT,
	GPIO_INPUT,
	GPIO_PULL_ENABLE,
	GPIO_PULL,
};

static int
get_gpio_pin(const struct gpio_ctrl *gpio, enum gpio_op index, struct gpio_pin *pin)
{
	uint64_t base;
	int r = prom_get_prop_index(gpio->node, "reg-names", "gpio");
	if (r < 0)
		return -1;
	if (get_reg_addr(gpio->node, r, &base))
		return -1;
	boolean_t is_ao = (base == 0xc8100024);

	if (is_ao) {
		static const struct gpio_pin gpio_register[][5] = {
#define GPIO_PIN(offset, bit)    { (offset),  (bit)}, { (offset), (bit) + 16}, { (offset) + 1,  (bit)}
#define PULL_EN_PIN(offset, bit) { (offset),  (bit)}
#define PULL_PIN(offset, bit)    { (offset),  (bit) + 16}
			//		  gpio                  pull-enable            pull
			[GPIOAO_0]	= { GPIO_PIN(0x0, 0), PULL_EN_PIN(0x0, 0), PULL_PIN(0x0,  0)},
#undef GPIO_PIN
#undef PULL_EN_PIN
#undef PULL_PIN
		};
		switch (gpio->pin) {
		case GPIOAO_0 ... GPIOAO_13:
			pin->reg = gpio_register[GPIOAO_0][index].reg;
			pin->bit = gpio_register[GPIOAO_0][index].bit + gpio->pin - GPIOAO_0;
			break;
		default:
			return -1;
		}
	} else {
		static const struct gpio_pin gpio_register[][5] = {
#define GPIO_PIN(offset, bit)    { ((offset) - 0xc) + 0,  (bit)}, { ((offset) - 0xc) + 1,  (bit)}, { ((offset) - 0xc) + 2,  (bit)}
#define PULL_EN_PIN(offset, bit) { ((offset) - 0x48),  (bit)}
#define PULL_PIN(offset, bit)    { ((offset) - 0x3a),  (bit)}
			//		  gpio                  pull-enable            pull
			[GPIOX_0]	= { GPIO_PIN(0x18,  0), PULL_EN_PIN(0x4c, 0),  PULL_PIN(0x3e,  0)},
			[GPIOY_0]	= { GPIO_PIN(0x0f,  0), PULL_EN_PIN(0x49, 0),  PULL_PIN(0x3b,  0)},
			[GPIODV_0]	= { GPIO_PIN(0x0c,  0), PULL_EN_PIN(0x48, 0),  PULL_PIN(0x3a,  0)},
			[GPIOH_0]	= { GPIO_PIN(0x0f, 20), PULL_EN_PIN(0x49, 20), PULL_PIN(0x3b,  20)},
			[BOOT_0]	= { GPIO_PIN(0x12,  0), PULL_EN_PIN(0x4a, 0),  PULL_PIN(0x3c,  0)},
			[CARD_0]	= { GPIO_PIN(0x12, 20), PULL_EN_PIN(0x4a, 20), PULL_PIN(0x3c,  20)},
			[GPIOCLK_0]	= { GPIO_PIN(0x15, 28), PULL_EN_PIN(0x4b, 28), PULL_PIN(0x3d,  28)},
#undef GPIO_PIN
#undef PULL_EN_PIN
#undef PULL_PIN
		};

		switch (gpio->pin) {
		case GPIOX_0 ... GPIOX_22:
			pin->reg = gpio_register[GPIOX_0][index].reg;
			pin->bit = gpio_register[GPIOX_0][index].bit + gpio->pin - GPIOX_0;
			break;
		case GPIOY_0 ... GPIOY_16:
			pin->reg = gpio_register[GPIOY_0][index].reg;
			pin->bit = gpio_register[GPIOY_0][index].bit + gpio->pin - GPIOY_0;
			break;
		case GPIODV_0 ... GPIODV_29:
			pin->reg = gpio_register[GPIODV_0][index].reg;
			pin->bit = gpio_register[GPIODV_0][index].bit + gpio->pin - GPIODV_0;
			break;
		case GPIOH_0 ... GPIOH_3:
			pin->reg = gpio_register[GPIOH_0][index].reg;
			pin->bit = gpio_register[GPIOH_0][index].bit + gpio->pin - GPIOH_0;
			break;
		case BOOT_0 ... BOOT_17:
			pin->reg = gpio_register[BOOT_0][index].reg;
			pin->bit = gpio_register[BOOT_0][index].bit + gpio->pin - BOOT_0;
			break;
		case CARD_0 ... CARD_6:
			pin->reg = gpio_register[CARD_0][index].reg;
			pin->bit = gpio_register[CARD_0][index].bit + gpio->pin - CARD_0;
			break;
		case GPIOCLK_0 ... GPIOCLK_3:
			pin->reg = gpio_register[GPIOCLK_0][index].reg;
			pin->bit = gpio_register[GPIOCLK_0][index].bit + gpio->pin - GPIOCLK_0;
			break;
		default:
			return -1;
		}
	}
	return 0;
}


int plat_gpio_direction_output(struct gpio_ctrl *gpio, int value)
{
	struct gpio_pin out;
	struct gpio_pin outen;
	if (get_gpio_pin(gpio, GPIO_OUTPUT, &out) < 0)
		return -1;
	if (get_gpio_pin(gpio, GPIO_OUTPUT_ENABLE, &outen) < 0)
		return -1;
	uint64_t base;
	int index = prom_get_prop_index(gpio->node, "reg-names", "gpio");
	if (index < 0)
		return -1;
	if (get_reg_addr(gpio->node, index, &base))
		return -1;

	base += SEGKPM_BASE;
	if (value)
		*((volatile uint32_t*)(base) + out.reg) |= (1u << out.bit);
	else
		*((volatile uint32_t*)(base) + out.reg) &= ~(0u << out.bit);

	*((volatile uint32_t*)(base) + outen.reg) |= (1u << outen.bit);
	return 0;
}

int plat_gpio_direction_input(struct gpio_ctrl *gpio)
{
	struct gpio_pin outen;
	if (get_gpio_pin(gpio, GPIO_OUTPUT_ENABLE, &outen) < 0)
		return -1;
	uint64_t base;
	int index = prom_get_prop_index(gpio->node, "reg-names", "gpio");
	if (index < 0)
		return -1;
	if (get_reg_addr(gpio->node, index, &base))
		return -1;

	base += SEGKPM_BASE;
	*((volatile uint32_t*)(base) + outen.reg) &= ~(1u << outen.bit);
	return 0;
}

int plat_gpio_get(struct gpio_ctrl *gpio)
{
	struct gpio_pin in;
	if (get_gpio_pin(gpio, GPIO_INPUT, &in) < 0)
		return -1;
	uint64_t base;
	int index = prom_get_prop_index(gpio->node, "reg-names", "gpio");
	if (index < 0)
		return -1;
	if (get_reg_addr(gpio->node, index, &base))
		return -1;

	base += SEGKPM_BASE;
	return (*((volatile uint32_t*)(base) + in.reg) >> in.bit) & 1;
}

int plat_gpio_set(struct gpio_ctrl *gpio, int value)
{
	struct gpio_pin out;
	if (get_gpio_pin(gpio, GPIO_OUTPUT, &out) < 0)
		return -1;
	uint64_t base;
	int index = prom_get_prop_index(gpio->node, "reg-names", "gpio");
	if (index < 0)
		return -1;
	if (get_reg_addr(gpio->node, index, &base))
		return -1;

	base += SEGKPM_BASE;
	if (value)
		*((volatile uint32_t*)(base) + out.reg) |= (1u << out.bit);
	else
		*((volatile uint32_t*)(base) + out.reg) &= ~(0u << out.bit);
	return 0;
}

int plat_gpio_set_pullup(struct gpio_ctrl *gpio, int value)
{
	struct gpio_pin pull;
	struct gpio_pin pullen;
	if (get_gpio_pin(gpio, GPIO_PULL, &pull) < 0)
		return -1;
	if (get_gpio_pin(gpio, GPIO_PULL_ENABLE, &pullen) < 0)
		return -1;
	uint64_t base;
	int index = prom_get_prop_index(gpio->node, "reg-names", "gpio");
	if (index < 0)
		return -1;
	if (get_reg_addr(gpio->node, index, &base))
		return -1;

	base += SEGKPM_BASE;
	if (value)
		*((volatile uint32_t*)(base) + pull.reg) |= (1u << pull.bit);
	else
		*((volatile uint32_t*)(base) + pull.reg) &= ~(0u << pull.bit);

	*((volatile uint32_t*)(base) + pullen.reg) |= (1u << pullen.bit);
	return 0;
}
