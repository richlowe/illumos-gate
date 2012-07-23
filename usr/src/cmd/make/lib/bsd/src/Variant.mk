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
# Copyright 1996 Sun Microsystems, Inc. All rights reserved.
# Use is subject to license terms.
#
# @(#)Variant.mk 1.13 06/12/12
#

TOP =		 ../../../..
include ${TOP}/rules/master.mk

PKG_TOP =	 $(TOP)/Make
CPPFLAGS +=	 -I${PKG_TOP}/include
MSG_FILE = libbsd.msg
I18N_DIRS = $(SRC)

CCSRCS =		bsd.cc
CSRCS =

HDRS_DIR =	 ${PKG_TOP}/include/bsd

.INIT: ${HDRS_DIR}/bsd.h

LIBNAME =	 libbsd.a

include ${TOP}/rules/lib.mk

