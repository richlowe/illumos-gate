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
#
# Copyright (c) 2018, Joyent, Inc.
# Copyright 2023 Oxide Computer Company
#


#
# Define the module and object file sets.
#
MODULE		= t4nex

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


OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
# Include common rules
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
# Define targets
#
ALL_TARGET = $(BINARY)
INSTALL_TARGET = $(BINARY) $(ROOTMODULE)

CFLAGS += -I$(UTSBASE)/common/io/cxgbe -I$(UTSBASE)/common/io/cxgbe/common \
	-I$(UTSBASE)/common/io/cxgbe/t4nex -I$(UTSBASE)/common/io/cxgbe/shared \
	-I$(UTSBASE)/common/io/cxgbe/firmware

#
# Driver depends
#
LDFLAGS += -N misc/mac -N drv/ip

# needs work
SMOFF += all_func_returns,snprintf_overflow

#
# Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

#
# Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/cxgbe/t4nex/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/cxgbe/common/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/cxgbe/shared/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
