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
MOD_SRCDIR	= $(UTSBASE)/common/io/i40e

# illumos source
OBJS		=			\
		i40e_gld.o		\
		i40e_intr.o		\
		i40e_main.o		\
		i40e_osdep.o		\
		i40e_stats.o		\
		i40e_transceiver.o

# Intel source
OBJS		+=		\
		i40e_adminq.o	\
		i40e_common.o	\
		i40e_dcb.o	\
		i40e_hmc.o	\
		i40e_lan_hmc.o	\
		i40e_nvm.o

include $(UTSBASE)/Makefile.kmod

INC_PATH	+= -I$(UTSBASE)/common/io/i40e
INC_PATH	+= -I$(UTSBASE)/common/io/i40e/core

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

# 3rd party code
SMOFF += all_func_returns

DEPENDS_ON	= misc/mac
MAPFILES	= ddi mac random

include $(UTSBASE)/Makefile.mapfile
include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/i40e/core/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
