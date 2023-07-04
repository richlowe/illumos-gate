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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2016 OmniTI Computer Consulting, Inc.  All rights reserved.
# Copyright (c) 2018, Joyent, Inc.


#
#	Define the module and object file sets.
#
MODULE		= ixgbe

# illumos-specific
OBJS		=			\
		ixgbe_buf.o		\
		ixgbe_debug.o		\
		ixgbe_gld.o		\
		ixgbe_log.o		\
		ixgbe_main.o		\
		ixgbe_osdep.o		\
		ixgbe_rx.o		\
		ixgbe_stat.o		\
		ixgbe_transceiver.o	\
		ixgbe_tx.o

# objects from os-independent Intel sources
# XXXMK: Should be sorted, but wsdiff
OBJS		+=			\
		ixgbe_82598.o \
		ixgbe_82599.o \
		ixgbe_api.o \
		ixgbe_common.o \
		ixgbe_phy.o \
		ixgbe_dcb.o \
		ixgbe_dcb_82598.o \
		ixgbe_dcb_82599.o \
		ixgbe_mbx.o \
		ixgbe_vf.o \
		ixgbe_x540.o \
		ixgbe_x550.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/ixgbe

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

INC_PATH	+= -I$(UTSBASE)/common/io/ixgbe
INC_PATH	+= -I$(UTSBASE)/common/io/ixgbe/core

CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-unused-value

# 3rd party code
SMOFF += all_func_returns

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(CONFMOD)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
# Driver depends on MAC
#
LDFLAGS		+= -N misc/mac
MAPFILES	+= ddi mac random kernel

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/Makefile.mapfile
include $(UTSBASE)/$(UTSMACH)/Makefile.targ


$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/ixgbe/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/ixgbe/core/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
