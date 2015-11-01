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

/usr/bin/psecflags -s aslr -e sleep 100000 &
pid=$!
trap 'kill $pid' EXIT

if ! pargs -x $pid | grep -q '^AT_SUN_SECFLAGS *0x00000001 aslr$'; then
    exit 1;
fi    

exit 0




