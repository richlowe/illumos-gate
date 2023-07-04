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
# Copyright 2018 Joyent, Inc.
#


MODULE		= mlxcx

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		mlxcx.o		\
		mlxcx_dma.o	\
		mlxcx_cmd.o	\
		mlxcx_intr.o	\
		mlxcx_gld.o	\
		mlxcx_ring.o	\
		mlxcx_sensor.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/mlxcx

include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

CPPFLAGS	+= -I$(UTSBASE)/common/io/mlxcx

ALL_TARGET	= $(BINARY) $(CONFMOD)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

LDFLAGS		+= -N misc/mac

.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/mlxcx/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
