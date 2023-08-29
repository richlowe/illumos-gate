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

gcc $cflags64 -Wall -Wextra -Werror ${TESTDIR}/bin.c -DMODEL='"initial-exec"' \
    -o initial-exec.64
if (( $? != 0 )); then
	print -u2 "Couldn't compile ${TESTDIR}/bin.c (initial-exec.64)"
	exit 1;
fi

./initial-exec.64
if (( $? != 0 )); then
	print -u2 "Execution test failed (initial-exec.64)"
	exit 1;
fi

# no 32bit support
if [[ $mach == "aarch64" ]]; then
	exit 0;
fi

gcc -m32 -Wall -Wextra -Werror ${TESTDIR}/bin.c -DMODEL='"initial-exec"' \
    -o initial-exec.32
if (( $? != 0 )); then
	print -u2 "Couldn't compile ${TESTDIR}/bin.c (initial-exec.32)"
	exit 1;
fi

./initial-exec.32
if (( $? != 0 )); then
	print -u2 "Execution test failed (initial-exec.32)"
	exit 1;
fi
