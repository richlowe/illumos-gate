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

MODULE		= viona
MOD_SRCDIR	= $(UTSBASE)/intel/io/viona

OBJS		=		\
	 	viona_hook.o	\
	 	viona_main.o	\
	 	viona_ring.o	\
	 	viona_rx.o	\
	 	viona_tx.o

ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

MAPFILE		= $(UTSBASE)/intel/io/viona/viona.mapfile

DEPENDS_ON	= \
	drv/dld \
	misc/mac \
	misc/dls \
	drv/vmm \
	misc/neti \
	misc/hook
LDFLAGS		+= -M $(MAPFILE)

include $(UTSBASE)/Makefile.kmod.targ
