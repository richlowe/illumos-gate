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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Copyright 2017 Hayashi Naoyuki
 * Use is subject to license terms.
 */

#pragma weak _makecontext = makecontext

#include <sys/stack.h>

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>

extern void __resumecontext(void);

void
makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
{
	long *sp;
	va_list ap;
	size_t size;
	int pusharg = (argc > 8 ? argc - 8 : 0);
	greg_t tmp;
	int i;

	ucp->uc_mcontext.gregs[REG_PC] = (greg_t)func;

	size = sizeof (long) * (pusharg + 1);

	sp = (long *)(((uintptr_t)ucp->uc_stack.ss_sp +
	    ucp->uc_stack.ss_size - size) & ~(STACK_ENTRY_ALIGN - 1));

	if (((uintptr_t)sp & (STACK_ALIGN - 1ul)) == 0)
		sp -= STACK_ENTRY_ALIGN / sizeof (*sp);

	ucp->uc_mcontext.gregs[REG_SP] = (greg_t)sp;

	va_start(ap, argc);

	for (i = 0; i < argc; i++) {
		tmp = va_arg(ap, long);
		switch (i) {
		case 0:
			ucp->uc_mcontext.gregs[REG_X0] = tmp;
			break;
		case 1:
			ucp->uc_mcontext.gregs[REG_X1] = tmp;
			break;
		case 2:
			ucp->uc_mcontext.gregs[REG_X2] = tmp;
			break;
		case 3:
			ucp->uc_mcontext.gregs[REG_X3] = tmp;
			break;
		case 4:
			ucp->uc_mcontext.gregs[REG_X4] = tmp;
			break;
		case 5:
			ucp->uc_mcontext.gregs[REG_X5] = tmp;
			break;
		case 6:
			ucp->uc_mcontext.gregs[REG_X6] = tmp;
			break;
		case 7:
			ucp->uc_mcontext.gregs[REG_X7] = tmp;
			break;
		default:
			*sp++ = tmp;
			break;
		}
	}

	va_end(ap);

	ucp->uc_mcontext.gregs[REG_X30] = (long)__resumecontext;
}

/*
 * On ARM at present we have no extended save, and just need to allocate a
 * ucontext_t.
 */
ucontext_t *
ucontext_alloc(uint32_t flags)
{
	if (flags != 0) {
		errno = EINVAL;
		return (NULL);
	}

	return (calloc(1, sizeof (ucontext_t)));
}

void
ucontext_free(ucontext_t *ucp)
{
	free(ucp);
}
