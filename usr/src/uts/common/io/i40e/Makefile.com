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
# Copyright (c) 2018, Joyent, Inc.
#


MODULE		= i40e

# illumos source
# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		i40e_main.o		\
		i40e_osdep.o		\
		i40e_intr.o		\
		i40e_transceiver.o	\
		i40e_stats.o		\
		i40e_gld.o

# Intel source
# XXXMK: Should be sorted, but wsdiff
OBJS		+=		\
		i40e_adminq.o	\
		i40e_common.o	\
		i40e_hmc.o	\
		i40e_lan_hmc.o	\
		i40e_nvm.o	\
		i40e_dcb.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/i40e

include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

CPPFLAGS	+= -I$(UTSBASE)/common/io/i40e
CPPFLAGS	+= -I$(UTSBASE)/common/io/i40e/core

ALL_TARGET	= $(BINARY) $(CONFMOD)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

# 3rd party code
SMOFF += all_func_returns

LDFLAGS		+= -N misc/mac

MAPFILES	+= ddi mac random

.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

include $(UTSBASE)/Makefile.mapfile
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/i40e/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/i40e/core/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
