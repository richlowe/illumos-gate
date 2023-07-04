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
# Copyright 2024 Oxide Computer Company
#

MODULE		= igc
MOD_SRCDIR	= $(UTSBASE)/common/io/igc

CORE_OBJS	=	\
	igc_api.o	\
	igc_base.o	\
	igc_i225.o	\
	igc_mac.o	\
	igc_nvm.o	\
	igc_phy.o

# XXXMK: Should be sorted, but wsdiff
OBJS	=		\
	$(CORE_OBJS)	\
	igc.o		\
	igc_osdep.o	\
	igc_gld.o	\
	igc_ring.o	\
	igc_stat.o

include $(UTSBASE)/Makefile.kmod

INC_PATH	+= -I$(UTSBASE)/common/io/igc
DEPENDS_ON	= misc/mac

#
# Smatch gags for the core code. We should consider fixing these and
# understanding the implications of these as part of figuring out how much
# divergence here is okay. For the moment we are opting for no divergence.
#
$(OBJS_DIR)/igc_api.o := SMOFF += all_func_returns
$(OBJS_DIR)/igc_base.o := SMOFF += all_func_returns
$(OBJS_DIR)/igc_i225.o := SMOFF += all_func_returns
$(OBJS_DIR)/igc_mac.o := SMOFF += all_func_returns
$(OBJS_DIR)/igc_nvm.o := SMOFF += all_func_returns
$(OBJS_DIR)/igc_phy.o := SMOFF += all_func_returns

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/igc/core/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
