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
 * Copyright 2012, Richard Lowe.
 */

#include <stdio.h>
#include <sys/types.h>
#include <saveargs.h>

#define	DEF_TEST(name)		\
    extern uint8_t name[];	\
    extern int name##_end

#define	SIZE_OF(name) ((caddr_t)&name##_end - (caddr_t)&name)

DEF_TEST(gcc_10_align);
DEF_TEST(gcc_10_basic);
DEF_TEST(gcc_10_stack_spill);
DEF_TEST(gcc_10_larger_fp_off);

int
main(int argc, char **argv)
{

#define	TEST_GOOD(name, argc)						\
	do {								\
		if (saveargs_has_args(name, SIZE_OF(name), argc, 0) ==	\
		    SAVEARGS_TRAD_ARGS) {				\
			printf("Pass: %s\n", #name);			\
		} else {						\
			res = 1;					\
			printf("FAIL: %s\n", #name);			\
		}							\
	} while (0)

#define	TEST_BAD(name, argc)						\
	do {								\
		if (saveargs_has_args(name, SIZE_OF(name), argc, 0) ==	\
		    SAVEARGS_NO_ARGS) {					\
			printf("Pass: %s\n", #name);			\
		} else {						\
			res = 1;					\
			printf("FAIL: %s\n", #name);			\
		}							\
	} while (0)

	int res = 0;

	TEST_GOOD(gcc_10_align, 5);
	TEST_GOOD(gcc_10_basic, 4);
	TEST_GOOD(gcc_10_stack_spill, 8);
	TEST_GOOD(gcc_10_larger_fp_off, 8);

	return (res);
}
