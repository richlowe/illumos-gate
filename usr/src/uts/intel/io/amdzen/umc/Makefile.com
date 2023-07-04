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
# Copyright 2022 Oxide Computer Company
#


MODULE		= zen_umc

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		zen_umc.o		\
		zen_umc_decode.o	\
		zen_umc_dump.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)
CPPFLAGS	+= -I$(UTSBASE)/intel/io/amdzen/umc -I$(UTSBASE)/intel/io/amdzen
LDFLAGS		+= -Ndrv/amdzen

.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/io/amdzen/umc/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/common/mc/zen_umc/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
