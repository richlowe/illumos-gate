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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/asm_linkage.h>
#include <sys/errno.h>
#include "assym.h"

#define THREADP(reg)			\
	   mrs reg, tpidr_el1

#define	SUWORD(NAME, INSTR, REG, COPYOP)	\
	ENTRY(NAME);				\
	THREADP(x3);				\
	ldr	x4, [x3, #T_LOFAULT];		\
	adr	x5, _flt_##NAME;		\
	str	x5, [x3, #T_LOFAULT];		\
	dmb	ish;				\
	INSTR	REG, [x0];			\
	dmb	ish;				\
	str	x4, [x3, #T_LOFAULT];		\
	mov	x0, #0;				\
	ret;					\
_flt_##NAME:					\
	str	x4, [x3, #T_LOFAULT];		\
	ldr	x5, [x3, #T_COPYOPS];		\
	cbz	x5, 1f;				\
	ldr	x6, [x5, #(COPYOP)];		\
	br	x6;				\
1:	mov	w0, #-1;			\
	ret;					\
	SET_SIZE(NAME)

	SUWORD(suword64, sttr,  x1, CP_SUWORD64)
	SUWORD(suword32, sttr,  w1, CP_SUWORD32)
	SUWORD(suword16, sttrh, w1, CP_SUWORD16)
	SUWORD(suword8,  sttrb, w1, CP_SUWORD8)

#define	FUWORD(NAME, INSTR, INSTR2, REG, COPYOP)	\
	ENTRY(NAME);				\
	THREADP(x3);				\
	ldr	x4, [x3, #T_LOFAULT];		\
	adr	x5, _flt_##NAME;		\
	str	x5, [x3, #T_LOFAULT];		\
	dmb	ish;				\
	INSTR	REG, [x0];			\
	dmb	ish;				\
	str	x4, [x3, #T_LOFAULT];		\
	INSTR2	REG, [x1];			\
	mov	x0, #0;				\
	ret;					\
_flt_##NAME:					\
	str	x4, [x3, #T_LOFAULT];		\
	ldr	x5, [x3, #T_COPYOPS];		\
	cbz	x5, 1f;				\
	ldr	x6, [x5, #(COPYOP)];		\
	br	x6;				\
1:	mov	w0, #-1;			\
	ret;					\
	SET_SIZE(NAME)

	FUWORD(fuword64, ldtr,  str,  x10, CP_FUWORD64)
	FUWORD(fuword32, ldtr,  str,  w10, CP_FUWORD32)
	FUWORD(fuword16, ldtrh, strh, w10, CP_FUWORD16)
	FUWORD(fuword8,  ldtrb, strb, w10, CP_FUWORD8)


#define	SUWORD_NOERR(NAME, INSTR, REG)		\
	ENTRY(NAME);				\
	INSTR	REG, [x0];			\
	ret;					\
	SET_SIZE(NAME)

	SUWORD_NOERR(suword64_noerr, sttr,  x1)
	SUWORD_NOERR(suword32_noerr, sttr,  w1)
	SUWORD_NOERR(suword16_noerr, sttrh, w1)
	SUWORD_NOERR(suword8_noerr,  sttrb, w1)

#define	FUWORD_NOERR(NAME, INSTR, INSTR2, REG)	\
	ENTRY(NAME);				\
	INSTR	REG, [x0];			\
	INSTR2	REG, [x1];			\
	ret;					\
	SET_SIZE(NAME)

	FUWORD_NOERR(fuword64_noerr, ldtr,  str,  x10)
	FUWORD_NOERR(fuword32_noerr, ldtr,  str,  w10)
	FUWORD_NOERR(fuword16_noerr, ldtrh, strh, w10)
	FUWORD_NOERR(fuword8_noerr,  ldtrb, strb, w10)

	.weak	subyte
	.weak	subyte_noerr
	.weak	fulword
	.weak	fulword_noerr
	.weak	sulword
	.weak	sulword_noerr
subyte=suword8
subyte_noerr=suword8_noerr
fulword=fuword64
fulword_noerr=fuword64_noerr
sulword=suword64
sulword_noerr=suword64_noerr


/*
   void uzero(void *addr, size_t count)
 */
	ENTRY(uzero)
	and	x2, x0, #0x7
	cbz	x2, 2f
	cmp	x1, #0x7
	b.ls	2f
	sub	x2, x1, #0x8
	bic	x2, x2, #7
	add	x2, x2, #0x8
	add	x2, x0, x2
1:	sttr	xzr, [x0]
	add	x0, x0, #8
	cmp	x0, x2
	b.ne	1b
	and	x1, x1, #0x7
2:	add	x2, x0, x1
	cbz	x1, 4f
3:	sttrb	wzr, [x0]
	add	x0, x0, #1
	cmp	x0, x2
	b.ne	3b
4:	ret
	SET_SIZE(uzero)

/*
   void ucopy(const void *ufrom, void *uto, size_t ulength)
 */
	ENTRY(ucopy)
	orr	x3, x0, x1
	ands	x3, x3, #0x7
	b.ne	2f
	cmp	x2, #0x7
	b.ls	2f
	sub	x5, x2, #0x8
	bic	x5, x5, #7
	add	x5, x5, #0x8
1:	ldtr	x4, [x0]
	sttr	x4, [x1]
	add	x3, x3, #0x8
	add	x1, x1, #0x8
	add	x0, x0, #0x8
	cmp	x3, x5
	b.ne	1b
	and	x2, x2, #0x7
2:	mov	x3, #0x0
	cbz	x2, 4f
3:	ldtrb	w4, [x0]
	sttrb	w4, [x1]
	add	x3, x3, #0x1
	add	x1, x1, #0x1
	add	x0, x0, #0x1
	cmp	x3, x2
	b.ne	3b

4:	ret
	SET_SIZE(ucopy)

/*
   void ucopystr(const char *ufrom, char *uto, size_t umaxlength, size_t *ulencopied)
 */
	ENTRY(ucopystr)
	mov	x5, #0
1:	cbz	x2, 11f
	ldtrb	w4, [x0]
	sttrb	w4, [x1]
	add	x5, x5, #1
	cbz	w4, 11f

	add	x0, x0, #1
	add	x1, x1, #1
	sub	x2, x2, #1
	b	1b

11:	cbz	x3, 12f
	str	x5, [x3]
12:	ret
	SET_SIZE(ucopystr)

/*
   int copyoutstr(const char *kaddr, char *uaddr, size_t maxlength, size_t *lencopied)
   int copyoutstr_noerr(const char *kaddr, char *uaddr, size_t maxlength, size_t *lencopied)
 */
	ENTRY(copyoutstr)
	mov	x10, x0
	mov	x11, x1
	mov	x12, x2
	THREADP(x7)
	ldr	x8, [x7, #T_LOFAULT]
	adr	x9, .Lcopyoutstr_err
	str	x9, [x7, #T_LOFAULT]
	dmb	ish

	mov	x6, x0
	mov	x5, #0
	mov	w0, #0
1:	cbz	x2, 10f
	ldrb	w4, [x6]
	sttrb	w4, [x1]
	add	x5, x5, #1
	cbz	w4, 11f
	add	x6, x6, #1
	add	x1, x1, #1
	sub	x2, x2, #1
	b	1b
10:	mov	x0, #ENAMETOOLONG
11:	cbz	x3, 12f
	str	x5, [x3]
12:	dmb	ish
	str	x8, [x7, #T_LOFAULT]
	ret
.Lcopyoutstr_err:
	str	x8, [x7, #T_LOFAULT]
	ldr	x9, [x7, #T_COPYOPS]
	cbz	x9, 13f
	ldr	x5, [x9, #CP_COPYOUTSTR]
	mov	x0, x10
	mov	x1, x11
	mov	x2, x12
	br	x5
13:
	mov	w0, EFAULT
	ret
	SET_SIZE(copyoutstr)

	ENTRY(copyoutstr_noerr)
	mov	x6, x0
	mov	x5, #0
	mov	w0, #0
1:	cbz	x2, 10f
	ldrb	w4, [x6]
	sttrb	w4, [x1]
	add	x5, x5, #1
	cbz	w4, 11f
	add	x6, x6, #1
	add	x1, x1, #1
	sub	x2, x2, #1
	b	1b
10:	mov	x0, #ENAMETOOLONG
11:	cbz	x3, 12f
	str	x5, [x3]
12:	ret
	SET_SIZE(copyoutstr_noerr)

/*
   int copyinstr(const char *uaddr, char *kaddr, size_t maxlength, size_t *lencopied)
   int copyinstr_noerr(const char *uaddr, char *kaddr, size_t maxlength, size_t *lencopied)
 */
	ENTRY(copyinstr)
	mov	x10, x0
	mov	x11, x1
	mov	x12, x2
	THREADP(x7)
	ldr	x8, [x7, #T_LOFAULT]
	adr	x9, .Lcopyinstr_err
	str	x9, [x7, #T_LOFAULT]
	dmb	ish

	mov	x6, x0
	mov	x5, #0
	mov	w0, #0
1:	cbz	x2, 10f
	ldtrb	w4, [x6]
	strb	w4, [x1]
	add	x5, x5, #1
	cbz	w4, 11f
	add	x6, x6, #1
	add	x1, x1, #1
	sub	x2, x2, #1
	b	1b
10:	mov	x0, #ENAMETOOLONG
11:	cbz	x3, 12f
	str	x5, [x3]
12:	dmb	ish
	str	x8, [x7, #T_LOFAULT]
	ret
.Lcopyinstr_err:
	str	x8, [x7, #T_LOFAULT]
	ldr	x9, [x7, #T_COPYOPS]
	cbz	x9, 13f
	ldr	x5, [x9, #CP_COPYINSTR]
	mov	x0, x10
	mov	x1, x11
	mov	x2, x12
	br	x5
13:
	mov	w0, EFAULT
	ret
	SET_SIZE(copyinstr)

	ENTRY(copyinstr_noerr)
	mov	x6, x0
	mov	x5, #0
	mov	w0, #0
1:	cbz	x2, 10f
	ldtrb	w4, [x6]
	strb	w4, [x1]
	add	x5, x5, #1
	cbz	w4, 11f
	add	x6, x6, #1
	add	x1, x1, #1
	sub	x2, x2, #1
	b	1b
10:	mov	x0, #ENAMETOOLONG
11:	cbz	x3, 12f
	str	x5, [x3]
12:	ret
	SET_SIZE(copyinstr_noerr)

/*
   int copyin(const void *uaddr, void *kaddr, size_t count)
   void copyin_noerr(const void *ufrom, void *kto, size_t count)
 */
	ENTRY(copyin)
	adr	x9, .Lcopyin_err
0:	mov	x10, x0
	mov	x11, x1
	mov	x12, x2
	THREADP(x7)
	ldr	x8, [x7, #T_LOFAULT]
	str	x9, [x7, #T_LOFAULT]
	dmb	ish

	orr	x3, x0, x1
	ands	x3, x3, #0x7
	b.ne	2f
	cmp	x2, #0x7
	b.ls	2f
	sub	x5, x2, #0x8
	bic	x5, x5, #7
	add	x5, x5, #0x8
1:	ldtr	x4, [x0]
	str	x4, [x1]
	add	x3, x3, #0x8
	add	x1, x1, #0x8
	add	x0, x0, #0x8
	cmp	x3, x5
	b.ne	1b
	and	x2, x2, #0x7
2:	mov	x3, #0x0
	cbz	x2, 4f
3:	ldtrb	w4, [x0]
	strb	w4, [x1]
	add	x3, x3, #0x1
	add	x1, x1, #0x1
	add	x0, x0, #0x1
	cmp	x3, x2
	b.ne	3b

4:	dmb	ish
	str	x8, [x7, #T_LOFAULT]
	mov	x0, #0
	ret
.Lcopyin_err:
	str	x8, [x7, #T_LOFAULT]
	ldr	x9, [x7, #T_COPYOPS]
	cbz	x9, 13f
	ldr	x5, [x9, #CP_COPYIN]
	mov	x0, x10
	mov	x1, x11
	mov	x2, x12
	br	x5
13:	mov	w0, #-1
	ret

	ALTENTRY(xcopyin)
	ALTENTRY(xcopyin_nta)
	adr	x9, .Lxcopyin_err
	b	0b
.Lxcopyin_err:
	str	x8, [x7, #T_LOFAULT]
	ldr	x9, [x7, #T_COPYOPS]
	cbz	x9, 14f
	ldr	x5, [x9, #CP_XCOPYIN]
	mov	x0, x10
	mov	x1, x11
	mov	x2, x12
	br	x5
14:	mov	w0, #EFAULT
	ret
	SET_SIZE(xcopyin_nta)
	SET_SIZE(xcopyin)
	SET_SIZE(copyin)

	ENTRY(copyin_noerr)
	dmb	ish

	orr	x3, x0, x1
	ands	x3, x3, #0x7
	b.ne	2f
	cmp	x2, #0x7
	b.ls	2f
	sub	x5, x2, #0x8
	bic	x5, x5, #7
	add	x5, x5, #0x8
1:	ldtr	x4, [x0]
	str	x4, [x1]
	add	x3, x3, #0x8
	add	x1, x1, #0x8
	add	x0, x0, #0x8
	cmp	x3, x5
	b.ne	1b
	and	x2, x2, #0x7
2:	mov	x3, #0x0
	cbz	x2, 4f
3:	ldtrb	w4, [x0]
	strb	w4, [x1]
	add	x3, x3, #0x1
	add	x1, x1, #0x1
	add	x0, x0, #0x1
	cmp	x3, x2
	b.ne	3b

4:	dmb	ish
	ret
	SET_SIZE(copyin_noerr)


/*
   int copyout(const void *kaddr, void *uaddr, size_t count)
   void copyout_noerr(const void *kaddr, void *uaddr, size_t count)
 */
	ENTRY(copyout)
	adr	x9, .Lcopyout_err
0:	mov	x10, x0
	mov	x11, x1
	mov	x12, x2
	THREADP(x7)
	ldr	x8, [x7, #T_LOFAULT]
	str	x9, [x7, #T_LOFAULT]
	dmb	ish

	orr	x3, x0, x1
	ands	x3, x3, #0x7
	b.ne	2f
	cmp	x2, #0x7
	b.ls	2f
	sub	x5, x2, #0x8
	bic	x5, x5, #7
	add	x5, x5, #0x8
1:	ldr	x4, [x0]
	sttr	x4, [x1]
	add	x3, x3, #0x8
	add	x1, x1, #0x8
	add	x0, x0, #0x8
	cmp	x3, x5
	b.ne	1b
	and	x2, x2, #0x7
2:	mov	x3, #0x0
	cbz	x2, 4f
3:	ldrb	w4, [x0]
	sttrb	w4, [x1]
	add	x3, x3, #0x1
	add	x1, x1, #0x1
	add	x0, x0, #0x1
	cmp	x3, x2
	b.ne	3b

4:	dmb	ish
	str	x8, [x7, #T_LOFAULT]
	mov	x0, #0
	ret
.Lcopyout_err:
	str	x8, [x7, #T_LOFAULT]
	ldr	x9, [x7, #T_COPYOPS]
	cbz	x9, 13f
	ldr	x5, [x9, #CP_COPYOUT]
	mov	x0, x10
	mov	x1, x11
	mov	x2, x12
	br	x5
13:	mov	w0, #-1
	ret

	ALTENTRY(xcopyout)
	ALTENTRY(xcopyout_nta)
	adr	x9, .Lxcopyout_err
	b	0b
.Lxcopyout_err:
	str	x8, [x7, #T_LOFAULT]
	ldr	x9, [x7, #T_COPYOPS]
	cbz	x9, 14f
	ldr	x5, [x9, #CP_XCOPYOUT]
	mov	x0, x10
	mov	x1, x11
	mov	x2, x12
	br	x5
14:	mov	w0, #EFAULT
	ret
	SET_SIZE(xcopyout_nta)
	SET_SIZE(xcopyout)
	SET_SIZE(copyout)

	ENTRY(copyout_noerr)
	dmb	ish

	orr	x3, x0, x1
	ands	x3, x3, #0x7
	b.ne	2f
	cmp	x2, #0x7
	b.ls	2f
	sub	x5, x2, #0x8
	bic	x5, x5, #7
	add	x5, x5, #0x8
1:	ldr	x4, [x0]
	sttr	x4, [x1]
	add	x3, x3, #0x8
	add	x1, x1, #0x8
	add	x0, x0, #0x8
	cmp	x3, x5
	b.ne	1b
	and	x2, x2, #0x7
2:	mov	x3, #0x0
	cbz	x2, 4f
3:	ldrb	w4, [x0]
	sttrb	w4, [x1]
	add	x3, x3, #0x1
	add	x1, x1, #0x1
	add	x0, x0, #0x1
	cmp	x3, x2
	b.ne	3b

4:	dmb	ish
	ret
	SET_SIZE(copyout_noerr)



/*
   int kcopy(const void *from, void *to, size_t count)
 */
	ENTRY(kcopy)
	ALTENTRY(kcopy_nta)
	adr	x9, .Lkcopy_err
	THREADP(x7)
	ldr	x8, [x7, #T_LOFAULT]
	str	x9, [x7, #T_LOFAULT]
	dmb	ish

	orr	x3, x0, x1
	ands	x3, x3, #0x7
	b.ne	2f
	cmp	x2, #0x7
	b.ls	2f
	sub	x5, x2, #0x8
	bic	x5, x5, #7
	add	x5, x5, #0x8
1:	ldr	x4, [x0]
	str	x4, [x1]
	add	x3, x3, #0x8
	add	x1, x1, #0x8
	add	x0, x0, #0x8
	cmp	x3, x5
	b.ne	1b
	and	x2, x2, #0x7
2:	mov	x3, #0x0
	cbz	x2, 4f
3:	ldrb	w4, [x0]
	strb	w4, [x1]
	add	x3, x3, #0x1
	add	x1, x1, #0x1
	add	x0, x0, #0x1
	cmp	x3, x2
	b.ne	3b

4:	dmb	ish
	str	x8, [x7, #T_LOFAULT]
	mov	x0, #0
	ret

.Lkcopy_err:
	str	x8, [x7, #T_LOFAULT]
13:	mov	w0, #EFAULT
	ret
	SET_SIZE(kcopy_nta)
	SET_SIZE(kcopy)

/*
   int kzero(void *addr, size_t count)
 */
	ENTRY(kzero)
	adr	x9, .Lkzero_err
	THREADP(x7)
	ldr	x8, [x7, #T_LOFAULT]
	str	x9, [x7, #T_LOFAULT]
	dmb	ish

	and	x2, x0, #0x7
	cbz	x2, 2f
	cmp	x1, #0x7
	b.ls	2f
	sub	x2, x1, #0x8
	bic	x2, x2, #7
	add	x2, x2, #0x8
	add	x2, x0, x2
1:	str	xzr, [x0],#8
	cmp	x0, x2
	b.ne	1b
	and	x1, x1, #0x7
2:	add	x2, x0, x1
	cbz	x1, 4f
3:	strb	wzr, [x0],#1
	cmp	x0, x2
	b.ne	3b

4:	dmb	ish
	str	x8, [x7, #T_LOFAULT]
	mov	x0, #0
	ret
.Lkzero_err:
	str	x8, [x7, #T_LOFAULT]
13:	mov	w0, #EFAULT
	ret
	SET_SIZE(kzero)


/*
   int copystr(const char *from, char *to, size_t maxlength, size_t *lencopied)
 */
	ENTRY(copystr)
	THREADP(x7)
	ldr	x8, [x7, #T_LOFAULT]
	adr	x9, .Lcopystr_err
	str	x9, [x7, #T_LOFAULT]
	dmb	ish

	mov	x6, x0
	mov	x5, #0
	mov	w0, #0
1:	cbz	x2, 10f
	ldrb	w4, [x6]
	strb	w4, [x1]
	add	x5, x5, #1
	cbz	w4, 11f
	add	x6, x6, #1
	add	x1, x1, #1
	sub	x2, x2, #1
	b	1b
10:	mov	x0, #ENAMETOOLONG
11:	cbz	x3, 12f
	str	x5, [x3]
12:	dmb	ish
	str	x8, [x7, #T_LOFAULT]
	ret
.Lcopystr_err:
	str	x8, [x7, #T_LOFAULT]
	mov	w0, EFAULT
	ret
	SET_SIZE(copystr)
