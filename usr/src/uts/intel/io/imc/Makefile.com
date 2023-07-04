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
# Copyright 2019 Joyent, Inc.
#


MODULE		= imc
MOD_SRCDIR	= $(UTSBASE)/intel/io/imc
OBJS		=		\
		imc.o		\
		imc_decode.o	\
		imc_dump.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(MOD_SRCDIR)

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(SRC)/common/mc/imc/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
