#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright 2004 Sun Microsystems, Inc. All rights reserved.
# Use is subject to license terms.
#
# @(#)Makefile 1.7 06/12/12
#

TOP =		../../..
include $(TOP)/rules/variant.mk

V9_VARIANT :sh= \
if [ -x /usr/bin/isalist ] ; \
then \
    for f in `/usr/bin/isalist` ; \
    do \
	if [ "$f" = sparcv9 ] ; \
	then \
	    echo sparcv9 ; \
	    break ; \
	fi ; \
	if [ "$f" = amd64 ] ; \
	then \
	    echo amd64-S2 ; \
	    break ; \
	fi ; \
    done ; \
fi

include $(TOP)/rules/derived.mk
