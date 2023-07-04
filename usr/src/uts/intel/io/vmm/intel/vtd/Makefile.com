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
# Copyright 2022 Oxide Computer Company
#

MODULE		= vmm_vtd
OBJS		= vtd.o
MOD_SRCDIR	= $(UTSBASE)/intel/io/vmm/intel/vtd
ROOTMODULE	= $(USR_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

MAPFILE		= $(UTSBASE)/intel/io/vmm/intel/vtd/vmm_vtd.mapfile

PRE_INC_PATH	= \
	-I$(COMPAT)/bhyve \
	-I$(COMPAT)/bhyve/amd64 \
	-I$(CONTRIB)/bhyve \
	-I$(CONTRIB)/bhyve/amd64

INC_PATH	+= -I$(UTSBASE)/intel/io/vmm -I$(UTSBASE)/intel/io/vmm/io
INC_PATH	+= -I$(UTSBASE)/intel/io/vmm -I$(OBJS_DIR)

DEPENDS_ON	= \
	drv/vmm \
	misc/acpica \
	misc/pcie

LDFLAGS		+= -M $(MAPFILE)

include $(UTSBASE)/Makefile.kmod.targ
