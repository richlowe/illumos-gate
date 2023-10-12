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

#include <sys/ieeefp.h>
#include <base_conversion.h>
#include "fp.h"

enum fp_direction_type
_QgetRD(void)
{
	return (FPCR_RM(read_fpcr()));
}

/*
 * Returns IEEE mode/status and
 * sets up standard environment for base conversion.
 */
void
__get_ieee_flags(__ieee_flags_type *b)
{
	uint32_t fpcr = read_fpcr();
	uint32_t fpsr = read_fpsr();

	b->mode = fpcr;
	b->status = fpsr;

	/* The defaults as from __fpstart */
	write_fpcr(FPCR_RM_RN << FPCR_RM_SHIFT);
	write_fpsr(0);
}

void
__set_ieee_flags(__ieee_flags_type *b)
{
	write_fpcr(b->mode);
	write_fpsr(b->status);
}

double
__dabs(double *d)
{
	return ((*d < 0.0) ? -*d : *d);
}

double
__mul_set(double x, double y, int *pe)
{
	uint32_t fpcr = read_fpcr();
	uint32_t fpsr = read_fpsr();

	write_fpcr(fpcr & FPCR_RM_MASK);
	write_fpsr(0);

	/*
	 * This must happen explicitly in program order, if it is re-ordered
	 * by the compiler the flag checks may not take place _after_ the
	 * multiplication.
	 */
	volatile double z = x * y;

	volatile uint32_t fpsr_new = read_fpsr();

	write_fpsr(fpsr);
	write_fpcr(fpcr);

	if ((fpsr_new & FPSR_TRAP_MASK) == 0) {
		*pe = 0;
	} else {
		*pe = 1;
	}
	return (z);
}

double
__div_set(double x, double y, int *pe)
{
	uint32_t fpcr = read_fpcr();
	uint32_t fpsr = read_fpsr();

	write_fpcr(fpcr & FPCR_RM_MASK);
	write_fpsr(0);

	double z = x / y;

	uint32_t fpsr_new = read_fpsr();

	write_fpsr(fpsr);
	write_fpcr(fpcr);

	if ((fpsr_new & FPSR_TRAP_MASK) == 0) {
		*pe = 0;
	} else {
		*pe = 1;
	}
	return (z);
}
