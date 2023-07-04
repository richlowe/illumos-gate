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
# Copyright (c) 2013 by Chelsio Communications, Inc. All rights reserved.
#

MODULE		= cxgbe
MOD_SRCDIR	= $(UTSBASE)/common/io/cxgbe/cxgbe

include $(UTSBASE)/Makefile.kmod

INC_PATH += -I$(UTSBASE)/common/io/cxgbe -I$(UTSBASE)/common/io/cxgbe/common \
	-I$(UTSBASE)/common/io/cxgbe/t4nex -I$(UTSBASE)/common/io/cxgbe/shared

DEPENDS_ON = misc/mac drv/ip

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/cxgbe/common/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/cxgbe/shared/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
