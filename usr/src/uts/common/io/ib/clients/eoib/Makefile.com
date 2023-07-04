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

MODULE		= eoib
MOD_SRCDIR	= $(UTSBASE)/common/io/ib/clients/eoib

OBJS		=		\
		eib_adm.o	\
		eib_chan.o	\
		eib_cmn.o	\
		eib_ctl.o	\
		eib_data.o	\
		eib_fip.o	\
		eib_ibt.o	\
		eib_log.o	\
		eib_mac.o	\
		eib_main.o	\
		eib_rsrc.o	\
		eib_svc.o	\
		eib_vnic.o

include $(UTSBASE)/Makefile.kmod

CPPFLAGS += -DEIB_DEBUG

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
$(OBJS_DIR)/eib_ibt.o := SMOFF += deref_check

DEPENDS_ON	= misc/mac misc/ibtl misc/ibcm misc/ibmf

include $(UTSBASE)/Makefile.kmod.targ
