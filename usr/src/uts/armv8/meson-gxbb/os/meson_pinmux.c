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

int plat_pinmux_set(pnode_t node)
{
	struct pin_setting {
		uint32_t id;
		uint32_t mask;
	};
	enum {
		PERIPHS_PIN_MUX_0 = 0,
		PERIPHS_PIN_MUX_1,
		PERIPHS_PIN_MUX_2,
		PERIPHS_PIN_MUX_3,
		PERIPHS_PIN_MUX_4,
		PERIPHS_PIN_MUX_5,
		PERIPHS_PIN_MUX_6,
		PERIPHS_PIN_MUX_7,
		PERIPHS_PIN_MUX_8,
		PERIPHS_PIN_MUX_9,
		AO_RTI_PIN_MUX_REG = 0x10,
		AO_RTI_PIN_MUX_REG2,
	};

	static const uint64_t pinctrl_addr[] = {
		[PERIPHS_PIN_MUX_0] = 0xc88344b0,
		[PERIPHS_PIN_MUX_1] = 0xc88344b4,
		[PERIPHS_PIN_MUX_2] = 0xc88344b8,
		[PERIPHS_PIN_MUX_3] = 0xc88344bc,
		[PERIPHS_PIN_MUX_4] = 0xc88344c0,
		[PERIPHS_PIN_MUX_5] = 0xc88344c4,
		[PERIPHS_PIN_MUX_6] = 0xc88344c8,
		[PERIPHS_PIN_MUX_7] = 0xc88344cc,
		[PERIPHS_PIN_MUX_8] = 0xc88344d0,
		[PERIPHS_PIN_MUX_9] = 0xc88344d4,
		[AO_RTI_PIN_MUX_REG] = 0xc8100014,
		[AO_RTI_PIN_MUX_REG2] = 0xc8100018,
	};

	int len = prom_getproplen(node, "amlogic,clrmask");
	if (len > 0) {
		if (len % sizeof(struct pin_setting))
			return -1;

		struct pin_setting *setting = __builtin_alloca(len);
		prom_getprop(node, "amlogic,clrmask", (caddr_t)setting);
		for (int i = 0; i < len / sizeof(struct pin_setting); i++) {
			uint32_t id = ntohl(setting[i].id);
			uint32_t mask = ntohl(setting[i].mask);
			if (id > AO_RTI_PIN_MUX_REG2)
				return -1;

			*(volatile uint32_t *)(SEGKPM_BASE + pinctrl_addr[id]) &= ~mask;
		}
	}

	len = prom_getproplen(node, "amlogic,setmask");
	if (len > 0) {
		if (len % sizeof(struct pin_setting))
			return -1;
		struct pin_setting *setting = __builtin_alloca(len);
		prom_getprop(node, "amlogic,setmask", (caddr_t)setting);
		for (int i = 0; i < len / sizeof(struct pin_setting); i++) {
			uint32_t id = ntohl(setting[i].id);
			uint32_t mask = ntohl(setting[i].mask);
			if (id > AO_RTI_PIN_MUX_REG2)
				return -1;
			*(volatile uint32_t *)(SEGKPM_BASE + pinctrl_addr[id]) |= mask;
		}
	}
	return 0;
}

