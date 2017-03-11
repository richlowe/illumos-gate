#! /usr/bin/sh
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

# Copyright 2017, Richard Lowe.

#	DEBUG and release builds (-D, no -F)
#	do not bringover from the parent (-n)
#	runs 'make check' (-C)
#	checks ELF ABI versioning (-A)
#	runs lint in usr/src (-l plus the LINTDIRS variable)
#	sends mail on completion (-m and the MAILTO variable)
#	checks for changes in ELF runpaths (-r)
#	build packages (-p)
#	build and use this workspace's tools in $SRC/tools (-t)
#
export NIGHTLY_OPTIONS="-nCDAlmprt"

if [[ -f $(dirname $1)/common.sh ]]; then
	source $(dirname $1)/common.sh
else
	source /opt/onbld/env/common.sh
fi
