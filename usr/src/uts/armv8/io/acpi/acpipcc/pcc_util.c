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
 * Copyright 2026 Michael van der Westhuizen
 */

#include "pcc_impl.h"

/*
 * Derive the command completion timeout for a channel from its nominal latency.
 *
 * The nominal latency is in microseconds, with a floor of 1000us when the
 * firmware declares zero.  The result is the latency in nanoseconds, scaled
 * by PCC_TIMEOUT_MULTIPLIER and capped at PCC_MAX_TIMEOUT.
 */
hrtime_t
pcc_cmd_timeout(pcc_chan_t *pc)
{
	hrtime_t timeout_ns;

	timeout_ns = (hrtime_t)pc->pc_nominal_lat * NANOSEC / MICROSEC;
	if (timeout_ns == 0) {
		timeout_ns = (hrtime_t)1000 * NANOSEC / MICROSEC;
	}

	timeout_ns *= PCC_TIMEOUT_MULTIPLIER;
	if (timeout_ns > PCC_MAX_TIMEOUT) {
		timeout_ns = PCC_MAX_TIMEOUT;
	}

	return (timeout_ns);
}
