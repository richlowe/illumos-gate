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
# Copyright 2013 Pluribus Networks Inc.
# Copyright 2019 Joyent, Inc.
# Copyright 2022 Oxide Computer Company
#

MODULE		= ppt
MOD_SRCDIR	= $(UTSBASE)/intel/io/vmm/io/ppt
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOTMODULE) $(ROOT_CONFFILE)

PRE_INC_PATH	= \
	-I$(COMPAT)/bhyve \
	-I$(COMPAT)/bhyve/amd64 \
	-I$(CONTRIB)/bhyve \
	-I$(CONTRIB)/bhyve/amd64

INC_PATH	+= -I$(UTSBASE)/intel/io/vmm -I$(UTSBASE)/intel/io/vmm/io
INC_PATH	+= -I$(UTSBASE)/intel/io/vmm -I$(OBJS_DIR)

MAPFILE		= $(MOD_SRCDIR)/ppt.mapfile

DEPENDS_ON	= drv/vmm misc/pcie
LDFLAGS		+= -M $(MAPFILE)

$(OBJS_DIR)/ppt.o := CERRWARN	+= -_gcc=-Wno-unused-variable

# needs work
SMOFF += all_func_returns

include $(UTSBASE)/Makefile.kmod.targ
