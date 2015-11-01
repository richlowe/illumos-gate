#! /usr/bin/ksh
#
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

#
# Copyright 2015, Richard Lowe.
#

mkdir /tmp/secflags-test.$$
cd /tmp/secflags-test.$$

/usr/bin/psecflags -s aslr -e sleep 100000 &
pid=$!
coreadm -p core $pid # We need to be able to reliably find the core

cleanup() {
    kill $pid >/dev/null 2>&1
    cd /
    rm -fr /tmp/secflags-test.$$
}

trap cleanup EXIT

## gcore-produced core
gcore $pid >/dev/null

cat > gcore-expected.$$ <<EOF
    namesz: 0x5
    descsz: 0xc
    type:   [ NT_SECFLAGS ]
    name:
        CORE\0
    desc: (prsecflags_t)
        pr_version:    1
        psf_effective: [ ASLR ]
        psf_inherit:   [ ASLR ]
EOF

/usr/bin/elfdump -n core.${pid} | grep -B5 -A3 prsecflags_t > gcore-output.$$

if ! diff -u gcore-expected.$$ gcore-output.$$; then
    exit 1;
fi

## kernel-produced core
kill -SEGV $pid

cat > core-expected.$$ <<EOF
    namesz: 0x5
    descsz: 0xc
    type:   [ NT_SECFLAGS ]
    name:
        CORE\0
    desc: (prsecflags_t)
        pr_version:    1
        psf_effective: [ ASLR ]
        psf_inherit:   [ ASLR ]
EOF

/usr/bin/elfdump -n core | grep -B5 -A3 prsecflags_t > core-output.$$

if ! diff -u core-expected.$$ core-output.$$; then
    exit 1;
fi

exit 0
