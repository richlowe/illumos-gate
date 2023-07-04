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


#
#	Define the module and object file sets.
#
MODULE		= emlxs_fw
OBJS		= emlxs_fw.o
OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_EMLXS_FW_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/fibre-channel/fca/emlxs


#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(CONFMOD) $(ITUMOD)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

intel_EMLXS_FLAGS             = -DEMLXS_I386
EMLXS_FLAGS		+= $($(UTSMACH)_EMLXS_FLAGS)
EMLXS_FLAGS             += -DS11
EMLXS_FLAGS             += -DVERSION=\"11\"
EMLXS_FLAGS             += -DMACH=\"$(MACH)\"
EMLXS_CFLAGS            = $(EMLXS_FLAGS)
EMLXS_LFLAGS            = $(EMLXS_FLAGS)
CFLAGS	                += $(EMLXS_CFLAGS) -DEMLXS_ARCH=\"$(CLASS)\"

INC_PATH	+= -I$(UTSBASE)/common/sys/fibre-channel/fca/emlxs

LDFLAGS		+= -Nmisc/fctl

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

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/fibre-channel/fca/emlxs/fw/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
