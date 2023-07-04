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
# Copyright (c) 2013 by Chelsio Communications, Inc. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
# Copyright 2023 Oxide Computer Company
#

MODULE		= t4nex
MOD_SRCDIR	= $(UTSBASE)/common/io/cxgbe/t4nex

# Common
# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		t4_hw.o		\
		common.o


# Nexus
# XXXMK: Should be sorted, but wsdiff
OBJS		+=			\
		t4_nexus.o \
		t4_sge.o \
		t4_mac.o \
		t4_ioctl.o \
		shared.o \
		t4_l2t.o \
		osdep.o \
		cudbg_lib.o \
		cudbg_wtp.o \
		fastlz.o \
		fastlz_api.o \
		cudbg_common.o \
		cudbg_flash_utils.o \
		cudbg.o

include $(UTSBASE)/Makefile.kmod

INC_PATH += -I$(UTSBASE)/common/io/cxgbe -I$(UTSBASE)/common/io/cxgbe/common \
	-I$(UTSBASE)/common/io/cxgbe/t4nex -I$(UTSBASE)/common/io/cxgbe/shared \
	-I$(UTSBASE)/common/io/cxgbe/firmware

DEPENDS_ON = misc/mac drv/ip

# needs work
SMOFF += all_func_returns,snprintf_overflow

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/cxgbe/common/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/cxgbe/shared/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
