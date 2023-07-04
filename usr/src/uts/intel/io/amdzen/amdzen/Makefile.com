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
# Copyright 2020 Oxide Computer Company
#

MODULE		= amdzen
OBJS		= amdzen.o zen_fabric_utils.o
MOD_SRCDIR	= $(UTSBASE)/intel/io/amdzen/amdzen

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(UTSBASE)/intel/io/amdzen

$(OBJS_DIR)/%.o:		$(SRC)/common/amdzen/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

include $(UTSBASE)/Makefile.kmod.targ
