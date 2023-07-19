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

#include <sys/param.h>
#include <sys/time.h>
#include <sys/clock.h>
#include <sys/rtc.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/platmod.h>

static pnode_t rtc_node = -1;
static uint64_t rtc_phys;

#define RTCDR (*(volatile uint32_t *)(SEGKPM_BASE + rtc_phys + 0x00))
#define RTCLR (*(volatile uint32_t *)(SEGKPM_BASE + rtc_phys + 0x08))
#define RTCCR (*(volatile uint32_t *)(SEGKPM_BASE + rtc_phys + 0x0c))

static pnode_t
find_compatible_node(pnode_t node, const char *compatible)
{
	if (prom_is_compatible(node, compatible)) {
		return node;
	}

	pnode_t child = prom_childnode(node);
	while (child > 0) {
		node = find_compatible_node(child, compatible);
		if (node > 0)
			return node;
		child = prom_nextnode(child);
	}
	return OBP_NONODE;
}

static void
init_rtc(void)
{
	if (rtc_node > 0)
		return;

	pnode_t node;

	node = find_compatible_node(prom_rootnode(), "arm,pl031");
	if (node > 0) {
		uint64_t base;
		if (prom_get_reg(node, 0, &base) == 0) {
			rtc_phys = base;
			rtc_node = node;
			if ((RTCCR & 0x1) == 0)
				RTCCR |= 0x1;
		}
	}
}

static void
todpl031_set(timestruc_t ts)
{
	ASSERT(MUTEX_HELD(&tod_lock));

	init_rtc();

	if (rtc_node < 0)
		return;

	uint32_t sec = ts.tv_sec - ggmtl();
	RTCLR = sec;
}

static timestruc_t
todpl031_get(void)
{
	ASSERT(MUTEX_HELD(&tod_lock));

	init_rtc();

	if (rtc_node < 0) {
		timestruc_t ts = {0};
		tod_status_set(TOD_GET_FAILED);
		return (ts);
	}

	tod_status_clear(TOD_GET_FAILED);

	uint32_t sec = RTCDR;

	timestruc_t ts = { .tv_sec = sec + ggmtl(), .tv_nsec = 0};
	return ts;
}

static struct modlmisc modlmisc = {
	&mod_miscops, "todpl031"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	extern tod_ops_t tod_ops;
	if (strcmp(tod_module_name, "todpl031") == 0) {
		tod_ops.tod_get = todpl031_get;
		tod_ops.tod_set = todpl031_set;
		tod_ops.tod_set_watchdog_timer = NULL;
		tod_ops.tod_clear_watchdog_timer = NULL;
		tod_ops.tod_set_power_alarm = NULL;
		tod_ops.tod_clear_power_alarm = NULL;
	}

	return mod_install(&modlinkage);
}

int
_fini(void)
{
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
