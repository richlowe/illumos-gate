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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */


/*
 * The Sun Studio and GCC (patched for opensolaris/illumos) compilers
 * implement a argument saving scheme on amd64 via the -Wu,save-args or
 * options.  When the option is specified, INTEGER type function arguments
 * passed via registers will be saved on the stack immediately after %rbp, and
 * will not be modified through out the life of the routine.
 *
 *				+--------+
 *		%rbp	-->     |  %rbp  |
 *				+--------+
 *		-0x8(%rbp)	|  %rdi  |
 *				+--------+
 *		-0x10(%rbp)	|  %rsi  |
 *				+--------+
 *		-0x18(%rbp)	|  %rdx  |
 *				+--------+
 *		-0x20(%rbp)	|  %rcx  |
 *				+--------+
 *		-0x28(%rbp)	|  %r8   |
 *				+--------+
 *		-0x30(%rbp)	|  %r9   |
 *				+--------+
 *
 *
 * For example, for the following function,
 *
 * void
 * foo(int a1, int a2, int a3, int a4, int a5, int a6, int a7)
 * {
 * ...
 * }
 *
 * Disassembled code will look something like the following:
 *
 *     pushq	%rbp
 *     movq	%rsp, %rbp
 *     subq	$imm8, %rsp			**
 *     movq	%rdi, -0x8(%rbp)
 *     movq	%rsi, -0x10(%rbp)
 *     movq	%rdx, -0x18(%rbp)
 *     movq	%rcx, -0x20(%rbp)
 *     movq	%r8, -0x28(%rbp)
 *     movq	%r9, -0x30(%rbp)
 *     ...
 * or
 *     pushq	%rbp
 *     movq	%rsp, %rbp
 *     subq	$imm8, %rsp			**
 *     movq	%r9, -0x30(%rbp)
 *     movq	%r8, -0x28(%rbp)
 *     movq	%rcx, -0x20(%rbp)
 *     movq	%rdx, -0x18(%rbp)
 *     movq	%rsi, -0x10(%rbp)
 *     movq	%rdi, -0x8(%rbp)
 *     ...
 *
 * **: The space being reserved is in addition to what the current
 *     function prolog already reserves.
 *
 * If there are odd number of arguments to a function, additional space is
 * reserved on the stack to maintain 16-byte alignment.  For example,
 *
 *     argc == 0: no argument saving.
 *     argc == 3: save 3, but space for 4 is reserved
 *     argc == 7: save 6.
 */

#include <sys/sysmacros.h>
#include <sys/types.h>
#include <saveargs.h>

/*
 * Size of the instruction sequence arrays.  It should correspond to
 * the maximum number of arguments passed via registers.
 */
#define	INSTR_ARRAY_SIZE	6

#define	INSTR4(ins, off)	\
	(ins[(off)] + (ins[(off) + 1] << 8) + (ins[(off + 2)] << 16) + \
	(ins[(off) + 3] << 24))

/*
 * Sun Studio 10 patch implementation saves %rdi first;
 * GCC 3.4.3 Sun branch implementation saves them in reverse order.
 */
static const uint32_t save_instr[INSTR_ARRAY_SIZE] = {
	0xf87d8948,	/* movq %rdi, -0x8(%rbp) */
	0xf0758948,	/* movq %rsi, -0x10(%rbp) */
	0xe8558948,	/* movq %rdx, -0x18(%rbp) */
	0xe04d8948,	/* movq %rcx, -0x20(%rbp) */
	0xd845894c,	/* movq %r8, -0x28(%rbp) */
	0xd04d894c	/* movq %r9, -0x30(%rbp) */
};

/*
 * If the return type of a function is a structure greater than 16 bytes in
 * size, %rdi will contain the address to which it should be stored, and
 * arguments will begin at %rsi.  Studio will push all of the normal argument
 * registers anyway, GCC will start pushing at %rsi, so we need a separate
 * pattern.
 */
static const uint32_t save_instr_sr[INSTR_ARRAY_SIZE-1] = {
	0xd84d894c,	/* movq %r9,-0x28(%rbp) */
	0xe045894c,	/* movq %r8,-0x20(%rbp) */
	0xe84d8948,	/* movq %rcx,-0x18(%rbp) */
	0xf0558948,	/* movq %rdx,-0x10(%rbp) */
	0xf8758948	/* movq %rsi,-0x8(%rbp) */
};

static const uint32_t save_fp_instr[] = {
	0xe5894855,	/* pushq %rbp; movq %rsp,%rbp, encoding 1 */
	0xec8b4855,	/* pushq %rbp; movq %rsp,%rbp, encoding 2 */
	0xe58948cc,	/* int $0x3; movq %rsp,%rbp, encoding 1 */
	0xec8b48cc,	/* int $0x3; movq %rsp,%rbp, encoding 2 */
	NULL
};

int
saveargs_has_args(uint8_t *ins, size_t size, uint_t argc, int start_index)
{
	int		i, j;
	uint32_t	n;

	argc = MIN((start_index + argc), INSTR_ARRAY_SIZE);

	/*
	 * Make sure framepointer has been saved.
	 */
	n = INSTR4(ins, 0);
	for (i = 0; save_fp_instr[i] != NULL; i++) {
		if (n == save_fp_instr[i])
			break;
	}

	if (save_fp_instr[i] == NULL)
		return (0);

	/*
	 * Compare against Sun Studio implementation
	 */
	for (i = 8, j = 0; i < size - 4; i++) {
		n = INSTR4(ins, i);

		if (n == save_instr[j]) {
			i += 3;
			if (++j >= argc)
				return (1);
		} else {
			break;
		}
	}

	/*
	 * Compare against GCC implementation
	 */
	for (i = 8, j = argc - 1; i < size - 4; i++) {
		n = INSTR4(ins, i);

		if (n == save_instr[j]) {
			i += 3;
			if (--j < start_index)
				return (1);
		} else {
			break;
		}
	}


	/* Look for a GCC-style returned structure */
	if (start_index != 0) {
		for (i = 8, j = 0; i < size - 8; i++) {
			n = INSTR4(ins, i);

			if (n == save_instr_sr[j]) {
				i += 3;
				if (++j >= (argc - 1))
					return (1);
			} else {
				break;
			}
		}
	}

	return (0);
}
