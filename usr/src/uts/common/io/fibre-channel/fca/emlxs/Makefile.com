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
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
# Copyright 2019 Joyent, Inc.
#

COMMON_BASE	= $(SRC)/common

#
#	Define the module and object file sets.
#
MODULE		= emlxs

OBJS		=			\
		emlxs_clock.o		\
		emlxs_dfc.o		\
		emlxs_dhchap.o		\
		emlxs_diag.o		\
		emlxs_download.o	\
		emlxs_dump.o		\
		emlxs_els.o		\
		emlxs_event.o		\
		emlxs_fcf.o		\
		emlxs_fcp.o		\
		emlxs_fct.o		\
		emlxs_hba.o		\
		emlxs_ip.o		\
		emlxs_mbox.o		\
		emlxs_mem.o		\
		emlxs_msg.o		\
		emlxs_node.o		\
		emlxs_pkt.o		\
		emlxs_sli3.o		\
		emlxs_sli4.o		\
		emlxs_solaris.o		\
		emlxs_thread.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/fibre-channel/fca/emlxs

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

intel_ARCH_FLAG		= -DEMLXS_I386
aarch64_ARCH_FLAG	= -DEMLXS_AARCH64

EMLXS_FLAGS             = $($(UTSMACH)_ARCH_FLAG)
EMLXS_FLAGS             += -DS11
EMLXS_FLAGS             += -DVERSION=\"11\"
EMLXS_FLAGS             += -DMACH=\"$(MACH)\"
EMLXS_CFLAGS            = $(EMLXS_FLAGS)
EMLXS_LFLAGS            = $(EMLXS_FLAGS)
CFLAGS	                += $(EMLXS_CFLAGS) -DEMLXS_ARCH=\"$(CLASS)\"


#
#	Overrides and depends_on
#
INC_PATH	+= -I$(ROOT)/usr/include
INC_PATH	+= -I$(UTSBASE)/common/sys
INC_PATH	+= -I$(COMMON_BASE)/bignum
INC_PATH	+= -I$(UTSBASE)/common/sys/fibre-channel
INC_PATH	+= -I$(UTSBASE)/common/sys/fibre-channel/fca
INC_PATH	+= -I$(UTSBASE)/common/sys/fibre-channel/fca/emlxs
INC_PATH	+= -I$(UTSBASE)/common/sys/fibre-channel/impl
INC_PATH	+= -I$(UTSBASE)/common/sys/fibre-channel/ulp

#
#	misc/fctl required because #ifdef MODSYM_LOAD code
#	triggered by -DS11; uses DDI calls to load FCA symbols
#
LDFLAGS		+= -Nmisc/md5 -Nmisc/sha1
LDFLAGS		+= -Nmisc/bignum -Nmisc/fctl

CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
SMOFF += indenting,deref_check,all_func_returns,index_overflow

# seems definitely wrong
$(OBJS_DIR)/emlxs_fcf.o := SMOFF += logical_instead_of_bitwise

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
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/fibre-channel/fca/emlxs/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
