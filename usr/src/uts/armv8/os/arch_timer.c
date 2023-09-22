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
 * Copyright 2023 Brian McKenzie
 */

/*
 * Generic timer interface.
 */

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/cmn_err.h>
#include <sys/controlregs.h>
#include <sys/arch_timer.h>

/* Generic timer control bits */
#define	CNT_CTL_ENABLE	(1u << 0)
#define	CNT_CTL_IMASK	(1u << 1)
#define	CNT_CTL_ISTATUS	(1u << 2)

struct arch_timer_ops {
	void (*write_cnt_cval)(uint64_t);
	uint64_t (*read_cnt_ctl)(void);
	void (*write_cnt_ctl)(uint64_t);
	uint64_t (*read_cnt_ct)(void);
};

static struct arch_timer_ops timer_ops[] = {
	[TMR_PHYS] = {
		.write_cnt_cval = write_cntp_cval,
		.read_cnt_ctl = read_cntp_ctl,
		.write_cnt_ctl = write_cntp_ctl,
		.read_cnt_ct = read_cntpct,
	},
	[TMR_VIRT] = {
		.write_cnt_cval = write_cntv_cval,
		.read_cnt_ctl = read_cntv_ctl,
		.write_cnt_ctl = write_cntv_ctl,
		.read_cnt_ct = read_cntvct,
	},
};

static struct arch_timer_ops *ops;

static uint64_t timer_freq = 0;

void
arch_timer_mask_irq(void)
{
	uint64_t ctl;

	ctl = ops->read_cnt_ctl();
	ctl |= CNT_CTL_IMASK;
	ops->write_cnt_ctl(ctl);
}

void
arch_timer_unmask_irq(void)
{
	uint64_t ctl;

	ctl = ops->read_cnt_ctl();
	ctl &= ~CNT_CTL_IMASK;
	ops->write_cnt_ctl(ctl);
}

void
arch_timer_enable(void)
{
	uint64_t ctl;

	ctl = ops->read_cnt_ctl();
	ctl |= CNT_CTL_ENABLE;
	ops->write_cnt_ctl(ctl);
}

void
arch_timer_disable(void)
{
	uint64_t ctl;

	ctl = ops->read_cnt_ctl();
	ctl &= ~CNT_CTL_ENABLE;
	ops->write_cnt_ctl(ctl);
}

void
arch_timer_set_cval(uint64_t cval)
{
	ops->write_cnt_cval(cval);
}

static void
arch_timer_find_node(pnode_t node, void *arg)
{
	if (!prom_is_compatible(node, "arm,armv8-timer"))
		return;

	*(pnode_t *)arg = node;
}

static uint64_t
arch_timer_find_clock_freq(void)
{
	pnode_t node;
	uint64_t freq = 0;

	prom_walk(arch_timer_find_node, &node);

	if (node > 0)
		freq = prom_get_prop_int(node, "clock-frequency", 0);

	return (freq);
}

uint64_t
arch_timer_freq(void)
{
	if (timer_freq)
		return (timer_freq);

	/*
	 * Attempt to get the clock frequency from the device tree before
	 * falling back to cntfrq_el0. This is a workaround for systems where
	 * the the firmware either misconfigures or never sets cntfrq_el0.
	 */
	if ((timer_freq = arch_timer_find_clock_freq()) > 0)
		return (timer_freq);

	if ((timer_freq = read_cntfrq()) > 0)
		return (timer_freq);

	/* If we got here then we don't have a frequency at all. */
	cmn_err(CE_PANIC, "arch_timer_freq: missing frequency");

	/* NOTREACHED */
	return (0);
}

uint64_t
arch_timer_count(void)
{
	return (ops->read_cnt_ct());
}

void
arch_timer_select(arch_timer_t type)
{
	ops = &timer_ops[type];
}

void
arch_timer_udelay(uint32_t us)
{
	int32_t delay;
	uint32_t first, last;
	uint32_t ticks_per_usec = (arch_timer_freq() / 1000000) + 1;

	first = arch_timer_count();
	delay = us * ticks_per_usec;

	while (delay > 0) {
		last = arch_timer_count();
		delay -= (last - first);
		first = last;
	}
}
