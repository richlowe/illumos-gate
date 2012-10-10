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

#include <sys/types.h>
#include <sys/ieeefp.h>

#ifdef __cplusplus
extern "C" {
#endif

extern __inline__ enum fp_class_type
fp_classf(float f)
{
	enum fp_class_type ret;
	int fint;                   /* scratch for f as int */

	__asm__ __volatile__(
	    "fabss  %2,%2\n\t"
	    "st	    %2,%1\n\t"
	    "ld	    %1,%0\n\t"
	    "orcc   %%g0,%0,%%g0\n\t"
	    "be,pn  %%icc,2f\n\t"
	    "nop\n\t"
	    "1:\n\t"
	    "sethi  %%hi(0x7f800000),%%o1\n\t"
	    "andcc  %0,%%o1,%%g0\n\t"
	    "bne,pt %%icc,1f\n\t"
	    "nop\n\t"
	    "or	    %%g0,1,%0\n\t"
	    "ba	    2f\n\t"
	    "nop\n\t"
	    "1:\n\t"
	    "subcc  %0,%%o1,%%g0\n\t"
	    "bge,pn %%icc,1f\n\t"
	    "nop\n\t"
	    "or	    %%g0,2,%0\n\t"
	    "ba	    2f\n\t"
	    "nop\n\t"
	    "1:\n\t"
	    "bg,pn  %%icc,1f\n\t"
	    "nop\n\t"
	    "or	    %%g0,3,%0\n\t"
	    "ba	    2f\n\t"
	    "nop\n\t"
	    "1:\n\t"
	    "sethi  %%hi(0x00400000),%%o1\n\t"
	    "andcc  %0,%%o1,%%g0\n\t"
	    "or	    %%g0,4,%0\n\t"
	    "bne,pt %%icc,2f\n\t"
	    "nop\n\t"
	    "or	    %%g0,5,%0\n\t"
	    "2:\n\t"
	    : "=r" (ret), "=m" (fint)
	    : "f" (f)
	    : "o1");

    return (ret);
}

extern __inline__ enum fp_class_type
fp_class(double d)
{
	enum fp_class_type ret;
	uint64_t dint;		/* Scratch for d-as-long */

	__asm__ __volatile__(
	    "fabsd  %2,%2\n\t"
	    "std    %2,%1\n\t"
	    "ldx    %1,%0\n\t"
	    "orcc   %%g0,%0,%%g0\n\t"
	    "be,pn  %%xcc,2f\n\t"
	    "nop\n\t"
	    "sethi  %%hi(0x7ff00000),%%o1\n\t"
	    "sllx   %%o1,32,%%o1\n\t"
	    "andcc  %0,%%o1,%%g0\n\t"
	    "bne,pt %%xcc,1f\n\t"
	    "nop\n\t"
	    "or	    %%g0,1,%0\n\t"
	    "ba	    2f\n\t"
	    "nop\n\t"
	    "1:\n\t"
	    "subcc  %0,%%o1,%%g0\n\t"
	    "bge,pn %%xcc,1f\n\t"
	    "nop\n\t"
	    "or	    %%g0,2,%0\n\t"
	    "ba	    2f\n\t"
	    "nop\n\t"
	    "1:\n\t"
	    "andncc %0,%%o1,%0\n\t"
	    "bne,pn %%xcc,1f\n\t"
	    "nop\n\t"
	    "or	    %%g0,3,%0\n\t"
	    "ba	    2f\n\t"
	    "nop\n\t"
	    "1:\n\t"
	    "sethi  %%hi(0x00080000),%%o1\n\t"
	    "sllx   %%o1,32,%%o1\n\t"
	    "andcc  %0,%%o1,%%g0\n\t"
	    "or	    %%g0,4,%0\n\t"
	    "bne,pt %%xcc,2f\n\t"
	    "nop\n\t"
	    "or	    %%g0,5,%0\n\t"
	    "2:\n\t"
	    : "=r" (ret), "=m" (dint)
	    : "e" (d)
	    : "o1");

	return (ret);
}

extern __inline__ float
__inline_sqrtf(float f)
{
	float ret;

	__asm__ __volatile__("fsqrts %0,%0\n\t" : "=f" (ret) : "f" (f));
	return (ret);
}

extern __inline__ double
__inline_sqrt(double d)
{
	double ret;

	__asm__ __volatile__("fsqrtd %0,%0\n\t" : "=f" (ret) : "0" (d));
	return (ret);
}

extern __inline__ int
__swapEX(int i)
{
	int ret;
	uint32_t fsr;

	__asm__ __volatile__(
	    "and  %0,0x1f,%%o1\n\t"
	    "sll  %%o1,5,%%o1\n\t"    /* input to aexc bit location */
	    ".volatile\n\t"
	    "st   %%fsr,%2\n\t"
	    "ld	  %2,%0\n\t"	      /* = fsr */
	    "andn %0,0x3e0,%%o2\n\t"
	    "or   %%o1,%%o2,%%o1\n\t" /* o1 = new fsr */
	    "st	  %%o1,%2\n\t"
	    "ld	  %2,%%fsr\n\t"
	    "srl  %0,5,%0\n\t"
	    "and  %0,0x1f,%0\n\t"
	    ".nonvolatile\n\t"
	    : "=r" (ret)
	    : "0" (i), "m" (fsr)
	    : "o1", "o2");

	return (ret);
}

/*
 * On the SPARC, __swapRP is a no-op; always return 0 for backward
 * compatibility
 */
/* ARGSUSED */
extern __inline__ enum fp_precision_type
__swapRP(enum fp_precision_type i)
{
	return (0);
}

extern __inline__ enum fp_direction_type
__swapRD(enum fp_direction_type d)
{
	enum fp_direction_type ret;
	uint32_t fsr;

	__asm__ __volatile__(
	    "and   %0,0x3,%0\n\t"
	    "sll   %0,30,%%o1\n\t"	      /* input to RD bit location */
	    ".volatile\n\t"
	    "st    %%fsr,%2\n\t"
	    "ld	   %2,%0\n\t"                 /* o0 = fsr */
	    "sethi %%hi(0xc0000000),%%o4\n\t" /* mask of rounding direction bits */
	    "andn  %0,%%o4,%%o2\n\t"
	    "or    %%o1,%%o2,%%o1\n\t"	      /* o1 = new fsr */
	    "st	   %%o1,%2\n\t"
	    "ld	   %2,%%fsr\n\t"
	    "srl   %0,30,%0\n\t"
	    "and   %0,0x3,%0\n\t"
	    ".nonvolatile\n\t"
	    : "=r" (ret)
	    : "0" (d), "m" (fsr)
	    : "o1", "o2", "o4");

	return (ret);
}

extern __inline__ int
__swapTE(int i)
{
	int ret;
	uint32_t fsr;
	
	__asm__ __volatile__(
	    "and   %0,0x1f,%0\n\t"
	    "sll   %0,23,%%o1\n\t"            /* input to TEM bit location */
	    ".volatile\n\t"
	    "st    %%fsr,%2\n\t"
	    "ld	   %2,%0\n\t"                 /* o0 = fsr */
	    "sethi %%hi(0x0f800000),%%o4\n\t" /* mask of TEM (Trap Enable Mode bits) */
	    "andn  %0,%%o4,%%o2\n\t"	  
	    "or    %%o1,%%o2,%%o1\n\t"        /* o1 = new fsr */
	    "st	   %%o1,%2\n\t"
	    "ld	   %2,%%fsr\n\t"
	    "srl   %0,23,%0\n\t"
	    "and   %0,0x1f,%0\n\t"
	    ".nonvolatile\n\t"
	    : "=r" (ret)
	    : "0" (i), "m" (fsr)
	    : "o1", "o2", "o4");

	return (ret);
}


extern __inline__ double
sqrt(double d)
{
    double ret;

    __asm__ __volatile__("fsqrtd %0,%0\n\t" : "=f" (ret) : "0" (d));
    return (ret);
}

extern __inline__ float
sqrtf(float f)
{
    float ret;

    __asm__ __volatile__("fsqrts %0,%0\n\t" : "=f" (ret) : "0" (f));
    return (ret);
}

extern __inline__ double
fabs(double d)
{
    double ret;

    __asm__ __volatile__("fabsd %0,%0\n\t" : "=e" (ret) : "0" (d));
    return (ret);
}

extern __inline__ float
fabsf(float f)
{
    float ret;

    __asm__ __volatile__("fabss %0,%0\n\t" : "=f" (ret) : "0" (f));
    return (ret);
}

#ifdef __cplusplus
}
#endif

#endif  /* __GNUC__ */

#endif /* _LIBM_INLINES_H */
