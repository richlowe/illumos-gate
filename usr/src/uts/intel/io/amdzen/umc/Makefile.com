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
MOD_SRCDIR	= $(UTSBASE)/intel/io/amdzen/umc

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		zen_umc.o		\
		zen_umc_decode.o	\
		zen_umc_dump.o

include $(UTSBASE)/Makefile.kmod

INC_PATH	+= -I$(UTSBASE)/intel/io/amdzen/umc -I$(UTSBASE)/intel/io/amdzen
DEPENDS_ON	= drv/amdzen

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(SRC)/common/mc/zen_umc/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
