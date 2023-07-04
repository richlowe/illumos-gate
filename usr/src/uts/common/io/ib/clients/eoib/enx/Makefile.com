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

#
# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= eibnx
MOD_SRCDIR	= $(UTSBASE)/common/io/ib/clients/eoib/enx

# XXXMK: Should be sorted but wsdiff
OBJS		=		\
		enx_main.o	\
		enx_hdlrs.o	\
		enx_ibt.o	\
		enx_log.o	\
		enx_fip.o	\
		enx_misc.o	\
		enx_q.o		\
		enx_ctl.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

# Module specific debug flag
CPPFLAGS += -DENX_DEBUG

DEPENDS_ON	= misc/ibcm misc/ibtl

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN += -_gcc=-Wno-parentheses
CERRWARN += $(CNOWARN_UNINIT)

# needs work
$(OBJS_DIR)/enx_ibt.o := SMOFF += deref_check

include $(UTSBASE)/Makefile.kmod.targ
