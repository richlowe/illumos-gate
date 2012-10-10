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
 * Copyright 2011 Nexenta Systems, Inc.  All rights reserved.
 */
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright 2011, Richard Lowe.
 */

/* Functions in this file are duplicated in libm.m4.  Keep them in sync */

#ifndef _LIBM_INLINES_H
#define	_LIBM_INLINES_H

#ifdef __GNUC__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/ieeefp.h>

extern __inline__ double
__ieee754_sqrt(double a)
{
	double ret;

	__asm__ __volatile__("sqrtsd %1, %0\n\t" : "=x" (ret) : "x" (a));
	return (ret);
}

extern __inline__ float
__inline_sqrtf(float a)
{
	float ret;

	__asm__ __volatile__("sqrtss %1, %0\n\t" : "=x" (ret) : "x" (a));
	return (ret);
}

extern __inline__ double
__inline_sqrt(double a)
{
	double ret;

	__asm__ __volatile__("sqrtsd %1, %0\n\t" : "=x" (ret) : "x" (a));
	return (ret);
}

/* XXX: Not actually called */
extern __inline__ short
__inline_fstsw(void)
{
	short ret;

	__asm__ __volatile__("fstsw %0\n\t" : "=r" (ret));
	return (ret);
}

/*
 * 00 - 24 bits
 * 01 - reserved
 * 10 - 53 bits
 * 11 - 64 bits
 */
extern __inline__ int
__swapRP(int i)
{
	int ret;
	uint16_t cw;

	__asm__ __volatile__("fstcw %0\n\t" : "=m" (cw));

	ret = (cw >> 8) & 0x3;
	cw = (cw & 0xfcff) | ((i & 0x3) << 8);

	__asm__ __volatile__("fldcw %0\n\t" : : "m" (cw));

	return (ret);
}

/*
 * 00 - Round to nearest, with even preferred
 * 01 - Round down
 * 10 - Round up
 * 11 - Chop
 */
extern __inline__ enum fp_direction_type
__swap87RD(enum fp_direction_type i)
{
	int ret;
	uint16_t cw;

	__asm__ __volatile__("fstcw %0\n\t" : "=m" (cw));

	ret = (cw >> 10) & 0x3;
	cw = (cw & 0xf3ff) | ((i & 0x3) << 10);

	__asm__ __volatile__("fldcw %0\n\t" : : "m" (cw));

	return (ret);
}

extern __inline__ int
abs(int i)
{
	int ret;
	__asm__ __volatile__(
	    "movl    %1,%0\n\t"
	    "negl    %1\n\t"
	    "cmovnsl %1,%0\n\t"
	    : "=r" (ret), "+r" (i));
	return (ret);
}

extern __inline__ double
copysign(double d1, double d2)
{
	double ret;

	__asm__ __volatile__(
	    "movq   $0x7fffffffffffffff,%%rax\n\t"
	    "movd   %%rax,%%xmm2\n\t"
	    "andpd  %%xmm2,%0\n\t"
	    "andnpd %1,%%xmm2\n\t"
	    "orpd   %%xmm2,%0\n\t"
	    : "=x" (ret)
	    : "x" (d2), "0" (d1)
	    : "xmm2", "rax");

	return (ret);
}

extern __inline__ double
d_sqrt_(double *d)
{
	double ret;
	__asm__ __volatile__(
	    "movlpd %1,%0\n\t"
	    "sqrtsd %0,%0"
	    : "=x" (ret)
	    : "m" (*d));
	return (ret);
}

extern __inline__ double
fabs(double d)
{
	double ret;

	__asm__ __volatile__(
	    "movq  $0x7fffffffffffffff,%%rax\n\t"
	    "movd  %%rax,%%xmm1\n\t"
	    "andpd %%xmm1,%0"
	    : "=x" (ret)
	    : "0" (d)
	    : "rax", "xmm1");

	return (ret);
}

extern __inline__ float
fabsf(float d)
{
	float ret;

	__asm__ __volatile__(
	    "andpd %2,%0"
	    : "=x" (ret)
	    : "0" (d), "x" (0x7fffffff));

	return (ret);
}

extern __inline__ int
finite(double d)
{
	long ret;		    /* A long, so gcc chooses an %r* for %0 */

	__asm__ __volatile__(
	    "movq %1,%%rcx\n\t"
	    "movq $0x7fffffffffffffff,%0\n\t"
	    "andq %%rcx,%0\n\t"
	    "movq $0x7ff0000000000000,%%rcx\n\t"
	    "subq %%rcx,%0\n\t"
	    "shrq $63,%0\n\t"
	    : "=r" (ret)
	    : "x" (d)
	    : "rcx");

	return (ret);
}

extern __inline__ float
r_sqrt_(float *f)
{
	float ret;

	__asm__ __volatile__(
	    "movss  %1,%0\n\t"
	    "sqrtss %0,%0\n\t"
	    : "+x" (ret)
	    : "m" (*f));
	return (ret);
}

extern __inline__ int
signbit(double d)
{
	long ret;
	__asm__ __volatile__(
	    "movmskpd %1,%0\n\t"
	    "andq     $1, %0\n\t"
	    : "=r" (ret)
	    : "x" (d));
	return (ret);
}

extern __inline__ int
signbitf(float f)
{
	int ret;
	__asm__ __volatile__(
	    "movskps %1,%0\n\t"
	    "andq    $1, %0\n\t"
	    : "=r" (ret)
	    : "x" (f));
	return (ret);
}

extern __inline__ double
sqrt(double d)
{
	double ret;

	__asm__ __volatile__(
	    "sqrtsd %0, %0"
	    : "=x" (ret)
	    : "0" (d));
	return (ret);
}

extern __inline__ float
sqrtf(float f)
{
	float ret;

	__asm__ __volatile__(
	    "sqrtss %0, %0"
	    : "=x" (ret)
	    : "0" (f));
	return (ret);
}

#ifdef __cplusplus
}
#endif

#endif  /* __GNUC__ */

#endif /* _LIBM_INLINES_H */
