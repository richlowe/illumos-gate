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
# Copyright 2002 Sun Microsystems, Inc. All rights reserved.
# Use is subject to license terms.
#
# @(#)Variant.mk 1.19 06/12/12
#

TOP =		 ../../../..
include $(TOP)/rules/master.mk
include $(TOP)/rules/dmake.mk

PKG_TOP =	$(TOP)/Make
CPPFLAGS +=	-I$(PKG_TOP)/include

CCSRCS =	\
		dosys.cc \
		globals.cc \
		i18n.cc \
		macro.cc \
		misc.cc \
		mksh.cc \
		read.cc

HDRS_DIR =	$(PKG_TOP)/include/mksh
HDRS_LIST =	$(HDRS_DIR)/defs.h \
		$(CCSRCS:%.cc=$(HDRS_DIR)/%.h) \
		$(CSRCS:%.c=$(HDRS_DIR)/%.h)

.INIT: $(HDRS_LIST)

LIBNAME =	libmksh.a
MSG_FILE = libmksh.msg
I18N_DIRS = $(SRC)

#CPPFLAGS	+= -DTEAMWARE_MAKE_CMN -DDISTRIBUTED

include $(TOP)/Make/lib/Lib.mk
include $(TOP)/rules/lib.mk

