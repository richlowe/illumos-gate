#!/bin/ksh
#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

# Copyright 2023, Richard Lowe.

TESTDIR=$(dirname $0)

tmpdir=/tmp/test.$$
mkdir $tmpdir
cd $tmpdir

cleanup() {
	cd /
	rm -fr $tmpdir
}

trap 'cleanup' EXIT

if [[ $PWD != $tmpdir ]]; then
	print -u2 "Failed to create temporary directory: $tmpdir"
	exit 1;
fi

if [[ -n $PROTO ]]; then
	export LD_ALTEXEC=$PROTO/bin/ld
fi

mach=$(mach)
if [[ $mach != "aarch64" ]]; then
	cflags64=-m64
else
	cflags64="-mtls-dialect=trad"
fi

gcc $cflags64 -Wall -Wextra -Werror -shared -fPIC ${TESTDIR}/lib.c \
    -DMODEL='"global-dynamic"' -o global-dynamic.64.so

if (( $? != 0 )); then
	print -u2 "Couldn't compile ${TESTDIR}/lib.c (global-dynamic.64)"
	exit 1;
fi

${TESTDIR}/loader.64 ./global-dynamic.64.so
if (( $? != 0 )); then
	print -u2 "Execution test failed (global-dynamic.64)"
	exit 1;
fi

# no 32bit support
if [[ $mach == "aarch64" ]]; then
	exit 0;
fi

gcc -m32 -Wall -Wextra -Werror -shared -fPIC ${TESTDIR}/lib.c \
    -DMODEL='"global-dynamic"' -o global-dynamic.32.so

if (( $? != 0 )); then
	print -u2 "Couldn't compile ${TESTDIR}/lib.c (global-dynamic.32)"
	exit 1;
fi

${TESTDIR}/loader.32 ./global-dynamic.32.so
if (( $? != 0 )); then
	print -u2 "Execution test failed (global-dynamic.32)"
	exit 1;
fi
